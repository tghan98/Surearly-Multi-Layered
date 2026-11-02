/**
  ******************************************************************************
  * @file    : HW_Tick_Drive.h
  * @brief   : HW Driver layer - 1 ms system time base (TIM4)
  *
  * Hardware facts:
  *   TIM4 is an 8-bit up-counter clocked from fMASTER (HSI/8, nominally 2 MHz).
  *   Prescaler 8 -> 250 kHz, ARR 249 -> 250 counts -> 1.000 ms per update IRQ.
  *
  * TEMPORARY: HW_TICK_ARR is the NOMINAL value, assuming fMASTER is exactly
  * 2 MHz. The old FW measured the real rate about 4 % lower and compensated
  * by hand (see MFx_New_Ver DM_HW_Drv.c, TIM4_TimeBaseInit(..., 149) and the
  * note above it). Until this project is calibrated on a board the same way,
  * every elapsed time runs about 4 % long. Do NOT run timing-dependent tests
  * before calibration. Remove this marker once the measured value is fixed.
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
#define HW_TICK_PERIOD_MS        1u

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
  * @brief  Configures TIM4 for a 1 ms update interrupt and starts it.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t HW_Tick_Init(void);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Current tick count in milliseconds since HW_Tick_Init().
  * @note   Layer: HW Driver. Blocking: no. ISR: yes (read is ISR safe).
  *         The counter is 32-bit on an 8-bit core, so it is read twice and
  *         accepted only when both reads agree.
  * @retval tick count
  */
uint32_t HW_Tick_Get(void);

/**
  * @brief  Milliseconds elapsed since dwStart. Wrap safe.
  * @param  dwStart : tick value captured earlier with HW_Tick_Get()
  * @note   Layer: HW Driver. Blocking: no. ISR: yes.
  * @retval elapsed milliseconds
  */
uint32_t HW_Tick_Elapsed(uint32_t dwStart);

/**
  * @brief  Tells whether a periodic slot is due, and advances its timestamp.
  * @param  p_dwLast   : in/out, timestamp of the previous run
  * @param  dwPeriodMs : slot period in milliseconds, must be non-zero
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  *         On DUE the timestamp advances by exactly one period, so slots keep
  *         their phase. Two or more periods late means the loop could not
  *         keep up; the timestamp is reset to now instead of catching up.
  * @retval HW_TICK_NOT_DUE, HW_TICK_DUE, HW_TICK_DUE_LATE
  */
HW_Tick_Due HW_Tick_IsDue(uint32_t *p_dwLast, uint32_t dwPeriodMs);

/**
  * @brief  TIM4 update interrupt body. Called from stm8l15x_it.c only.
  * @note   Layer: HW Driver. Blocking: no. ISR: yes.
  * @retval None
  */
void HW_Tick_Isr(void);

#endif /* __HW_TICK_DRIVE_H__ */
/*****************************END OF FILE****/
