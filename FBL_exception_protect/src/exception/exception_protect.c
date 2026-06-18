#include "exception_protect.h"
#include "exception_ring_buffer.h"
#include "FlexCAN.h"

/* =========================================================
 * 基础寄存器定义（不依赖 CMSIS）
 * ========================================================= */
#define REG32(addr) (*(volatile uint32_t *)(addr))

/* SCB */
#define SCB_CCR         REG32(0xE000ED14u)
#define SCB_SHCSR       REG32(0xE000ED24u)
#define SCB_CFSR        REG32(0xE000ED28u)
#define SCB_HFSR        REG32(0xE000ED2Cu)
#define SCB_DFSR        REG32(0xE000ED30u)
#define SCB_MMFAR       REG32(0xE000ED34u)
#define SCB_BFAR        REG32(0xE000ED38u)
#define SCB_AFSR        REG32(0xE000ED3Cu)
#define SCB_AIRCR       REG32(0xE000ED0Cu)

/* SCB bit */
#define SCB_SHCSR_MEMFAULTENA_Msk   (1UL << 16)
#define SCB_SHCSR_BUSFAULTENA_Msk   (1UL << 17)
#define SCB_SHCSR_USGFAULTENA_Msk   (1UL << 18)

#define SCB_CCR_UNALIGN_TRP_Msk     (1UL << 3)
#define SCB_CCR_DIV_0_TRP_Msk       (1UL << 4)

/* AIRCR */
#define AIRCR_VECTKEY_Pos           (16u)
#define AIRCR_VECTKEY_Msk           (0xFFFFu << AIRCR_VECTKEY_Pos)
#define AIRCR_VECTKEY               (0x5FAu << AIRCR_VECTKEY_Pos)
#define AIRCR_SYSRESETREQ_Msk       (1u << 2)

/* =========================================================
 * .noinit 区异常记录
 * ========================================================= */
__attribute__((section(".noinit")))
volatile ExceptionRecord g_exceptionRecord;

/* =========================================================
 * 内部函数
 * ========================================================= */
static void DisableIrq(void)
{
    __asm volatile ("cpsid i" : : : "memory");
}

static void EnableIrq(void)
{
    __asm volatile ("cpsie i" : : : "memory");
}

static void SyncDataInstr(void)
{
    __asm volatile ("dsb" : : : "memory");
    __asm volatile ("isb" : : : "memory");
}

static void SystemSoftReset(void)
{
    /* 软件复位 */
    SCB_AIRCR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ_Msk;

    /* 等待复位 */
    while (1)
    {
        __asm volatile ("nop");
    }
}

static uint32_t MinU32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static uint32_t ExcCrc32(uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;

    if (data == 0)
    {
        return 0u;
    }

    while (len > 0u)
    {
        crc ^= (uint32_t)(*data);
        data++;
        len--;

        for (i = 0u; i < 8u; i++)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1u) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1u;
            }
        }
    }

    return ~crc;
}

static uint32_t ReadControl(void)
{
    uint32_t value;
    __asm volatile ("mrs %0, control" : "=r"(value));
    return value;
}

static uint32_t ReadPrimask(void)
{
    uint32_t value;
    __asm volatile ("mrs %0, primask" : "=r"(value));
    return value;
}

static uint32_t ReadBasepri(void)
{
    uint32_t value;
    __asm volatile ("mrs %0, basepri" : "=r"(value));
    return value;
}

static uint32_t ReadFaultmask(void)
{
    uint32_t value;
    __asm volatile ("mrs %0, faultmask" : "=r"(value));
    return value;
}

static uint32_t ReadMsp(void)
{
    uint32_t value;
    __asm volatile ("mrs %0, msp" : "=r"(value));
    return value;
}

static uint32_t ReadPsp(void)
{
    uint32_t value;
    __asm volatile ("mrs %0, psp" : "=r"(value));
    return value;
}

static void FillExceptionSlot(ExceptionRecord *excRecord,
							  uint32_t *stack,
							  uint32_t *swRegs,
							  RecordSpecialRegs *specialRegs)
{
	if ((NULL == excRecord) ||
	   (NULL == stack) ||
	   (NULL == swRegs) ||
	   (NULL == specialRegs))
	{
		return;
	}

	/* 先清空整帧，保证 reserved 字段和未使用字段为 0 */
	memset(excRecord, 0, sizeof(ExceptionRecord));

	/* 获取下一次写入的 Flash slot 和 recordIndex */
	uint32_t recordIndex = 0u;
	(void)ExceptionFlashRing_GetNextRecordIndex(&recordIndex);

	/* 记录头，固定 32 字节 */
	excRecord->header.magic       = EXC_RECORD_MAGIC;
	//excRecord->header.version     = 0u;
	excRecord->header.recordIndex = recordIndex;
	excRecord->header.recordState = EXC_RECORD_STATE_VALID;

    /* Cortex-M 硬件自动压栈帧，固定 32 字节 */
	excRecord->hwFrame.r0   = stack[0];
	excRecord->hwFrame.r1   = stack[1];
	excRecord->hwFrame.r2   = stack[2];
	excRecord->hwFrame.r3   = stack[3];
	excRecord->hwFrame.r12  = stack[4];
	excRecord->hwFrame.lr   = stack[5];
	excRecord->hwFrame.pc   = stack[6];
	excRecord->hwFrame.xpsr = stack[7];

    /* 软件手动保存寄存器 R4~R11，固定 32 字节 */
    excRecord->swRegs.r4  = swRegs[0];
    excRecord->swRegs.r5  = swRegs[1];
    excRecord->swRegs.r6  = swRegs[2];
    excRecord->swRegs.r7  = swRegs[3];
    excRecord->swRegs.r8  = swRegs[4];
    excRecord->swRegs.r9  = swRegs[5];
    excRecord->swRegs.r10 = swRegs[6];
    excRecord->swRegs.r11 = swRegs[7];

	/* 特殊寄存器，固定 32 字节 */
    excRecord->specialRegs = *specialRegs;

	/* Fault 状态寄存器，固定 32 字节 */
    excRecord->faultRegs.cfsr  = SCB_CFSR;
    excRecord->faultRegs.hfsr  = SCB_HFSR;
    excRecord->faultRegs.dfsr  = SCB_DFSR;
    excRecord->faultRegs.afsr  = SCB_AFSR;
    excRecord->faultRegs.mmfar = SCB_MMFAR;
    excRecord->faultRegs.bfar  = SCB_BFAR;
    excRecord->faultRegs.shcsr = SCB_SHCSR;
    excRecord->faultRegs.ccr   = SCB_CCR;

    /* 栈快照描述，固定 32 字节 */
	uint32_t stackLimit = (uint32_t)&__StackLimit;
	uint32_t stackTop   = (uint32_t)&__StackTop;
	uint32_t stackStart = (uint32_t)stack;
    excRecord->stackInfo.stackLimit = stackLimit;
    excRecord->stackInfo.stackTop   = stackTop;

    /*
     * stack 指向异常发生时硬件自动压栈帧起始地址。
     * 栈快照应保存 [stack, __StackTop) 范围。
     */
    if ((stackStart >= stackLimit) && (stackStart < stackTop))
    {
    	uint32_t stackSize = MinU32(stackTop - stackStart,
    								EXC_STACK_SNAPSHOT_SIZE);

        excRecord->stackInfo.stackStartAddr = stackStart;
        excRecord->stackInfo.stackSize      = stackSize;

        memcpy(excRecord->stackData,
               (uint8_t *)stackStart,
               stackSize);
    }
    else
    {
        /* 栈指针异常时，不拷贝栈快照，只保留寄存器现场 */
        excRecord->stackInfo.stackStartAddr = stackStart;
        excRecord->stackInfo.stackSize      = 0u;
    }

    /* 校验 crc32 字段本身不参与计算 */
    excRecord->crc32 = ExcCrc32((uint8_t *)excRecord,
    							EXC_DUMP_SLOT_SIZE - 4u);

    /* 数据同步/指令同步，尽量保证保存完成 */
    SyncDataInstr();
}

void SendExceptionData(void)
{
	uint32_t id;
	uint8_t dlc;
	uint8_t buf[8];
	ExceptionRecord *excRecord = (ExceptionRecord *)&g_exceptionRecord;

	uint32_t recordIndex = 0u;
	ExceptionFlashRing_GetNextRecordIndex(&recordIndex);

	while (recordIndex)
    {
		uint8_t ret = CAN_RecvData(&id, buf, &dlc);
		if ((0 == ret) &&
			(0x511 == id) &&
			(0x23 == buf[0]) &&
			(0xff == buf[1]))
		{
			memset(excRecord, 0, sizeof(ExceptionRecord));
			ExceptionFlashRingRetE ret = ExceptionFlashRing_Read(buf[2], excRecord);
			if (EXC_FLASH_RING_RET_OK == ret)
			{
			    uint8_t *recordBytes = (uint8_t *)excRecord;
			    uint32_t byteOffset = 0u;
			    for (byteOffset = 0u; byteOffset < EXC_DUMP_SLOT_SIZE; byteOffset += 8u)
			    {
			        CAN_SendData(0x666, recordBytes + byteOffset, 8);
			    }
			}
		}
    }
}

/* =========================================================
 * 统一异常入口
 * ========================================================= */
void ExceptionHandlerEntry(uint32_t *stack,
						   uint32_t *swRegs,
						   uint32_t excReturn,
						   uint32_t faultType)
{
	if ((NULL == stack) ||
	   (NULL == swRegs) ||
	   (0u == excReturn) ||
	   ((EXC_FAULT_NMI > faultType) &&
	   (EXC_FAULT_USAGE < faultType)))
	{
		return;
	}

    /*
     * 这些特殊寄存器先读出来，再关中断。
     * 这样 primask/basepri/faultmask 更接近进入异常后的原始状态。
     */
	RecordSpecialRegs specialRegs;
    specialRegs.msp       = ReadMsp();
    specialRegs.psp       = ReadPsp();
    specialRegs.excReturn = excReturn;
    specialRegs.control   = ReadControl();
    specialRegs.primask   = ReadPrimask();
    specialRegs.basepri   = ReadBasepri();
    specialRegs.faultmask = ReadFaultmask();
    specialRegs.faultType = faultType;

    /* 关中断，防止异常数据帧填充过程被普通中断打断 */
	DisableIrq();

    /* 填充异常数据帧 */
	ExceptionRecord *excRecord = (ExceptionRecord *)&g_exceptionRecord;
	FillExceptionSlot(excRecord, stack, swRegs, &specialRegs);

	/* 写入 Flash 环形缓冲区 */
	(void)ExceptionFlashRing_Write(excRecord);

#if 0
	/* 软件复位 */
	SystemSoftReset();
#else
	SendExceptionData();
#endif

    for (;;)
    {
    	__asm volatile ("nop");
    }
}

/* =========================================================
 * Fault Handler
 * 注意：
 * 1. 这些函数名要和你的启动文件向量表一致
 * 2. naked 函数里不要写 C 代码
 * 3. 各故障入口只负责传入 faultType，公共入口第一时间保存 R4~R11
 * ========================================================= */
__attribute__((naked, noinline)) void ExceptionCommonEntry(void)
{
    __asm volatile(
        "tst   lr, #4                \n"
        "ite   eq                    \n"
        "mrseq r0, msp               \n"
        "mrsne r0, psp               \n"
        "push  {r4-r11}              \n"
        "mov   r1, sp                \n"
        "mov   r2, lr                \n"
        "b     ExceptionHandlerEntry \n"
    );
}

__attribute__((naked)) void NMI_Handler(void)
{
    __asm volatile(
        "ldr   r3, =1                \n"
        "b     ExceptionCommonEntry  \n"
    );
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "ldr   r3, =2                \n"
        "b     ExceptionCommonEntry  \n"
    );
}

__attribute__((naked)) void MemManage_Handler(void)
{
    __asm volatile(
        "ldr   r3, =3                \n"
        "b     ExceptionCommonEntry  \n"
    );
}

__attribute__((naked)) void BusFault_Handler(void)
{
    __asm volatile(
        "ldr   r3, =4                \n"
        "b     ExceptionCommonEntry  \n"
    );
}

__attribute__((naked)) void UsageFault_Handler(void)
{
    __asm volatile(
        "ldr   r3, =5                \n"
        "b     ExceptionCommonEntry  \n"
    );
}

/* =========================================================
 * 对外接口
 * ========================================================= */
void EXC_InitExceptionModule(void)
{
    /* 使能 MemManage / BusFault / UsageFault */
    SCB_SHCSR |= (SCB_SHCSR_MEMFAULTENA_Msk |
                  SCB_SHCSR_BUSFAULTENA_Msk |
                  SCB_SHCSR_USGFAULTENA_Msk);

    /* 开启除零陷阱、非对齐陷阱 */
    SCB_CCR |= (SCB_CCR_DIV_0_TRP_Msk |
                SCB_CCR_UNALIGN_TRP_Msk);

    /* 初始化 Flash 环形缓冲区 */
    (void)ExceptionFlashRing_Init();

    /* 数据同步/指令同步，尽量保证保存完成 */
    SyncDataInstr();
}
