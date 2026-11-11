/**
  ******************************************************************************
  * @file    : HW_ADC_Drive.c
  * @brief   : HW Driver layer - photo sensor analog inputs
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "HW_ADC_Drive.h"
#include "HW_Tick_Drive.h"
#include "stm8l15x.h"
#include "stm8l15x_adc.h"
#include "stm8l15x_clk.h"

/* Private variables ---------------------------------------------------------*/
/* Logical ID -> vendor channel. The only place channel numbers live. */
static const ADC_Channel_TypeDef s_map[HW_ADC_COUNT] =
{
    [HW_ADC_FRNT] = ADC_Channel_13,   /* old ADC_IDD_FRNT_CHANNEL */
    [HW_ADC_MIDL] = ADC_Channel_12,   /* old ADC_IDD_MIDL_CHANNEL */
    [HW_ADC_REAR] = ADC_Channel_11    /* old ADC_IDD_REAR_CHANNEL */
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

    ADC_Cmd(ADC1, DISABLE);
    CLK_PeripheralClockConfig(CLK_Peripheral_ADC1, DISABLE);
    s_bEnabled = 0u;

    return RET_OK;
}

Ret_t HW_ADC_Enable(void)
{
    CLK_PeripheralClockConfig(CLK_Peripheral_ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);
    s_bEnabled = 1u;

    return RET_OK;
}

Ret_t HW_ADC_Disable(void)
{
    ADC_Cmd(ADC1, DISABLE);
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

    ADC_ChannelCmd(ADC1, s_map[tId], ENABLE);

    return RET_OK;
}

void HW_ADC_Start(void)
{
    ADC_SoftwareStartConv(ADC1);
}

Ret_t HW_ADC_Fetch(HW_ADC_Id tId, uint16_t *p_wOut)
{
    uint16_t wStart;
    Ret_t    tRet = RET_TIMEOUT;

    wStart = HW_Tick_Get();
    while (HW_Tick_Elapsed(wStart) <= HW_ADC_CONV_TIMEOUT_TK)
    {
        if (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) != RESET)
        {
            /* Reading the result clears EOC. */
            *p_wOut = ADC_GetConversionValue(ADC1);
            tRet = RET_OK;
            break;
        }
    }

    /* One analog input connected at a time, whatever the outcome. */
    ADC_ChannelCmd(ADC1, s_map[tId], DISABLE);

    return tRet;
}

Ret_t HW_ADC_Read(HW_ADC_Id tId, uint16_t *p_wOut)
{
    Ret_t tRet;

    tRet = HW_ADC_Select(tId);
    if (tRet != RET_OK)
    {
        return tRet;
    }
    HW_ADC_Start();

    return HW_ADC_Fetch(tId, p_wOut);
}

/*****************************END OF FILE****/
