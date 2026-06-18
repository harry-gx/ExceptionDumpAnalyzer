#include <stdint.h>
#include <stddef.h>
#include "add_firmware_head.h"

uint32_t g_download_addr = 0;
uint32_t g_download_size = 0;
uint32_t g_block_count = 0;


__attribute__((section(".firmware_info")))
const fbl_firmware_info_t fbl_firmware_info = {
		.fill_1 = {0},
		.firmware = {
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353,
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353,
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353,
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353,
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353,
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353,
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353,
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353,
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353,
				0xAAAAAAAA, 0x4C424623, 0xAAAAAAAA, 0x4B323353},
		.fill_2 = {0}
};

uint32_t crc32_calculate(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;

    if (data == NULL)
    {
        return 0U;
    }

    uint32_t i = 0;
    uint8_t j = 0;
    for (i = 0; i < length; i++)
    {
        crc ^= data[i];

        for (j = 0; j < 8; j++)
        {
            if (crc & 1U)
            {
                crc = (crc >> 1) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}
