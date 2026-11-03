/**
  ******************************************************************************
  * @file    : APP_DATA_Record.h
  * @brief   : Application layer (data) - EEPROM layout and stored records
  *
  * The only place that knows what lives at which EEPROM offset. The Eeprom
  * module moves bytes; this file says where they go.
  *
  * Layout (256 bytes, unchanged from the old FW - the QC program and every
  * unit in the field depend on it):
  *
  *   0    insert count                 1 B
  *   1    test complete count          1 B
  *   2    error step                   1 B
  *   3    error code                   1 B
  *   4    initial stick readings      16 B   (8 words)
  *   20   result records             192 B   (6 slots x 32 B)
  *   212  QC reference                 8 B   (4 words)
  *   244  newest result slot           1 B
  *   248  wake-gate fail minimum       2 B
  *   250  wake-gate fail maximum       2 B
  *   252  reserved                     4 B
  *
  * A byte that has never been written reads as 0xFF. The status accessors
  * report 0 in that case, as the old FW did.
  *
  * Differences from the common skeleton (spec v2.0.3 §15):
  *   Spec §8 asks for a RAM mirror of every block with a dirty flag. Only the
  *   blocks that are written keep a mirror here (52 bytes): the Eeprom module
  *   holds the caller's pointer until the write finishes, so written data
  *   must sit somewhere stable. The large read-only blocks (the six stored
  *   results, the QC reference) are read straight from EEPROM; they are only
  *   touched when a dump is requested, and mirroring them would cost 200 of
  *   the 600 bytes of free RAM.
  *
  * @par Upper App
  * - none (data layer, called from every layer above)
  ******************************************************************************
  */
#ifndef __APP_DATA_RECORD_H__
#define __APP_DATA_RECORD_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported constants --------------------------------------------------------*/
#define APP_REC_SLOT_COUNT           6u    /* old ROM_RESULT_SLOT_COUNT  */
#define APP_REC_SLOT_WORDS          16u    /* 32 bytes per slot          */
#define APP_REC_STICK_PTR_WORDS      8u
#define APP_REC_QC_REF_WORDS         4u    /* old ROM_QC_REFERENCE_WORDS */
#define APP_REC_STATUS_BYTES         4u

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Loads the mirrored blocks from EEPROM into RAM.
  * @note   Layer: App (data). Blocking: no. ISR: no.
  *         A block that reads back as never-written becomes 0.
  * @retval RET_OK, RET_ERROR
  */
Ret_t APP_DATA_Record_Init(void);

/**
  * @brief  Pushes any dirty mirrored block to the Eeprom module.
  * @note   Layer: App (data). Blocking: no. ISR: no.
  *         Call from the 10 ms slot. One block at a time; a block whose
  *         write failed is marked dirty again and retried (spec §8).
  * @retval RET_OK
  */
Ret_t APP_DATA_Record_Process(void);

/* Counters and the last error ----------------------------------------------*/
/**
  * @brief  Sets the stick insert count. Stored on a later pass.
  * @param  bCount : new value
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_DATA_Record_SetInsertCount(uint8_t bCount);

/**
  * @brief  Sets the completed test count. Stored on a later pass.
  * @param  bCount : new value
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_DATA_Record_SetTestCount(uint8_t bCount);

/**
  * @brief  Records which step failed and why. Stored on a later pass.
  * @param  bStep : sequence step number
  * @param  bCode : APP_DATA_ErrorCode value
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_DATA_Record_SetError(uint8_t bStep, uint8_t bCode);

/* Measurement blocks --------------------------------------------------------*/
/**
  * @brief  Stores the eight initial stick readings. Written on a later pass.
  * @param  p_wData : APP_REC_STICK_PTR_WORDS words, must not be NULL
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL
  */
Ret_t APP_DATA_Record_SetStickPtr(const uint16_t *p_wData);

/**
  * @brief  Stores one finished test into the next slot, newest-first order
  *         kept by advancing the dump position.
  * @param  p_wData : APP_REC_SLOT_WORDS words, must not be NULL
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL, RET_BUSY
  */
Ret_t APP_DATA_Record_SaveResult(const uint16_t *p_wData);

/**
  * @brief  Widens the stored wake-gate fail range if this reading is outside.
  * @param  wAdc : raw reading that failed the wake gate
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_DATA_Record_UpdateWakeFail(uint16_t wAdc);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Stick insert count.
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval count
  */
uint8_t APP_DATA_Record_GetInsertCount(void);

/**
  * @brief  Completed test count.
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval count
  */
uint8_t APP_DATA_Record_GetTestCount(void);

/**
  * @brief  Slot index of the newest stored result.
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval 0 .. APP_REC_SLOT_COUNT-1
  */
uint8_t APP_DATA_Record_GetDumpPos(void);

/**
  * @brief  Reads one stored result slot straight from EEPROM.
  * @param  bSlot   : 0 .. APP_REC_SLOT_COUNT-1
  * @param  p_wData : APP_REC_SLOT_WORDS words, must not be NULL
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_PARAM
  */
Ret_t APP_DATA_Record_GetResult(uint8_t bSlot, uint16_t *p_wData);

/**
  * @brief  Reads the stored QC reference straight from EEPROM.
  * @param  p_wData : APP_REC_QC_REF_WORDS words, must not be NULL
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL
  */
Ret_t APP_DATA_Record_GetQcRef(const uint16_t **p_wData);

/**
  * @brief  Copies the four status bytes out, with 0xFF reported as 0.
  * @param  p_wOut : APP_REC_STATUS_BYTES words, must not be NULL
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL
  */
Ret_t APP_DATA_Record_GetStatus(uint16_t *p_wOut);

/**
  * @brief  Stored wake-gate fail range.
  * @param  p_wMin : out, must not be NULL
  * @param  p_wMax : out, must not be NULL
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL
  */
Ret_t APP_DATA_Record_GetWakeFail(uint16_t *p_wMin, uint16_t *p_wMax);

/**
  * @brief  Pointer to the mirrored initial stick readings.
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval const pointer to APP_REC_STICK_PTR_WORDS words
  */
const uint16_t *APP_DATA_Record_GetStickPtr(void);

/**
  * @brief  Whether a store is still in progress.
  * @note   Layer: App (data). Blocking: no. ISR: no.
  * @retval 1 busy, 0 idle
  */
uint8_t APP_DATA_Record_IsBusy(void);

#endif /* __APP_DATA_RECORD_H__ */
/*****************************END OF FILE****/
