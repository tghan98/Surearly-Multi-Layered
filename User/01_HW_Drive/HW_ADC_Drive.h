/**
  ******************************************************************************
  * @file    : HW_ADC_Drive.h
  * @brief   : HW Driver layer - photo sensor analog inputs
  *
  * Hardware facts (v1.1 board):
  *   Three phototransistor channels, each loaded by a 100 kohm resistor and a
  *   22 nF capacitor (BOM; the schematic's 1 nF is an error, confirmed
  *   2026-10-30). RC = 2.2 ms, so the caller must allow settling time after
  *   switching an LED on - that wait belongs to the Module, not here.
  *   PB5 -> ADC_Channel_13 (Q1, front)
  *   PB6 -> ADC_Channel_12 (Q2, middle)
  *   PB7 -> ADC_Channel_11 (Q3, rear)
  *   12-bit single conversion, 384-cycle sampling window (the 100 kohm source
  *   needs a long window).
  *
  * Responsibility boundary: one conversion, raw counts out. Gain, thresholds,
  * averaging and stick-present judgement all belong to the Module and App.
  *
  * Profile note: CR1632 battery. The ADC and its clock are off unless a
  * measurement is running (spec v2.0.3 §13 item 3). Enable -> read -> Disable.
  *
  * Differences from the old FW (DM_HW_Drv_ADC_*):
  *   - No SystemTick_AddUntracked_us() compensation. TIM4 free-runs at 1 ms
  *     and its interrupt keeps counting during the conversion, so no time is
  *     lost and nothing needs to be added back.
  *   - The channel is identified by HW_ADC_Id, not by a vendor enum.
  *
  * Context: main loop only.
  ******************************************************************************
  */
#ifndef __HW_ADC_DRIVE_H__
#define __HW_ADC_DRIVE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    HW_ADC_FRNT = 0,   /* PB5, ADC_Channel_13, Q1 */
    HW_ADC_MIDL,       /* PB6, ADC_Channel_12, Q2 */
    HW_ADC_REAR,       /* PB7, ADC_Channel_11, Q3 */
    HW_ADC_COUNT
} HW_ADC_Id;

/* Exported constants --------------------------------------------------------*/
/* One conversion takes about 198 us at 384 cycles / 2 MHz. The bound is in
   whole ms because the tick is 1 ms (spec §10: a ms timeout is at least 2). */
#define HW_ADC_CONV_TIMEOUT_MS   2u

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Configures ADC1. Leaves the converter powered down.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t HW_ADC_Init(void);

/**
  * @brief  Powers up ADC1 and its clock.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  *         Call before a burst of reads, HW_ADC_Disable() after it.
  * @retval RET_OK
  */
Ret_t HW_ADC_Enable(void);

/**
  * @brief  Powers down ADC1 and gates its clock.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t HW_ADC_Disable(void);

/**
  * @brief  Runs one conversion on one channel and returns the raw count.
  * @param  tId    : channel to convert
  * @param  p_wOut : destination, must not be NULL. Untouched on failure.
  * @note   Layer: HW Driver. Blocking: yes, bounded by
  *         HW_ADC_CONV_TIMEOUT_MS. ISR: no.
  *         The channel is enabled and disabled around the conversion so that
  *         only one analog input is connected at a time.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_PARAM, RET_ERR_NOT_INIT, RET_TIMEOUT
  */
Ret_t HW_ADC_Read(HW_ADC_Id tId, uint16_t *p_wOut);

#endif /* __HW_ADC_DRIVE_H__ */
/*****************************END OF FILE****/
