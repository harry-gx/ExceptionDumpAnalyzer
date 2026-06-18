/*
 * Copyright (c) 2014 - 2016, Freescale Semiconductor, Inc.
 * Copyright (c) 2016 - 2018, NXP.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY NXP "AS IS" AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL NXP OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*!
 * Description:
 * ==================================================================================================
 * This project provides common initialization for clocks and an LPIT channel counter function.
 * Core clock is set to 80 MHz. LPIT0 channel 0 is configured to count one second of SPLL clocks.
 * Software polls the channel抯 timeout flag and toggles the GPIO output to the LED when the flag sets.
 */

#include "device_registers.h"            /* include peripheral declarations S32K144 */
#include "clocks_and_modes.h"
#include "LPUART.h"
#include "FlexCAN.h"
#include "uds_port.h"
#include "Flash.h"
#include "add_firmware_head.h"

void PORT_init (void)
{
	/*!
	 * Pins definitions
	 * ===================================================
	 *
	 * Pin number        | Function
	 * ----------------- |------------------
	 * PTD0              | GPIO [BLUE LED]
	 */
  PCC-> PCCn[PCC_PORTD_INDEX] = PCC_PCCn_CGC_MASK; /* Enable clock for PORT D */
  PTD->PDDR |= 1<<0;            		/* Port D0:  Data Direction= output */
  PORTD->PCR[0] |=  PORT_PCR_MUX(1);  	/* Port D0:  MUX = ALT1, GPIO (to blue LED on EVB) */

  	/*!
	 * Pins definitions
	 * ===================================================
	 *
	 * Pin number        | Function
	 * ----------------- |------------------
	 * PTC6              | UART1 TX
	 * PTC7              | UART1 RX
	 */
  PCC->PCCn[PCC_PORTC_INDEX ]|=PCC_PCCn_CGC_MASK; /* Enable clock for PORTC */
  PORTC->PCR[6]|=PORT_PCR_MUX(2);	/* Port C6: MUX = ALT2, UART1 TX */
  PORTC->PCR[7]|=PORT_PCR_MUX(2);   /* Port C7: MUX = ALT2, UART1 RX */


    /*!
	 * Pins definitions
	 * ===================================================
	 *
	 * Pin number        | Function
	 * ----------------- |------------------
	 * PTE4              | CAN0_RX
	 * PTE5              | CAN0_TX
	 */
  PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;	/* Enable clock for PORTE */
  PORTE->PCR[4] |= PORT_PCR_MUX(5);	/* Port E4: MUX = ALT5, CAN0_RX */
  PORTE->PCR[5] |= PORT_PCR_MUX(5); /* Port E5: MUX = ALT5, CAN0_TX */

}

void LPIT0_init (void)
{
	/*!
	 * LPIT Clocking:
	 * ==============================
	 */
  PCC->PCCn[PCC_LPIT_INDEX] = PCC_PCCn_PCS(6);    /* Clock Src = 6 (SPLL2_DIV2_CLK)*/
  PCC->PCCn[PCC_LPIT_INDEX] |= PCC_PCCn_CGC_MASK; /* Enable clk to LPIT0 regs 		*/

  /*!
   * LPIT Initialization:
   */
  LPIT0->MCR |= LPIT_MCR_M_CEN_MASK;  /* DBG_EN-0: Timer chans stop in Debug mode */
                              	  	  /* DOZE_EN=0: Timer chans are stopped in DOZE mode */
                              	  	  /* SW_RST=0: SW reset does not reset timer chans, regs */
                              	  	  /* M_CEN=1: enable module clk (allows writing other LPIT0 regs) */
  LPIT0->TMR[0].TVAL = 40000;      	  /* Chan 0 Timeout period: 40M clocks */ // 40000000 -> 40000

  LPIT0->TMR[0].TCTRL |= LPIT_TMR_TCTRL_T_EN_MASK;
  	  	  	  	  	  	  	  /* T_EN=1: Timer channel is enabled */
                              /* CHAIN=0: channel chaining is disabled */
                              /* MODE=0: 32 periodic counter mode */
                              /* TSOT=0: Timer decrements immediately based on restart */
                              /* TSOI=0: Timer does not stop after timeout */
                              /* TROT=0 Timer will not reload on trigger */
                              /* TRG_SRC=0: External trigger soruce */
                              /* TRG_SEL=0: Timer chan 0 trigger source is selected*/
}

void WDOG_disable (void)
{
  WDOG->CNT=0xD928C520;     /* Unlock watchdog 		*/
  WDOG->TOVAL=0x0000FFFF;   /* Maximum timeout value 	*/
  WDOG->CS = 0x00002100;    /* Disable watchdog 		*/
}

// 升级标志定义
union UpdatedateFlagType
{
	uint16_t 	updateFlag;
	uint8_t 	updateFlagArr[8];
};
union UpdatedateFlagType UF;
union UpdatedateFlagType * pUF;

typedef void (*pFunction)(void);
void Jump_to_APP(uint32_t appAddr, uint32_t appHeadLen)
{
	LPUART1_transmit_string("Jump to APP.\r\n");

	appAddr += appHeadLen; /* 跳过app固件头(64字节) */
    uint32_t appStack = *(uint32_t *)(appAddr + 0);
    uint32_t appReset = *(uint32_t *)(appAddr + 4);

    /* 1. 关中断 */
    __asm volatile ("cpsid i" ::: "memory");

    /* 2. 关闭 + 清除 NVIC */
    uint8_t i = 0;
    for (i = 0; i < S32_NVIC_ICER_COUNT; i++)
    {
    	S32_NVIC->ICER[i] = 0xFFFFFFFF;
    	S32_NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 3. 设置中断向量表 */
    S32_SCB->VTOR = appAddr;

    /* 4. 设置 MSP */
    __asm volatile ("msr msp, %0" :: "r"(appStack) : );

    /* 5. 跳转 */
    pFunction JumpToApplication = (pFunction)appReset;
    JumpToApplication();

    while (1);
}

#if 0
int main(void)
{
	/*!
	* Initialization:
	* =======================
	*/
	WDOG_disable();		    /* Disable WDOG */
	PORT_init();            /* Configure ports */
	SOSC_init_8MHz();       /* Initialize system oscilator for 8 MHz xtal */
	SPLL_init_160MHz();     /* Initialize SPLL to 160 MHz with 8 MHz SOSC */
	NormalRUNmode_80MHz();  /* Init clocks: 80 MHz sysclk & core, 40 MHz bus, 20 MHz flash */
	LPIT0_init();           /* Initialize PIT0 for 1 second timeout  */
	LPUART1_init();         /* Initialize LPUART: 9600 baud, 1 stop bit, 8 bit format, no parity */
	FLEXCAN0_init();        /* Init FlexCAN0 */
	uds_init();
	InitFlash();

	LPUART1_transmit_string("FBL Init OK.\r\n");

	for (;;)
	{
		extern volatile uint32_t mainLoopCnt;
		mainLoopCnt++;

		FLEXCAN0_receive_msg();
	
		if(0 != (LPIT0->MSR & LPIT_MSR_TIF0_MASK)) // 1ms
		{
			uds_1ms_task();

			static int cnt = 0;
			if(cnt++ >= 200)
			{
				cnt = 0;
				PTD->PTOR |= 1<<0; // 200ms 翻转
			}

			LPIT0->MSR |= LPIT_MSR_TIF0_MASK; /* Clear LPIT0 timer flag 0 */
		}

		// 读取 D-Flash 中 0x10000000 处的升级标志
		pUF = (union UpdatedateFlagType *)0x10000000;
		UF.updateFlag = pUF->updateFlag;
		if (0x55AA == UF.updateFlag)
		{
			continue;
		}
		else
		{
			if (0xA55AA55A == APP_HEADER->magic)
			{
				if ((0xFFFFFFFF != APP_HEADER->length) && (0xFFFFFFFF != APP_HEADER->crc32))
				{
					uint32_t crc32_temp = crc32_calculate((uint8_t *)(APP_START_ADDR +
										sizeof(app_header_t)), APP_HEADER->length);
					if (crc32_temp == APP_HEADER->crc32)
					{
						Jump_to_APP(APP_START_ADDR, 0x00000040);
					}
				}
			}
		}
	}
}
#else



void test_4(void)
{
#if 0
    volatile int a = 1;
    volatile int b = 0;
    volatile int c = a / b;
#elif 0
    volatile uint32_t *p = (uint32_t *)0xFFFFFFFCu;
    *p = 0x12345678u;
#elif 0
    void (*func)(void) = (void (*)(void))0x40000001u;
    func();
#elif 1
    volatile int a = 10;
    volatile int b = 0;
    volatile int c;
    c = a / b;
    (void)c;
#endif
}
void test_3(void)
{
	test_4();
}
void test_2(void)
{
	test_3();
}
void test_1(void)
{
	test_2();
}

#include "exception_protect.h"
int main(void)
{
	WDOG_disable();		    /* Disable WDOG */
	PORT_init();            /* Configure ports */
	SOSC_init_8MHz();       /* Initialize system oscilator for 8 MHz xtal */
	SPLL_init_160MHz();     /* Initialize SPLL to 160 MHz with 8 MHz SOSC */
	NormalRUNmode_80MHz();  /* Init clocks: 80 MHz sysclk & core, 40 MHz bus, 20 MHz flash */
	LPIT0_init();           /* Initialize PIT0 for 1 second timeout  */
	LPUART1_init();         /* Initialize LPUART: 9600 baud, 1 stop bit, 8 bit format, no parity */
	FLEXCAN0_init();        /* Init FlexCAN0 */
	uds_init();
	//InitFlash();

	LPUART1_transmit_string("FBL Init OK.\r\n");

    /* 初始化异常保护模块 */
	EXC_InitExceptionModule();

	test_1();

    while (1)
    {
        /* 正常业务 */
    }
}
#endif

