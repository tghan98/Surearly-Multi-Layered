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
  *   12-bit single conversion, 16-cycle sampling window (8 us). The node is
  *   read while it is still charging, so the hold must come early and at a
  *   fixed moment; the 22 nF supplies the sampling capacitor (2026-11-11).
  *
  * A reading is Select, then a burst of back-to-back conversions:
  *   HW_ADC_Select -> [mask IRQ, LED on, delay] -> HW_ADC_Burst -> [unmask]
  * The caller masks interrupts for the whole burst so that every conversion
  * falls at the same moment after the LED; the EOC wait counts passes.
  *
  * Responsibility boundary: conversions, raw counts out. Gain, thresholds,
  * averaging and stick-present judgement all belong to the Module and App.
  *
  * Profile note: CR1632 battery. The ADC and its clock are off unless a
  * measurement is running (spec v2.0.3 §13 item 3). Enable -> read -> Disable.
  *
  * Differences from the old FW (DM_HW_Drv_ADC_*):
  *   - No SystemTick_AddUntracked_us() compensation. The burst is about
  *     85 us; with interrupts masked a tick due in it is served late, not
  *     lost.
  *   - The channel is identified by HW_ADC_Id, not by a vendor enum.
  *
  * Context: main loop only.
  ******************************************************************************
  */
#ifndef __HW_ADC_DRIVE_H__
#define __HW_ADC_DRIVE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"
#include "HW_Tick_Drive.h"   /* HW_TICK_MS(), HW_TICK_WAIT_MS() */

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    HW_ADC_FRNT = 0,   /* PB5, ADC_Channel_13, Q1 */
    HW_ADC_MIDL,       /* PB6, ADC_Channel_12, Q2 */
    HW_ADC_REAR,       /* PB7, ADC_Channel_11, Q3 */
    HW_ADC_COUNT
} HW_ADC_Id;

/* Exported constants --------------------------------------------------------*/
/* One conversion takes about 14 us at 16 + 12 cycles / 2 MHz, a few
   passes of the EOC poll. 255 passes bound a stuck converter to about
   1 ms (2 MHz CPU) with interrupts masked. */
#define HW_ADC_EOC_POLLS         255u

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
  * @brief  Connects one channel to the converter, ready for HW_ADC_Burst().
  * @param  tId : channel to convert
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_PARAM, RET_ERR_NOT_INIT
  */
Ret_t HW_ADC_Select(HW_ADC_Id tId);

/**
  * @brief  Runs bCount conversions back to back on the selected channel,
  *         then disconnects it.
  * @param  tId     : the channel given to HW_ADC_Select()
  * @param  p_awOut : bCount words, must not be NULL. Filled up to a failure.
  * @param  bCount  : conversions, 1..255
  * @note   Layer: HW Driver. Blocking: yes, about 14 us a conversion, each
  *         wait bounded by HW_ADC_EOC_POLLS passes. ISR: no.
  *         Safe with interrupts masked: it does not use the tick.
  * @retval RET_OK, RET_TIMEOUT
  */
Ret_t HW_ADC_Burst(HW_ADC_Id tId, uint16_t *p_awOut, uint8_t bCount);

#endif /* __HW_ADC_DRIVE_H__ */
/*****************************END OF FILE****/
