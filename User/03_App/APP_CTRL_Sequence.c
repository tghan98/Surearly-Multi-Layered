/**
  ******************************************************************************
  * @file    : APP_CTRL_Sequence.c
  * @brief   : Application layer (control) - the nine-step test sequence
  *
  * Each step keeps a phase of its own, because the work it waits on - an
  * optical sweep, a stored write, a display animation - now finishes on a
  * later pass instead of inside the call.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "APP_CTRL_Sequence.h"
#include "APP_DATA_Config.h"
#include "APP_DATA_Record.h"
#include "APP_EXEC_Measure.h"
#include "Eeprom.h"
#include "HW_CLK_Drive.h"
#include "HW_GPIO_Drive.h"
#include "HW_PWR_Drive.h"
#include "HW_Tick_Drive.h"
#include "Lcd.h"
#include "Optic.h"
#include "PcComm.h"

/* Private defines -----------------------------------------------------------*/
/* The first few insertions of a unit are treated as QC cycles: a failure is
   shown on the display instead of returning to sleep without a sign. */
#define SEQ_QC_CYCLE_LIMIT       5u
#define SEQ_INSERT_COUNT_CAP   254u

/* One tick of settling after the clock comes back, as the old FW did. */
#define SEQ_RESUME_SETTLE_TK     HW_TICK_WAIT_MS(10u)

/* Private types -------------------------------------------------------------*/
typedef enum
{
    SEQ_PH_ENTER = 0,      /* first pass in this step  */
    SEQ_PH_WAIT,           /* waiting on time or a job */
    SEQ_PH_SETTLE,
    SEQ_PH_GATE,
    SEQ_PH_START_CHANNEL,
    SEQ_PH_CHANNEL,
    SEQ_PH_SAVE,
    SEQ_PH_SAVED,
    SEQ_PH_ANIM,           /* waiting for an LCD animation to finish */
    SEQ_PH_COUNT
} Seq_Phase;

typedef enum
{
    SEQ_RES_T = 0,
    SEQ_RES_C,
    SEQ_RES_YES_NO,
    SEQ_RES_COUNT
} Seq_Result;

typedef struct
{
    APP_CTRL_Sequence_Step tStep;
    Seq_Phase              tPhase;
    uint16_t               wStart;       /* step or phase entry tick       */
    APP_DATA_ErrorCode     tError;
    uint8_t                bErrorStep;   /* step the error happened in     */
    uint8_t                bQcCycle;
    uint8_t                bTestCount;
    uint16_t               awScaled[APP_EXEC_MEASURE_INITIAL_WORDS];
    uint16_t               awEmptyBand[OPTIC_CH_COUNT];
    uint16_t               awResultBand[OPTIC_CH_COUNT];
    uint16_t               awResult[SEQ_RES_COUNT];
    uint16_t               wThreshold;
    uint16_t               wSleepStart;
    uint16_t               wStickStart;
    uint8_t                bChecking;
    uint8_t                bChannel;
    uint8_t                bSleeps;
} Seq_Ctx;

/* Private variables ---------------------------------------------------------*/
/* __near: this buffer lives in RAM above page zero. Page zero is only
   256 bytes and every variable defaults there in the small data model;
   the bulk buffers are moved out so the frequently touched state can
   stay in the short-addressed page. See EWSTM8/MFx_Layered.icf. */
__near static Seq_Ctx s_seq;

/* LCD animation tables -------------------------------------------------------
   What the panel shows is an App decision; Lcd only plays the table
   (spec v2.0.3 §8). Hold values are refresh ticks of 10 ms, the same
   numbers the old FW used. Carried over from MFx_New_Ver
   DM_LCD_Stick_Check_App.c gs_atStickInsertAnim / gs_atErrOverUsedAnim. */
#define SEQ_ANIM_HOLD            50u    /* 500 ms */

/* Stick inserted: three whole-panel flashes, then each icon once.
   Total about 4.5 s - the unit waits for it before asking for the sample,
   as the old FW did. */
static const Lcd_AnimStep s_animStickInsert[] =
{
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, 0u            },
    { (uint8_t)LCD_ICON_DROP, (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, 0u            },
    { (uint8_t)LCD_ICON_YES,  (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, 0u            },
    { (uint8_t)LCD_ICON_NO,   (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, 0u            },
    { (uint8_t)LCD_ICON_BOOK, (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, 0u            },
    { (uint8_t)LCD_ICON_DROP, (uint8_t)LCD_ON,  SEQ_ANIM_HOLD }
};
#define SEQ_ANIM_INSERT_STEPS  (sizeof(s_animStickInsert) / sizeof(s_animStickInsert[0]))

/* Used up: the four icons cycle until the stick is pulled out. */
static const Lcd_AnimStep s_animOverUsed[] =
{
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, 0u            },
    { (uint8_t)LCD_ICON_BOOK, (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, 0u            },
    { (uint8_t)LCD_ICON_NO,   (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, 0u            },
    { (uint8_t)LCD_ICON_YES,  (uint8_t)LCD_ON,  SEQ_ANIM_HOLD },
    { (uint8_t)LCD_ICON_ALL,  (uint8_t)LCD_OFF, 0u            },
    { (uint8_t)LCD_ICON_DROP, (uint8_t)LCD_ON,  SEQ_ANIM_HOLD }
};
#define SEQ_ANIM_OVERUSED_STEPS (sizeof(s_animOverUsed) / sizeof(s_animOverUsed[0]))

/* Private function prototypes -----------------------------------------------*/
static void s_GoStep(APP_CTRL_Sequence_Step tStep);
static void s_ShowError(APP_DATA_ErrorCode tCode);
static void s_EnterError(APP_DATA_ErrorCode tCode);
static void s_InitialOpticError(APP_DATA_ErrorCode tCode);
static void s_ClearRun(void);
static void s_StepIdle(void);
static void s_StepStickInsert(void);
static uint16_t s_Ratio(uint16_t wValue, uint16_t wPercent);
static uint8_t s_SleepAndCheckStick(APP_DATA_ErrorCode tError);
static Ret_t s_SaveResult(void);
static void s_SequenceError(APP_DATA_ErrorCode tError);
static void s_StepSampleLoadWait(void);
static void s_StepOverSampleCheck(void);
static void s_StepLowSampleCheck(void);
static void s_StepReactionWait(void);
static void s_StepResultScan(void);
static void s_StepResultOut(void);
static void s_StepErrorWait(void);

/* Private functions ---------------------------------------------------------*/
static void s_GoStep(APP_CTRL_Sequence_Step tStep)
{
    s_seq.tStep   = tStep;
    s_seq.tPhase  = SEQ_PH_ENTER;
    s_seq.wStart = HW_Tick_Get();
}

/* The panel an error shows is decided here, from the error code, not by the
   caller. Letting each call site pass an icon is how the port lost the old
   FW's three distinct displays and showed everything as a blink
   (found on the bench 2026-11-03). The old mapping is: STICK_REMOVE gets
   BOOK plus a blinking DROP, OVER_USED gets the four-icon cycle, and every
   other error gets the steady BOOK of the old LCD_SQ_ERR_STICK_FAIL. */
static void s_ShowError(APP_DATA_ErrorCode tCode)
{
    if (tCode == APP_ERR_OVER_USED)
    {
        (void)Lcd_PlayAnim(s_animOverUsed,
                           (uint8_t)SEQ_ANIM_OVERUSED_STEPS, 1u);
        return;
    }

    (void)Lcd_SetAll(LCD_OFF);
    (void)Lcd_SetIcon(LCD_ICON_BOOK, LCD_ON);

    if (tCode == APP_ERR_STICK_REMOVE)
    {
        (void)Lcd_SetIcon(LCD_ICON_DROP, LCD_BLINK);
    }
}

/* Show the error and hold it for APP_ERR_RUN_TIME_TK. */
static void s_EnterError(APP_DATA_ErrorCode tCode)
{
    s_seq.tError     = tCode;
    s_seq.bErrorStep = (uint8_t)s_seq.tStep;

    s_ShowError(tCode);

    s_GoStep(APP_SEQ_ERROR_WAIT);
}

/* A failure during the initial optics: shown only on a QC cycle. Otherwise
   it is recorded and the unit goes quietly back to sleep, as before. */
static void s_InitialOpticError(APP_DATA_ErrorCode tCode)
{
    if (s_seq.bQcCycle != 0u)
    {
        s_EnterError(tCode);
    }
    else
    {
        s_seq.tError     = tCode;
        s_seq.bErrorStep = (uint8_t)s_seq.tStep;
        (void)APP_DATA_Record_SetError(s_seq.bErrorStep, (uint8_t)tCode);
        s_GoStep(APP_SEQ_IDLE);
    }
}

static void s_ClearRun(void)
{
    uint8_t bCh;

    s_seq.tError     = APP_ERR_NONE;
    s_seq.bErrorStep = 0u;

    for (bCh = 0u; bCh < (uint8_t)OPTIC_CH_COUNT; bCh++)
    {
        (void)Optic_SetGain((Optic_Ch)bCh, 0u);
        s_seq.awEmptyBand[bCh] = 0u;
        s_seq.awResultBand[bCh] = 0u;
    }
    for (bCh = 0u; bCh < (uint8_t)SEQ_RES_COUNT; bCh++)
    {
        s_seq.awResult[bCh] = 0u;
    }
    for (bCh = 0u; bCh < (uint8_t)APP_EXEC_MEASURE_INITIAL_WORDS; bCh++)
    {
        s_seq.awScaled[bCh] = 0u;
    }
}

/* ---- Step 1: IDLE --------------------------------------------------------*/
/* Deep sleep and the wake-up gate. The order of the shutdown matters: the
   measured 71 uA (unit 12, PPK2, 2026-10-30) depends on the LEDs being off,
   the display parked, the regulator in ultra-low-power mode and all three
   peripheral clocks gated before halt() is reached. */
static void s_StepIdle(void)
{
    uint8_t  bInsertCount;
    uint16_t wGateAdc;

    switch (s_seq.tPhase)
    {
        case SEQ_PH_ENTER:
            /* Park what this layer owns, then hand the power domain over. */
            (void)Optic_Abort();        /* both LEDs off, ADC powered down   */
            (void)Lcd_Park();           /* COM and SEG low, no DC on the panel*/

            (void)HW_PWR_PrepareHalt(); /* regulator, wake pin, clocks gated */
            (void)HW_PWR_EnterHalt();   /* stops here until a stick arrives  */

            /* --- awake again --- */
            (void)HW_PWR_Resume();
            (void)HW_CLK_Init();
            (void)HW_GPIO_Init();       /* wake pin back to a plain input    */
            (void)Lcd_Init();
            (void)PcComm_Init();

            s_seq.wStart = HW_Tick_Get();
            s_seq.tPhase  = SEQ_PH_SETTLE;
            break;

        case SEQ_PH_SETTLE:
            /* Let the clock and the ADC settle before the gate measurement,
               as the old FW did with one sleep tick after Power_Resume(). */
            if (HW_Tick_Elapsed(s_seq.wStart) >= SEQ_RESUME_SETTLE_TK)
            {
                s_ClearRun();
                (void)APP_EXEC_Measure_StartStickCheck(0u);  /* raw axis */
                s_seq.tPhase = SEQ_PH_GATE;
            }
            break;

        case SEQ_PH_GATE:
            if (APP_EXEC_Measure_IsBusy() != 0u)
            {
                break;
            }

            wGateAdc     = APP_EXEC_Measure_GetStickValue();
            bInsertCount = APP_DATA_Record_GetInsertCount();

            if (APP_EXEC_Measure_IsStickPresent() != 0u)
            {
                if (bInsertCount < (uint8_t)SEQ_INSERT_COUNT_CAP)
                {
                    bInsertCount++;
                    (void)APP_DATA_Record_SetInsertCount(bInsertCount);
                }
                s_seq.bQcCycle = (bInsertCount <= (uint8_t)SEQ_QC_CYCLE_LIMIT)
                                 ? 1u : 0u;
                s_GoStep(APP_SEQ_STICK_INSERT);
            }
            else
            {
                /* Keep the reading that failed the gate: it is the only record
                   of a unit that woke mechanically but saw nothing. */
                (void)APP_DATA_Record_UpdateWakeFail(wGateAdc);
                s_seq.bQcCycle = (bInsertCount < (uint8_t)SEQ_QC_CYCLE_LIMIT)
                                 ? 1u : 0u;

                if (s_seq.bQcCycle != 0u)
                {
                    s_EnterError(APP_ERR_STICK_FAIL);
                }
                else
                {
                    s_GoStep(APP_SEQ_IDLE);
                }
            }
            break;

        default:
            s_seq.tPhase = SEQ_PH_ENTER;
            break;
    }
}

/* ---- Step 2: STICK_INSERT ------------------------------------------------*/
/* Initial sweep, acceptance window, gain tuning, scaled set. The sweep is
   nine measurements and takes about 280 ms; it now runs across passes. */
static void s_StepStickInsert(void)
{
    APP_DATA_ErrorCode tErr;

    switch (s_seq.tPhase)
    {
        case SEQ_PH_ENTER:
            s_seq.bTestCount = APP_DATA_Record_GetTestCount();

            if (s_seq.bTestCount >= (uint8_t)APP_SCAN_MAX_COUNT)
            {
                s_EnterError(APP_ERR_OVER_USED);
                break;
            }

            if (APP_EXEC_Measure_StartInitial() != RET_OK)
            {
                break;      /* optics still busy, try again next pass */
            }

            /* Started here, not on step entry: entry can be re-run while the
               optics are busy, and the animation would restart each pass.
               It plays while the nine initial readings are taken. */
            (void)Lcd_PlayAnim(s_animStickInsert,
                               (uint8_t)SEQ_ANIM_INSERT_STEPS, 0u);

            s_seq.tPhase = SEQ_PH_WAIT;
            break;

        case SEQ_PH_WAIT:
            if (APP_EXEC_Measure_IsBusy() != 0u)
            {
                break;
            }

            /* The sweep only reports DONE after its closing presence check,
               so a partial set never reaches the stored record. */
            if (APP_EXEC_Measure_GetResult() != APP_EXEC_MEASURE_DONE)
            {
                s_InitialOpticError(APP_ERR_STICK_REMOVE);
                break;
            }

            (void)APP_DATA_Record_SetStickPtr(APP_EXEC_Measure_GetInitial());

            /* Acceptance window and gain in one call: both walk the same four
               readings and fail on the same channel number. */
            tErr = APP_EXEC_Measure_TuneGain();
            if (tErr != APP_ERR_NONE)
            {
                s_InitialOpticError(tErr);
                break;
            }

            tErr = APP_EXEC_Measure_GetScaledSet(s_seq.awScaled);
            if (tErr != APP_ERR_NONE)
            {
                s_InitialOpticError(tErr);
                break;
            }

            /* The readings are done (about 0.28 s) but the insert animation
               runs about 4.5 s. The old FW waited for it before asking for
               the sample, so this does too. */
            s_seq.tPhase = SEQ_PH_ANIM;
            break;

        case SEQ_PH_ANIM:
            if (Lcd_IsAnimBusy() != 0u)
            {
                break;
            }

            /* DROP steady, not blinking. The old FW blinks DROP only while
               the reaction runs and on a stick-removed error, so a blink
               here reads to the operator as "wait", and the sample never
               gets loaded. (old DM_App_LCD_Sq_SampleLoadWait_Process)
               (found on the bench 2026-11-03) */
            (void)Lcd_SetAll(LCD_OFF);
            (void)Lcd_SetIcon(LCD_ICON_DROP, LCD_ON);

            s_GoStep(APP_SEQ_SAMPLE_LOAD_WAIT);
            break;

        default:
            s_seq.tPhase = SEQ_PH_ENTER;
            break;
    }
}

/* Legacy Cal_Ratio: truncate to a word, without saturation. */
static uint16_t s_Ratio(uint16_t wValue, uint16_t wPercent)
{
    return (uint16_t)(((uint32_t)wValue * wPercent) / OPTIC_GAIN_SCALE);
}

/* Kept as a name the sequence steps read well with; the display choice
   itself lives in s_ShowError(). */
static void s_SequenceError(APP_DATA_ErrorCode tError)
{
    s_EnterError(tError);
}

/* One legacy 250 ms sleep, with the shipped 6250 ms optical recheck.
   A pending check must finish before its caller advances the wait phase. */
static uint8_t s_SleepAndCheckStick(APP_DATA_ErrorCode tError)
{
    if (s_seq.bChecking != 0u)
    {
        if (APP_EXEC_Measure_IsBusy() != 0u)
        {
            return 0u;
        }
        s_seq.bChecking = 0u;
        if (APP_EXEC_Measure_IsStickPresent() == 0u)
        {
            s_SequenceError(tError);
            return 0u;
        }
    }
    else
    {
        if (HW_Tick_Elapsed(s_seq.wSleepStart)
            < APP_STICK_CHECK_INTENDED_TK)
        {
            return 0u;
        }
        if (HW_Tick_Elapsed(s_seq.wStickStart) >= APP_STICK_CHECK_INTERVAL_TK)
        {
            if (APP_EXEC_Measure_StartStickCheck(1u) == RET_OK)
            {
                s_seq.wStickStart = HW_Tick_Get();
                s_seq.bChecking = 1u;
            }
            return 0u;
        }
    }
    s_seq.wSleepStart = HW_Tick_Get();
    return 1u;
}

static Ret_t s_SaveResult(void)
{
    uint16_t awRecord[APP_REC_SLOT_WORDS];
    uint8_t bCh;
    uint8_t bIndex = 0u;

    for (bCh = 0u; bCh < (uint8_t)OPTIC_CH_COUNT; bCh++)
    {
        awRecord[bIndex++] = Optic_GetGain((Optic_Ch)bCh);
    }
    for (bCh = 0u; bCh < (uint8_t)OPTIC_CH_COUNT; bCh++)
    {
        awRecord[bIndex++] = s_seq.awEmptyBand[bCh];
    }
    for (bCh = 0u; bCh < (uint8_t)OPTIC_CH_COUNT; bCh++)
    {
        awRecord[bIndex++] = s_seq.awResultBand[bCh];
    }
    for (bCh = 0u; bCh < (uint8_t)SEQ_RES_COUNT; bCh++)
    {
        awRecord[bIndex++] = s_seq.awResult[bCh];
    }
    /* The record format stores the step in the high byte. */
    awRecord[bIndex] = (uint16_t)(((uint16_t)s_seq.bErrorStep << 8u)
                                | (uint8_t)s_seq.tError);
    return APP_DATA_Record_SaveResult(awRecord);
}

/* ---- Step 3: SAMPLE_LOAD_WAIT -------------------------------------------*/
static void s_StepSampleLoadWait(void)
{
    uint8_t bCh;

    switch (s_seq.tPhase)
    {
        case SEQ_PH_ENTER:
            for (bCh = 0u; bCh < (uint8_t)OPTIC_CH_COUNT; bCh++)
            {
                s_seq.awEmptyBand[bCh] = s_seq.awScaled[bCh];
            }
            s_seq.wThreshold = s_Ratio(APP_EXEC_Measure_Intensity(
                s_seq.awEmptyBand[OPTIC_CH_T], s_seq.awEmptyBand[OPTIC_CH_BT]),
                APP_EMPTY_BTB_RATIO);
            s_seq.wStart = HW_Tick_Get();
            s_seq.wSleepStart = s_seq.wStart;
            /* The old cached-status reset forces the first recheck. */
            s_seq.wStickStart = (uint16_t)(s_seq.wStart - APP_STICK_CHECK_INTERVAL_TK);
            s_seq.bChecking = 0u;
            s_seq.bSleeps = 0u;
            s_seq.tPhase = SEQ_PH_WAIT;
            break;

        case SEQ_PH_WAIT:
            if (s_SleepAndCheckStick(APP_ERR_STICK_REMOVE) == 0u)
            {
                break;
            }
            s_seq.bSleeps++;
            /* Four 250 ms sleeps precede each T/BT pair in the original. */
            if (s_seq.bSleeps < (uint8_t)OPTIC_CH_COUNT)
            {
                break;
            }
            s_seq.bSleeps = 0u;
            if (HW_Tick_Elapsed(s_seq.wStart) >= APP_SAMPLE_LOAD_WAIT_TK)
            {
                s_seq.awResultBand[OPTIC_CH_T] = s_seq.awScaled[OPTIC_CH_T];
                s_seq.awResultBand[OPTIC_CH_BT] = s_seq.awScaled[OPTIC_CH_BT];
                s_seq.tError = APP_ERR_SAMPLE_LOAD_FAIL;
                s_seq.bErrorStep = (uint8_t)s_seq.tStep;
                (void)APP_DATA_Record_SetError(s_seq.bErrorStep,
                                               (uint8_t)s_seq.tError);
                s_seq.tPhase = SEQ_PH_SAVE;
                break;
            }
            s_seq.bChannel = (uint8_t)OPTIC_CH_T;
            s_seq.tPhase = SEQ_PH_START_CHANNEL;
            break;

        case SEQ_PH_START_CHANNEL:
            if (APP_EXEC_Measure_StartSingle((Optic_Ch)s_seq.bChannel) == RET_OK)
            {
                s_seq.tPhase = SEQ_PH_CHANNEL;
            }
            break;

        case SEQ_PH_CHANNEL:
            if (APP_EXEC_Measure_IsBusy() != 0u)
            {
                break;
            }
            s_seq.awScaled[s_seq.bChannel] = Optic_GetScaled((Optic_Ch)s_seq.bChannel);
            if (s_seq.bChannel == (uint8_t)OPTIC_CH_BT)
            {
                if (APP_EXEC_Measure_Intensity(s_seq.awScaled[OPTIC_CH_T],
                    s_seq.awScaled[OPTIC_CH_BT]) >= s_seq.wThreshold)
                {
                    s_seq.wSleepStart = HW_Tick_Get();
                    s_seq.tPhase = SEQ_PH_WAIT;
                    break;
                }
                s_seq.awResultBand[OPTIC_CH_T] = s_seq.awScaled[OPTIC_CH_T];
                s_seq.awResultBand[OPTIC_CH_BT] = s_seq.awScaled[OPTIC_CH_BT];
            }
            else if (s_seq.bChannel >= (uint8_t)OPTIC_CH_BC)
            {
                s_seq.awResultBand[s_seq.bChannel] = s_seq.awScaled[s_seq.bChannel];
            }
            s_seq.bChannel++;
            if (s_seq.bChannel == (uint8_t)OPTIC_CH_COUNT)
            {
                s_GoStep(APP_SEQ_OVER_SAMPLE_CHECK);
            }
            else
            {
                s_seq.tPhase = SEQ_PH_START_CHANNEL;
            }
            break;

        case SEQ_PH_SAVE:
            if (s_SaveResult() == RET_OK)
            {
                s_seq.tPhase = SEQ_PH_SAVED;
            }
            break;

        case SEQ_PH_SAVED:
            /* Complete the deferred store before IDLE stops the scheduler. */
            if (APP_DATA_Record_IsBusy() == 0u)
            {
                s_GoStep(APP_SEQ_IDLE);
            }
            break;

        default:
            s_seq.tPhase = SEQ_PH_ENTER;
            break;
    }
}

/* Shared wait/one-C-channel shape for the two volume checks. */
static void s_SampleCheck(uint16_t wWait, uint16_t wBase, uint16_t wRatio,
                          APP_CTRL_Sequence_Step tNext, APP_DATA_ErrorCode tError)
{
    uint16_t wC;
    uint8_t bPass;

    switch (s_seq.tPhase)
    {
        case SEQ_PH_ENTER:
            s_seq.wThreshold = s_Ratio(wBase, wRatio);
            s_seq.wStart = HW_Tick_Get();
            s_seq.wSleepStart = s_seq.wStart;
            s_seq.tPhase = SEQ_PH_WAIT;
            break;

        case SEQ_PH_WAIT:
            if ((s_SleepAndCheckStick(APP_ERR_STICK_FAIL) != 0u)
                && (HW_Tick_Elapsed(s_seq.wStart) >= wWait))
            {
                s_seq.tPhase = SEQ_PH_START_CHANNEL;
            }
            break;

        case SEQ_PH_START_CHANNEL:
            if (APP_EXEC_Measure_StartSingle(OPTIC_CH_C) == RET_OK)
            {
                s_seq.tPhase = SEQ_PH_CHANNEL;
            }
            break;

        case SEQ_PH_CHANNEL:
            if (APP_EXEC_Measure_IsBusy() != 0u)
            {
                break;
            }
            wC = Optic_GetScaled(OPTIC_CH_C);
            bPass = (tError == APP_ERR_OVER_FLOW)
                    ? (wC > s_seq.wThreshold) : (wC < s_seq.wThreshold);
            if (bPass != 0u)
            {
                s_GoStep(tNext);
            }
            else
            {
                s_SequenceError(tError);
            }
            break;

        default:
            s_seq.tPhase = SEQ_PH_ENTER;
            break;
    }
}

/* ---- Steps 4 and 5: OVER_SAMPLE_CHECK, LOW_SAMPLE_CHECK -------------------*/
static void s_StepOverSampleCheck(void)
{
    s_SampleCheck(APP_OVER_SAMPLE_CHECK_TK, s_seq.awResultBand[OPTIC_CH_C],
                  APP_OVER_FLOW_RATIO, APP_SEQ_LOW_SAMPLE_CHECK, APP_ERR_OVER_FLOW);

    /* DROP starts blinking the moment the over-sample check passes, not at
       the reaction wait 20 s later. The old FW sets its LCD step to
       REACTION_WAIT right here (DM_Main_Sq_App.c:529), from the sequence
       handler - the LCD file's own OVER/LOW_SAMPLE_CHECK cases are empty
       because the display was already set. Reading only the LCD file is how
       this was first put 20 s late.
       The blink therefore covers 20 s + 155 s = 175 s, which is the
       "blinks, then about 3 minutes" in the user manual.
       (corrected on the bench 2026-11-03) */
    if (s_seq.tStep == APP_SEQ_LOW_SAMPLE_CHECK)
    {
        (void)Lcd_SetAll(LCD_OFF);
        (void)Lcd_SetIcon(LCD_ICON_DROP, LCD_BLINK);
    }
}

static void s_StepLowSampleCheck(void)
{
    s_SampleCheck(APP_LOW_SAMPLE_CHECK_TK, s_seq.awEmptyBand[OPTIC_CH_C],
                  APP_SMALL_SAMPLE_RATIO, APP_SEQ_REACTION_WAIT, APP_ERR_LOW_SAMPLE);
}

/* ---- Step 6: REACTION_WAIT ----------------------------------------------*/
static void s_StepReactionWait(void)
{
    if (s_seq.tPhase == SEQ_PH_ENTER)
    {
        /* No display change: DROP has been blinking since the over-sample
           check passed. */
        s_seq.wStart = HW_Tick_Get();
        s_seq.wSleepStart = s_seq.wStart;
        s_seq.tPhase = SEQ_PH_WAIT;
    }
    else if ((s_SleepAndCheckStick(APP_ERR_STICK_FAIL) != 0u)
             && (HW_Tick_Elapsed(s_seq.wStart) >= APP_REACTION_WAIT_TK))
    {
        s_GoStep(APP_SEQ_RESULT_SCAN);
    }
}

/* ---- Step 7: RESULT_SCAN ------------------------------------------------*/
static void s_StepResultScan(void)
{
    uint16_t wT;
    uint16_t wBT;
    uint16_t wC;
    uint16_t wBC;

    switch (s_seq.tPhase)
    {
        case SEQ_PH_ENTER:
            s_seq.bChannel = (uint8_t)OPTIC_CH_C;
            s_seq.tPhase = SEQ_PH_START_CHANNEL;
            break;

        case SEQ_PH_START_CHANNEL:
            if (APP_EXEC_Measure_StartSingle((Optic_Ch)s_seq.bChannel) == RET_OK)
            {
                s_seq.tPhase = SEQ_PH_CHANNEL;
            }
            break;

        case SEQ_PH_CHANNEL:
            if (APP_EXEC_Measure_IsBusy() != 0u)
            {
                break;
            }
            s_seq.awResultBand[s_seq.bChannel] = Optic_GetScaled((Optic_Ch)s_seq.bChannel);
            if (s_seq.bChannel != (uint8_t)OPTIC_CH_T)
            {
                s_seq.bChannel--;
                s_seq.tPhase = SEQ_PH_START_CHANNEL;
                break;
            }
            wT = APP_EXEC_Measure_Intensity(s_seq.awResultBand[OPTIC_CH_T],
                                           s_seq.awEmptyBand[OPTIC_CH_T]);
            wBT = APP_EXEC_Measure_Intensity(s_seq.awResultBand[OPTIC_CH_BT],
                                            s_seq.awEmptyBand[OPTIC_CH_BT]);
            wC = APP_EXEC_Measure_Intensity(s_seq.awResultBand[OPTIC_CH_C],
                                           s_seq.awEmptyBand[OPTIC_CH_C]);
            wBC = APP_EXEC_Measure_Intensity(s_seq.awResultBand[OPTIC_CH_BC],
                                            s_seq.awEmptyBand[OPTIC_CH_BC]);
            s_seq.awResult[SEQ_RES_T] = APP_EXEC_Measure_Intensity(wBT, wT);
            s_seq.awResult[SEQ_RES_C] = APP_EXEC_Measure_Intensity(wBC, wC);
            if (s_seq.awResult[SEQ_RES_C] < APP_C_LINE_THRESHOLD)
            {
                s_SequenceError(APP_ERR_C_LINE_FAIL);
                break;
            }
            s_seq.awResult[SEQ_RES_YES_NO] =
                (s_seq.awResult[SEQ_RES_T] > APP_T_LINE_THRESHOLD) ? 1u : 0u;
            (void)Lcd_SetAll(LCD_OFF);
            (void)Lcd_SetIcon((s_seq.awResult[SEQ_RES_YES_NO] != 0u)
                              ? LCD_ICON_YES : LCD_ICON_NO, LCD_ON);
            s_GoStep(APP_SEQ_RESULT_OUT);
            break;

        default:
            s_seq.tPhase = SEQ_PH_ENTER;
            break;
    }
}

/* ---- Step 8: RESULT_OUT -------------------------------------------------*/
static void s_StepResultOut(void)
{
    if (s_seq.tPhase == SEQ_PH_ENTER)
    {
        s_seq.bTestCount++;
        (void)APP_DATA_Record_SetTestCount(s_seq.bTestCount);
        s_seq.tPhase = SEQ_PH_SAVE;
    }
    if (s_seq.tPhase == SEQ_PH_SAVE)
    {
        if (s_SaveResult() == RET_OK)
        {
            s_seq.wStart = HW_Tick_Get();
            s_seq.tPhase = SEQ_PH_WAIT;
        }
    }
    else if (HW_Tick_Elapsed(s_seq.wStart) >= APP_RESULT_OUT_WAIT_TK)
    {
        s_GoStep(APP_SEQ_IDLE);
    }
}

/* ---- Step 9: ERROR_WAIT -------------------------------------------------*/
static void s_StepErrorWait(void)
{
    if (s_seq.tPhase == SEQ_PH_ENTER)
    {
        /* IDLE gate failures have only the wake-fail record, no result slot. */
        if (s_seq.bErrorStep != (uint8_t)APP_SEQ_IDLE)
        {
            (void)APP_DATA_Record_SetError(s_seq.bErrorStep, (uint8_t)s_seq.tError);
            if (s_SaveResult() != RET_OK)
            {
                return;
            }
        }
        s_seq.wStart = HW_Tick_Get();
        s_seq.tPhase = SEQ_PH_WAIT;
    }
    else if (HW_Tick_Elapsed(s_seq.wStart) >= APP_ERR_RUN_TIME_TK)
    {
        s_GoStep(APP_SEQ_IDLE);
    }
}

/* Public functions ---------------------------------------------------------*/
Ret_t APP_CTRL_Sequence_Init(void)
{
    s_seq.tStep      = APP_SEQ_IDLE;
    s_seq.tPhase     = SEQ_PH_ENTER;
    s_seq.wStart    = HW_Tick_Get();
    s_seq.tError     = APP_ERR_NONE;
    s_seq.bErrorStep = 0u;
    s_seq.bQcCycle   = 0u;
    s_seq.bTestCount = 0u;

    return RET_OK;
}

Ret_t APP_CTRL_Sequence_Process(void)
{
    switch (s_seq.tStep)
    {
        case APP_SEQ_IDLE:
            s_StepIdle();
            break;

        case APP_SEQ_STICK_INSERT:
            s_StepStickInsert();
            break;

        case APP_SEQ_SAMPLE_LOAD_WAIT:
            s_StepSampleLoadWait();
            break;
        case APP_SEQ_OVER_SAMPLE_CHECK:
            s_StepOverSampleCheck();
            break;
        case APP_SEQ_LOW_SAMPLE_CHECK:
            s_StepLowSampleCheck();
            break;
        case APP_SEQ_REACTION_WAIT:
            s_StepReactionWait();
            break;
        case APP_SEQ_RESULT_SCAN:
            s_StepResultScan();
            break;
        case APP_SEQ_RESULT_OUT:
            s_StepResultOut();
            break;
        case APP_SEQ_ERROR_WAIT:
            s_StepErrorWait();
            break;

        default:
            s_GoStep(APP_SEQ_IDLE);
            break;
    }

    return RET_OK;
}

Ret_t APP_CTRL_Sequence_ResetTestCount(void)
{
    s_seq.bTestCount = 0u;

    return APP_DATA_Record_SetTestCount(0u);
}

Ret_t APP_CTRL_Sequence_RestartQc(void)
{
    (void)APP_DATA_Record_SetInsertCount(0u);
    (void)APP_DATA_Record_SetTestCount(0u);
    (void)APP_DATA_Record_SetError(0u, (uint8_t)APP_ERR_NONE);

    s_ClearRun();
    s_seq.bQcCycle = 1u;
    s_GoStep(APP_SEQ_IDLE);

    return RET_OK;
}

APP_CTRL_Sequence_Step APP_CTRL_Sequence_GetStep(void)
{
    return s_seq.tStep;
}

APP_DATA_ErrorCode APP_CTRL_Sequence_GetError(void)
{
    return s_seq.tError;
}

uint8_t APP_CTRL_Sequence_IsQcCycle(void)
{
    return s_seq.bQcCycle;
}

/*****************************END OF FILE****/
