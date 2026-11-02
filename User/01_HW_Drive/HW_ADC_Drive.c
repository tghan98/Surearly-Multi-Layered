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

    /* 384 cycles: the 100 kohm source needs a long sampling window. */
    ADC_SamplingTimeConfig(ADC1, ADC_Group_SlowChannels,
                           ADC_SamplingTime_384Cycles);

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

Ret_t HW_ADC_Read(HW_ADC_Id tId, uint16_t *p_wOut)
{
    uint32_t dwStart;
    Ret_t    tRet = RET_TIMEOUT;

    if (p_wOut == NULL)
    {
        return RET_ERR_NULL;
    }
    if ((uint8_t)tId >= (uint8_t)HW_ADC_COUNT)
    {
        return RET_ERR_PARAM;
    }
    if (s_bEnabled == 0u)
    {
        return RET_ERR_NOT_INIT;
    }

    ADC_ChannelCmd(ADC1, s_map[tId], ENABLE);
    ADC_SoftwareStartConv(ADC1);

    dwStart = HW_Tick_Get();
    while (HW_Tick_Elapsed(dwStart) <= (uint32_t)HW_ADC_CONV_TIMEOUT_MS)
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

/*****************************END OF FILE****/
