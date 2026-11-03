/**
  ******************************************************************************
  * @file    : APP_DATA_ErrorCode.h
  * @brief   : Application layer (data) - error numbers reported outside
  *
  * These numbers reach the QC program and the stored records, so they are a
  * compatibility requirement: the values must not change (spec v2.0.3 §8).
  * They are separate from the internal Ret_t codes in HW_Ret_Code.h.
  *
  * @par Upper App
  * - none (data layer, called from every layer above)
  ******************************************************************************
  */
#ifndef __APP_DATA_ERRORCODE_H__
#define __APP_DATA_ERRORCODE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    APP_ERR_NONE             = 0x00,  /* no error                              */
    APP_ERR_STICK_REMOVE     = 0x01,  /* removed before the C band changed     */
    APP_ERR_STICK_FAIL       = 0x02,  /* removed after the C band changed      */
    APP_ERR_OVER_FLOW        = 0x03,  /* too much sample                       */
    APP_ERR_LOW_SAMPLE       = 0x04,  /* too little sample                     */
    APP_ERR_OVER_USED        = 0x05,  /* more than APP_SCAN_MAX_COUNT runs     */
    APP_ERR_C_LINE_FAIL      = 0x06,  /* control line below threshold          */
    APP_ERR_SAMPLE_LOAD_FAIL = 0x07,  /* no sample within the load window      */
    APP_ERR_OPTIC_FAIL_CH0   = 0x10,  /* channel tuning failed                 */
    APP_ERR_OPTIC_FAIL_CH1   = 0x11,
    APP_ERR_OPTIC_FAIL_CH2   = 0x12,
    APP_ERR_OPTIC_FAIL_CH3   = 0x13
} APP_DATA_ErrorCode;

#endif /* __APP_DATA_ERRORCODE_H__ */
/*****************************END OF FILE****/
