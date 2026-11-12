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
/* The initial readings of the first five insertions are kept as the QC
   reference in the dump, as MFx_New_Ver did on its QC cycles
   (DM_Main_Sq_App.c:296). The port dropped the write and every new unit
   dumped 0/0/0/0 (field units, 2026-11-10). Only the write is restored:
   the display no longer depends on the insert count (review D4). */
#define SEQ_QC_REF_INSERTS       5u
#define SEQ_INSERT_COUNT_CAP   254u

/* Wake-up switch check: one read per interval. A period, not a floor - the
   first read may come less than 10 ms after the wake, which the 8-of-10
   rule absorbs. */
#define SEQ_WAKE_SW_TK           HW_TICK_MS(APP_WAKE_SW_INTERVAL_MS)

/* Private types -------------------------------------------------------------*/
typedef enum
{
    SEQ_PH_ENTER = 0,      /* first pass in this step  */
    SEQ_PH_WAIT,           /* waiting on time or a job */
    SEQ_PH_SWITCH,         /* wake-up switch check, 10 reads */
    SEQ_PH_START_CHANNEL,
    SEQ_PH_CHANNEL,
    SEQ_PH_SAVE,
    SEQ_PH_SAVED,
    SEQ_PH_ANIM,           /* waiting for an LCD animation to finish */
    SEQ_PH_HOLD,           /* sample seen: is the stick still in?   */
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
#if (APP_TEST_MINLOG == 1)
    uint16_t               awTestMin[OPTIC_CH_COUNT]; /* lowest, gain applied */
    uint16_t               wTestRun;     /* current switch-open stretch    */
    uint16_t               wTestRunMax;  /* longest stretch                */
    uint8_t                bTestCh;      /* channel the next read takes    */
#endif
    uint8_t                bSwReads;     /* switch reads in this window    */
    uint8_t                bSwLow;       /* of those, reads that were low  */
    uint8_t                bSwOpenRun;   /* windows in a row judged open   */
    uint8_t                bStrikes;     /* open window + dark C, in a row */
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
static void s_ClearRun(void);
static void s_StepIdle(void);
static void s_StepStickInsert(void);
static uint16_t s_Ratio(uint16_t wValue, uint16_t wPercent);
static uint8_t s_Sleep(void);
static void s_RemoveCheck(void);
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

static void s_ClearRun(void)
{
    uint8_t bCh;

    s_seq.tError     = APP_ERR_NONE;
    s_seq.bErrorStep = 0u;
    s_seq.bChecking  = 0u;
    s_seq.bSwOpenRun = 0u;
    s_seq.bStrikes   = 0u;
#if (APP_TEST_MINLOG == 1)
    for (bCh = 0u; bCh < (uint8_t)OPTIC_CH_COUNT; bCh++)
    {
        s_seq.awTestMin[bCh] = 0xFFFFu;
    }
    s_seq.wTestRun    = 0u;
    s_seq.wTestRunMax = 0u;
    s_seq.bTestCh     = 0u;
#endif

    for (bCh = 0u; bCh < (uint8_t)OPTIC_CH_COUNT; bCh++)
    {
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
/* Deep sleep and the wake-up switch check. The order of the shutdown matters: the
   measured 71 uA (unit 12, PPK2, 2026-10-30) depends on the LEDs being off,
   the display parked, the regulator in ultra-low-power mode and all three
   peripheral clocks gated before halt() is reached. */
static void s_StepIdle(void)
{
    uint8_t bInsertCount;

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

            s_seq.bSwReads = 0u;
            s_seq.bSwLow   = 0u;
            s_seq.wStart   = HW_Tick_Get();
            s_seq.tPhase   = SEQ_PH_SWITCH;
            break;

        case SEQ_PH_SWITCH:
            /* Is the stick really in? The switch is what woke the unit, so
               it is what is asked: 10 reads, 10 ms apart, at least 8 low
               (hs.kim review D1). This replaces the optical gate on raw
               C >= 2600, which turned some good insertions away (unit
               152510, 2026-11-06). The optics are not touched here. */
            if (HW_Tick_Elapsed(s_seq.wStart) < SEQ_WAKE_SW_TK)
            {
                break;
            }
            s_seq.wStart = HW_Tick_Get();

            if (HW_GPIO_Read(HW_GPIO_STICK_SW) == HW_GPIO_LEVEL_LOW)
            {
                s_seq.bSwLow++;
            }
            s_seq.bSwReads++;
            if (s_seq.bSwReads < (uint8_t)APP_WAKE_SW_SAMPLES)
            {
                break;
            }

            if (s_seq.bSwLow < (uint8_t)APP_WAKE_SW_PASS)
            {
                /* Bounce or noise: back to sleep, nothing shown, nothing
                   counted. */
                s_GoStep(APP_SEQ_IDLE);
                break;
            }

            bInsertCount = APP_DATA_Record_GetInsertCount();
            if (bInsertCount < (uint8_t)SEQ_INSERT_COUNT_CAP)
            {
                bInsertCount++;
                (void)APP_DATA_Record_SetInsertCount(bInsertCount);
            }
            s_ClearRun();
            s_GoStep(APP_SEQ_STICK_INSERT);
            break;

        default:
            s_seq.tPhase = SEQ_PH_ENTER;
            break;
    }
}

/* ---- Step 2: STICK_INSERT ------------------------------------------------*/
/* Initial sweep, acceptance window, gain tuning, scaled set, then the insert
   animation. The four readings come first (about 0.1 s) so that a reading
   outside the window shows BOOK at once, before any animation - as the old
   FW did (hs.kim, 2026-11-10). Every failure here is shown: the old rule of
   showing it only on the first five insertions and otherwise going quietly
   to sleep left a unit that seemed dead (review D4). Done in this step, not
   in IDLE, so Error Step 1 in a dump keeps meaning "initial optics"
   (the step number is the 0-based APP_CTRL_Sequence_Step). */
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

            s_seq.tPhase = SEQ_PH_WAIT;
            break;

        case SEQ_PH_WAIT:
            if (APP_EXEC_Measure_IsBusy() != 0u)
            {
                break;
            }

            /* The job ran the steady sweep, the acceptance window and the
               charge-time tuning; a failed conversion is reported as
               OPTIC_FAIL on the channel it hit, like the rest. */
            (void)APP_DATA_Record_SetStickPtr(APP_EXEC_Measure_GetInitial());
#if (APP_TEST_MINLOG == 0)
            if (APP_DATA_Record_GetInsertCount() <= (uint8_t)SEQ_QC_REF_INSERTS)
            {
                (void)APP_DATA_Record_SetQcRef(APP_EXEC_Measure_GetInitial());
            }
#endif

            tErr = APP_EXEC_Measure_TuneError();
            APP_EXEC_Measure_GetTunedSet(s_seq.awScaled);
            if (tErr != APP_ERR_NONE)
            {
                s_EnterError(tErr);
                break;
            }

            /* Readings good: now the insert animation, about 4.5 s. The old
               FW waited for it before asking for the sample, so this does
               too. */
            (void)Lcd_PlayAnim(s_animStickInsert,
                               (uint8_t)SEQ_ANIM_INSERT_STEPS, 0u);

            /* Charge times are set: the removal check can judge from here
               on (review D6). */
            s_seq.bChecking   = 0u;
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

/* One legacy 250 ms sleep. Only paces the waiting loops; the stick is
   checked by s_RemoveCheck(), on its own clock (review D6, M5/M6). */
static uint8_t s_Sleep(void)
{
    if (HW_Tick_Elapsed(s_seq.wSleepStart) < APP_SEQ_SLEEP_TK)
    {
        return 0u;
    }
    s_seq.wSleepStart = HW_Tick_Get();
    return 1u;
}

/* Stick removal check, run ahead of the step on every 10 ms pass. It is
   separate from the steps so that neither a running animation nor a step's
   own timing can delay it (hs.kim, 2026-11-10):
     STICK_INSERT animation, SAMPLE_LOAD_WAIT -> STICK_REMOVE (BOOK + DROP)
     OVER/LOW_SAMPLE_CHECK, REACTION_WAIT     -> STICK_FAIL   (BOOK)
   Removed means the switch AND the optics say so, see APP_STICK_SW_WINDOW
   (hs.kim, 2026-11-11). The wake check's two counters carry straight on as
   the first window: they are the same switch's history.
   An optical check starts only while the step is in its WAIT (or animation)
   phase, never while the step measures a channel itself (review D7), so the
   two never compete for the optics. The result is taken on the first pass
   after the check finishes, before the step can start anything new. */
static void s_RemoveCheck(void)
{
    uint8_t            bActive;
    APP_DATA_ErrorCode tErr = APP_ERR_STICK_FAIL;
#if (APP_TEST_MINLOG == 1)
    uint16_t           wTest;
#else
    uint8_t            bStart = 0u;
#endif

    switch (s_seq.tStep)
    {
        case APP_SEQ_STICK_INSERT:
            bActive = (s_seq.tPhase == SEQ_PH_ANIM) ? 1u : 0u;
            tErr    = APP_ERR_STICK_REMOVE;
            break;

        case APP_SEQ_SAMPLE_LOAD_WAIT:
            bActive = ((s_seq.tPhase == SEQ_PH_WAIT)
                       || (s_seq.tPhase == SEQ_PH_HOLD)) ? 1u : 0u;
            tErr    = APP_ERR_STICK_REMOVE;
            break;

        case APP_SEQ_OVER_SAMPLE_CHECK:
        case APP_SEQ_LOW_SAMPLE_CHECK:
        case APP_SEQ_REACTION_WAIT:
            bActive = (s_seq.tPhase == SEQ_PH_WAIT) ? 1u : 0u;
            break;

        default:
            bActive = 0u;
            break;
    }

#if (APP_TEST_MINLOG == 1)
    /* Test build: log, never end the run. The switch is read on every pass
       from the end of tuning to the result scan; high means open. */
    (void)tErr;
    if ((bActive != 0u)
        || ((s_seq.tStep >= APP_SEQ_SAMPLE_LOAD_WAIT)
            && (s_seq.tStep <= APP_SEQ_RESULT_SCAN)))
    {
        if (HW_GPIO_Read(HW_GPIO_STICK_SW) != HW_GPIO_LEVEL_LOW)
        {
            s_seq.wTestRun++;
            if (s_seq.wTestRun > s_seq.wTestRunMax)
            {
                s_seq.wTestRunMax = s_seq.wTestRun;
            }
        }
        else
        {
            s_seq.wTestRun = 0u;
        }
    }

    if (s_seq.bChecking != 0u)
    {
        if (APP_EXEC_Measure_IsBusy() != 0u)
        {
            return;
        }
        s_seq.bChecking = 0u;
        wTest = Optic_GetRaw((Optic_Ch)s_seq.bTestCh);
        if (wTest < s_seq.awTestMin[s_seq.bTestCh])
        {
            s_seq.awTestMin[s_seq.bTestCh] = wTest;
        }
        s_seq.bTestCh = (uint8_t)((s_seq.bTestCh + 1u)
                                  & (uint8_t)(OPTIC_CH_COUNT - 1u));
        return;
    }

    if ((bActive == 0u)
        || (HW_Tick_Elapsed(s_seq.wStickStart) < APP_TEST_MINLOG_TK))
    {
        return;
    }
    if (APP_EXEC_Measure_StartSingle((Optic_Ch)s_seq.bTestCh) == RET_OK)
    {
        s_seq.wStickStart = HW_Tick_Get();
        s_seq.bChecking   = 1u;
    }
#else
    if (s_seq.tStep == APP_SEQ_IDLE)
    {
        return;         /* the wake check owns the two counters there */
    }

    /* The switch, one read per pass, judged every window. It is read while
       an optical check runs too, so that a contact that closes again is
       seen before the check's answer is used. */
    if (HW_GPIO_Read(HW_GPIO_STICK_SW) == HW_GPIO_LEVEL_LOW)
    {
        s_seq.bSwLow++;
    }
    s_seq.bSwReads++;
    if (s_seq.bSwReads >= (uint8_t)APP_STICK_SW_WINDOW)
    {
        if (s_seq.bSwLow >= (uint8_t)APP_STICK_SW_PASS)
        {
            s_seq.bSwOpenRun = 0u;
        }
        else
        {
            if (s_seq.bSwOpenRun < 0xFFu)
            {
                s_seq.bSwOpenRun++;
            }
            bStart = 1u;
        }
        s_seq.bSwReads = 0u;
        s_seq.bSwLow   = 0u;
    }

    /* The optical check that was started. A dark C is a strike only while
       the switch still reads open; two in a row are a removal. */
    if (s_seq.bChecking != 0u)
    {
        if (APP_EXEC_Measure_IsBusy() != 0u)
        {
            return;
        }
        s_seq.bChecking = 0u;
        if ((bActive != 0u) && (s_seq.bSwOpenRun != 0u)
            && (APP_EXEC_Measure_IsStickPresent() == 0u))
        {
            s_seq.bStrikes++;
            if (s_seq.bStrikes >= (uint8_t)APP_STICK_STRIKES)
            {
                s_SequenceError(tErr);
                return;
            }
            bStart = 1u;            /* look again without waiting */
        }
        else
        {
            s_seq.bStrikes = 0u;
        }
    }

    if ((bStart != 0u) && (bActive != 0u)
        && (APP_EXEC_Measure_StartStickCheck() == RET_OK))
    {
        s_seq.bChecking = 1u;
    }
#endif
}

static Ret_t s_SaveResult(void)
{
    uint16_t awRecord[APP_REC_SLOT_WORDS];
    uint8_t bCh;
    uint8_t bIndex = 0u;

    for (bCh = 0u; bCh < (uint8_t)OPTIC_CH_COUNT; bCh++)
    {
        awRecord[bIndex++] = Optic_GetTime((Optic_Ch)bCh);   /* was the gain */
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
#if (APP_TEST_MINLOG == 1)
    /* T's slot carries the switch figure: the removal design needs BT, BC
       and C, and the dump has only four free words that it shows. */
    /* A run that never reached the logging window (an early error, or the
       used stick put back in to wake the unit for the dump) must not wipe
       the figures of the run before it (2026-11-11, 물책 1번). */
    if (s_seq.awTestMin[OPTIC_CH_C] != 0xFFFFu)
    {
        s_seq.awTestMin[OPTIC_CH_T] = s_seq.wTestRunMax;
        (void)APP_DATA_Record_SetQcRef(s_seq.awTestMin);
    }
#endif
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
            s_seq.bSleeps = 0u;
            s_seq.tPhase = SEQ_PH_WAIT;
            break;

        case SEQ_PH_WAIT:
            if (s_Sleep() == 0u)
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
            s_seq.awScaled[s_seq.bChannel] = Optic_GetRaw((Optic_Ch)s_seq.bChannel);
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
                /* Not on to the volume checks yet: a pulled stick looks
                   like an arriving sample. Hold, and let the removal check
                   say STICK_REMOVE if that is what happened
                   (APP_ARRIVAL_HOLD_TK). */
                s_seq.wStart = HW_Tick_Get();
                s_seq.tPhase = SEQ_PH_HOLD;
            }
            else
            {
                s_seq.tPhase = SEQ_PH_START_CHANNEL;
            }
            break;

        case SEQ_PH_HOLD:
            if (HW_Tick_Elapsed(s_seq.wStart) >= APP_ARRIVAL_HOLD_TK)
            {
                s_GoStep(APP_SEQ_OVER_SAMPLE_CHECK);
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
            if ((s_Sleep() != 0u)
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
            wC = Optic_GetRaw(OPTIC_CH_C);
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
    s_SampleCheck((uint16_t)(APP_OVER_SAMPLE_CHECK_TK - APP_ARRIVAL_HOLD_TK),
                  s_seq.awResultBand[OPTIC_CH_C],
                  APP_OVER_FLOW_RATIO, APP_SEQ_LOW_SAMPLE_CHECK, APP_ERR_OVER_FLOW);

    /* DROP starts blinking the moment the over-sample check passes, not at
       the reaction wait 20 s later. The old FW sets its LCD step to
       REACTION_WAIT right here (DM_Main_Sq_App.c:529), from the sequence
       handler - the LCD file's own OVER/LOW_SAMPLE_CHECK cases are empty
       because the display was already set. Reading only the LCD file is how
       this was first put 20 s late.
       The blink therefore covers 20 s + 160 s = 180 s, the manual's
       "물방울이 깜빡이고 3분 후" (APP_BLINK_TO_RESULT_TK).
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
    else if ((s_Sleep() != 0u)
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
            s_seq.awResultBand[s_seq.bChannel] = Optic_GetRaw((Optic_Ch)s_seq.bChannel);
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
        /* Every error reaching here has a step past IDLE: since the switch
           check (2026-11-10) a failed wake goes back to sleep unshown. */
        (void)APP_DATA_Record_SetError(s_seq.bErrorStep, (uint8_t)s_seq.tError);
        if (s_SaveResult() != RET_OK)
        {
            return;
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
    s_seq.bTestCount = 0u;

    return RET_OK;
}

Ret_t APP_CTRL_Sequence_Process(void)
{
    s_RemoveCheck();

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

/*****************************END OF FILE****/
