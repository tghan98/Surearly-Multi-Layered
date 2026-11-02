/**
  ******************************************************************************
  * @file    : HW_GPIO_Drive.h
  * @brief   : HW Driver layer - digital pins addressed by logical ID
  *
  * Hardware facts (v1.1 board, schematic orcad-surearly_multi-v1.1-r1574):
  *   LED1 PB4, LED2 PB3   - cathode driven, push-pull. High = off.
  *   LCD  COM  PB2, SEG BOOK PC5, SEG NO PC6, SEG YES PD0, SEG DROP PC4.
  *     The schematic draws the drop segment on PB0; the built boards use PC4.
  *     Confirmed on 13 recovered units - every unit whose display worked was
  *     PC4 (2026-10-15).
  *   STICK_SW PB1 - stick switch, pulled up, closes to ground.
  *
  * Responsibility boundary: physical levels only. This driver does not know
  * that low means "LED on" - a Module converts the active-low meaning
  * (spec v2.0.3 §12). Pin numbers never leave this driver.
  *
  * Not listed here: the photo sensor inputs (PB5/6/7) and the USART pins
  * (PA2/PA3). HW_ADC_Drive and HW_UART_Drive configure those themselves.
  *
  * Profile note: CR1632 battery. Unused pins are driven low at init so that
  * no input floats (spec §13 item 4).
  *
  * Context: main loop only.
  ******************************************************************************
  */
#ifndef __HW_GPIO_DRIVE_H__
#define __HW_GPIO_DRIVE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    HW_GPIO_LED1 = 0,        /* PB4 */
    HW_GPIO_LED2,            /* PB3 */
    HW_GPIO_LCD_COM,         /* PB2 */
    HW_GPIO_LCD_SEG_BOOK,    /* PC5 */
    HW_GPIO_LCD_SEG_NO,      /* PC6 */
    HW_GPIO_LCD_SEG_YES,     /* PD0 */
    HW_GPIO_LCD_SEG_DROP,    /* PC4, see the file header */
    HW_GPIO_STICK_SW,        /* PB1, input */
    HW_GPIO_COUNT
} HW_GPIO_Id;

/* Exported constants --------------------------------------------------------*/
#define HW_GPIO_LEVEL_LOW        0u
#define HW_GPIO_LEVEL_HIGH       1u

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Puts every pin in this table into its reset-safe state.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  *         Outputs start low, the stick switch starts as a pulled-up input.
  * @retval RET_OK
  */
Ret_t HW_GPIO_Init(void);

/**
  * @brief  Drives one output pin to a physical level.
  * @param  tId    : logical pin
  * @param  bLevel : HW_GPIO_LEVEL_LOW or HW_GPIO_LEVEL_HIGH
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_PARAM, RET_ERR_STATE (pin is an input)
  */
Ret_t HW_GPIO_Write(HW_GPIO_Id tId, uint8_t bLevel);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Reads the physical level of one pin.
  * @param  tId : logical pin
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  *         An out-of-range ID reads as low, so a caller that ignores the ID
  *         range still gets a defined value.
  * @retval HW_GPIO_LEVEL_LOW or HW_GPIO_LEVEL_HIGH
  */
uint8_t HW_GPIO_Read(HW_GPIO_Id tId);

#endif /* __HW_GPIO_DRIVE_H__ */
/*****************************END OF FILE****/
