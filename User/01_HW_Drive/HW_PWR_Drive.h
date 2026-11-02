/**
  ******************************************************************************
  * @file    : HW_PWR_Drive.h
  * @brief   : HW Driver layer - deep sleep, wake-up source, MCU reset
  *
  * Hardware facts (v1.1 board):
  *   Halt stops the core and every clock. The only thing that wakes the unit
  *   is a falling edge on the stick switch PB1 (EXTI pin 1), which closes to
  *   ground when a stick is inserted.
  *   The internal regulator is put in ultra-low-power mode before Halt.
  *
  * Measured (PPK2, 3.0 V, 2026-10-30):
  *   wfi() / Wait  about 382 .. 390 uA   (3 units)
  *   halt()        about 71 uA           (unit 12)
  *   A CR1632 at 140 mAh gives roughly 15 days on Wait and 82 days on Halt.
  *
  * Responsibility boundary: this driver puts the MCU to sleep and brings it
  * back. Deciding *when* to sleep is the App's business.
  *
  * Caller contract: HW_PWR_PrepareHalt() only handles what belongs to the
  * power block (regulator, wake-up pin, TIM4 clock). Every other peripheral
  * turns itself off through its own driver before this is called, so that no
  * single file has to know every peripheral on the board.
  *
  * Context: main loop only. HW_PWR_EnterHalt() returns after the wake-up
  * interrupt, with the system clock still at its reset default.
  ******************************************************************************
  */
#ifndef __HW_PWR_DRIVE_H__
#define __HW_PWR_DRIVE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Arms the stick switch as the wake-up source and lowers the regulator.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  *         Call immediately before HW_PWR_EnterHalt().
  * @retval RET_OK
  */
Ret_t HW_PWR_PrepareHalt(void);

/**
  * @brief  Stops the core until the stick switch fires.
  * @note   Layer: HW Driver. Blocking: yes, until the wake-up interrupt.
  *         ISR: no.
  * @retval RET_OK
  */
Ret_t HW_PWR_EnterHalt(void);

/**
  * @brief  Restores the regulator after waking.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  *         The caller re-initialises the clock and the peripherals it needs;
  *         this function does not reach into other drivers.
  * @retval RET_OK
  */
Ret_t HW_PWR_Resume(void);

/**
  * @brief  Resets the MCU through the window watchdog.
  * @note   Layer: HW Driver. Blocking: yes, never returns. ISR: no.
  *         Only System may call this (spec v2.0.3 §5); an App raises a
  *         request flag instead.
  * @retval does not return
  */
void HW_PWR_SystemReset(void);

/**
  * @brief  Stick switch wake-up interrupt body. Called from stm8l15x_it.c only.
  * @note   Layer: HW Driver. Blocking: no. ISR: yes.
  *         Clears the pending bit. Waking up is the whole effect.
  * @retval None
  */
void HW_PWR_WakeIsr(void);

#endif /* __HW_PWR_DRIVE_H__ */
/*****************************END OF FILE****/
