/******************************************************************************
* 文件名称: SID34_RequestDownload.c
* 内容摘要: 请求下载
* 创建者の: 孔佳伟
* 个人主页: https://gitee.com/openes
* 修改记录:
******************************************************************************/

#include "SID34_RequestDownload.h"
#include "service_cfg.h"
#include "uds_service.h"
#include "add_firmware_head.h"


// 36 服务数据传输报文总大小
#define TOTAL_LEN_36	(512 + 2)


/******************************************************************************
* 函数名称: bool_t service_34_check_len(const uint8_t* msg_buf, uint16_t msg_dlc)
* 功能说明: 检查 34 服务数据长度是否合法
* 输入参数: uint16_t msg_dlc         --数据长度
* 输出参数: 无
* 函数返回: TRUE: 合法; FALSE: 非法
* 其它说明: 无
******************************************************************************/
bool_t service_34_check_len(const uint8_t* msg_buf, uint16_t msg_dlc)
{
	bool_t ret = FALSE;
	
	ret = TRUE;

	return ret;
}


/******************************************************************************
* 函数名称: void service_34_RequestDownload(const uint8_t* msg_buf, uint16_t msg_dlc)
* 功能说明: 34 服务 - 请求下载
* 输入参数: uint8_t*    msg_buf         -- 数据首地址
*          uint16_t    msg_dlc         -- 数据长度
* 输出参数: 无
* 函数返回: 无
* 其它说明:
*   1. 按 QT 工程当前发包格式解析：34 00 44 AA AA AA AA LL LL LL LL
*   2. 即：
*      msg_buf[0] = 0x34
*      msg_buf[1] = DataFormatIdentifier，当前应为 0x00
*      msg_buf[2] = AddressAndLengthFormatIdentifier，当前应为 0x44
*      后面 4 字节地址 + 4 字节长度
******************************************************************************/
void service_34_RequestDownload(const uint8_t* msg_buf, uint16_t msg_dlc)
{
    uint8_t rsp_buf[8];
    uint8_t dataFormatId;
    uint8_t addrLenFormat;
    uint8_t mem_addr_len;
    uint8_t mem_size_len;
    uint32_t download_addr = 0;
    uint32_t download_size = 0;
    uint16_t i;
    uint16_t index;

    if ((msg_buf == 0) || (msg_dlc < 3))
    {
        /* 长度错误 */
        uds_negative_rsp(SID_34, NRC_INVALID_MESSAGE_LENGTH_OR_FORMAT);
        return;
    }

    /* QT 当前发送格式:
       34 00 44 addr[4] size[4]
    */
    dataFormatId  = msg_buf[1];
    addrLenFormat = msg_buf[2];

    /* 当前 QT 工程固定发 0x00 */
    if (dataFormatId != 0x00)
    {
        uds_negative_rsp(SID_34, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    /* ALFID:
       高4位 = 长度字节数
       低4位 = 地址字节数
       QT 当前发 0x44 -> 4字节地址 + 4字节长度
    */
    mem_size_len = (uint8_t)((addrLenFormat >> 4) & 0x0F);
    mem_addr_len = (uint8_t)(addrLenFormat & 0x0F);

    if ((mem_addr_len == 0U) || (mem_size_len == 0U) ||
        (mem_addr_len > 4U) || (mem_size_len > 4U))
    {
        uds_negative_rsp(SID_34, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    /* 校验总长度:
       SID(1) + DFI(1) + ALFID(1) + addr(n) + size(n)
    */
    if (msg_dlc != (uint16_t)(3U + mem_addr_len + mem_size_len))
    {
        uds_negative_rsp(SID_34, NRC_INVALID_MESSAGE_LENGTH_OR_FORMAT);
        return;
    }

    index = 3U;

    /* 解析地址 */
    for (i = 0U; i < mem_addr_len; i++)
    {
        download_addr = (download_addr << 8) | msg_buf[index + i];
    }
    index = (uint16_t)(index + mem_addr_len);

    /* 解析长度 */
    for (i = 0U; i < mem_size_len; i++)
    {
        download_size = (download_size << 8) | msg_buf[index + i];
    }

    /* 基本合法性检查 */
    if (download_size == 0U)
    {
        uds_negative_rsp(SID_34, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    /* 这里把解析出来的地址和长度保存起来，供 36/37 使用 */
    g_download_addr = download_addr;
    g_download_size = download_size;
    g_block_count = 0;

    /* 如果你有地址范围校验，建议在这里加，例如：
    if ((download_addr < APP_START_ADDR) ||
        ((download_addr + download_size) > APP_END_ADDR))
    {
        uds_negative_rsp(SID_34, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }
    */

    rsp_buf[0] = USD_GET_POSITIVE_RSP(SID_34);
    rsp_buf[1] = 0x20;
    rsp_buf[2] = (uint8_t)(TOTAL_LEN_36 >> 8);
    rsp_buf[3] = (uint8_t)(TOTAL_LEN_36 >> 0);
    uds_positive_rsp(rsp_buf, 4);
}

/****************EOF****************/
