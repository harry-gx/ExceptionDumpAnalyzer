/******************************************************************************
* 鏂囦欢鍚嶇О: SID36_TransferData.c
* 鍐呭鎽樿: 鏁版嵁浼犺緭
* 鍒涘缓鑰呫伄: 瀛斾匠浼?
* 涓汉涓婚〉: https://gitee.com/openes
* 淇敼璁板綍:
******************************************************************************/

#include "SID37_RequestTransferExit.h"
#include "service_cfg.h"
#include "uds_service.h"
#include "add_firmware_head.h"
#include "Flash.h"


/******************************************************************************
* 鍑芥暟鍚嶇О: bool_t service_37_check_len(const uint8_t* msg_buf, uint16_t msg_dlc)
* 鍔熻兘璇存槑: 妫€鏌?37 鏈嶅姟鏁版嵁闀垮害鏄惁鍚堟硶
* 杈撳叆鍙傛暟: uint16_t msg_dlc         --鏁版嵁闀垮害
* 杈撳嚭鍙傛暟: 鏃?
* 鍑芥暟杩斿洖: TRUE: 鍚堟硶; FALSE: 闈炴硶
* 鍏跺畠璇存槑: 鏃?
******************************************************************************/
bool_t service_37_check_len(const uint8_t* msg_buf, uint16_t msg_dlc)
{
	bool_t ret = FALSE;
	
	ret = TRUE;

	return ret;
}


/******************************************************************************
* 鍑芥暟鍚嶇О: void service_37_RequestTransferExit(const uint8_t* msg_buf, uint16_t msg_dlc)
* 鍔熻兘璇存槑: 37 鏈嶅姟 - 璇锋眰閫€鍑轰紶杈?
* 杈撳叆鍙傛暟: uint8_t*    msg_buf         --鏁版嵁棣栧湴鍧€
    銆€銆€銆€銆€uint8_t     msg_dlc         --鏁版嵁闀垮害
* 杈撳嚭鍙傛暟: 鏃?
* 鍑芥暟杩斿洖: 鏃?
* 鍏跺畠璇存槑: 鏃?
******************************************************************************/
void service_37_RequestTransferExit(const uint8_t* msg_buf, uint16_t msg_dlc)
{
	uint8_t rsp_buf[8];
	app_header_t hdr;

	if (0xA55AA55A == APP_HEADER->magic)
	{
	    hdr.length = g_download_size - sizeof(app_header_t);
	    hdr.crc32 = crc32_calculate((uint8_t *)(g_download_addr + sizeof(app_header_t)), hdr.length);
	    WriteFlash(g_download_addr + 0x10, 8, (uint8_t *)&hdr.length);
	}

	g_block_count = 0;

	rsp_buf[0] = USD_GET_POSITIVE_RSP(SID_37);
	uds_positive_rsp(rsp_buf, 1);
}


/****************EOF****************/
