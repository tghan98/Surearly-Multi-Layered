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
    uint16_t                wTime;        /* charge time being tried      */
    APP_DATA_ErrorCode      tErr;         /* outcome of sweep and tuning  */
    uint16_t                awInitial[APP_EXEC_MEASURE_INITIAL_WORDS];
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
static void     s_EndTune(APP_DATA_ErrorCode tErr);
static APP_DATA_ErrorCode s_FailCh(void);

/* Private functions ---------------------------------------------------------*/
/* One tuning reading of the current channel at wTime loops. */
static void s_TuneTry(uint16_t wTime)
{
    /* 0 would mean a steady reading to Optic, and 65536 loops to the CPU. */
    if (wTime == 0u)
    {
        wTime = 1u;
    }
    s_meas.wTime = wTime;
    s_meas.bTries++;
    (void)Optic_SetTime((Optic_Ch)s_meas.bCh, wTime);
    (void)s_StartOptic((Optic_Ch)s_meas.bCh);
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

    if (s_meas.tJob == MEAS_JOB_NONE)
    {
        return RET_OK;
    }
    if (Optic_IsBusy() != 0u)
    {
        return RET_OK;
    }

    /* A failed conversion ends the whole procedure. In the sweep or the
       tuning it is reported as OPTIC_FAIL on the channel it hit. */
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
            if (s_meas.bTries != 0u)
            {
                wRead = Optic_GetRaw((Optic_Ch)s_meas.bCh);

                if ((wRead >= (uint16_t)(APP_OPTIC_TARGET_LIGHT
                                         - APP_OPTIC_TUNE_TOLERANCE))
                    && (wRead <= (uint16_t)(APP_OPTIC_TARGET_LIGHT
                                            + APP_OPTIC_TUNE_TOLERANCE)))
                {
                    /* This channel is set; the next starts from scratch. */
                    s_meas.bCh++;
                    s_meas.bTries = 0u;
                    if (s_meas.bCh >= MEAS_CH_USED)
                    {
                        s_EndTune(APP_ERR_NONE);
                        break;
                    }
                }
                else if (s_meas.bTries >= (uint8_t)APP_OPTIC_TUNE_TRIES)
                {
                    s_EndTune(s_FailCh());
                    break;
                }
                else
                {
                    /* Scale by the shortfall. The curve bends down, so this
                       never overshoots and closes in from below. */
                    if (wRead == 0u)
                    {
                        wRead = 1u;
                    }
                    dwTime = ((uint32_t)s_meas.wTime
                              * (uint32_t)APP_OPTIC_TARGET_LIGHT) / wRead;
                    s_TuneTry((dwTime > (uint32_t)APP_OPTIC_TIME_MAX_LOOPS)
                              ? (uint16_t)APP_OPTIC_TIME_MAX_LOOPS
                              : (uint16_t)dwTime);
                    break;
                }
            }

            /* First guess from the steady reading: tau x 1200 / S. */
            dwTime = ((uint32_t)APP_OPTIC_TAU_LOOPS
                      * (uint32_t)APP_OPTIC_TARGET_LIGHT)
                     / s_meas.awInitial[s_meas.bCh];
            s_TuneTry((uint16_t)dwTime);
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
        p_wOut[bCh] = (bCh < MEAS_CH_USED) ? Optic_GetRaw((Optic_Ch)bCh) : 0u;
    }
}

APP_EXEC_Measure_Result APP_EXEC_Measure_GetResult(void)
{
    return s_meas.tResult;
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
