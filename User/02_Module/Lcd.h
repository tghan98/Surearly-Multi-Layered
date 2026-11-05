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
#include "HW_Tick_Drive.h"   /* HW_TICK_MS(), HW_TICK_WAIT_MS() */

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    LCD_ICON_BOOK = 0,
    LCD_ICON_DROP,
    LCD_ICON_YES,
    LCD_ICON_NO,
    LCD_ICON_COUNT,
    LCD_ICON_ALL        /* animation steps only, never an index */
} Lcd_Icon;

typedef enum
{
    LCD_OFF = 0,
    LCD_ON,
    LCD_BLINK,
    LCD_STATE_COUNT
} Lcd_State;

/* One line of an animation: what to show, and for how long.
   The table itself belongs to the App - this module plays it, it does not
   know what "stick inserted" means (spec v2.0.3 §8). */
typedef struct
{
    uint8_t bIcon;      /* Lcd_Icon, or LCD_ICON_ALL                       */
    uint8_t bState;     /* Lcd_State                                       */
    uint8_t bHold;      /* ticks to hold; 0 falls through to the next line */
} Lcd_AnimStep;

/* Exported constants --------------------------------------------------------*/
#define LCD_REFRESH_TK           HW_TICK_MS(10u)   /* COM toggle period */
#define LCD_BLINK_HALF_TK        HW_TICK_MS(500u)  /* old LCD_BLINK_INTERVAL_MS */

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

/**
  * @brief  Clears every icon and drives COM and all SEG lines low.
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         For the moment before the unit goes into deep sleep. With both
  *         sides at the same level the panel sees no voltage, which is the
  *         only state in which it is safe to stop refreshing.
  *         Lcd_Init() brings it back.
  * @retval RET_OK
  */
Ret_t Lcd_Park(void);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  What one icon is currently set to.
  * @param  tIcon : icon
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval LCD_OFF .. LCD_BLINK; LCD_OFF if the icon is out of range
  */
Lcd_State Lcd_GetIcon(Lcd_Icon tIcon);

/**
  * @brief  Starts playing an animation table.
  * @param  p_steps : table, must stay valid until the animation ends
  *                   (point it at a const array, not a local)
  * @param  bCount  : number of lines, 1 .. 255
  * @param  bRepeat : 0 plays once and clears the panel, 1 loops forever
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         Advanced by Lcd_Process(), one line per refresh tick.
  *         Lcd_SetIcon(), Lcd_SetAll() and Lcd_Park() cancel it, so an error
  *         display always wins over a running animation.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_PARAM
  */
Ret_t Lcd_PlayAnim(const Lcd_AnimStep *p_steps, uint8_t bCount, uint8_t bRepeat);

/**
  * @brief  Tells whether an animation is still playing.
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         A repeating animation never reports idle.
  * @retval 1 while playing, 0 otherwise
  */
uint8_t Lcd_IsAnimBusy(void);

#endif /* __LCD_H__ */
/*****************************END OF FILE****/
