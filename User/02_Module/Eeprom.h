/**
  ******************************************************************************
  * @file    : Eeprom.h
  * @brief   : Module layer - non-blocking writes to the on-chip data EEPROM
  *
  * Hardware facts (STM8L151F3):
  *   256 bytes of data EEPROM. Programming one byte takes several ms and the
  *   core stalls for that time, so a 32-byte block written in one go would
  *   hold the main loop for roughly 200 ms.
  *
  * This module spreads a write over successive Eeprom_Process() passes,
  * EEPROM_BYTES_PER_PASS bytes at a time (spec v2.0.3 §7, §13 item 9).
  * Reads are immediate: reading EEPROM costs no more than reading flash.
  *
  * Responsibility boundary: this module moves bytes. It does not know what
  * lives at which offset - that map belongs to the App data layer.
  *
  * Caller contract: Eeprom_Write() keeps the caller's buffer pointer until
  * the write finishes. The buffer must stay valid and unchanged until
  * Eeprom_IsBusy() returns 0. A RAM copy is not made on purpose: this part
  * has 1 KB of RAM.
  *
  * Context: main loop only. Call Eeprom_Process() from the 10 ms slot.
  ******************************************************************************
  */
#ifndef __EEPROM_H__
#define __EEPROM_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    EEPROM_IDLE = 0,
    EEPROM_WRITING,
    EEPROM_STATE_COUNT
} Eeprom_State;

/* Exported constants --------------------------------------------------------*/
#define EEPROM_BYTES_PER_PASS    1u

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Clears any pending write.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t Eeprom_Init(void);

/**
  * @brief  Reads a block straight away.
  * @param  wOffset : byte offset in the EEPROM
  * @param  p_bBuf  : destination, must not be NULL
  * @param  wLen    : how many bytes
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_RANGE
  */
Ret_t Eeprom_Read(uint16_t wOffset, uint8_t *p_bBuf, uint16_t wLen);

/**
  * @brief  Queues a block to be written across later passes.
  * @param  wOffset : byte offset in the EEPROM
  * @param  p_bBuf  : source; must stay valid until Eeprom_IsBusy() is 0
  * @param  wLen    : how many bytes
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         One write at a time; a second request while busy is refused.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_RANGE, RET_BUSY
  */
Ret_t Eeprom_Write(uint16_t wOffset, const uint8_t *p_bBuf, uint16_t wLen);

/**
  * @brief  Programs the next chunk of the queued write.
  * @note   Layer: Module. Blocking: yes, a few ms per byte programmed.
  *         ISR: no.
  * @retval RET_OK
  */
Ret_t Eeprom_Process(void);

/**
  * @brief  Drops any pending write. The bytes already programmed stay.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t Eeprom_Abort(void);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Whether a write is still in progress.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval 1 busy, 0 idle
  */
uint8_t Eeprom_IsBusy(void);

/**
  * @brief  Current state.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval EEPROM_IDLE or EEPROM_WRITING
  */
Eeprom_State Eeprom_GetState(void);

/**
  * @brief  Takes a failed-write report; reading clears it.
  * @param  p_wOffset : out, offset of the byte that would not program
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         The caller is expected to mark its data dirty again and retry
  *         (spec v2.0.3 §8).
  * @retval 1 if a failure was pending, 0 otherwise
  */
uint8_t Eeprom_TakeWriteFail(uint16_t *p_wOffset);

#endif /* __EEPROM_H__ */
/*****************************END OF FILE****/
