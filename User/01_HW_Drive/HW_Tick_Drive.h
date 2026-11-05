/**
  ******************************************************************************
  * @file    : HW_Tick_Drive.h
  * @brief   : HW Driver layer - 10 ms system time base (TIM4)
  *
  * Hardware facts:
  *   TIM4 is an 8-bit up-counter clocked from fMASTER (HSI/8, nominally 2 MHz).
  *   Prescaler 128 -> 15.625 kHz, ARR 155 -> 156 counts -> 9.984 ms per IRQ.
  *
  * Why 10 ms and not 1 ms: the longest timeout in this product is 300 s.
  * At 1 ms that is 300000, which does not fit in 16 bits, so every elapsed
  * time had to be uint32_t. On an 8-bit core each 32-bit subtract, compare
  * and increment becomes a library call, and the whole set cost this part
  * about 400 bytes of flash it does not have. At 10 ms the same 300 s is
  * 30000, which fits in 16 bits and compiles to single instructions.
  * The coarser step costs nothing: the shortest interval measured anywhere
  * is the 20 ms optical settle. (decided 2026-11-03)
  *
  * HW_TICK_ARR is 155, the calculated value: 156 counts / 15625 Hz =
  * 9.984 ms, which is 0.16 % short of 10 ms and the closest an 8-bit
  * counter gets at this prescaler.
  *
  * The shipped FW uses 149 with the same prescaler and calls it an
  * empirical correction for a slow fMASTER (MFx_New_Ver DM_HW_Drv.c).
  * That was carried over here at first and then measured on 12 호기
  * (2026-11-03): the reaction wait, specified as 155 s, came out at
  * 149 s - matching 150 counts / 15625 Hz = 9.6 ms exactly. fMASTER is
  * accurate, the correction was not needed, and the shipped product has
  * been running every interval 4 % short (180 s reaction = 172.8 s).
  *
  * Changed to 155 so the specified times are the times that run.
  * NOTE: this makes every interval about 4 % LONGER than the shipped
  * product. Colour develops with time, so a result near a line threshold
  * can move. 12 호기 was last seen at T Intensity 1034 against a 1040
  * threshold - six counts.
  *
  * Responsibility boundary: this driver only counts. It does not know what any
  * period means; callers compare tick differences themselves.
  *
  * Profile note: CR1632 battery. TIM4 and its clock are stopped before Halt,
  * so the tick does not run and does not cost current while the unit sleeps.
  *
  * Context: HW_Tick_Isr() runs in interrupt context, everything else in the
  * main loop.
  ******************************************************************************
  */
#ifndef __HW_TICK_DRIVE_H__
#define __HW_TICK_DRIVE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported constants --------------------------------------------------------*/
#define HW_TICK_PERIOD_MS        10u    /* one tick = 10 ms */

/* Milliseconds -> ticks, at compile time. Rounds up, so a period never comes
   out shorter than asked. Lets every caller keep writing plain milliseconds
   while nothing wider than 16 bits reaches the generated code. */
#define HW_TICK_MS(ms)           ((uint16_t)(((ms) + (HW_TICK_PERIOD_MS - 1u)) \
                                             / HW_TICK_PERIOD_MS))

/* Same, plus one tick. Use this where the comparison is "elapsed >= N" and
   the wait must never come out SHORT. The start tick can be captured just
   before a tick edge, so "elapsed >= N" can return after as little as
   (N-1) ticks of real time. One extra tick makes the floor the asked value.
   Use HW_TICK_MS() for periods, where the average matters, not the floor. */
#define HW_TICK_WAIT_MS(ms)      ((uint16_t)(HW_TICK_MS(ms) + 1u))

/* Exported types ------------------------------------------------------------*/
/* Result of a periodic-slot due check. */
typedef enum
{
    HW_TICK_NOT_DUE = 0,   /* period has not elapsed yet                      */
    HW_TICK_DUE,           /* due; p_last advanced by one period              */
    HW_TICK_DUE_LATE,      /* two or more periods late; p_last reset to now   */
    HW_TICK_DUE_COUNT
} HW_Tick_Due;

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Configures TIM4 for a 10 ms update interrupt and starts it.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t HW_Tick_Init(void);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Current tick count (10 ms units) since HW_Tick_Init().
  * @note   Layer: HW Driver. Blocking: no. ISR: yes (read is ISR safe).
  *         The counter is 16-bit on an 8-bit core, so it is read twice and
  *         accepted only when both reads agree.
  *         Wraps every 65536 ticks = 655 s. Safe because elapsed times are
  *         differences and the longest single interval is 300 s.
  * @retval tick count
  */
uint16_t HW_Tick_Get(void);

/**
  * @brief  Ticks elapsed since wStart. Wrap safe.
  * @param  wStart : tick value captured earlier with HW_Tick_Get()
  * @note   Layer: HW Driver. Blocking: no. ISR: yes.
  * @retval elapsed ticks (10 ms units)
  */
uint16_t HW_Tick_Elapsed(uint16_t wStart);

/**
  * @brief  Tells whether a periodic slot is due, and advances its timestamp.
  * @param  p_wLast  : in/out, timestamp of the previous run
  * @param  wPeriod  : slot period in ticks (10 ms units), must be non-zero
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  *         On DUE the timestamp advances by exactly one period, so slots keep
  *         their phase. Two or more periods late means the loop could not
  *         keep up; the timestamp is reset to now instead of catching up.
  * @retval HW_TICK_NOT_DUE, HW_TICK_DUE, HW_TICK_DUE_LATE
  */
HW_Tick_Due HW_Tick_IsDue(uint16_t *p_wLast, uint16_t wPeriod);

/**
  * @brief  TIM4 update interrupt body. Called from stm8l15x_it.c only.
  * @note   Layer: HW Driver. Blocking: no. ISR: yes.
  * @retval None
  */
void HW_Tick_Isr(void);

#endif /* __HW_TICK_DRIVE_H__ */
/*****************************END OF FILE****/
