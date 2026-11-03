/**
  ******************************************************************************
  * @file    : Optic.c
  * @brief   : Module layer - photo channel measurement
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Optic.h"
#include "HW_ADC_Drive.h"
#include "HW_GPIO_Drive.h"
#include "HW_Tick_Drive.h"

/* Private defines -----------------------------------------------------------*/
/* The LEDs are cathode driven: low turns one on. The driver only knows
   levels, so the meaning lives here (spec v2.0.3 §12). */
#define OPTIC_LED_ON             HW_GPIO_LEVEL_LOW
#define OPTIC_LED_OFF            HW_GPIO_LEVEL_HIGH

#define OPTIC_BURST_COUNT        (OPTIC_SAMPLE_COUNT)
#define OPTIC_KEEP_COUNT         (OPTIC_SAMPLE_COUNT - (2u * OPTIC_TRIM_EACH_SIDE))

/* Private types -------------------------------------------------------------*/
typedef struct
{
    Optic_State tState;
    Optic_Ch    tCh;                        /* channel being measured     */
    uint32_t    dwStart;                    /* tick at the step entry     */
    uint16_t    awRaw[OPTIC_CH_COUNT];      /* last result per channel    */
    uint16_t    awGain[OPTIC_CH_COUNT];     /* integer percent            */
    Ret_t       tError;                     /* pending, taken by _TakeError */
    uint8_t     bErrPending;
} Optic_Ctx;

/* Private variables ---------------------------------------------------------*/
static Optic_Ctx s_optic;

/* Channel -> LED. CH0/CH1 share LED2, CH2/CH3 share LED1 (old gs_tOptic_LED). */
static const HW_GPIO_Id s_ledMap[OPTIC_CH_COUNT] =
{
    [OPTIC_CH_T]  = HW_GPIO_LED2,
    [OPTIC_CH_BT] = HW_GPIO_LED2,
    [OPTIC_CH_BC] = HW_GPIO_LED1,
    [OPTIC_CH_C]  = HW_GPIO_LED1
};

/* Channel -> sensor. CH1 and CH2 share the middle sensor (old gs_tOptic_ADC). */
static const HW_ADC_Id s_adcMap[OPTIC_CH_COUNT] =
{
    [OPTIC_CH_T]  = HW_ADC_FRNT,
    [OPTIC_CH_BT] = HW_ADC_MIDL,
    [OPTIC_CH_BC] = HW_ADC_MIDL,
    [OPTIC_CH_C]  = HW_ADC_REAR
};

/* Private function prototypes -----------------------------------------------*/
static void  s_LedsOff(void);
static Ret_t s_Burst(Optic_Ch tCh, uint16_t *p_wOut);

/* Private functions ---------------------------------------------------------*/
static void s_LedsOff(void)
{
    (void)HW_GPIO_Write(HW_GPIO_LED1, OPTIC_LED_OFF);
    (void)HW_GPIO_Write(HW_GPIO_LED2, OPTIC_LED_OFF);
}

/* One burst of conversions, kept in a single pass on purpose: splitting it
   would hold the 20 mA LED on for about 200 ms instead of 4.2 ms. */
static Ret_t s_Burst(Optic_Ch tCh, uint16_t *p_wOut)
{
    uint16_t awSample[OPTIC_BURST_COUNT];
    uint16_t wDiscard;
    uint16_t wTemp;
    uint32_t dwSum = 0uL;
    uint8_t  i;
    uint8_t  j;

    /* The first conversion after a channel change is not trusted. */
    if (HW_ADC_Read(s_adcMap[tCh], &wDiscard) != RET_OK)
    {
        return OPTIC_ERR_ADC;
    }

    for (i = 0u; i < (uint8_t)OPTIC_BURST_COUNT; i++)
    {
        if (HW_ADC_Read(s_adcMap[tCh], &awSample[i]) != RET_OK)
        {
            return OPTIC_ERR_ADC;
        }
    }

    /* Insertion sort: 20 items, and it beats bubble sort on a 2 MHz core. */
    for (i = 1u; i < (uint8_t)OPTIC_BURST_COUNT; i++)
    {
        wTemp = awSample[i];
        j = i;
        while ((j > 0u) && (awSample[j - 1u] > wTemp))
        {
            awSample[j] = awSample[j - 1u];
            j--;
        }
        awSample[j] = wTemp;
    }

    /* Mean of the middle 16: the two lowest and two highest are dropped. */
    for (i = (uint8_t)OPTIC_TRIM_EACH_SIDE;
         i < (uint8_t)(OPTIC_BURST_COUNT - OPTIC_TRIM_EACH_SIDE); i++)
    {
        dwSum += (uint32_t)awSample[i];
    }

    *p_wOut = (uint16_t)(dwSum / (uint32_t)OPTIC_KEEP_COUNT);

    return RET_OK;
}

/* Functions -----------------------------------------------------------------*/
Ret_t Optic_Init(void)
{
    uint8_t bCh;

    s_LedsOff();
    (void)HW_ADC_Disable();

    s_optic.tState      = OPTIC_IDLE;
    s_optic.tCh         = OPTIC_CH_T;
    s_optic.bErrPending = 0u;
    s_optic.tError      = RET_OK;

    for (bCh = 0u; bCh < (uint8_t)OPTIC_CH_COUNT; bCh++)
    {
        s_optic.awRaw[bCh]  = 0u;
        s_optic.awGain[bCh] = 0u;
    }

    return RET_OK;
}

Ret_t Optic_Start(Optic_Ch tCh)
{
    if ((uint8_t)tCh >= (uint8_t)OPTIC_CH_COUNT)
    {
        return RET_ERR_PARAM;
    }
    if (s_optic.tState != OPTIC_IDLE)
    {
        return RET_BUSY;
    }

    s_optic.tCh = tCh;

    (void)HW_ADC_Enable();
    (void)HW_GPIO_Write(s_ledMap[tCh], OPTIC_LED_ON);

    s_optic.dwStart = HW_Tick_Get();
    s_optic.tState  = OPTIC_SETTLE;

    return RET_OK;
}

Ret_t Optic_Process(void)
{
    uint16_t wResult = 0u;
    Ret_t    tRet;

    switch (s_optic.tState)
    {
        case OPTIC_IDLE:
            break;

        case OPTIC_SETTLE:
            /* 100 kohm x 22 nF = 2.2 ms, so 20 ms reaches about 99.99 %. */
            if (HW_Tick_Elapsed(s_optic.dwStart) >= (uint32_t)OPTIC_SETTLE_MS)
            {
                s_optic.tState = OPTIC_SAMPLE;
            }
            break;

        case OPTIC_SAMPLE:
            tRet = s_Burst(s_optic.tCh, &wResult);

            /* LED off before anything else: a stuck ADC must not hold 20 mA. */
            (void)HW_GPIO_Write(s_ledMap[s_optic.tCh], OPTIC_LED_OFF);
            (void)HW_ADC_Disable();

            if (tRet == RET_OK)
            {
                s_optic.awRaw[s_optic.tCh] = wResult;
            }
            else
            {
                s_optic.awRaw[s_optic.tCh] = 0u;
                s_optic.tError      = tRet;
                s_optic.bErrPending = 1u;
            }

            s_optic.dwStart = HW_Tick_Get();
            s_optic.tState  = OPTIC_RECOVER;
            break;

        case OPTIC_RECOVER:
            /* Let the supply recover before the next burst draws 20 mA. */
            if (HW_Tick_Elapsed(s_optic.dwStart) >= (uint32_t)OPTIC_RECOVER_MS)
            {
                s_optic.tState = OPTIC_IDLE;
            }
            break;

        default:
            (void)Optic_Abort();
            break;
    }

    return RET_OK;
}

Ret_t Optic_Abort(void)
{
    s_LedsOff();
    (void)HW_ADC_Disable();
    s_optic.tState = OPTIC_IDLE;

    return RET_OK;
}

Ret_t Optic_SetGain(Optic_Ch tCh, uint16_t wGain)
{
    if ((uint8_t)tCh >= (uint8_t)OPTIC_CH_COUNT)
    {
        return RET_ERR_PARAM;
    }

    s_optic.awGain[tCh] = wGain;

    return RET_OK;
}

uint8_t Optic_IsBusy(void)
{
    return (s_optic.tState == OPTIC_IDLE) ? 0u : 1u;
}

Optic_State Optic_GetState(void)
{
    return s_optic.tState;
}

uint16_t Optic_GetRaw(Optic_Ch tCh)
{
    if ((uint8_t)tCh >= (uint8_t)OPTIC_CH_COUNT)
    {
        return 0u;
    }

    return s_optic.awRaw[tCh];
}

uint16_t Optic_GetScaled(Optic_Ch tCh)
{
    uint32_t dwScaled;

    if ((uint8_t)tCh >= (uint8_t)OPTIC_CH_COUNT)
    {
        return 0u;
    }

    /* old Cal_Ratio(value, gain) = value * gain / 100 */
    dwScaled = ((uint32_t)s_optic.awRaw[tCh] * (uint32_t)s_optic.awGain[tCh])
               / (uint32_t)OPTIC_GAIN_SCALE;

    return (dwScaled > 0xFFFFuL) ? 0xFFFFu : (uint16_t)dwScaled;
}

uint16_t Optic_GetGain(Optic_Ch tCh)
{
    if ((uint8_t)tCh >= (uint8_t)OPTIC_CH_COUNT)
    {
        return 0u;
    }

    return s_optic.awGain[tCh];
}

uint8_t Optic_TakeError(Ret_t *p_tErr)
{
    if ((p_tErr == NULL) || (s_optic.bErrPending == 0u))
    {
        return 0u;
    }

    *p_tErr = s_optic.tError;
    s_optic.bErrPending = 0u;

    return 1u;
}

/*****************************END OF FILE****/
