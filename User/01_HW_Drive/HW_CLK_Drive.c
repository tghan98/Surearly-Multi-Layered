/**
  ******************************************************************************
  * @file    : HW_CLK_Drive.c
  * @brief   : HW Driver layer - system clock source and speed
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "HW_CLK_Drive.h"
#include "stm8l15x.h"
#include "stm8l15x_clk.h"

/* Functions -----------------------------------------------------------------*/
Ret_t HW_CLK_Init(void)
{
    /* HSI is the reset default source; only the divider is changed.
       (old DM_HW_Drv_SystemClock_Init) */
    CLK_SYSCLKDivConfig(CLK_SYSCLKDiv_8);

    return RET_OK;
}

/*****************************END OF FILE****/
