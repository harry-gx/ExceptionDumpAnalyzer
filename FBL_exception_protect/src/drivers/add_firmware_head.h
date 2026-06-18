/******************************************************************************
* 鏂囦欢鍚嶇О: service_cfg.h
* 鍐呭鎽樿: UDS 璇婃柇璇锋眰鏈嶅姟閰嶇疆澶存枃浠?
* 鍒涘缓鑰呫伄: 瀛斾匠浼?
* 涓汉涓婚〉: https://gitee.com/openes
* 淇敼璁板綍:
******************************************************************************/

#ifndef _ADD_FIRMWARE_HEAD_H_
#define _ADD_FIRMWARE_HEAD_H_

typedef struct
{
    uint32_t fill_1[54];   /* 濉厖  */
    uint32_t firmware[40]; /* 鍥轰欢淇℃伅 */
    uint32_t fill_2[54];   /* 濉厖 */
} fbl_firmware_info_t;

typedef struct
{
    uint32_t magic;    /* 榄旀湳瀛? */
    uint32_t firmware; /* 鍥轰欢淇℃伅 */
    uint32_t board;    /* 鏉垮崱淇℃伅 */
    uint32_t version;  /* 鍥轰欢鐗堟湰 */
    uint32_t length;   /* 鍥轰欢闀垮害 */
    uint32_t crc32;    /* CRC鏍￠獙 */
    uint32_t resv[10]; /* 棰勭暀 */
} app_header_t;

#define APP_START_ADDR  0x00014000
#define APP_HEADER  ((const app_header_t *)APP_START_ADDR)

extern uint32_t g_download_addr;
extern uint32_t g_download_size;
extern uint32_t g_block_count;

uint32_t crc32_calculate(const uint8_t *data, uint32_t length);

#endif
/****************EOF****************/
