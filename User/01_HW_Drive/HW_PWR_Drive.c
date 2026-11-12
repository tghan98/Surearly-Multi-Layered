/**
  ******************************************************************************
  * @file    : HW_PWR_Drive.c
  * @brief   : HW Driver layer - deep sleep, wake-up source, MCU reset
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "HW_PWR_Drive.h"
#include "stm8l15x.h"
#include "stm8l15x_clk.h"
#include "stm8l15x_exti.h"
#include "stm8l15x_gpio.h"
#include "stm8l15x_pwr.h"
#include "stm8l15x_wwdg.h"

/* Private defines -----------------------------------------------------------*/
#define HW_PWR_WAKE_PORT         GPIOB
#define HW_PWR_WAKE_PIN          GPIO_Pin_1      /* stick switch, old STP_CK */
#define HW_PWR_WAKE_EXTI         EXTI_Pin_1

/* Functions -----------------------------------------------------------------*/
Ret_t HW_PWR_PrepareHalt(void)
{
    /* The switch closes to ground, so the falling edge is the insertion.
       Floating: the board's 100 kohm resistor is the pull-up. */
    GPIO_Init(HW_PWR_WAKE_PORT, HW_PWR_WAKE_PIN, GPIO_Mode_In_FL_IT);

    /* Falling edge only on pin 1. Written to the register directly:
       EXTI_SetPinSensitivity() is a 160-byte switch over all eight pins,
       and this is its EXTI_Pin_1 case, nothing else (2026-11-12, flash). */
    disableInterrupts();
    EXTI->CR1 = (uint8_t)((EXTI->CR1 & (uint8_t)~EXTI_CR1_P1IS)
                          | (uint8_t)((uint8_t)EXTI_Trigger_Falling
                                      << (uint8_t)HW_PWR_WAKE_EXTI));
    enableInterrupts();

    /* Ultra-low-power regulator: this is most of the 382 uA -> 71 uA step. */
    PWR_UltraLowPowerCmd(ENABLE);

    /* Gate every peripheral clock. Keeping this in one place matches the old
       Power_PrepareSleep(), and the measured 71 uA depends on all three being
       off. Pins are parked by the module that owns them (Lcd_Park,
       Optic_Abort) - this driver only handles the power domain. */
    CLK_PeripheralClockConfig(CLK_Peripheral_TIM4, DISABLE);
    CLK_PeripheralClockConfig(CLK_Peripheral_ADC1, DISABLE);
    CLK_PeripheralClockConfig(CLK_Peripheral_USART1, DISABLE);

    return RET_OK;
}

Ret_t HW_PWR_EnterHalt(void)
{
    halt();

    return RET_OK;
}

Ret_t HW_PWR_Resume(void)
{
    PWR_UltraLowPowerCmd(DISABLE);
    CLK_PeripheralClockConfig(CLK_Peripheral_TIM4, ENABLE);

    return RET_OK;
}

void HW_PWR_SystemReset(void)
{
    /* Shortest legal window: the refresh never comes, so the watchdog fires. */
    WWDG_SetCounter(0x80u);
    WWDG_Enable(0x80u);

    for (;;)
    {
        /* waiting for the reset */
    }
}

void HW_PWR_WakeIsr(void)
{
    EXTI_ClearITPendingBit(EXTI_IT_Pin1);
}

/*****************************END OF FILE****/
