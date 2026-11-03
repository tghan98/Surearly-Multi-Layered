/**
  ******************************************************************************
  * @file    : APP_CTRL_Sequence.h
  * @brief   : Application layer (control) - the nine-step test sequence
  *
  * One test, from a sleeping unit to a displayed result:
  *
  *   IDLE              deep sleep; a stick closes the switch and wakes it
  *   STICK_INSERT      initial sweep, acceptance window, gain tuning
  *   SAMPLE_LOAD_WAIT  waiting for sample, up to 5 minutes
  *   OVER_SAMPLE_CHECK 5 s window, too much sample
  *   LOW_SAMPLE_CHECK  20 s window, too little sample
  *   REACTION_WAIT     remainder of the 3-minute reaction
  *   RESULT_SCAN       final readings, YES or NO
  *   RESULT_OUT        result on the display for 5 minutes
  *   ERROR_WAIT        error icon for 2 minutes
  *
  * Step numbers and error codes are stored and reported to the QC program,
  * so their values are a compatibility requirement.
  *
  * Differences from the old FW (DM_App_Main_Sq_App):
  *   The old handlers blocked: nineteen places called a sleep routine or spun
  *   in a wait loop, the longest for five minutes. Every one becomes a step
  *   that records a start time and returns. The visible behaviour and the
  *   timings are unchanged.
  *
  * @par Upper App
  * - none (top layer)
  *
  * Context: main loop only. Call APP_CTRL_Sequence_Process() from the 10 ms
  * slot. The IDLE step is the one place that stops the core, on purpose.
  ******************************************************************************
  */
#ifndef __APP_CTRL_SEQUENCE_H__
#define __APP_CTRL_SEQUENCE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"
#include "APP_DATA_ErrorCode.h"

/* Exported types ------------------------------------------------------------*/
/* Values are stored in EEPROM and reported outside: do not renumber. */
typedef enum
{
    APP_SEQ_IDLE = 0,
    APP_SEQ_STICK_INSERT,
    APP_SEQ_SAMPLE_LOAD_WAIT,
    APP_SEQ_OVER_SAMPLE_CHECK,
    APP_SEQ_LOW_SAMPLE_CHECK,
    APP_SEQ_REACTION_WAIT,
    APP_SEQ_RESULT_SCAN,
    APP_SEQ_RESULT_OUT,
    APP_SEQ_ERROR_WAIT,
    APP_SEQ_STEP_COUNT
} APP_CTRL_Sequence_Step;

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Starts at IDLE with no error pending.
  * @note   Layer: App (control). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_CTRL_Sequence_Init(void);

/**
  * @brief  Advances the test by one pass.
  * @note   Layer: App (control). Blocking: no, except that the IDLE step
  *         stops the core until a stick wakes it. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_CTRL_Sequence_Process(void);

/**
  * @brief  Clears the completed-test count, so the unit may be used again.
  * @note   Layer: App (control). Blocking: no. ISR: no.
  *         Same-layer call from APP_CTRL_Cmd on the R command.
  * @retval RET_OK
  */
Ret_t APP_CTRL_Sequence_ResetTestCount(void);

/**
  * @brief  Clears the stored records and returns to IDLE. Development only.
  * @note   Layer: App (control). Blocking: no. ISR: no.
  *         Same-layer call from APP_CTRL_Cmd on the Q command.
  * @retval RET_OK
  */
Ret_t APP_CTRL_Sequence_RestartQc(void);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Which step the test is on.
  * @note   Layer: App (control). Blocking: no. ISR: no.
  * @retval APP_SEQ_IDLE .. APP_SEQ_ERROR_WAIT
  */
APP_CTRL_Sequence_Step APP_CTRL_Sequence_GetStep(void);

/**
  * @brief  The error that stopped the last test, if any.
  * @note   Layer: App (control). Blocking: no. ISR: no.
  * @retval APP_ERR_NONE when the test is running or finished cleanly
  */
APP_DATA_ErrorCode APP_CTRL_Sequence_GetError(void);

/**
  * @brief  Whether this run counts as one of the first few QC cycles, which
  *         are shown on the display instead of failing silently.
  * @note   Layer: App (control). Blocking: no. ISR: no.
  * @retval 1 QC cycle, 0 ordinary use
  */
uint8_t APP_CTRL_Sequence_IsQcCycle(void);

#endif /* __APP_CTRL_SEQUENCE_H__ */
/*****************************END OF FILE****/
