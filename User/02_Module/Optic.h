/**
  ******************************************************************************
  * @file    : Optic.h
  * @brief   : Module layer - photo channel measurement
  *
  * Four optical channels share three sensors and two LEDs:
  *   CH0 (T)  LED2, ADC front    CH1 (BT) LED2, ADC middle
  *   CH2 (BC) LED1, ADC middle   CH3 (C)  LED1, ADC rear
  *
  * The sensor load is 100 kohm with 22 nF: a 2.2 ms time constant. (The
  * 22 nF value was confirmed from the BOM on 2026-10-30 - the schematic's
  * 1 nF is an error.)
  *
  * Two kinds of measurement, chosen by the channel's charge time:
  *
  * Charge time 0 - steady reading, used once per insertion to check the
  *   sensor (acceptance window) and as the first guess for tuning:
  *     LED on -> settle 20 ms -> 1 conversion -> LED off -> recover 20 ms
  *   At this level a bright channel sits at the phototransistor's saturation
  *   ceiling, which is why it is not used for judging (2026-11-11).
  *
  * Charge time n - timed reading on the charging curve (hs.kim
  *   Optic_Solution v1.2, 5-6), five times:
  *     [mask IRQ] LED on -> n delay loops -> ADC start [unmask] -> fetch
  *     -> LED off -> recover 20 ms (5 tau = 11 ms drains the node)
  *   The result is the mean of the middle three: the sum less the highest
  *   and the lowest (old FW: five scans, middle three).
  *   n is found by tuning so that a dry stick reads about 1200, well below
  *   the ceiling; every later reading of that channel uses the same n, so a
  *   band's ratio to the dry value stays proportional to the light.
  *
  * Responsibility boundary: this module reports what the sensor read. It does
  * not decide whether a stick is present, whether a reading is in range, or
  * what to do next - those are App decisions driven by APP_DATA_Config.h
  * thresholds.
  *
  * Differences from the common skeleton (spec v2.0.3 §15):
  *   A timed reading blocks for its charge time, up to about 3.5 ms on a
  *   dim channel, with interrupts masked. That exceeds the 2 ms slot budget;
  *   it cannot be split, the charge time is the measurement.
  *
  * Context: main loop only. Call Optic_Process() from the 10 ms slot.
  ******************************************************************************
  */
#ifndef __OPTIC_H__
#define __OPTIC_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"
#include "HW_Tick_Drive.h"   /* HW_TICK_MS(), HW_TICK_WAIT_MS() */

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    OPTIC_CH_T = 0,    /* old OPTIC_CH_0, front  */
    OPTIC_CH_BT,       /* old OPTIC_CH_1, middle */
    OPTIC_CH_BC,       /* old OPTIC_CH_2, middle */
    OPTIC_CH_C,        /* old OPTIC_CH_3, rear   */
    OPTIC_CH_COUNT
} Optic_Ch;

typedef enum
{
    OPTIC_IDLE = 0,
    OPTIC_SETTLE,
    OPTIC_SAMPLE,
    OPTIC_RECOVER,
    OPTIC_STATE_COUNT
} Optic_State;

/* Exported constants --------------------------------------------------------*/
#define OPTIC_SETTLE_TK          HW_TICK_WAIT_MS(20u)
#define OPTIC_RECOVER_TK         HW_TICK_WAIT_MS(20u)  /* >= 5 tau = 11 ms */
#define OPTIC_READS              5u    /* timed readings, middle three kept */
#define OPTIC_GAIN_SCALE         100u  /* percent scale of the App ratios */

/* Module-specific return codes ----------------------------------------------*/
#define OPTIC_ERR_ADC            (RET_MODULE_BASE + 0)   /* conversion failed */

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Puts the module in a safe state: both LEDs off, ADC powered down.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t Optic_Init(void);

/**
  * @brief  Starts one measurement on one channel.
  * @param  tCh : channel to measure
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         Returns immediately; poll Optic_IsBusy() and then Optic_GetResult().
  * @retval RET_OK, RET_ERR_PARAM, RET_BUSY
  */
Ret_t Optic_Start(Optic_Ch tCh);

/**
  * @brief  Starts a single timed reading instead of five (charge-time
  *         tuning, where +/-100 is good enough and time matters).
  * @param  tCh : channel to measure, its charge time already set (not 0)
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_PARAM, RET_BUSY
  */
Ret_t Optic_StartOnce(Optic_Ch tCh);

/**
  * @brief  Advances the measurement. Call from the 10 ms slot.
  * @note   Layer: Module. Blocking: a timed reading takes its charge time,
  *         up to about 3.5 ms, see the file header. ISR: no.
  * @retval RET_OK
  */
Ret_t Optic_Process(void);

/**
  * @brief  Aborts any measurement and turns the LEDs off at once.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t Optic_Abort(void);

/**
  * @brief  Sets the charge time of one channel.
  * @param  tCh    : channel
  * @param  wLoops : 0 for a steady reading, else HW_Cpu_DelayLoop() passes
  *                  (about 1.5 us each) between LED on and ADC start
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_PARAM
  */
Ret_t Optic_SetTime(Optic_Ch tCh, uint16_t wLoops);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Whether a measurement is running.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval 1 busy, 0 idle
  */
uint8_t Optic_IsBusy(void);

/**
  * @brief  Current state machine step.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval OPTIC_IDLE .. OPTIC_RECOVER
  */
Optic_State Optic_GetState(void);

/**
  * @brief  Result of the last finished measurement, in ADC counts.
  *         After tuning a dry stick reads about 1200, the same axis the
  *         gain-applied value used to be on, so the App thresholds stand.
  * @param  tCh : channel
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval counts, or 0 if that channel has not been measured
  */
uint16_t Optic_GetRaw(Optic_Ch tCh);

/**
  * @brief  Charge time of one channel.
  * @param  tCh : channel
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval delay loops, 0 if steady or the ID is out of range
  */
uint16_t Optic_GetTime(Optic_Ch tCh);

/**
  * @brief  Takes the error of the last measurement; reading clears it.
  * @param  p_tErr : out, OPTIC_ERR_ADC when a conversion failed
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval 1 if an error was pending, 0 otherwise
  */
uint8_t Optic_TakeError(Ret_t *p_tErr);

#endif /* __OPTIC_H__ */
/*****************************END OF FILE****/
