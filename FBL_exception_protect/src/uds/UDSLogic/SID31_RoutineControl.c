/******************************************************************************
* 文件名称: SID31_RoutineControl.c
* 内容摘要: 例程控制
* 创建者の: 孔佳伟
* 个人主页: https://gitee.com/openes
* 修改记录:
******************************************************************************/

#include "SID31_RoutineControl.h"
#include "service_cfg.h"
#include "uds_service.h"
#include "Flash.h"


/*
 * 31 01 ff 00 擦除内存
 * 31 01 02 02 检查传输数据完整性
 * 31 01 02 03 更新条件查询
 * 31 01 ff 01 软硬件一致性检查
 * 31 01 df e0 从节点模式选择
 * 31 01 02 04 数字签名
*/ 


typedef enum __UDS_ROUTINE_CTRL_TYPE__
{
	UDS_ROUTINE_CTRL_NONE = 0,
	UDS_ROUTINE_CTRL_START = 0x01,
	UDS_ROUTINE_CTRL_STOP = 0x02,
	UDS_ROUTINE_CTRL_REQUEST_RESULT = 0x03
}uds_routine_ctrl_type;

// 升级标志定义
union UpdatedateFlagType
{
	uint16_t 	updateFlag;
	uint8_t 	updateFlagArr[8];
};
union UpdatedateFlagType UF;
union UpdatedateFlagType * pUF;

/******************************************************************************
* 函数名称: bool_t service_31_check_len(const uint8_t* msg_buf, uint16_t msg_dlc)
* 功能说明: 检查 31 服务数据长度是否合法
* 输入参数: uint16_t msg_dlc         --数据长度
* 输出参数: 无
* 函数返回: TRUE: 合法; FALSE: 非法
* 其它说明: 无
******************************************************************************/
bool_t service_31_check_len(const uint8_t* msg_buf, uint16_t msg_dlc)
{
	bool_t ret = TRUE;

	return ret;
}


/******************************************************************************
* 函数名称: void service_31_RoutineControl(const uint8_t* msg_buf, uint16_t msg_dlc)
* 功能说明: 31 服务 - 例程控制
* 输入参数: uint8_t*    msg_buf         --数据首地址
    　　　　uint8_t     msg_dlc         --数据长度
* 输出参数: 无
* 函数返回: 无
* 其它说明: 无
******************************************************************************/
void service_31_RoutineControl(const uint8_t* msg_buf, uint16_t msg_dlc)
{
    uint8_t subfunction;
	uint8_t rsp_buf[8];
	uint16_t rid;
	uint32_t eraseStartAddr;
	uint32_t eraseSize;

    subfunction = UDS_GET_SUB_FUNCTION (msg_buf[1]);
	rid = ((uint16_t)msg_buf[2]) << 8;
	rid |= msg_buf[3];

	rsp_buf[0] = USD_GET_POSITIVE_RSP(SID_31);
	rsp_buf[1] = msg_buf[1];
	rsp_buf[2] = msg_buf[2];
	rsp_buf[3] = msg_buf[3];

    switch (subfunction)
	{
		case UDS_ROUTINE_CTRL_START: // 01
			if(0x0203 == rid) // 检查编程前提条件
			{

			}
			if(0xFF00 == rid) // 擦除 Flash
			{
				eraseStartAddr = msg_buf[4] << 24 | msg_buf[5] << 16 | msg_buf[6] << 8 | msg_buf[7];
				eraseSize = msg_buf[8] << 24 | msg_buf[9] << 16 | msg_buf[10] << 8 | msg_buf[11];
				// 注意：擦除最小单位是 4K，这里暂时写死，擦除整个 APP flash 区域
				EraseFlash(eraseStartAddr, eraseSize);
				// EraseFlash(0x00014000, 0x0006C000);
			}
			if(0x0202 == rid) // CRC 校验
			{
				
			}

			uds_positive_rsp (rsp_buf, 4);
		    break;
		case UDS_ROUTINE_CTRL_STOP: // 02
		    uds_positive_rsp (rsp_buf,4);
		    break;
		case UDS_ROUTINE_CTRL_REQUEST_RESULT: // 03
		    uds_positive_rsp (rsp_buf,4);
		    break;
		default:
		    uds_negative_rsp (SID_31, NRC_SUBFUNCTION_NOT_SUPPORTED);
		    break;
	}
}


/****************EOF****************/
