/**
  ******************************************************************************
  * @file    : HW_Tick_Drive.c
  * @brief   : HW Driver layer - 1 ms system time base (TIM4)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "HW_Tick_Drive.h"
#include "stm8l15x.h"
#include "stm8l15x_clk.h"
#include "stm8l15x_tim4.h"

/* Private defines -----------------------------------------------------------*/
/* TEMPORARY: nominal value, see the note in HW_Tick_Drive.h. */
#define HW_TICK_PRESCALER        TIM4_Prescaler_8
#define HW_TICK_ARR              249u

/* Private variables ---------------------------------------------------------*/
/* Written by HW_Tick_Isr() only, read by the main loop. */
static volatile uint32_t s_dwTickMs = 0uL;

/* Functions -----------------------------------------------------------------*/
Ret_t HW_Tick_Init(void)
{
    CLK_PeripheralClockConfig(CLK_Peripheral_TIM4, ENABLE);

    TIM4_TimeBaseInit(HW_TICK_PRESCALER, (uint8_t)HW_TICK_ARR);
    TIM4_ClearFlag(TIM4_FLAG_Update);
    TIM4_ITConfig(TIM4_IT_Update, ENABLE);
    TIM4_Cmd(ENABLE);

    return RET_OK;
}

uint32_t HW_Tick_Get(void)
{
    uint32_t dwFirst;
    uint32_t dwSecond;

    /* 32-bit value on an 8-bit core: the ISR may land between byte reads,
       so read until two consecutive reads agree. */
    do
    {
        dwFirst  = s_dwTickMs;
        dwSecond = s_dwTickMs;
    } while (dwFirst != dwSecond);

    return dwFirst;
}

uint32_t HW_Tick_Elapsed(uint32_t dwStart)
{
    return (uint32_t)(HW_Tick_Get() - dwStart);
}

HW_Tick_Due HW_Tick_IsDue(uint32_t *p_dwLast, uint32_t dwPeriodMs)
{
    uint32_t    dwNow;
    uint32_t    dwDiff;
    HW_Tick_Due tDue;

    if (p_dwLast == NULL)
    {
        return HW_TICK_NOT_DUE;
    }
    if (dwPeriodMs == 0uL)
    {
        return HW_TICK_NOT_DUE;
    }

    dwNow  = HW_Tick_Get();
    dwDiff = (uint32_t)(dwNow - *p_dwLast);

    if (dwDiff < dwPeriodMs)
    {
        tDue = HW_TICK_NOT_DUE;
    }
    else if (dwDiff < (dwPeriodMs + dwPeriodMs))
    {
        *p_dwLast += dwPeriodMs;
        tDue = HW_TICK_DUE;
    }
    else
    {
        /* Two or more periods late: do not catch up, resynchronise. */
        *p_dwLast = dwNow;
        tDue = HW_TICK_DUE_LATE;
    }

    return tDue;
}

void HW_Tick_Isr(void)
{
    s_dwTickMs++;
    TIM4_ClearITPendingBit(TIM4_IT_Update);
}

/*****************************END OF FILE****/
