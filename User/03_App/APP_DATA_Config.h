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

/* Charge-time tuning (hs.kim Optic_Solution v1.1 5-5, v1.2 5-6, 2026-11-11).
   Each channel's charge time is set so that a dry stick reads this value on
   the charging curve, well below the phototransistor's saturation ceiling.
   It replaced a digital gain on the steady reading, which only rescaled a
   reading that was already clipped. */
#define APP_OPTIC_TARGET_LIGHT           1200u   /* old _nTAGET_LIGHT          */
#define APP_OPTIC_TUNE_TOLERANCE           25u   /* old OPTIC_TUNED_TOLERANCE  */
/* First guess: the curve starts as a straight line of slope S / tau, so
   tau x 1200 / S is a little short of the target, and each correction
   t x 1200 / reading closes in from below without overshooting (the curve
   bends down). The parts give tau = 2.2 ms = 1467 loops; measured on 12호기
   (2026-11-11) it is about 2750 loops on bright channels and 3300..3500 on
   dim ones, so the charge is about twice as slow as the parts say. Only the
   first guess uses this: 12호기 still needs 3 readings either way. */
#define APP_OPTIC_TAU_LOOPS              2750u
#define APP_OPTIC_TUNE_TRIES               10u
/* Stops a runaway. Sized from the measured charge: a channel at the bottom
   of the acceptance window (S 1500) needs 1200 / 1500 = 80 % of its steady
   level, about 1.6 tau = 6,100..6,200 loops, more than the 6000 first used.
   10000 loops is about 15 ms with interrupts masked: one 10 ms tick can be
   lost, which the test timing (seconds) does not notice. (2026-11-11) */
#define APP_OPTIC_TIME_MAX_LOOPS        10000u

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
/* Test build: short error screen (2026-11-12). 1 holds an error for 10 s
   instead of 2 min, so that repeated insert/pull tests on many units do not
   spend most of their time waiting. Nothing else changes. Keep 0 for
   production. */
#ifndef APP_TEST_FAST_ERR
#define APP_TEST_FAST_ERR                   0
#endif
#if (APP_TEST_FAST_ERR == 1)
#define APP_ERR_RUN_TIME_TK        HW_TICK_MS(10000uL)
#else
#define APP_ERR_RUN_TIME_TK        HW_TICK_MS(120000uL) /* old ERR_RUN_TIME        12000 */
#endif
#define APP_SAMPLE_LOAD_WAIT_TK    HW_TICK_MS(300000uL) /* old SAMPLE_LOAD_WAIT_TIME 30000 */
#define APP_OVER_SAMPLE_CHECK_TK   HW_TICK_MS(  5000uL) /* old OVER_SAMPLE_CHECK_TIME  500 */
#define APP_LOW_SAMPLE_CHECK_TK    HW_TICK_MS( 20000uL) /* old LOW_SAMPLE_CHECK_TIME  2000 */
/* User manual, step 5: "물방울이 깜빡이고 3분 후에 검사결과가 결과창에
   나타납니다." DROP starts blinking when the over-sample check passes, so
   the blink covers the low-sample check and the reaction wait, and that
   span is what must be 3 minutes. MFx_New_Ver's 180 s counted from sample
   arrival and so blinked 175 s; the shipped alpha.2 blinked about 204 s.
   (hs.kim: "설명서와 동일하게", 2026-11-11) */
#define APP_BLINK_TO_RESULT_TK     HW_TICK_MS(180000uL)
#define APP_RESULT_OUT_WAIT_TK     HW_TICK_MS(300000uL) /* old RESULT_OUT_WAIT_TIME  30000 */

/* Remainder of the blink after the low-sample check: 160 s. */
#define APP_REACTION_WAIT_TK     ((uint16_t)(APP_BLINK_TO_RESULT_TK \
                                             - APP_LOW_SAMPLE_CHECK_TK))

/* Stick removal: the switch AND the optics (hs.kim, 2026-11-11).
   Either one alone raises false removals. The optics: as the colour
   develops the whole strip turns red and darkens, much as a pulled stick
   does. The switch: its contact lets go now and then with the stick in.
   So the stick counts as removed only when both say so.
   The switch is read on every 10 ms pass and judged in windows of 25 reads
   (250 ms): 20 or more low is "in", and the optics are not fired at all.
   Fewer starts one optical check of C; a dark C is one strike, and the
   check repeats at once. Two strikes in a row are a removal. A good window
   or a bright C clears the strikes. */
#define APP_STICK_SW_WINDOW                25u
#define APP_STICK_SW_PASS                  20u
#define APP_STICK_STRIKES                   2u

/* Switch contact fault, for the production line (QC call 2026-11-11). Some
   units close the switch while the stick is pushed in and let go as soon as
   the hand does. A contact that opens now and then is tolerated above; one
   that stays open is to be sorted out, and the operator tells the two
   faults apart on the panel:
     BOOK at once on insertion          -> optics (acceptance window, tuning)
     BOOK after the insert animation    -> switch contact
   "Stays open" is this many windows in a row, open at the moment the
   animation ends: 4 x 250 ms = 1 s. Stored as STICK_FAIL in step 1, which
   no other path produces, so a dump tells it apart without a new code. */
#define APP_SW_FAULT_WINDOWS                4u

/* "Sample arrived" is a drop in T against BT, and a stick being pulled out
   darkens both, which passes for the same drop (bench dumps 2026-11-06,
   defect 1). So the run does not move on at once: it holds for about a
   second with the removal check running, and only a stick that is still in
   goes on to the volume checks (hs.kim, 2026-11-11). The over-sample wait is
   shortened by the same second, so C is still read again 5 s after arrival
   and DROP starts blinking when it did before. */
#define APP_ARRIVAL_HOLD_TK          HW_TICK_MS(1000u)

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
