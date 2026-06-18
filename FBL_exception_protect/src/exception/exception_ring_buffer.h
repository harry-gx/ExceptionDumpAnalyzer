#ifndef EXCEPTION_RING_BUFFER_H
#define EXCEPTION_RING_BUFFER_H

#include <stdint.h>
#include "exception_protect.h"

/**
 * @brief Flash 环形缓冲区接口返回值
 */
typedef enum __ExceptionFlashRingRet
{
    EXC_FLASH_RING_RET_OK = 0u,         /* 操作成功 */
    EXC_FLASH_RING_RET_PARAM_ERR,       /* 参数错误 */
    EXC_FLASH_RING_RET_NOT_READY,       /* 底层 Flash 驱动未适配或模块未准备好 */
    EXC_FLASH_RING_RET_NO_VALID_RECORD, /* Flash 中没有有效记录 */
    EXC_FLASH_RING_RET_ERASE_FAIL,      /* 擦除失败 */
    EXC_FLASH_RING_RET_PROGRAM_FAIL,    /* 写入失败 */
    EXC_FLASH_RING_RET_READ_FAIL,       /* 读取失败 */
    EXC_FLASH_RING_RET_VERIFY_FAIL      /* 写入后校验失败 */
} ExceptionFlashRingRetE;

/**
 * @brief 初始化 Flash 环形缓冲区
 *
 * @details
 * 1. 调用底层 Flash 初始化插桩函数。
 * 2. 遍历 EXC_DUMP_SLOT_NUM 个 slot。
 * 3. 检查 magic、recordState 和 crc32。
 * 4. 找到最新有效记录，并计算下一次写入 slot 和下一次 recordIndex。
 */
ExceptionFlashRingRetE ExceptionFlashRing_Init(void);

/**
 * @brief 写入一帧异常记录
 *
 * @note
 * 1. 写入位置由 ExceptionFlashRing_Init() 或 ExceptionFlashRing_GetNextRecordIndex() 内部计算。
 * 2. 本接口不修改 ExceptionRecord 内容，不重新计算 crc32。
 * 3. recordIndex 和 crc32 应由 FillExceptionSlot() 完成。
 */
ExceptionFlashRingRetE ExceptionFlashRing_Write(const ExceptionRecord *excRecord);

/**
 * @brief 读取指定 slot 中的一帧异常记录
 *
 * @param slotId slot 编号，范围 0 ~ EXC_DUMP_SLOT_NUM - 1。
 * @param excRecord 读取输出缓存。
 */
ExceptionFlashRingRetE ExceptionFlashRing_Read(uint32_t slotId,
                                               ExceptionRecord *excRecord);

/**
 * @brief 获取下一条异常记录的 recordIndex
 *
 * @note
 * 该接口会同时在模块内部锁定下一次写入的 slot。
 * 后续调用 ExceptionFlashRing_Write() 会写入该 slot。
 */
ExceptionFlashRingRetE ExceptionFlashRing_GetNextRecordIndex(uint32_t *recordIndex);

#endif /* EXCEPTION_RING_BUFFER_H */
