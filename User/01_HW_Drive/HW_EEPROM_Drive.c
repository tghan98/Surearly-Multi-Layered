/**
  ******************************************************************************
  * @file    : HW_EEPROM_Drive.c
  * @brief   : HW Driver layer - on-chip data EEPROM
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "HW_EEPROM_Drive.h"
#include "stm8l15x.h"
#include "stm8l15x_flash.h"

/* Private defines -----------------------------------------------------------*/
#define HW_EEPROM_BASE_ADDR      0x001000uL   /* old EEPROM_START_ADDR */

/* Functions -----------------------------------------------------------------*/
Ret_t HW_EEPROM_Read(uint16_t wOffset, uint8_t *p_bBuf, uint16_t wLen)
{
    uint16_t wIdx;

    if (p_bBuf == NULL)
    {
        return RET_ERR_NULL;
    }
    /* Offset + length past the end. The same test as the 32-bit sum, done
       by subtraction so 16 bits cannot wrap (2026-11-20, flash). */
    if ((wLen > (uint16_t)HW_EEPROM_SIZE_BYTES)
        || (wOffset > (uint16_t)((uint16_t)HW_EEPROM_SIZE_BYTES - wLen)))
    {
        return RET_ERR_RANGE;
    }

    for (wIdx = 0u; wIdx < wLen; wIdx++)
    {
        p_bBuf[wIdx] = FLASH_ReadByte(HW_EEPROM_BASE_ADDR
                                      + (uint32_t)wOffset + (uint32_t)wIdx);
    }

    return RET_OK;
}

Ret_t HW_EEPROM_Write(uint16_t wOffset, const uint8_t *p_bBuf, uint16_t wLen)
{
    uint16_t wIdx;

    if (p_bBuf == NULL)
    {
        return RET_ERR_NULL;
    }
    /* Same range test as in HW_EEPROM_Read, 16-bit, no wrap. */
    if ((wLen > (uint16_t)HW_EEPROM_SIZE_BYTES)
        || (wOffset > (uint16_t)((uint16_t)HW_EEPROM_SIZE_BYTES - wLen)))
    {
        return RET_ERR_RANGE;
    }

    FLASH_Unlock(FLASH_MemType_Data);

    for (wIdx = 0u; wIdx < wLen; wIdx++)
    {
        FLASH_ProgramByte(HW_EEPROM_BASE_ADDR
                          + (uint32_t)wOffset + (uint32_t)wIdx, p_bBuf[wIdx]);
        (void)FLASH_WaitForLastOperation(FLASH_MemType_Data);
    }

    FLASH_Lock(FLASH_MemType_Data);

    return RET_OK;
}

/*****************************END OF FILE****/
