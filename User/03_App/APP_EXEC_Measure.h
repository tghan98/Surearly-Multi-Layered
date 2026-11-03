/**
  ******************************************************************************
  * @file    : APP_EXEC_Measure.h
  * @brief   : Application layer (exec) - optical measurement procedures
  *
  * Turns single channel readings from the Optic module into the procedures
  * the test sequence needs: the initial four-channel sweep, gain tuning, and
  * the stick-present judgement.
  *
  * The initial sweep interleaves presence checks, exactly as the old FW did:
  *   check - CH0 - check - CH1 - check - CH2 - check - CH3 - check
  * Nine measurements. A stick pulled out part way through aborts the sweep,
  * so a stored record is always a complete set.
  *
  * Stick judgement lives here, not in the Optic module and not in the control
  * layer. The thresholds are specification values from APP_DATA_Config.h, and
  * both the control layer and this layer need the judgement - putting it in
  * the control layer would force a call upward (decided 2026-11-03).
  *
  * Two axes, as in the old FW:
  *   raw    - the initial gate, threshold APP_OPTIC_STICK_THRESHOLD_RAW
  *   scaled - the waiting loop, threshold APP_OPTIC_STICK_THRESHOLD_SCALED
  * The supervisor's instruction of 2026-10-14 was that the judgement that
  * matters is the one on the gain-applied axis.
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
  * @brief  Clears the stored readings and gains.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_EXEC_Measure_Init(void);

/**
  * @brief  Starts the nine-measurement initial sweep.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval RET_OK, RET_BUSY
  */
Ret_t APP_EXEC_Measure_StartInitial(void);

/**
  * @brief  Starts one presence check on the C channel.
  * @param  bScaled : 1 to judge on the gain-applied axis, 0 on the raw axis
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval RET_OK, RET_BUSY
  */
Ret_t APP_EXEC_Measure_StartStickCheck(uint8_t bScaled);

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
  * @brief  Checks the initial readings against the acceptance window and sets
  *         each channel's gain so a dry stick reads APP_OPTIC_TARGET_LIGHT.
  * @note   Layer: App (exec). Blocking: no (arithmetic only). ISR: no.
  *         Run after the sweep reports APP_EXEC_MEASURE_DONE.
  * @retval APP_ERR_NONE, or APP_ERR_OPTIC_FAIL_CHn for the first bad channel
  */
APP_DATA_ErrorCode APP_EXEC_Measure_TuneGain(void);

/**
  * @brief  Fills a buffer with the initial readings after gain is applied.
  * @param  p_wOut : APP_EXEC_MEASURE_INITIAL_WORDS words, must not be NULL
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval APP_ERR_NONE, or APP_ERR_OPTIC_FAIL_CHn if a gain is still zero
  */
APP_DATA_ErrorCode APP_EXEC_Measure_GetScaledSet(uint16_t *p_wOut);

/* Getters (cannot fail) -----------------------------------------------------*/
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
  * @brief  Reading that the last presence check judged on.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval ADC counts on whichever axis was asked for
  */
uint16_t APP_EXEC_Measure_GetStickValue(void);

/**
  * @brief  Pointer to the four raw initial readings plus four zero words.
  * @note   Layer: App (exec). Blocking: no. ISR: no.
  * @retval const pointer to APP_EXEC_MEASURE_INITIAL_WORDS words
  */
const uint16_t *APP_EXEC_Measure_GetInitial(void);

#endif /* __APP_EXEC_MEASURE_H__ */
/*****************************END OF FILE****/
