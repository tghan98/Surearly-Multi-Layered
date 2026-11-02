/**
  ******************************************************************************
  * @file    TIM4/TIM4_TimeBase/stm8l15x_it.c
  * @author  MCD Application Team
  * @version V1.5.0
  * @date    13-May-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "stm8l15x_it.h"
#include "HW_PWR_Drive.h"
#include "HW_Tick_Drive.h"
#include "HW_UART_Drive.h"

//#include "accusense.h"


/* Private functions ---------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

#ifdef _COSMIC_
/**
  * @brief Dummy interrupt routine
  */
INTERRUPT_HANDLER(NonHandledInterrupt, 0)
{
}
#endif

/**
  * @brief TRAP interrupt routine
  */
INTERRUPT_HANDLER_TRAP(TRAP_IRQHandler)
{
}
/**
  * @brief FLASH Interrupt routine.
  */
INTERRUPT_HANDLER(FLASH_IRQHandler, 1)
{
}
/**
  * @brief DMA1 channel0 and channel1 Interrupt routine.
  */
INTERRUPT_HANDLER(DMA1_CHANNEL0_1_IRQHandler, 2)
{
}
/**
  * @brief DMA1 channel2 and channel3 Interrupt routine.
  */
INTERRUPT_HANDLER(DMA1_CHANNEL2_3_IRQHandler, 3)
{
}
/**
  * @brief RTC / CSS_LSE Interrupt routine.
  */
INTERRUPT_HANDLER(RTC_CSSLSE_IRQHandler, 4)
{
  /* Clear Interrupt pending bit */
  //RTC_ClearITPendingBit(RTC_IT_WUT);  
}
/**
  * @brief External IT PORTE/F and PVD Interrupt routine.
  */
INTERRUPT_HANDLER(EXTIE_F_PVD_IRQHandler, 5)
{
}

/**
  * @brief External IT PORTB / PORTG Interrupt routine.
  * @note This handler is implemented in User_Main.c
  */

/**
  * @brief External IT PORTD /PORTH Interrupt routine.
  */
INTERRUPT_HANDLER(EXTID_H_IRQHandler, 7)
{
}

/**
  * @brief External IT PIN0 Interrupt routine.
  */
INTERRUPT_HANDLER(EXTI0_IRQHandler, 8)
{
	//EXTI_ClearITPendingBit(EXTI_IT_Pin0);  
}

/**
  * @brief External IT PIN1 Interrupt routine.
  * @note This handler is implemented in User_Main.c
  */

/**
  * @brief External IT PIN1 Interrupt routine. Stick switch wake-up.
  */
INTERRUPT_HANDLER(EXTI1_IRQHandler, 9)
{
  HW_PWR_WakeIsr();
}

/**
  * @brief External IT PIN2 Interrupt routine.
  */
INTERRUPT_HANDLER(EXTI2_IRQHandler, 10)
{
}

/**
  * @brief External IT PIN3 Interrupt routine.
  */
INTERRUPT_HANDLER(EXTI3_IRQHandler, 11)
{
}

/**
  * @brief External IT PIN4 Interrupt routine.
  */
INTERRUPT_HANDLER(EXTI4_IRQHandler, 12)
{
}

/**
  * @brief External IT PIN5 Interrupt routine.
  */
INTERRUPT_HANDLER(EXTI5_IRQHandler, 13)
{
}

/**
  * @brief External IT PIN6 Interrupt routine.
  */
INTERRUPT_HANDLER(EXTI6_IRQHandler, 14)
{
	//EXTI_ClearITPendingBit(EXTI_IT_Pin6);  
}

/**
  * @brief External IT PIN7 Interrupt routine.
  */
INTERRUPT_HANDLER(EXTI7_IRQHandler, 15)
{
	//EXTI_ClearITPendingBit(EXTI_IT_Pin7);  
}
/**
  * @brief LCD /AES Interrupt routine.
  */
INTERRUPT_HANDLER(LCD_AES_IRQHandler, 16)
{
}
/**
  * @brief CLK switch/CSS/TIM1 break Interrupt routine.
  */
INTERRUPT_HANDLER(SWITCH_CSS_BREAK_DAC_IRQHandler, 17)
{
}

/**
  * @brief  ADC1/Comparator Interrupt routine.
  * @note   This handler is implemented in User_Main.c
  */

/**
  * @brief TIM2 Update/Overflow/Trigger/Break /USART2 TX Interrupt routine.
  */
INTERRUPT_HANDLER(TIM2_UPD_OVF_TRG_BRK_USART2_TX_IRQHandler, 19)
{
}

/**
  * @brief Timer2 Capture/Compare / USART2 RX Interrupt routine.
  */
INTERRUPT_HANDLER(TIM2_CC_USART2_RX_IRQHandler, 20)
{
}


/**
  * @brief Timer3 Update/Overflow/Trigger/Break Interrupt routine.
  */
INTERRUPT_HANDLER(TIM3_UPD_OVF_TRG_BRK_USART3_TX_IRQHandler, 21)
{
}
/**
  * @brief Timer3 Capture/Compare /USART3 RX Interrupt routine.
  */
INTERRUPT_HANDLER(TIM3_CC_USART3_RX_IRQHandler, 22)
{
}
/**
  * @brief TIM1 Update/Overflow/Trigger/Commutation Interrupt routine.
  */
INTERRUPT_HANDLER(TIM1_UPD_OVF_TRG_COM_IRQHandler, 23)
{
}
/**
  * @brief TIM1 Capture/Compare Interrupt routine.
  */
INTERRUPT_HANDLER(TIM1_CC_IRQHandler, 24)
{
}

/**
  * @brief  TIM4 Update/Overflow/Trigger Interrupt routine.
  * @note   This handler is implemented in User_Main.c
  */

/**
  * @brief SPI1 Interrupt routine.
  */
INTERRUPT_HANDLER(SPI1_IRQHandler, 26)
{
}

/**
  * @brief USART1 TX / TIM5 Update/Overflow/Trigger/Break Interrupt routine.
  * @note This handler is implemented in User_Main.c (if needed)
  */

/**
  * @brief USART1 RX / Timer5 Capture/Compare Interrupt routine.
  * @note This handler is implemented in User_Main.c
  */

/**
  * @brief I2C1 / SPI2 Interrupt routine.
  */
INTERRUPT_HANDLER(I2C1_SPI2_IRQHandler, 29)
{
}
/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/

/**
  * @brief TIM4 Update/Overflow/Trigger Interrupt routine.
  */
INTERRUPT_HANDLER(TIM4_UPD_OVF_TRG_IRQHandler, 25)
{
  HW_Tick_Isr();
}

/**
  * @brief USART1 RX / TIM5 CC Interrupt routine.
  */
INTERRUPT_HANDLER(USART1_RX_TIM5_CC_IRQHandler, 28)
{
  HW_UART_Isr();
}

