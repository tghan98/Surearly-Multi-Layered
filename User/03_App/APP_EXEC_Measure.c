/**
  ******************************************************************************
  * @file    : APP_EXEC_Measure.c
  * @brief   : Application layer (exec) - optical measurement procedures
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "APP_EXEC_Measure.h"
#include "APP_DATA_Config.h"

/* Private defines -----------------------------------------------------------*/
#define MEAS_CH_USED             ((uint8_t)OPTIC_CH_COUNT)   /* 4 */

/* Private types -------------------------------------------------------------*/
typedef enum
{
    MEAS_JOB_NONE = 0,
    MEAS_JOB_STICK,        /* one presence check              */
    MEAS_JOB_SINGLE,       /* one channel                     */
    MEAS_JOB_INITIAL,      /* the four-channel steady sweep   */
    MEAS_JOB_TUNE,         /* charge time, channel by channel */
    MEAS_JOB_COUNT
} Meas_Job;

typedef struct
{
    Meas_Job                tJob;
    APP_EXEC_Measure_Result tResult;
    uint8_t                 bCh;          /* channel under way            */
    uint8_t                 bPresent;     /* last presence outcome        */
    uint8_t                 bTries;       /* tuning readings, this channel */
    uint16_t                wTime;        /* first tuning point, loops    */
    uint16_t                wY1;          /* reading at the first point   */
    APP_DATA_ErrorCode      tErr;         /* outcome of sweep and tuning  */
    uint16_t                awInitial[APP_EXEC_MEASURE_INITIAL_WORDS];
    uint16_t                awEmpty[MEAS_CH_USED];   /* dry, tuned time */
} Meas_Ctx;

/* Private variables ---------------------------------------------------------*/
/* __near: this buffer lives in RAM above page zero. Page zero is only
   256 bytes and every variable defaults there in the small data model;
   the bulk buffers are moved out so the frequently touched state can
   stay in the short-addressed page. See EWSTM8/MFx_Layered.icf. */
__near static Meas_Ctx s_meas;

/* Private function prototypes -----------------------------------------------*/
static void     s_JudgeStick(void);
static Ret_t    s_StartOptic(Optic_Ch tCh);
static void     s_TuneTry(uint16_t wTime);
static void     s_TuneFirst(void);
static void     s_EndTune(APP_DATA_ErrorCode tErr);
static APP_DATA_ErrorCode s_FailCh(void);

/* Private functions ---------------------------------------------------------*/
/* One tuning reading of the current channel at wTime loops. */
static void s_TuneTry(uint16_t wTime)
{
    /* Never 0 (a steady reading to Optic, 65536 loops to the CPU): the first
       point is at least 3000 x 1200 / 3900 - 200 = 723, the second higher. */
    s_meas.bTries++;
    (void)Optic_SetTime((Optic_Ch)s_meas.bCh, wTime);
    (void)Optic_StartOnce((Optic_Ch)s_meas.bCh);
}

/* First point of the current channel: e - span, e = full x 1200 / S.
   S is 1500..3900 here, so e is 923..2400 and e - 200 never reaches 0. */
static void s_TuneFirst(void)
{
    uint32_t dwE;

    dwE = ((uint32_t)APP_OPTIC_FULL_LOOPS * (uint32_t)APP_OPTIC_TARGET_LIGHT)
          / s_meas.awInitial[s_meas.bCh];
    s_meas.wTime = (uint16_t)(dwE - APP_OPTIC_TUNE_SPAN);
    s_TuneTry(s_meas.wTime);
}

/* OPTIC_FAIL on the channel under way. */
static APP_DATA_ErrorCode s_FailCh(void)
{
    return (APP_DATA_ErrorCode)((uint8_t)APP_ERR_OPTIC_FAIL_CH0 + s_meas.bCh);
}

static void s_EndTune(APP_DATA_ErrorCode tErr)
{
    s_meas.tErr    = tErr;
    s_meas.tJob    = MEAS_JOB_NONE;
    s_meas.tResult = APP_EXEC_MEASURE_DONE;
}

static Ret_t s_StartOptic(Optic_Ch tCh)
{
    return Optic_Start(tCh);
}

/* The C channel is the one that sees the stick (old OPTIC_CH_3). A tuned
   reading is on the axis the gain-applied value used to be on (dry stick
   about 1200), so the threshold stands (2026-11-11). */
static void s_JudgeStick(void)
{
    s_meas.bPresent = (Optic_GetRaw(OPTIC_CH_C)
                       < (uint16_t)APP_OPTIC_STICK_THRESHOLD_SCALED)
                      ? 0u : 1u;
}

/* Functions -----------------------------------------------------------------*/
Ret_t APP_EXEC_Measure_Init(void)
{
    uint8_t bIdx;

    s_meas.tJob     = MEAS_JOB_NONE;
    s_meas.tResult  = APP_EXEC_MEASURE_IDLE;
    s_meas.bCh      = 0u;
    s_meas.bPresent = 0u;
    s_meas.tErr     = APP_ERR_NONE;

    for (bIdx = 0u; bIdx < (uint8_t)APP_EXEC_MEASURE_INITIAL_WORDS; bIdx++)
    {
        s_meas.awInitial[bIdx] = 0u;
    }

    return RET_OK;
}

Ret_t APP_EXEC_Measure_StartInitial(void)
{
    uint8_t bIdx;

    if (s_meas.tJob != MEAS_JOB_NONE)
    {
        return RET_BUSY;
    }

    /* Only four channels are swept; the trailing words stay zero so that the
       16-byte stored record and the PC-side parser are unchanged.
       Charge time 0: the sweep takes steady readings. */
    for (bIdx = 0u; bIdx < (uint8_t)APP_EXEC_MEASURE_INITIAL_WORDS; bIdx++)
    {
        s_meas.awInitial[bIdx] = 0u;
    }
    for (bIdx = 0u; bIdx < MEAS_CH_USED; bIdx++)
    {
        (void)Optic_SetTime((Optic_Ch)bIdx, 0u);
    }

    s_meas.tErr    = APP_ERR_NONE;
    s_meas.bTries  = 0u;
    s_meas.tJob    = MEAS_JOB_INITIAL;
    s_meas.tResult = APP_EXEC_MEASURE_RUNNING;
    s_meas.bCh     = 0u;

    return s_StartOptic((Optic_Ch)0u);
}

Ret_t APP_EXEC_Measure_StartStickCheck(void)
{
    if (s_meas.tJob != MEAS_JOB_NONE)
    {
        return RET_BUSY;
    }

    s_meas.tJob    = MEAS_JOB_STICK;
    s_meas.tResult = APP_EXEC_MEASURE_RUNNING;
    /* For s_FailCh(): after the tuning bCh is 4, and 0x10 + 4 is no error
       code (hs.kim review B1, 2026-11-13). */
    s_meas.bCh     = (uint8_t)OPTIC_CH_C;

    return s_StartOptic(OPTIC_CH_C);
}

Ret_t APP_EXEC_Measure_StartSingle(Optic_Ch tCh)
{
    if ((uint8_t)tCh >= (uint8_t)OPTIC_CH_COUNT)
    {
        return RET_ERR_PARAM;
    }
    if (s_meas.tJob != MEAS_JOB_NONE)
    {
        return RET_BUSY;
    }

    s_meas.tJob    = MEAS_JOB_SINGLE;
    s_meas.tResult = APP_EXEC_MEASURE_RUNNING;
    s_meas.bCh     = (uint8_t)tCh;

    return s_StartOptic(tCh);
}

Ret_t APP_EXEC_Measure_Process(void)
{
    Ret_t    tOpticErr;
    uint16_t wRead;
    uint32_t dwTime;
    uint8_t  bUp;

    if (s_meas.tJob == MEAS_JOB_NONE)
    {
        return RET_OK;
    }
    if (Optic_IsBusy() != 0u)
    {
        return RET_OK;
    }

    /* A failed conversion ends the job, reported as OPTIC_FAIL on the
       channel it hit (bCh is that channel in every job). The sequence
       turns FAILED into the error in any step (hs.kim review B1/B2,
       2026-11-13). */
    if (Optic_TakeError(&tOpticErr) != 0u)
    {
        s_meas.tErr    = s_FailCh();
        s_meas.tJob    = MEAS_JOB_NONE;
        s_meas.tResult = APP_EXEC_MEASURE_FAILED;
        return RET_OK;
    }

    switch (s_meas.tJob)
    {
        case MEAS_JOB_STICK:
            s_JudgeStick();
            s_meas.tJob    = MEAS_JOB_NONE;
            s_meas.tResult = APP_EXEC_MEASURE_DONE;
            break;

        case MEAS_JOB_SINGLE:
            s_meas.tJob    = MEAS_JOB_NONE;
            s_meas.tResult = APP_EXEC_MEASURE_DONE;
            break;

        case MEAS_JOB_INITIAL:
            /* Each channel once (hs.kim review D2). The presence checks the
               old FW interleaved are gone: a stick pulled out during these
               ~0.1 s reads dark and fails the acceptance window instead. */
            s_meas.awInitial[s_meas.bCh] = Optic_GetRaw((Optic_Ch)s_meas.bCh);
            s_meas.bCh++;

            if (s_meas.bCh < MEAS_CH_USED)
            {
                (void)s_StartOptic((Optic_Ch)s_meas.bCh);
                break;
            }

            /* Acceptance window on the steady readings: outside it the
               sensor or LED is faulty, and the unit shows the book at once
               (hs.kim review D4). */
            for (s_meas.bCh = 0u; s_meas.bCh < MEAS_CH_USED; s_meas.bCh++)
            {
                wRead = s_meas.awInitial[s_meas.bCh];
                if ((wRead < (uint16_t)APP_OPTIC_INITIAL_MIN)
                    || (wRead > (uint16_t)APP_OPTIC_INITIAL_MAX))
                {
                    s_EndTune(s_FailCh());
                    return RET_OK;
                }
            }

            s_meas.tJob = MEAS_JOB_TUNE;
            s_meas.bCh  = 0u;
            /* fall through: first guess for channel 0 */

        case MEAS_JOB_TUNE:
            if (s_meas.bTries == 0u)
            {
                s_TuneFirst();
                break;
            }
            if (s_meas.bTries >= 3u)
            {
                /* Dry reading in (five, at the tuned time): it is what
                   GetTunedSet hands on as the empty value. Kept here, as
                   the removal check may read C again before the sequence
                   asks (tuning runs behind the insert animation). */
                s_meas.awEmpty[s_meas.bCh] = Optic_GetRaw((Optic_Ch)s_meas.bCh);
                s_meas.bCh++;
                s_meas.bTries = 0u;
                if (s_meas.bCh >= MEAS_CH_USED)
                {
                    s_EndTune(APP_ERR_NONE);
                    break;
                }
                s_TuneFirst();
                break;
            }
            wRead = Optic_GetRaw((Optic_Ch)s_meas.bCh);
            if (s_meas.bTries == 1u)
            {
                /* Second point, 2 x span later on the curve. */
                s_meas.wY1 = wRead;
                s_TuneTry((uint16_t)(s_meas.wTime + (2u * APP_OPTIC_TUNE_SPAN)));
                break;
            }

            /* Both points in: the line through them, solved for 1200.
               One 32-bit product and quotient for the distance from the
               first point, then up or down by it.
               No book from the tuning (hs.kim 2026-11-12 02:17: only the
               steady readings outside 1500..3900 show it before the
               animation; to be confirmed in his review 11-13). A second
               reading that is not higher leaves no line: the estimate e
               (between the two points) is used. A time past the cap is
               held at the cap. */
            if (wRead <= s_meas.wY1)
            {
                dwTime = (uint32_t)s_meas.wTime + APP_OPTIC_TUNE_SPAN;
            }
            else
            {
                bUp = (s_meas.wY1 <= (uint16_t)APP_OPTIC_TARGET_LIGHT) ? 1u : 0u;
                wRead = (uint16_t)(wRead - s_meas.wY1);         /* rise over 2 x span */
                dwTime = ((uint32_t)((bUp != 0u)
                                     ? ((uint16_t)APP_OPTIC_TARGET_LIGHT - s_meas.wY1)
                                     : (s_meas.wY1 - (uint16_t)APP_OPTIC_TARGET_LIGHT))
                          * (2u * APP_OPTIC_TUNE_SPAN)) / wRead;
                if (bUp != 0u)
                {
                    dwTime += s_meas.wTime;
                }
                else
                {
                    dwTime = (dwTime < s_meas.wTime) ? (s_meas.wTime - dwTime) : 1uL;
                }
            }
            if (dwTime > (uint32_t)APP_OPTIC_TIME_MAX_LOOPS)
            {
                dwTime = (uint32_t)APP_OPTIC_TIME_MAX_LOOPS;
            }
            (void)Optic_SetTime((Optic_Ch)s_meas.bCh, (uint16_t)dwTime);

            /* The dry (empty) reading at the time just set, five readings as
               every measurement. Without it the empty value left in Optic
               was the second tuning point, 100..300 below 1200
               (스틱책 7번, 2026-11-12 19:30 dump). */
            s_meas.bTries = 3u;
            (void)s_StartOptic((Optic_Ch)s_meas.bCh);
            break;

        default:
            (void)APP_EXEC_Measure_Abort();
            break;
    }

    return RET_OK;
}

Ret_t APP_EXEC_Measure_Abort(void)
{
    s_meas.tJob    = MEAS_JOB_NONE;
    s_meas.tResult = APP_EXEC_MEASURE_IDLE;

    return Optic_Abort();
}

APP_DATA_ErrorCode APP_EXEC_Measure_TuneError(void)
{
    return s_meas.tErr;
}

void APP_EXEC_Measure_GetTunedSet(uint16_t *p_wOut)
{
    uint8_t bCh;

    for (bCh = 0u; bCh < (uint8_t)APP_EXEC_MEASURE_INITIAL_WORDS; bCh++)
    {
        p_wOut[bCh] = (bCh < MEAS_CH_USED) ? s_meas.awEmpty[bCh] : 0u;
    }
}

APP_EXEC_Measure_Result APP_EXEC_Measure_GetResult(void)
{
    return s_meas.tResult;
}

uint8_t APP_EXEC_Measure_IsSweeping(void)
{
    return (s_meas.tJob == MEAS_JOB_INITIAL) ? 1u : 0u;
}

uint8_t APP_EXEC_Measure_IsBusy(void)
{
    return (s_meas.tJob == MEAS_JOB_NONE) ? 0u : 1u;
}

uint8_t APP_EXEC_Measure_IsStickPresent(void)
{
    return s_meas.bPresent;
}

const uint16_t *APP_EXEC_Measure_GetInitial(void)
{
    return s_meas.awInitial;
}

uint16_t APP_EXEC_Measure_Intensity(uint16_t wDividend, uint16_t wDivisor)
{
    uint16_t wQuotient = 0u;
    uint16_t wTmp;
    uint8_t bDigit;

    /* Four decimal digits, exactly as CalculateIntensity in DM_Utill.c.
       Do not widen the remainder: its 16-bit wrap is shipped behaviour. */
    if ((wDividend != 0u) && (wDivisor != 0u))
    {
        for (bDigit = 0u; bDigit < 4u; bDigit++)
        {
            wTmp = wDividend / wDivisor;
            wQuotient = (uint16_t)(wQuotient * 10u + wTmp);
            wDividend = (uint16_t)(wDividend - wTmp * wDivisor);
            wDividend = (uint16_t)(wDividend * 10u);
        }
    }
    return wQuotient;
}

/*****************************END OF FILE****/
