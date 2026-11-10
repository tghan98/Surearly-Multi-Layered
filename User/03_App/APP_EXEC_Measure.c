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
    MEAS_JOB_INITIAL,      /* the four-channel sweep          */
    MEAS_JOB_COUNT
} Meas_Job;

typedef struct
{
    Meas_Job                tJob;
    APP_EXEC_Measure_Result tResult;
    uint8_t                 bCh;          /* channel under way            */
    uint8_t                 bPresent;     /* last presence outcome        */
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
static uint16_t s_Scale(uint16_t wRaw, uint16_t wGain);

/* Private functions ---------------------------------------------------------*/
/* old Cal_Ratio(value, gain) = value * gain / 100 */
static uint16_t s_Scale(uint16_t wRaw, uint16_t wGain)
{
    uint32_t dwVal;

    dwVal = ((uint32_t)wRaw * (uint32_t)wGain) / (uint32_t)OPTIC_GAIN_SCALE;

    return (dwVal > 0xFFFFuL) ? 0xFFFFu : (uint16_t)dwVal;
}

static Ret_t s_StartOptic(Optic_Ch tCh)
{
    return Optic_Start(tCh);
}

/* The C channel is the one that sees the stick (old OPTIC_CH_3). Judged on
   the gain-applied axis only: the raw-axis gate (2600) went with the switch
   check and the plain four-channel sweep (2026-11-10). */
static void s_JudgeStick(void)
{
    s_meas.bPresent = (Optic_GetScaled(OPTIC_CH_C)
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
       A channel whose conversion fails is never written and stays zero, so
       TuneGain() reports it as OPTIC_FAIL on that channel. */
    for (bIdx = 0u; bIdx < (uint8_t)APP_EXEC_MEASURE_INITIAL_WORDS; bIdx++)
    {
        s_meas.awInitial[bIdx] = 0u;
    }

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
    Ret_t tOpticErr;

    if (s_meas.tJob == MEAS_JOB_NONE)
    {
        return RET_OK;
    }
    if (Optic_IsBusy() != 0u)
    {
        return RET_OK;
    }

    /* A failed conversion ends the whole procedure. */
    if (Optic_TakeError(&tOpticErr) != 0u)
    {
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
            }
            else
            {
                s_meas.tJob    = MEAS_JOB_NONE;
                s_meas.tResult = APP_EXEC_MEASURE_DONE;
            }
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

APP_DATA_ErrorCode APP_EXEC_Measure_TuneGain(void)
{
    uint8_t  bCh;
    uint16_t wRaw;
    uint16_t wGain;
    uint16_t wTuned;

    for (bCh = 0u; bCh < MEAS_CH_USED; bCh++)
    {
        wRaw = s_meas.awInitial[bCh];

        if ((wRaw < (uint16_t)APP_OPTIC_INITIAL_MIN)
            || (wRaw > (uint16_t)APP_OPTIC_INITIAL_MAX))
        {
            return (APP_DATA_ErrorCode)((uint8_t)APP_ERR_OPTIC_FAIL_CH0 + bCh);
        }

        /* Round to nearest: truncating biases the applied value low by up to
           about 32 counts at this target. wRaw is 1500..3900 here, so the
           gain lands in 31..80. */
        wGain = (uint16_t)((((uint32_t)APP_OPTIC_TARGET_LIGHT
                             * (uint32_t)OPTIC_GAIN_SCALE)
                            + ((uint32_t)wRaw / 2uL)) / (uint32_t)wRaw);

        /* Check the exact value that will be stored, so a channel that cannot
           be brought near the target fails here instead of silently landing
           outside tolerance. */
        wTuned = s_Scale(wRaw, wGain);
        if ((wTuned < (uint16_t)(APP_OPTIC_TARGET_LIGHT - APP_OPTIC_TUNE_TOLERANCE))
            || (wTuned > (uint16_t)(APP_OPTIC_TARGET_LIGHT + APP_OPTIC_TUNE_TOLERANCE)))
        {
            return (APP_DATA_ErrorCode)((uint8_t)APP_ERR_OPTIC_FAIL_CH0 + bCh);
        }

        (void)Optic_SetGain((Optic_Ch)bCh, wGain);
    }

    return APP_ERR_NONE;
}

APP_DATA_ErrorCode APP_EXEC_Measure_GetScaledSet(uint16_t *p_wOut)
{
    uint8_t  bCh;
    uint16_t wGain;

    if (p_wOut == NULL)
    {
        return APP_ERR_NONE;
    }

    for (bCh = 0u; bCh < (uint8_t)APP_EXEC_MEASURE_INITIAL_WORDS; bCh++)
    {
        p_wOut[bCh] = 0u;
    }

    for (bCh = 0u; bCh < MEAS_CH_USED; bCh++)
    {
        wGain = Optic_GetGain((Optic_Ch)bCh);

        /* Zero gain means TuneGain() has not run. Fail loudly rather than
           store a plausible-looking number. */
        if (wGain == 0u)
        {
            return (APP_DATA_ErrorCode)((uint8_t)APP_ERR_OPTIC_FAIL_CH0 + bCh);
        }

        p_wOut[bCh] = s_Scale(s_meas.awInitial[bCh], wGain);
    }

    return APP_ERR_NONE;
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
