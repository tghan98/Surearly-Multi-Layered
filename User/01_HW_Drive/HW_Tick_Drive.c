/**
  ******************************************************************************
  * @file    : HW_Tick_Drive.c
  * @brief   : HW Driver layer - 10 ms system time base (TIM4)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "HW_Tick_Drive.h"
#include "stm8l15x.h"
#include "stm8l15x_clk.h"
#include "stm8l15x_tim4.h"

/* Private defines -----------------------------------------------------------*/
/* 155, the calculated value. See the note in HW_Tick_Drive.h: the shipped
   FW's 149 was measured to be a correction that is not needed. */
#define HW_TICK_PRESCALER        TIM4_Prescaler_128
#define HW_TICK_ARR              155u

/* Private variables ---------------------------------------------------------*/
/* Written by HW_Tick_Isr() only, read by the main loop. */
static volatile uint16_t s_wTick = 0u;

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

uint16_t HW_Tick_Get(void)
{
    uint16_t wFirst;
    uint16_t wSecond;

    /* 16-bit value on an 8-bit core: the ISR may land between byte reads,
       so read until two consecutive reads agree. */
    do
    {
        wFirst  = s_wTick;
        wSecond = s_wTick;
    } while (wFirst != wSecond);

    return wFirst;
}

uint16_t HW_Tick_Elapsed(uint16_t wStart)
{
    return (uint16_t)(HW_Tick_Get() - wStart);
}

HW_Tick_Due HW_Tick_IsDue(uint16_t *p_wLast, uint16_t wPeriod)
{
    uint16_t    wNow;
    uint16_t    wDiff;
    HW_Tick_Due tDue;

    if (p_wLast == NULL)
    {
        return HW_TICK_NOT_DUE;
    }
    if (wPeriod == 0u)
    {
        return HW_TICK_NOT_DUE;
    }

    wNow  = HW_Tick_Get();
    wDiff = (uint16_t)(wNow - *p_wLast);

    if (wDiff < wPeriod)
    {
        tDue = HW_TICK_NOT_DUE;
    }
    else if (wDiff < (uint16_t)(wPeriod + wPeriod))
    {
        *p_wLast += wPeriod;
        tDue = HW_TICK_DUE;
    }
    else
    {
        /* Two or more periods late: do not catch up, resynchronise. */
        *p_wLast = wNow;
        tDue = HW_TICK_DUE_LATE;
    }

    return tDue;
}

void HW_Tick_Isr(void)
{
    s_wTick++;
    TIM4_ClearITPendingBit(TIM4_IT_Update);
}

/*****************************END OF FILE****/
