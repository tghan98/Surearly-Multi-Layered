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
static Ret_t s_Read(uint16_t *p_wOut);

/* Private functions ---------------------------------------------------------*/
static void s_LedsOff(void)
{
    (void)HW_GPIO_Write(HW_GPIO_LED1, OPTIC_LED_OFF);
    (void)HW_GPIO_Write(HW_GPIO_LED2, OPTIC_LED_OFF);
}

/* One reading, the same for every measurement (hs.kim 2026-11-17: "측정하는
   방식은 다 똑같아야 돼"). Six conversions back to back while the LED is on:
   the first follows an idle gap and is discarded (RM0031 14.3.4), the
   other five give the mean of the middle three - the sum less the highest
   and the lowest, the same number a sort would give.
   Timed (charge time n): the node starts empty, the LED goes on here and
   the delay runs with interrupts masked, so nothing stretches the charge
   time. The five kept conversions climb the curve by about 8 counts each
   (14 us apart); they fall at the same moments in every reading, so the
   ratios stand and the tuning absorbs the offset.
   Steady (n = 0): the LED has been on for OPTIC_SETTLE_TK, the node is flat.
   Interrupts stay masked to the end of the burst (about 85 us): the tick
   is frozen, so the driver bounds its EOC wait by a pass count. */
static Ret_t s_Read(uint16_t *p_wOut)
{
    HW_ADC_Id tAdc = s_adcMap[s_optic.tCh];
    uint16_t  awConv[OPTIC_CONVS];
    uint16_t  wSum = 0u;
    uint16_t  wMin = 0xFFFFu;
    uint16_t  wMax = 0u;
    uint8_t   bIdx;
    Ret_t     tRet;

    if (HW_ADC_Select(tAdc) != RET_OK)
    {
        return OPTIC_ERR_ADC;
    }

    HW_Cpu_IrqDisable();
    if (s_optic.awTime[s_optic.tCh] != 0u)
    {
        (void)HW_GPIO_Write(s_ledMap[s_optic.tCh], OPTIC_LED_ON);
        HW_Cpu_DelayLoop(s_optic.awTime[s_optic.tCh]);
    }
    tRet = HW_ADC_Burst(tAdc, awConv, (uint8_t)OPTIC_CONVS);
    HW_Cpu_IrqEnable();

    if (tRet != RET_OK)
    {
        return OPTIC_ERR_ADC;
    }

    for (bIdx = 1u; bIdx < (uint8_t)OPTIC_CONVS; bIdx++)
    {
        wSum += awConv[bIdx];               /* 5 x 4095 fits in 16 bits */
        if (awConv[bIdx] < wMin)
        {
            wMin = awConv[bIdx];
        }
        if (awConv[bIdx] > wMax)
        {
            wMax = awConv[bIdx];
        }
    }
    *p_wOut = (uint16_t)((wSum - wMin - wMax) / (OPTIC_CONVS - 3u));

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
            tRet = s_Read(&wRead);

            /* LED off before anything else: a stuck ADC must not hold 20 mA. */
            (void)HW_GPIO_Write(s_ledMap[s_optic.tCh], OPTIC_LED_OFF);
            (void)HW_ADC_Disable();

            if (tRet != RET_OK)
            {
                wRead               = 0u;
                s_optic.tError      = tRet;
                s_optic.bErrPending = 1u;
            }
            s_optic.awRaw[s_optic.tCh] = wRead;

            s_optic.wStart = HW_Tick_Get();
            s_optic.tState = OPTIC_RECOVER;
            break;

        case OPTIC_RECOVER:
            /* The node drains through 100 kohm: 5 tau = 11 ms. The next
               reading must start from empty, and the supply recovers from
               the 20 mA LED pulse in the same wait. */
            if (HW_Tick_Elapsed(s_optic.wStart) >= OPTIC_RECOVER_TK)
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
