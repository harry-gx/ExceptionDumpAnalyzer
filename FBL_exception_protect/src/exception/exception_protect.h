#ifndef EXCEPTION_PROTECT_H
#define EXCEPTION_PROTECT_H

#include <stdint.h>
#include <string.h>

#define EXC_RECORD_MAGIC (0x45584350u) /* 异常记录 magic: 'EXCP' */

#define EXC_DUMP_SLOT_NUM       (5u)          /* flash存储异常数据帧数量 */
#define EXC_DUMP_SLOT_SIZE      (0x00002000u) /* flash存储异常数据1帧大小(8KB) */
#define EXC_STACK_SNAPSHOT_SIZE (0x00001000u) /* 栈大小(4KB) */

#define EXC_FLASH_BASE_ADDR  (0x10006000u) /* flash存储起始地址 */
#define EXC_FLASH_END_ADDR   (0x10010000u) /* flash存储结束地址 */
#define EXC_FLASH_TOTAL_SIZE (0x0000A000u) /* flash存储总大小 */
//#define EXC_FLASH_BASE_ADDR  (0x10016000u) /* flash存储起始地址 */
//#define EXC_FLASH_END_ADDR   (0x10020000u) /* flash存储结束地址 */
//#define EXC_FLASH_TOTAL_SIZE (0x0000A000u) /* flash存储总大小 */
#if ((EXC_FLASH_END_ADDR - EXC_FLASH_BASE_ADDR) != EXC_FLASH_TOTAL_SIZE)
#error "S32K348 exception flash range must be 40KB."
#endif

#define EXC_RECORD_STATE_EMPTY   (0xFFFFFFFFu)
#define EXC_RECORD_STATE_VALID   (0xFFFFFFFCu)
#define EXC_RECORD_STATE_INVALID (0x00000000u)

/* flash存储异常数据偏移地址 */
#define EXC_SLOT_ADDR(slotId) \
	(EXC_FLASH_BASE_ADDR + ((uint32_t)(slotId) * EXC_DUMP_SLOT_SIZE))

#define CHECK_SIZE(T, SIZE) \
    typedef char __check_size_pos_of_##T##__[(sizeof(T) == (SIZE)) ? 1 : -1]

/**
 * @brief 异常类型
 */
typedef enum __ExceptionType
{
	EXC_FAULT_NMI   = 1u,
	EXC_FAULT_HARD  = 2u,
	EXC_FAULT_MEM   = 3u,
	EXC_FAULT_BUS   = 4u,
	EXC_FAULT_USAGE = 5u
} ExceptionTypeE;

/**
 * @brief 记录头，固定 32 字节
 */
typedef struct
{
    uint32_t magic;       /* 魔术字:固定为 EXCEPTION_DUMP_MAGIC */
    uint32_t version[4];  /* 固件版本 */
    uint32_t recordIndex; /* 全局递增记录号,用于判断最新记录 */
    uint32_t recordState; /* EMPTY / VALID / INVALID */
    uint32_t reserved;    /* 保留字段,保持 32 字节对齐 */
} RecordHeader;
CHECK_SIZE(RecordHeader, 32u);

/**
 * @brief Cortex-M 硬件自动压栈帧，固定 32 字节
 */
typedef struct
{
    uint32_t r0;   /* R0:函数参数/临时变量/返回值 */
    uint32_t r1;   /* R1:函数参数/临时变量 */
    uint32_t r2;   /* R2:函数参数/临时变量 */
    uint32_t r3;   /* R3:函数参数/临时变量 */
    uint32_t r12;  /* R12：IP寄存器（中间变量寄存器），用于函数调用时的临时存储 */
    uint32_t lr;   /* LR(R14）：链接寄存器，保存函数返回地址（异常发生前） */
    uint32_t pc;   /* PC(R15）:程序计数器，异常发生时正在执行的指令地址 */
    uint32_t xpsr; /* 程序状态寄存器（包含标志位、中断状态、执行模式等） */
} RecordHwStackFrame;
CHECK_SIZE(RecordHwStackFrame, 32u);

/**
 * @brief 软件手动保存寄存器 R4~R11，固定 32 字节
 */
typedef struct
{
    uint32_t r4;  /* R4 寄存器，Callee-saved 通用寄存器，异常发生前由软件手动保存。 */
    uint32_t r5;  /* R5 寄存器，Callee-saved 通用寄存器，异常发生前由软件手动保存。 */
    uint32_t r6;  /* R6 寄存器，Callee-saved 通用寄存器，异常发生前由软件手动保存。 */
    uint32_t r7;  /* R7 寄存器，常用作 Frame Pointer，异常回溯时可用于定位上一层栈帧。 */
    uint32_t r8;  /* R8 寄存器，Callee-saved 高位通用寄存器，异常发生前由软件手动保存。 */
    uint32_t r9;  /* R9 寄存器，Callee-saved 高位通用寄存器，部分编译环境中可作为平台寄存器使用。 */
    uint32_t r10; /* R10 寄存器，Callee-saved 高位通用寄存器，异常发生前由软件手动保存。 */
    uint32_t r11; /* R11 寄存器，常用作 Frame Pointer，异常回溯时可用于定位上一层栈帧。 */
} RecordSwRegs;
CHECK_SIZE(RecordSwRegs, 32u);

/**
 * @brief 特殊寄存器，固定 32 字节
 */
typedef struct
{
    uint32_t msp;       /* 主堆栈指针 */
    uint32_t psp;       /* 进程堆栈指针 */
    uint32_t excReturn; /* 异常返回值，用于判断使用MSP/PSP等，指示异常返回后的处理器状态 */
    uint32_t control;   /* 控制寄存器，定义栈指针选择和线程模式特权级别 */
    uint32_t primask;   /* 优先级屏蔽寄存器，用于禁用所有可配置优先级的异常 */
    uint32_t basepri;   /* 基本优先级屏蔽寄存器，用于屏蔽低于特定优先级的异常 */
    uint32_t faultmask; /* 故障屏蔽寄存器，用于屏蔽除 NMI 之外的所有异常和中断，常用于临界故障处理场景 */
    uint32_t faultType; /* 异常类型:NmiHandler、HardFault、MemManage、BusFault、UsageFault */
} RecordSpecialRegs;
CHECK_SIZE(RecordSpecialRegs, 32u);

/**
 * @brief Fault 状态寄存器，固定 32 字节
 */
typedef struct
{
    uint32_t cfsr;  /* 可配置故障状态寄存器，包含MemManage/BusFault/UsageFault的详细状态 */
    uint32_t hfsr;  /* 硬件故障状态寄存器，指示HardFault的原因 */
    uint32_t dfsr;  /* 调试故障状态寄存器，指示调试事件的原因 */
    uint32_t afsr;  /* 辅助故障状态寄存器，厂商定义的具体故障信息 */
    uint32_t mmfar; /* 存储管理故障地址寄存器，发生MemManage故障时访问的地址 */
    uint32_t bfar;  /* 总线故障地址寄存器，发生BusFault时访问的地址 */
    uint32_t shcsr; /* 系统处理程序控制和状态寄存器，包含异常使能和状态信息 */
    uint32_t ccr;   /* 配置和控制寄存器，包含栈对齐、除零陷阱等配置 */
} RecordFaultRegs;
CHECK_SIZE(RecordFaultRegs, 32u);

/**
 * @brief 栈快照描述，固定 32 字节
 */
typedef struct
{
    uint32_t stackStartAddr; /* stackData[0] 对应的真实 MCU 内存地址。 */
    uint32_t stackSize;      /* stackData 中有效数据长度，最大 4096。 */
    uint32_t stackLimit;     /* 栈底，低地址。 */
    uint32_t stackTop;       /* 栈顶，高地址。 */
    uint32_t reserved[4];    /* 保留字段，保持 32 字节对齐。 */
} RecordStackInfo;
CHECK_SIZE(RecordStackInfo, 32u);

/**
 * @brief 异常记录帧(用于异常现场保存与回溯)，固定 5120 字节 = 0x1400
 * 1. 每条异常记录固定 5KB，8 条记录组成 40KB Flash 环形缓冲区
 * 2. 该结构体同时作为设备端 Flash 存储格式和 PC 端二进制解析格式
 */
typedef struct
{
	RecordHeader header;           /* 记录头，固定 32 字节。 */
    RecordHwStackFrame hwFrame;    /* Cortex-M 硬件自动压栈帧，固定 32 字节。 */
    RecordSwRegs swRegs;           /* 软件手动保存寄存器 R4~R11，固定 32 字节。 */
    RecordSpecialRegs specialRegs; /* 特殊寄存器，固定 32 字节。 */
    RecordFaultRegs faultRegs;     /* Fault 状态寄存器，固定 32 字节。 */
    RecordStackInfo stackInfo;     /* 栈快照描述，固定 32 字节。 */

    uint8_t stackData[EXC_STACK_SNAPSHOT_SIZE]; /* 栈快照 4096 bytes */
    uint8_t reserved[3900];                     /* 保留字段，保持 32 字节对齐。 */
    uint32_t crc32;                             /* 校验 */
} ExceptionRecord;
CHECK_SIZE(ExceptionRecord, EXC_DUMP_SLOT_SIZE);

extern uint32_t __StackLimit;
extern uint32_t __StackTop;

/* 初始化异常保护模块 */
void EXC_InitExceptionModule(void);

void SendExceptionData(void);

#endif /* EXCEPTION_PROTECT_H */
