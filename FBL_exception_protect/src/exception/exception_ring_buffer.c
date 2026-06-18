#include "exception_ring_buffer.h"
#include <string.h>
#include "Flash.h"

/* =========================================================
 * Flash 底层驱动抽象插桩函数
 * =========================================================
 * 说明：
 * 1. 环形缓冲模块所有需要调用底层 Flash 驱动的地方，都只调用下面这一组函数。
 * 2. 当前文件中给出 weak 默认实现，默认返回 NOT_READY 或按内存映射方式读取。
 * 3. 移植到 S32K144/S32K348 时，在平台适配文件中重写这些函数即可。
 * 4. 真实 Flash 擦除/编程必须满足芯片的 sector/page 对齐和最小编程粒度要求。
 */
__attribute__((weak))
ExceptionFlashRingRetE ExceptionFlashRingDrv_Init(void)
{
	InitFlash();
    return EXC_FLASH_RING_RET_OK;
}

__attribute__((weak))
ExceptionFlashRingRetE ExceptionFlashRingDrv_Erase(uint32_t flashAddr,
                                                   uint32_t len)
{
	status_t ret = EraseFlash(flashAddr, len);
	if (STATUS_SUCCESS == ret)
	{
		return EXC_FLASH_RING_RET_OK;
	}
    return EXC_FLASH_RING_RET_ERASE_FAIL;
}

__attribute__((weak))
ExceptionFlashRingRetE ExceptionFlashRingDrv_Program(uint32_t flashAddr,
                                                     const uint8_t *data,
                                                     uint32_t len)
{
	status_t ret = WriteFlash(flashAddr, len, data);
	if (STATUS_SUCCESS == ret)
	{
		return EXC_FLASH_RING_RET_OK;
	}
    return EXC_FLASH_RING_RET_PROGRAM_FAIL;
}

__attribute__((weak))
ExceptionFlashRingRetE ExceptionFlashRingDrv_Read(uint32_t flashAddr,
                                                  uint8_t *data,
                                                  uint32_t len)
{
    if ((0u == flashAddr) || (NULL == data))
    {
        return EXC_FLASH_RING_RET_PARAM_ERR;
    }

    /* 默认按 memory mapped flash 读取，S32K 内部 Flash 一般可直接读。 */
    memcpy(data, (const void *)flashAddr, len);

    return EXC_FLASH_RING_RET_OK;
}

/* =========================================================
 * 内部状态
 * ========================================================= */
static uint8_t g_exceptionFlashRingInited = 0u;
static uint32_t g_exceptionFlashRingNextSlotId = 0u;
static uint32_t g_exceptionFlashRingNextRecordIndex = 0u;
static ExceptionRecord g_exceptionFlashRingTempRecord;

/* =========================================================
 * 内部函数
 * ========================================================= */
static uint32_t ExceptionFlashRingCrc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;

    if (NULL == data)
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

static uint8_t ExceptionFlashRingIsValidRecord(const ExceptionRecord *record)
{
    uint32_t crc32;

    if (NULL == record)
    {
        return 0u;
    }

    if (EXC_RECORD_MAGIC != record->header.magic)
    {
        return 0u;
    }

    if (EXC_RECORD_STATE_VALID != record->header.recordState)
    {
        return 0u;
    }

    crc32 = ExceptionFlashRingCrc32((const uint8_t *)record,
                                    EXC_DUMP_SLOT_SIZE - 4u);

    return (crc32 == record->crc32) ? 1u : 0u;
}

static ExceptionFlashRingRetE ExceptionFlashRingReadSlot(uint32_t slotId,
                                                         ExceptionRecord *record)
{
    uint32_t slotAddr;

    if ((slotId >= EXC_DUMP_SLOT_NUM) || (NULL == record))
    {
        return EXC_FLASH_RING_RET_PARAM_ERR;
    }

    slotAddr = EXC_SLOT_ADDR(slotId);

    return ExceptionFlashRingDrv_Read(slotAddr,
                                      (uint8_t *)record,
                                      EXC_DUMP_SLOT_SIZE);
}

static ExceptionFlashRingRetE ExceptionFlashRingScanNextInfo(void)
{
    uint32_t slotId;
    uint32_t newestSlotId = 0u;
    uint32_t newestRecordIndex = 0u;
    uint8_t hasValidRecord = 0u;
    ExceptionFlashRingRetE ret;

    g_exceptionFlashRingNextSlotId = 0u;
    g_exceptionFlashRingNextRecordIndex = 0u;

    for (slotId = 0u; slotId < EXC_DUMP_SLOT_NUM; slotId++)
    {
        ret = ExceptionFlashRingReadSlot(slotId, &g_exceptionFlashRingTempRecord);
        if (EXC_FLASH_RING_RET_OK != ret)
        {
            return EXC_FLASH_RING_RET_READ_FAIL;
        }

        if (0u != ExceptionFlashRingIsValidRecord(&g_exceptionFlashRingTempRecord))
        {
            if ((0u == hasValidRecord) ||
                (g_exceptionFlashRingTempRecord.header.recordIndex > newestRecordIndex))
            {
                hasValidRecord = 1u;
                newestSlotId = slotId;
                newestRecordIndex = g_exceptionFlashRingTempRecord.header.recordIndex;
            }
        }
    }

    if (0u != hasValidRecord)
    {
        g_exceptionFlashRingNextSlotId = (newestSlotId + 1u) % EXC_DUMP_SLOT_NUM;
        g_exceptionFlashRingNextRecordIndex = newestRecordIndex + 1u;
    }

    return EXC_FLASH_RING_RET_OK;
}

static ExceptionFlashRingRetE ExceptionFlashRingEnsureInited(void)
{
    if (0u == g_exceptionFlashRingInited)
    {
        return ExceptionFlashRing_Init();
    }

    return EXC_FLASH_RING_RET_OK;
}

static uint8_t ExceptionFlashRingVerifyWrittenRecord(uint32_t slotId,
                                                     const ExceptionRecord *record)
{
    ExceptionFlashRingRetE ret;

    if (NULL == record)
    {
        return 0u;
    }

    ret = ExceptionFlashRingReadSlot(slotId, &g_exceptionFlashRingTempRecord);
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        return 0u;
    }

    return (0 == memcmp(&g_exceptionFlashRingTempRecord, record, sizeof(ExceptionRecord))) ? 1u : 0u;
}

/* =========================================================
 * 对外接口
 * ========================================================= */
ExceptionFlashRingRetE ExceptionFlashRing_Init(void)
{
    ExceptionFlashRingRetE ret;

    ret = ExceptionFlashRingDrv_Init();
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        g_exceptionFlashRingInited = 0u;
        return ret;
    }

    ret = ExceptionFlashRingScanNextInfo();
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        g_exceptionFlashRingInited = 0u;
        return ret;
    }

    g_exceptionFlashRingInited = 1u;

    return EXC_FLASH_RING_RET_OK;
}

ExceptionFlashRingRetE ExceptionFlashRing_GetNextRecordIndex(uint32_t *recordIndex)
{
    ExceptionFlashRingRetE ret;

    if (NULL == recordIndex)
    {
        return EXC_FLASH_RING_RET_PARAM_ERR;
    }

    ret = ExceptionFlashRingEnsureInited();
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        return ret;
    }

    /* 每次获取前重新扫描，避免复位后或外部写入后状态不一致。 */
    ret = ExceptionFlashRingScanNextInfo();
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        return ret;
    }

    *recordIndex = g_exceptionFlashRingNextRecordIndex;

    return EXC_FLASH_RING_RET_OK;
}

ExceptionFlashRingRetE ExceptionFlashRing_Write(const ExceptionRecord *excRecord)
{
    uint32_t slotAddr;
    ExceptionFlashRingRetE ret;

    if (NULL == excRecord)
    {
        return EXC_FLASH_RING_RET_PARAM_ERR;
    }

    ret = ExceptionFlashRingEnsureInited();
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        return ret;
    }

    slotAddr = EXC_SLOT_ADDR(g_exceptionFlashRingNextSlotId);

    ret = ExceptionFlashRingDrv_Erase(slotAddr, EXC_DUMP_SLOT_SIZE);
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        return EXC_FLASH_RING_RET_ERASE_FAIL;
    }

    ret = ExceptionFlashRingDrv_Program(slotAddr,
                                        (const uint8_t *)excRecord,
										EXC_DUMP_SLOT_SIZE);
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        return EXC_FLASH_RING_RET_PROGRAM_FAIL;
    }

    if (0u == ExceptionFlashRingVerifyWrittenRecord(g_exceptionFlashRingNextSlotId,
                                                    excRecord))
    {
        return EXC_FLASH_RING_RET_VERIFY_FAIL;
    }

    g_exceptionFlashRingNextSlotId = (g_exceptionFlashRingNextSlotId + 1u) % EXC_DUMP_SLOT_NUM;
    g_exceptionFlashRingNextRecordIndex = excRecord->header.recordIndex + 1u;

    return EXC_FLASH_RING_RET_OK;
}

ExceptionFlashRingRetE ExceptionFlashRing_Read(uint32_t slotId, ExceptionRecord *excRecord)
{
    ExceptionFlashRingRetE ret;

    if (NULL == excRecord)
    {
        return EXC_FLASH_RING_RET_PARAM_ERR;
    }

    ret = ExceptionFlashRingEnsureInited();
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        return ret;
    }

    ret = ExceptionFlashRingReadSlot(slotId, excRecord);
    if (EXC_FLASH_RING_RET_OK != ret)
    {
        return ret;
    }

    if (0u == ExceptionFlashRingIsValidRecord(excRecord))
    {
        return EXC_FLASH_RING_RET_NO_VALID_RECORD;
    }

    return EXC_FLASH_RING_RET_OK;
}
