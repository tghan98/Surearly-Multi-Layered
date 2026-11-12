/**
  ******************************************************************************
  * @file    : APP_EXEC_Measure.h
  * @brief   : Application layer (exec) - optical measurement procedures
  *
  * Turns single channel readings from the Optic module into the procedures
  * the test sequence needs: the initial sweep with charge-time tuning, and
  * the stick-present judgement.
  *
  * StartInitial() runs one job in two parts:
  *   1. Steady sweep, each channel once: CH0 - CH1 - CH2 - CH3 (hs.kim
  *      review D2, 2026-11-10), then the acceptance window 1500..3900.
  *      A stick pulled out during the sweep reads dark and fails here.
  *   2. Charge-time tuning, channel by channel (hs.kim, 2026-11-12):
  *      e = 4.5 ms x 1200 / S, one reading each at e - 200 and e + 200
  *      loops, and the line through them solved for 1200. No check
  *      reading; about +/-50 from 1200 (APP_OPTIC_TUNE_SPAN).
  * TuneError() then gives the outcome.
  *
  * Stick judgement lives here, not in the Optic module and not in the control
  * layer. The thresholds are specification values from APP_DATA_Config.h, and
  * both the control layer and this layer need the judgement - putting it in
  * the control layer would force a call upward (decided 2026-11-03).
  *
  * One axis: the tuned reading, threshold APP_OPTIC_STICK_THRESHOLD_SCALED
  * (supervisor, 2026-10-14). A tuned dry stick reads about 1200, the axis
  * the gain-applied value was on, so the threshold carried over.
  *
  * Differences from the old FW (DM_App_Optic_*):
  *   The old routines blocked: one sweep held the main loop for about 280 ms.
  *   Each step now returns and resumes on the next pass.
  *
  * @par Upper App
  * - APP_CTRL_Sequence : initial sweep, stick checks during a test
  *
  * Context: main loop only. Call APP_EXEC_Measure_Process() from the 10 ms slot.
  ******************************************************************************
  */
#ifndef __APP_EXEC_MEASURE_H__
#define __APP_EXEC_MEASURE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"
#include "APP_DATA_ErrorCode.h"
#include "Optic.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    APP_EXEC_MEASURE_IDLE = 0,
    APP_EXEC_MEASURE_RUNNING,
    APP_EXEC_MEASURE_DONE,
    APP_EXEC_MEASURE_FAILED,
    APP_EXEC_MEASURE_RESULT_COUNT
} APP_EXEC_Measure_Result;

/* Exported constants --------------------------------------------------------*/
#define APP_EXEC_MEASURE_INITIAL_WORDS   8u   /* 4 used, 4 zero-filled */

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Clears the stored readings.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_EXEC_Measure_Init(void);

/**
  * @brief  Starts the initial sweep, one reading per channel.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval RET_OK, RET_BUSY
  */
Ret_t APP_EXEC_Measure_StartInitial(void);

/**
  * @brief  Starts one presence check on the C channel, gain-applied axis.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval RET_OK, RET_BUSY
  */
Ret_t APP_EXEC_Measure_StartStickCheck(void);

/**
  * @brief  Starts one measurement on one channel.
  * @param  tCh : channel
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_PARAM, RET_BUSY
  */
Ret_t APP_EXEC_Measure_StartSingle(Optic_Ch tCh);

/**
  * @brief  Advances whatever procedure is running.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_EXEC_Measure_Process(void);

/**
  * @brief  Stops any procedure and turns the optics off.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_EXEC_Measure_Abort(void);

/**
  * @brief  Outcome of the last StartInitial() job: the acceptance window,
  *         the tuning, or a failed conversion.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  *         Read after the job is no longer busy.
  * @retval APP_ERR_NONE, or APP_ERR_OPTIC_FAIL_CHn for the channel that failed
  */
APP_DATA_ErrorCode APP_EXEC_Measure_TuneError(void);

/**
  * @brief  Fills a buffer with the dry reading of each channel at its tuned
  *         time (about 1200 each), then zeros to APP_EXEC_MEASURE_INITIAL_WORDS.
  *         Valid once the StartInitial() job is no longer busy.
  * @param  p_wOut : APP_EXEC_MEASURE_INITIAL_WORDS words, must not be NULL
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  */
void APP_EXEC_Measure_GetTunedSet(uint16_t *p_wOut);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Whether the StartInitial() job is still on its steady sweep and
  *         acceptance window. After it the job either has ended (window
  *         failed) or goes on tuning and taking the dry readings.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval 1: sweeping, 0: not
  */
uint8_t APP_EXEC_Measure_IsSweeping(void);

/**
  * @brief  How the running or last procedure ended.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval APP_EXEC_MEASURE_IDLE .. APP_EXEC_MEASURE_FAILED
  */
APP_EXEC_Measure_Result APP_EXEC_Measure_GetResult(void);

/**
  * @brief  Whether a procedure is running.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval 1 busy, 0 idle
  */
uint8_t APP_EXEC_Measure_IsBusy(void);

/**
  * @brief  Outcome of the last presence check.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval 1 present, 0 absent
  */
uint8_t APP_EXEC_Measure_IsStickPresent(void);

/**
  * @brief  Pointer to the four raw initial readings plus four zero words.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval const pointer to APP_EXEC_MEASURE_INITIAL_WORDS words
  */
const uint16_t *APP_EXEC_Measure_GetInitial(void);

/**
  * @brief  Calculates the legacy four-digit intensity ratio.
  * @param  wDividend : numerator
  * @param  wDivisor : denominator
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  *         Preserves the legacy 16-bit intermediate arithmetic.
  * @retval Ratio, or zero if either operand is zero
  */
uint16_t APP_EXEC_Measure_Intensity(uint16_t wDividend, uint16_t wDivisor);

#endif /* __APP_EXEC_MEASURE_H__ */
/*****************************END OF FILE****/
