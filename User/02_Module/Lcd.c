/**
  ******************************************************************************
  * @file    : Lcd.c
  * @brief   : Module layer - 4-icon segment display
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Lcd.h"
#include "HW_GPIO_Drive.h"
#include "HW_Tick_Drive.h"

/* Private types -------------------------------------------------------------*/
typedef struct
{
    Lcd_State atIcon[LCD_ICON_COUNT];
    uint8_t   bComPhase;      /* 0 or 1, flipped every refresh      */
    uint8_t   bBlinkOn;       /* current half of the blink period   */
    uint32_t  dwBlinkStart;
    uint32_t  dwRefreshLast;
} Lcd_Ctx;

/* Private variables ---------------------------------------------------------*/
static Lcd_Ctx s_lcd;

/* Icon -> SEG pin. */
static const HW_GPIO_Id s_segMap[LCD_ICON_COUNT] =
{
    [LCD_ICON_BOOK] = HW_GPIO_LCD_SEG_BOOK,
    [LCD_ICON_DROP] = HW_GPIO_LCD_SEG_DROP,
    [LCD_ICON_YES]  = HW_GPIO_LCD_SEG_YES,
    [LCD_ICON_NO]   = HW_GPIO_LCD_SEG_NO
};

/* Functions -----------------------------------------------------------------*/
Ret_t Lcd_Init(void)
{
    uint8_t bIcon;

    for (bIcon = 0u; bIcon < (uint8_t)LCD_ICON_COUNT; bIcon++)
    {
        s_lcd.atIcon[bIcon] = LCD_OFF;
    }

    s_lcd.bComPhase     = 0u;
    s_lcd.bBlinkOn      = 0u;
    s_lcd.dwBlinkStart  = HW_Tick_Get();
    s_lcd.dwRefreshLast = s_lcd.dwBlinkStart;

    (void)HW_GPIO_Write(HW_GPIO_LCD_COM, HW_GPIO_LEVEL_LOW);
    for (bIcon = 0u; bIcon < (uint8_t)LCD_ICON_COUNT; bIcon++)
    {
        (void)HW_GPIO_Write(s_segMap[bIcon], HW_GPIO_LEVEL_LOW);
    }

    return RET_OK;
}

Ret_t Lcd_Process(void)
{
    uint8_t bIcon;
    uint8_t bOn;
    uint8_t bSegLevel;

    if (HW_Tick_Elapsed(s_lcd.dwRefreshLast) < (uint32_t)LCD_REFRESH_MS)
    {
        return RET_OK;
    }
    s_lcd.dwRefreshLast = HW_Tick_Get();

    /* 1. Flip COM. The panel sees AC only because this keeps happening. */
    s_lcd.bComPhase = (s_lcd.bComPhase == 0u) ? 1u : 0u;
    (void)HW_GPIO_Write(HW_GPIO_LCD_COM,
                        (s_lcd.bComPhase == 0u) ? HW_GPIO_LEVEL_LOW
                                                : HW_GPIO_LEVEL_HIGH);

    /* 2. Advance the blink half-period. */
    if (HW_Tick_Elapsed(s_lcd.dwBlinkStart) >= (uint32_t)LCD_BLINK_HALF_MS)
    {
        s_lcd.dwBlinkStart = HW_Tick_Get();
        s_lcd.bBlinkOn     = (s_lcd.bBlinkOn == 0u) ? 1u : 0u;
    }

    /* 3. Drive each SEG: in phase with COM means off, anti-phase means on. */
    for (bIcon = 0u; bIcon < (uint8_t)LCD_ICON_COUNT; bIcon++)
    {
        switch (s_lcd.atIcon[bIcon])
        {
            case LCD_ON:
                bOn = 1u;
                break;

            case LCD_BLINK:
                bOn = s_lcd.bBlinkOn;
                break;

            case LCD_OFF:
            default:
                bOn = 0u;
                break;
        }

        if (bOn != 0u)
        {
            bSegLevel = (s_lcd.bComPhase == 0u) ? HW_GPIO_LEVEL_HIGH
                                                : HW_GPIO_LEVEL_LOW;
        }
        else
        {
            bSegLevel = (s_lcd.bComPhase == 0u) ? HW_GPIO_LEVEL_LOW
                                                : HW_GPIO_LEVEL_HIGH;
        }

        (void)HW_GPIO_Write(s_segMap[bIcon], bSegLevel);
    }

    return RET_OK;
}

Ret_t Lcd_SetIcon(Lcd_Icon tIcon, Lcd_State tState)
{
    if (((uint8_t)tIcon >= (uint8_t)LCD_ICON_COUNT)
        || ((uint8_t)tState >= (uint8_t)LCD_STATE_COUNT))
    {
        return RET_ERR_PARAM;
    }

    s_lcd.atIcon[tIcon] = tState;

    return RET_OK;
}

Ret_t Lcd_SetAll(Lcd_State tState)
{
    uint8_t bIcon;

    if ((uint8_t)tState >= (uint8_t)LCD_STATE_COUNT)
    {
        return RET_ERR_PARAM;
    }

    for (bIcon = 0u; bIcon < (uint8_t)LCD_ICON_COUNT; bIcon++)
    {
        s_lcd.atIcon[bIcon] = tState;
    }

    return RET_OK;
}

Lcd_State Lcd_GetIcon(Lcd_Icon tIcon)
{
    if ((uint8_t)tIcon >= (uint8_t)LCD_ICON_COUNT)
    {
        return LCD_OFF;
    }

    return s_lcd.atIcon[tIcon];
}

/*****************************END OF FILE****/
