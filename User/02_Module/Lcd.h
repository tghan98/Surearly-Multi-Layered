/**
  ******************************************************************************
  * @file    : Lcd.h
  * @brief   : Module layer - 4-icon segment display
  *
  * Hardware facts (v1.1 board):
  *   A static segment LCD with no controller. One COM line (PB2) and four
  *   SEG lines are driven directly from GPIO.
  *   An LCD must not see DC, so COM is toggled every refresh and each SEG is
  *   driven in phase with COM (icon off, no voltage across it) or in
  *   anti-phase (icon on, full voltage). Stopping the toggle damages the
  *   panel over time.
  *
  * Icons: BOOK (insert a stick), DROP (load the sample), YES, NO.
  *
  * Responsibility boundary: this module shows what it is told to show. Which
  * icon belongs to which test step is an App decision.
  *
  * Profile note: CR1632 battery. The panel has no backlight; the cost is the
  * GPIO toggling, which is why the refresh stays on the 10 ms slot rather
  * than running faster.
  *
  * Context: main loop only. Call Lcd_Process() from the 10 ms slot.
  ******************************************************************************
  */
#ifndef __LCD_H__
#define __LCD_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    LCD_ICON_BOOK = 0,
    LCD_ICON_DROP,
    LCD_ICON_YES,
    LCD_ICON_NO,
    LCD_ICON_COUNT
} Lcd_Icon;

typedef enum
{
    LCD_OFF = 0,
    LCD_ON,
    LCD_BLINK,
    LCD_STATE_COUNT
} Lcd_State;

/* Exported constants --------------------------------------------------------*/
#define LCD_REFRESH_MS           10u    /* COM toggle period */
#define LCD_BLINK_HALF_MS        500u   /* old LCD_BLINK_INTERVAL_MS */

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Clears every icon and starts the COM toggle from a known phase.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t Lcd_Init(void);

/**
  * @brief  Toggles COM, advances the blink timer and refreshes every SEG.
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         Must keep being called even when nothing is displayed: a stopped
  *         COM line leaves DC across the panel.
  * @retval RET_OK
  */
Ret_t Lcd_Process(void);

/**
  * @brief  Sets what one icon should do.
  * @param  tIcon  : icon
  * @param  tState : LCD_OFF, LCD_ON or LCD_BLINK
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         Takes effect at the next Lcd_Process().
  * @retval RET_OK, RET_ERR_PARAM
  */
Ret_t Lcd_SetIcon(Lcd_Icon tIcon, Lcd_State tState);

/**
  * @brief  Sets every icon to the same state. Used to clear the display.
  * @param  tState : LCD_OFF, LCD_ON or LCD_BLINK
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_PARAM
  */
Ret_t Lcd_SetAll(Lcd_State tState);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  What one icon is currently set to.
  * @param  tIcon : icon
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval LCD_OFF .. LCD_BLINK; LCD_OFF if the icon is out of range
  */
Lcd_State Lcd_GetIcon(Lcd_Icon tIcon);

#endif /* __LCD_H__ */
/*****************************END OF FILE****/
