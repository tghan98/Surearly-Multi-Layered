/**
  ******************************************************************************
  * @file    : Optic.h
  * @brief   : Module layer - photo channel measurement
  *
  * Four optical channels share three sensors and two LEDs:
  *   CH0 (T)  LED2, ADC front    CH1 (BT) LED2, ADC middle
  *   CH2 (BC) LED1, ADC middle   CH3 (C)  LED1, ADC rear
  *
  * One measurement, driven by Optic_Process():
  *   LED on -> settle 20 ms -> 21 conversions -> LED off -> recover 10 ms
  * The first conversion after the channel change is discarded; the result is
  * the mean of the middle 16 of the remaining 20, after sorting.
  *
  * The 20 ms settle comes from the sensor load: 100 kohm with 22 nF is a
  * 2.2 ms time constant, so 20 ms reaches about 99.99 %. (The old comment
  * said 4.7 ms; the 22 nF value was confirmed from the BOM on 2026-10-30 -
  * the schematic's 1 nF is an error.)
  *
  * Responsibility boundary: this module reports what the sensor read. It does
  * not decide whether a stick is present, whether a reading is in range, or
  * what to do next - those are App decisions driven by APP_DATA_Config.h
  * thresholds.
  *
  * Differences from the common skeleton (spec v2.0.3 §15):
  *   The 21-conversion burst runs inside one Optic_Process() pass, about
  *   4.2 ms, which exceeds the 2 ms slot budget. Splitting it across passes
  *   would keep the 20 mA LED on for roughly 200 ms instead of 4.2 ms, which
  *   costs far more battery than the overrun costs scheduling.
  *
  * Context: main loop only. Call Optic_Process() from the 10 ms slot.
  ******************************************************************************
  */
#ifndef __OPTIC_H__
#define __OPTIC_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    OPTIC_CH_T = 0,    /* old OPTIC_CH_0, front  */
    OPTIC_CH_BT,       /* old OPTIC_CH_1, middle */
    OPTIC_CH_BC,       /* old OPTIC_CH_2, middle */
    OPTIC_CH_C,        /* old OPTIC_CH_3, rear   */
    OPTIC_CH_COUNT
} Optic_Ch;

typedef enum
{
    OPTIC_IDLE = 0,
    OPTIC_SETTLE,
    OPTIC_SAMPLE,
    OPTIC_RECOVER,
    OPTIC_STATE_COUNT
} Optic_State;

/* Exported constants --------------------------------------------------------*/
#define OPTIC_SETTLE_MS          20u
#define OPTIC_RECOVER_MS         10u
#define OPTIC_SAMPLE_COUNT       20u   /* kept, after one discarded */
#define OPTIC_TRIM_EACH_SIDE     2u    /* mean of the middle 16 */
#define OPTIC_GAIN_SCALE         100u  /* gain is an integer percent */

/* Module-specific return codes ----------------------------------------------*/
#define OPTIC_ERR_ADC            (RET_MODULE_BASE + 0)   /* conversion failed */

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Puts the module in a safe state: both LEDs off, ADC powered down.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t Optic_Init(void);

/**
  * @brief  Starts one measurement on one channel.
  * @param  tCh : channel to measure
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         Returns immediately; poll Optic_IsBusy() and then Optic_GetResult().
  * @retval RET_OK, RET_ERR_PARAM, RET_BUSY
  */
Ret_t Optic_Start(Optic_Ch tCh);

/**
  * @brief  Advances the measurement. Call from the 10 ms slot.
  * @note   Layer: Module. Blocking: the sampling pass takes about 4.2 ms,
  *         see the file header. ISR: no.
  * @retval RET_OK
  */
Ret_t Optic_Process(void);

/**
  * @brief  Aborts any measurement and turns the LEDs off at once.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t Optic_Abort(void);

/**
  * @brief  Sets the tuning gain of one channel, as an integer percent.
  * @param  tCh    : channel
  * @param  wGain  : 0 .. 65535, where OPTIC_GAIN_SCALE means x1.0
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_PARAM
  */
Ret_t Optic_SetGain(Optic_Ch tCh, uint16_t wGain);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Whether a measurement is running.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval 1 busy, 0 idle
  */
uint8_t Optic_IsBusy(void);

/**
  * @brief  Current state machine step.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval OPTIC_IDLE .. OPTIC_RECOVER
  */
Optic_State Optic_GetState(void);

/**
  * @brief  Raw result of the last finished measurement, in ADC counts.
  * @param  tCh : channel
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval counts, or 0 if that channel has not been measured
  */
uint16_t Optic_GetRaw(Optic_Ch tCh);

/**
  * @brief  Last result with the channel gain applied: raw * gain / 100.
  * @param  tCh : channel
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         Stick judgement uses this axis (supervisor, 2026-10-14).
  * @retval scaled counts, or 0
  */
uint16_t Optic_GetScaled(Optic_Ch tCh);

/**
  * @brief  Tuning gain of one channel.
  * @param  tCh : channel
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval integer percent, 0 if unset or the ID is out of range
  */
uint16_t Optic_GetGain(Optic_Ch tCh);

/**
  * @brief  Takes the error of the last measurement; reading clears it.
  * @param  p_tErr : out, OPTIC_ERR_ADC when a conversion failed
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval 1 if an error was pending, 0 otherwise
  */
uint8_t Optic_TakeError(Ret_t *p_tErr);

#endif /* __OPTIC_H__ */
/*****************************END OF FILE****/
