/**
  ******************************************************************************
  * @file    : APP_DATA_Config.h
  * @brief   : Application layer (data) - model, version and specification values
  *
  * Every specification number lives here. No other file carries one
  * (spec v2.0.3 §8). Values are carried over from the old FW unchanged; the
  * old name is noted beside each one so the two can be compared.
  *
  * TIME VALUES ARE WRITTEN IN MILLISECONDS and converted to ticks at compile
  * time by HW_TICK_MS(). One tick is 10 ms, the same unit the old FW counted,
  * so every constant below ends up as the old number again - but the source
  * still reads in milliseconds, which is the unit the spec is written in.
  * Nothing wider than 16 bits reaches the generated code.
  *
  * @par Upper App
  * - none (data layer, called from every layer above)
  ******************************************************************************
  */
#ifndef __APP_DATA_CONFIG_H__
#define __APP_DATA_CONFIG_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Tick_Drive.h"      /* HW_TICK_MS() */

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Build switches (model) ----------------------------------------------------*/
/* None yet: one board revision, one model. */

/* Version -------------------------------------------------------------------*/
#define APP_VER_LABEL            "v2.1"       /* old _strFIRMWARE_VER, 4 bytes  */
#define APP_VER_BUILD            "v2.1.1.2"   /* old _REAL_FIRMWARE_VER, 8 bytes*/
#define APP_LOT_PREFIX           "HCGM"       /* old _strLOT_UPPER_4CHAR        */
#define APP_LOT_SUFFIX           "19001"      /* old _strLOT_LOWER_5NUM         */

/* Optical thresholds --------------------------------------------------------*/
/* Stick present / absent, judged on the gain-applied axis (supervisor,
   2026-10-14: "a value without gain is not even worth considering").
   556 was fixed on 2026-10-29 from five healthy units reading 3,541..3,714.
   The raw-axis 2600 gate was removed on 2026-11-10: the wake check is the
   stick switch now (hs.kim review D1/M11). */
#define APP_OPTIC_STICK_THRESHOLD_SCALED   556u   /* old OPTIC_STICK_REMOVE_THRESHOLD_SCALED */

/* Wake-up switch check (hs.kim review 2026-11-09, D1/M14). After a wake the
   stick switch PB1 is read every 10 ms, 10 times; the stick counts as
   inserted when at least 8 reads are low. Anything less is a bounce or a
   noise pulse, and the unit goes back to halt without showing anything. */
#define APP_WAKE_SW_INTERVAL_MS            10u
#define APP_WAKE_SW_SAMPLES                10u
#define APP_WAKE_SW_PASS                    8u

/* Acceptance window for the initial per-channel reading at full LED drive. */
#define APP_OPTIC_INITIAL_MIN            1500u   /* old OPTIC_INITIAL_MIN_100 */
#define APP_OPTIC_INITIAL_MAX            3900u   /* old OPTIC_INITIAL_MAX_100 */

/* LED gain tuning: a dry stick is normalised to this reading. */
#define APP_OPTIC_TARGET_LIGHT           1200u   /* old _nTAGET_LIGHT          */
#define APP_OPTIC_TUNE_TOLERANCE           25u   /* old OPTIC_TUNED_TOLERANCE  */

/* Line intensity thresholds -------------------------------------------------*/
#define APP_T_LINE_THRESHOLD             1080u   /* old _nT_LINE_INT_THRESHOLD */
#define APP_C_LINE_THRESHOLD             1005u   /* old _nC_LINE_INT_THRESHOLD */

/* Sample volume ratios (percent) --------------------------------------------*/
#define APP_EMPTY_BTB_RATIO                90u   /* old _nEMPTY_BTB_RATIO      */
#define APP_OVER_FLOW_RATIO                92u   /* old _nOVER_FLOW_RATIO      */
#define APP_SMALL_SAMPLE_RATIO            120u   /* old _nSMALL_SAMPLE_RATIO   */

/* Usage limit ---------------------------------------------------------------*/
#define APP_SCAN_MAX_COUNT                 30u   /* old _nSCAN_MAX_CNT         */

/* Sequence timing (ticks, written as milliseconds) ---------------------------*/
#define APP_ERR_RUN_TIME_TK        HW_TICK_MS(120000uL) /* old ERR_RUN_TIME        12000 */
#define APP_SAMPLE_LOAD_WAIT_TK    HW_TICK_MS(300000uL) /* old SAMPLE_LOAD_WAIT_TIME 30000 */
#define APP_OVER_SAMPLE_CHECK_TK   HW_TICK_MS(  5000uL) /* old OVER_SAMPLE_CHECK_TIME  500 */
#define APP_LOW_SAMPLE_CHECK_TK    HW_TICK_MS( 20000uL) /* old LOW_SAMPLE_CHECK_TIME  2000 */
#define APP_TOTAL_REACTION_TK      HW_TICK_MS(180000uL) /* old TOTAL_REACTION_TIME   18000 */
#define APP_RESULT_OUT_WAIT_TK     HW_TICK_MS(300000uL) /* old RESULT_OUT_WAIT_TIME  30000 */

/* Remainder of the reaction window after the two sample checks. */
#define APP_REACTION_WAIT_TK     ((uint16_t)(APP_TOTAL_REACTION_TK \
                                             - APP_OVER_SAMPLE_CHECK_TK \
                                             - APP_LOW_SAMPLE_CHECK_TK))

/* How often the stick is re-checked for removal.
   The old FW declared 250 ms, but two throttles in series made the interval
   that shipped 6.25 s, and the port first kept that (2026-11-03). In
   production a pulled stick then took over 6 s to notice, and which error
   showed depended on the moment it was pulled (2026-11-06).
   Now a check of its own every 500 ms, from the end of gain tuning to the
   start of the result scan (hs.kim review D6, 2026-11-10). 250 ms would
   react sooner but fires the LED twice as often; 500 ms is the agreed
   floor. */
#define APP_STICK_CHECK_INTERVAL_TK  HW_TICK_MS(500u)

/* Test build: worst-case log (2026-11-11, hs.kim). 1 builds a test FW that
   does NOT end a run on stick removal. Instead, from the end of gain tuning
   to the result scan, it reads one channel every 250 ms in turn and keeps
   the lowest gain-applied value of each, and it reads the stick switch on
   every 10 ms pass and keeps its longest open stretch. They are stored
   when the run ends and read with the ordinary dump, in the QC reference
   field, which means something else in the product:
     QC T                 : longest switch-open stretch, 10 ms units
     QC BT / QC BC / QC C : lowest BT / BC / C (gain applied) during the run
   Purpose: see how dark each channel gets while the colour develops, and
   whether the switch drops out with the stick in. Keep 0 for production. */
#ifndef APP_TEST_MINLOG
#define APP_TEST_MINLOG                     0
#endif
#define APP_TEST_MINLOG_TK           HW_TICK_MS(250u)

/* Pace of the waiting loops: one legacy "sleep" of 250 ms. The sample-load
   wait takes one T/BT pair every four of them, as the old FW did. */
#define APP_SEQ_SLEEP_TK             HW_TICK_MS(250u)

#endif /* __APP_DATA_CONFIG_H__ */
/*****************************END OF FILE****/
