/**
  ******************************************************************************
  * @file    : Optic.c
  * @brief   : Module layer - photo channel measurement
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Optic.h"
#include "HW_ADC_Drive.h"
#include "HW_Cpu_Drive.h"
#include "HW_GPIO_Drive.h"
#include "HW_Tick_Drive.h"

/* Private defines -----------------------------------------------------------*/
/* The LEDs are cathode driven: low turns one on. The driver only knows
   levels, so the meaning lives here (spec v2.0.3 §12). */
#define OPTIC_LED_ON             HW_GPIO_LEVEL_LOW
#define OPTIC_LED_OFF            HW_GPIO_LEVEL_HIGH

/* Private types -------------------------------------------------------------*/
typedef struct
{
    Optic_State tState;
    Optic_Ch    tCh;                        /* channel being measured     */
    uint16_t    wStart;                     /* tick at the step entry     */
    uint16_t    awRaw[OPTIC_CH_COUNT];      /* last result per channel    */
    uint16_t    awTime[OPTIC_CH_COUNT];     /* charge time, delay loops   */
    uint16_t    wSum;                       /* of the reads so far        */
    uint16_t    wMin;
    uint16_t    wMax;
    uint8_t     bReads;                     /* reads taken so far         */
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
static Ret_t s_ReadSteady(uint16_t *p_wOut);
static Ret_t s_ReadTimed(uint16_t *p_wOut);

/* Private functions ---------------------------------------------------------*/
static void s_LedsOff(void)
{
    (void)HW_GPIO_Write(HW_GPIO_LED1, OPTIC_LED_OFF);
    (void)HW_GPIO_Write(HW_GPIO_LED2, OPTIC_LED_OFF);
}

/* The LED has been on for OPTIC_SETTLE_TK: the node is at its steady level.
   One conversion. The old FW discarded the first after a channel change;
   with a 16-cycle window the leftover on the ADC's few-pF sampling
   capacitor moves the 22 nF node by well under a count, the same reason
   the timed reading has no discard (2026-11-11, flash). */
static Ret_t s_ReadSteady(uint16_t *p_wOut)
{
    return (HW_ADC_Read(s_adcMap[s_optic.tCh], p_wOut) == RET_OK)
           ? RET_OK : OPTIC_ERR_ADC;
}

/* One reading on the charging curve (hs.kim Optic_Solution v1.2, 5-6).
   The node starts empty: the LED has been off for OPTIC_RECOVER_TK.
   From LED on to ADC start nothing may cut in, or the charge time stretches
   by a different amount each time - so interrupts are masked first.
   They come back right after the start: the input is held by then, and
   HW_ADC_Fetch() needs the tick for its timeout.
   No discarded conversion, see s_ReadSteady(). */
static Ret_t s_ReadTimed(uint16_t *p_wOut)
{
    HW_ADC_Id tAdc = s_adcMap[s_optic.tCh];
    Ret_t     tRet;

    if (HW_ADC_Select(tAdc) != RET_OK)
    {
        return OPTIC_ERR_ADC;
    }

    HW_Cpu_IrqDisable();
    (void)HW_GPIO_Write(s_ledMap[s_optic.tCh], OPTIC_LED_ON);
    HW_Cpu_DelayLoop(s_optic.awTime[s_optic.tCh]);
    HW_ADC_Start();
    HW_Cpu_IrqEnable();

    tRet = HW_ADC_Fetch(tAdc, p_wOut);

    return (tRet == RET_OK) ? RET_OK : OPTIC_ERR_ADC;
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
        s_optic.awTime[bCh] = 0u;
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

    s_optic.tCh    = tCh;
    s_optic.bReads = 0u;
    s_optic.wSum   = 0u;
    s_optic.wMin   = 0xFFFFu;
    s_optic.wMax   = 0u;

    (void)HW_ADC_Enable();

    if (s_optic.awTime[tCh] == 0u)
    {
        /* Steady reading: LED on now, read after the settle. */
        (void)HW_GPIO_Write(s_ledMap[tCh], OPTIC_LED_ON);
        s_optic.wStart = HW_Tick_Get();
        s_optic.tState = OPTIC_SETTLE;
    }
    else
    {
        /* Timed reading: the node is already empty, an idle module has had
           its LEDs off for at least OPTIC_RECOVER_TK. */
        s_optic.tState = OPTIC_SAMPLE;
    }

    return RET_OK;
}

Ret_t Optic_Process(void)
{
    uint16_t wRead = 0u;
    Ret_t    tRet;

    switch (s_optic.tState)
    {
        case OPTIC_IDLE:
            break;

        case OPTIC_SETTLE:
            /* 100 kohm x 22 nF = 2.2 ms, so 20 ms reaches about 99.99 %. */
            if (HW_Tick_Elapsed(s_optic.wStart) >= OPTIC_SETTLE_TK)
            {
                s_optic.tState = OPTIC_SAMPLE;
            }
            break;

        case OPTIC_SAMPLE:
            if (s_optic.awTime[s_optic.tCh] == 0u)
            {
                tRet = s_ReadSteady(&wRead);
                s_optic.bReads = OPTIC_READS;   /* one reading is the result */
            }
            else
            {
                tRet = s_ReadTimed(&wRead);
                s_optic.bReads++;
            }

            /* LED off before anything else: a stuck ADC must not hold 20 mA. */
            (void)HW_GPIO_Write(s_ledMap[s_optic.tCh], OPTIC_LED_OFF);

            if (tRet != RET_OK)
            {
                s_optic.awRaw[s_optic.tCh] = 0u;
                s_optic.tError      = tRet;
                s_optic.bErrPending = 1u;
                s_optic.bReads      = OPTIC_READS;
                s_optic.wSum        = 0u;
            }
            else
            {
                s_optic.wSum += wRead;   /* 5 x 4095 fits in 16 bits */
                if (wRead < s_optic.wMin)
                {
                    s_optic.wMin = wRead;
                }
                if (wRead > s_optic.wMax)
                {
                    s_optic.wMax = wRead;
                }
            }

            if (s_optic.bReads >= OPTIC_READS)
            {
                (void)HW_ADC_Disable();
                if (s_optic.awTime[s_optic.tCh] != 0u)
                {
                    /* Old FW: five readings, mean of the middle three. Without
                       a sort: the sum less the highest and the lowest. */
                    s_optic.wSum = (uint16_t)((s_optic.wSum - s_optic.wMin
                                               - s_optic.wMax) / 3u);
                }
                if (tRet == RET_OK)
                {
                    s_optic.awRaw[s_optic.tCh] = s_optic.wSum;
                }
            }

            s_optic.wStart = HW_Tick_Get();
            s_optic.tState = OPTIC_RECOVER;
            break;

        case OPTIC_RECOVER:
            /* The node drains through 100 kohm: 5 tau = 11 ms. Every timed
               reading must start from empty, and the supply recovers from
               the 20 mA LED pulse in the same wait. */
            if (HW_Tick_Elapsed(s_optic.wStart) >= OPTIC_RECOVER_TK)
            {
                s_optic.tState = (s_optic.bReads >= OPTIC_READS)
                                 ? OPTIC_IDLE : OPTIC_SAMPLE;
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

Ret_t Optic_SetTime(Optic_Ch tCh, uint16_t wLoops)
{
    if ((uint8_t)tCh >= (uint8_t)OPTIC_CH_COUNT)
    {
        return RET_ERR_PARAM;
    }

    s_optic.awTime[tCh] = wLoops;

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

uint16_t Optic_GetTime(Optic_Ch tCh)
{
    if ((uint8_t)tCh >= (uint8_t)OPTIC_CH_COUNT)
    {
        return 0u;
    }

    return s_optic.awTime[tCh];
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
