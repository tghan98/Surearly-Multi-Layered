/**
  ******************************************************************************
  * @file    : HW_GPIO_Drive.c
  * @brief   : HW Driver layer - digital pins addressed by logical ID
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "HW_GPIO_Drive.h"
#include "stm8l15x.h"
#include "stm8l15x_gpio.h"

/* Private types -------------------------------------------------------------*/
typedef struct
{
    GPIO_TypeDef   *p_Port;
    GPIO_Pin_TypeDef tPin;
    GPIO_Mode_TypeDef tMode;
} HW_GPIO_Map;

/* Private variables ---------------------------------------------------------*/
/* Logical ID -> port, pin, reset-safe mode. The only place pin numbers live. */
static const HW_GPIO_Map s_map[HW_GPIO_COUNT] =
{
    [HW_GPIO_LED1]         = { GPIOB, GPIO_Pin_4, GPIO_Mode_Out_PP_Low_Slow  },
    [HW_GPIO_LED2]         = { GPIOB, GPIO_Pin_3, GPIO_Mode_Out_PP_Low_Slow  },
    [HW_GPIO_LCD_COM]      = { GPIOB, GPIO_Pin_2, GPIO_Mode_Out_PP_Low_Slow  },
    [HW_GPIO_LCD_SEG_BOOK] = { GPIOC, GPIO_Pin_5, GPIO_Mode_Out_PP_Low_Slow  },
    [HW_GPIO_LCD_SEG_NO]   = { GPIOC, GPIO_Pin_6, GPIO_Mode_Out_PP_Low_Slow  },
    [HW_GPIO_LCD_SEG_YES]  = { GPIOD, GPIO_Pin_0, GPIO_Mode_Out_PP_Low_Slow  },
    [HW_GPIO_LCD_SEG_DROP] = { GPIOC, GPIO_Pin_4, GPIO_Mode_Out_PP_Low_Slow  },
    [HW_GPIO_STICK_SW]     = { GPIOB, GPIO_Pin_1, GPIO_Mode_In_PU_No_IT      }
};

/* Functions -----------------------------------------------------------------*/
Ret_t HW_GPIO_Init(void)
{
    uint8_t bId;

    for (bId = 0u; bId < (uint8_t)HW_GPIO_COUNT; bId++)
    {
        GPIO_Init(s_map[bId].p_Port, s_map[bId].tPin, s_map[bId].tMode);
    }

    return RET_OK;
}

Ret_t HW_GPIO_Write(HW_GPIO_Id tId, uint8_t bLevel)
{
    if ((uint8_t)tId >= (uint8_t)HW_GPIO_COUNT)
    {
        return RET_ERR_PARAM;
    }
    if (s_map[tId].tMode == GPIO_Mode_In_PU_No_IT)
    {
        return RET_ERR_STATE;
    }

    if (bLevel == HW_GPIO_LEVEL_LOW)
    {
        GPIO_ResetBits(s_map[tId].p_Port, s_map[tId].tPin);
    }
    else
    {
        GPIO_SetBits(s_map[tId].p_Port, s_map[tId].tPin);
    }

    return RET_OK;
}

uint8_t HW_GPIO_Read(HW_GPIO_Id tId)
{
    BitStatus tBit;

    if ((uint8_t)tId >= (uint8_t)HW_GPIO_COUNT)
    {
        return HW_GPIO_LEVEL_LOW;
    }

    if (s_map[tId].tMode == GPIO_Mode_In_PU_No_IT)
    {
        tBit = GPIO_ReadInputDataBit(s_map[tId].p_Port, s_map[tId].tPin);
    }
    else
    {
        tBit = GPIO_ReadOutputDataBit(s_map[tId].p_Port, s_map[tId].tPin);
    }

    return (tBit == RESET) ? HW_GPIO_LEVEL_LOW : HW_GPIO_LEVEL_HIGH;
}

/*****************************END OF FILE****/
