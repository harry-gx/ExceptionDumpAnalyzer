#ifndef Flash_H
#define Flash_H

/* MODULE Flash.
 *
 * @page misra_violations MISRA-C:2012 violations
 *
 * @section [global]
 * Violates MISRA 2012 Advisory Rule 2.5, Global macro not referenced.
 * The global macro will be used in function call of the module.
 */

/* Include inherited beans */
// #include "clockMan1.h"
#include "flash_driver.h"

/*! @brief User configuration structure 0 */
extern const flash_user_config_t Flash_InitConfig;

void InitFlash(void);
status_t WriteFlash(uint32_t startAddr, uint32_t size, const uint8_t * pData);
status_t EraseFlash(uint32_t startAddr, uint32_t size);

void FlashTest(void);


#endif
/* ifndef Flash_H */
/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.1 [05.21]
**     for the Freescale S32K series of microcontrollers.
**
** ###################################################################
*/
