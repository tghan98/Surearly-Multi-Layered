/**
  ******************************************************************************
  * @file    : HW_ADC_Drive.c
  * @brief   : HW Driver layer - photo sensor analog inputs
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "HW_ADC_Drive.h"
#include "stm8l15x.h"
#include "stm8l15x_adc.h"
#include "stm8l15x_clk.h"

/* Private variables ---------------------------------------------------------*/
/* Logical ID -> vendor channel. The only place channel numbers live.
   A channel code is register index (high byte) and bit (low byte); all three
   sit in the same SQR register, so only the bit is kept. The per-read paths
   write the registers directly instead of calling ADC_ChannelCmd(),
   ADC_GetFlagStatus(), ADC_GetConversionValue() and ADC_Cmd(): the same
   register operations, 87 bytes of library code (2026-11-12, flash). */
#define HW_ADC_SQR_IDX   ((uint8_t)((uint16_t)ADC_Channel_11 >> 8))
static const uint8_t s_map[HW_ADC_COUNT] =
{
    [HW_ADC_FRNT] = (uint8_t)ADC_Channel_13,   /* old ADC_IDD_FRNT_CHANNEL */
    [HW_ADC_MIDL] = (uint8_t)ADC_Channel_12,   /* old ADC_IDD_MIDL_CHANNEL */
    [HW_ADC_REAR] = (uint8_t)ADC_Channel_11    /* old ADC_IDD_REAR_CHANNEL */
};

static uint8_t s_bEnabled = 0u;

/* Functions -----------------------------------------------------------------*/
Ret_t HW_ADC_Init(void)
{
    CLK_PeripheralClockConfig(CLK_Peripheral_ADC1, ENABLE);

    ADC_Init(ADC1, ADC_ConversionMode_Single, ADC_Resolution_12Bit,
             ADC_Prescaler_1);

    /* 16 cycles (8 us), as the old alpha.2 FW. The reading is now taken
       while the 22 nF node is still charging, so the moment the sample is
       held must be short and early; the 22 nF itself supplies the sampling
       capacitor's charge, so the 100 kohm source does not need a long
       window. (hs.kim Optic_Solution v1.2, 4 and 5-6, 2026-11-11) */
    ADC_SamplingTimeConfig(ADC1, ADC_Group_SlowChannels,
                           ADC_SamplingTime_16Cycles);

    /* Schmitt triggers off on the three sensor inputs, as the old FW did
       (alpha.2 BSP_HW_Config.c): an analog level near the threshold keeps
       the digital input buffer switching and drawing current.
       A set TRIGR bit disables the trigger. All three channels sit in
       TRIGR[2] (high byte of the channel code), so one write covers them -
       ADC_SchmittTriggerConfig() would pull in a generic decoder for this. */
    ADC1->TRIGR[(uint8_t)((uint16_t)ADC_Channel_11 >> 8)] |=
        (uint8_t)((uint8_t)ADC_Channel_11 | (uint8_t)ADC_Channel_12
                  | (uint8_t)ADC_Channel_13);

    ADC1->CR1 &= (uint8_t)~ADC_CR1_ADON;
    CLK_PeripheralClockConfig(CLK_Peripheral_ADC1, DISABLE);
    s_bEnabled = 0u;

    return RET_OK;
}

Ret_t HW_ADC_Enable(void)
{
    CLK_PeripheralClockConfig(CLK_Peripheral_ADC1, ENABLE);
    ADC1->CR1 |= ADC_CR1_ADON;
    s_bEnabled = 1u;

    return RET_OK;
}

Ret_t HW_ADC_Disable(void)
{
    ADC1->CR1 &= (uint8_t)~ADC_CR1_ADON;
    CLK_PeripheralClockConfig(CLK_Peripheral_ADC1, DISABLE);
    s_bEnabled = 0u;

    return RET_OK;
}

Ret_t HW_ADC_Select(HW_ADC_Id tId)
{
    if ((uint8_t)tId >= (uint8_t)HW_ADC_COUNT)
    {
        return RET_ERR_PARAM;
    }
    if (s_bEnabled == 0u)
    {
        return RET_ERR_NOT_INIT;
    }

    ADC1->SQR[HW_ADC_SQR_IDX] |= s_map[tId];

    return RET_OK;
}

/* Conversions back to back, so only the first follows an idle gap: the
   caller discards it (RM0031 14.3.4: a conversion more than tIDLE after the
   previous one is to be discarded, keep the next; hs.kim 2026-11-17).
   Interrupts are masked by the caller for the whole burst, so the tick is
   frozen: the wait for EOC is bounded by a pass count instead. */
Ret_t HW_ADC_Burst(HW_ADC_Id tId, uint16_t *p_awOut, uint8_t bCount)
{
    uint8_t bIdx;
    uint8_t bPoll;
    Ret_t   tRet = RET_OK;

    for (bIdx = 0u; bIdx < bCount; bIdx++)
    {
        ADC1->CR1 |= ADC_CR1_START;
        bPoll = HW_ADC_EOC_POLLS;
        while (((ADC1->SR & (uint8_t)ADC_FLAG_EOC) == 0u) && (bPoll != 0u))
        {
            bPoll--;
        }
        if (bPoll == 0u)
        {
            tRet = RET_TIMEOUT;
            break;
        }
        /* Reading the result clears EOC. High byte first, as the library
           did. */
        p_awOut[bIdx] = (uint16_t)((uint16_t)ADC1->DRH << 8);
        p_awOut[bIdx] |= ADC1->DRL;
    }

    /* One analog input connected at a time, whatever the outcome. */
    ADC1->SQR[HW_ADC_SQR_IDX] &= (uint8_t)~s_map[tId];

    return tRet;
}

/*****************************END OF FILE****/
