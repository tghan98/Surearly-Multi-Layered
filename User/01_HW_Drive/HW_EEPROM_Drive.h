/**
  ******************************************************************************
  * @file    : HW_EEPROM_Drive.h
  * @brief   : HW Driver layer - on-chip data EEPROM
  *
  * Hardware facts (STM8L151F3):
  *   256 bytes of data EEPROM at 0x001000. Byte programming takes a few ms
  *   and the block must be unlocked first.
  *
  * Responsibility boundary: offset in, bytes in or out. What lives at which
  * offset is the Module's and App's business.
  *
  * Profile note: CR1632 battery. Spec v2.0.3 §13 item 9 asks for short writes
  * of changed bytes only. This driver writes exactly the bytes it is handed,
  * so the Module is the one that must compare first and split long writes
  * across several passes.
  *
  * Differences from the old FW (DM_HW_Drv_EEPROM_*):
  *   - Out-of-range offsets are rejected with RET_ERR_RANGE. The old code
  *     silently skipped every byte past the 256-byte boundary, so a caller
  *     could believe a write succeeded when nothing was stored.
  *
  * Context: main loop only.
  ******************************************************************************
  */
#ifndef __HW_EEPROM_DRIVE_H__
#define __HW_EEPROM_DRIVE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported constants --------------------------------------------------------*/
#define HW_EEPROM_SIZE_BYTES     256u

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Reads a block of bytes.
  * @param  wOffset : 0 .. HW_EEPROM_SIZE_BYTES-1
  * @param  p_bBuf  : destination, must not be NULL
  * @param  wLen    : how many bytes
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_RANGE
  */
Ret_t HW_EEPROM_Read(uint16_t wOffset, uint8_t *p_bBuf, uint16_t wLen);

/**
  * @brief  Programs a block of bytes.
  * @param  wOffset : 0 .. HW_EEPROM_SIZE_BYTES-1
  * @param  p_bBuf  : source, must not be NULL
  * @param  wLen    : how many bytes
  * @note   Layer: HW Driver. Blocking: yes, a few ms per byte. ISR: no.
  *         Keep wLen small; the Module splits long writes across passes.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_RANGE, RET_ERROR
  */
Ret_t HW_EEPROM_Write(uint16_t wOffset, const uint8_t *p_bBuf, uint16_t wLen);

#endif /* __HW_EEPROM_DRIVE_H__ */
/*****************************END OF FILE****/
