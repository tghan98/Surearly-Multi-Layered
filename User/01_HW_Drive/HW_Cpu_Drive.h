/**
  ******************************************************************************
  * @file    : HW_Cpu_Drive.h
  * @brief   : HW Driver layer - fixed-cycle delay loop and interrupt mask
  *
  * Implemented in HW_Cpu_Drive.s. See the notes there: one loop pass is
  * 3 cycles (1.5 us at 2 MHz), and 0 means 65536 passes.
  *
  * Context: main loop only.
  ******************************************************************************
  */
#ifndef __HW_CPU_DRIVE_H__
#define __HW_CPU_DRIVE_H__

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Busy-waits wLoops passes of a 3-cycle loop.
  * @param  wLoops : 1 .. 65535. 0 is 65536 passes - do not pass 0.
  * @note   Layer: HW Driver. Blocking: yes, wLoops x about 1.5 us.
  *         Mask interrupts around it when the delay must not stretch.
  */
void HW_Cpu_DelayLoop(uint16_t wLoops);

/**
  * @brief  Masks all maskable interrupts (SIM).
  * @note   Layer: HW Driver. Keep the masked span short: the 10 ms tick and
  *         UART RX are held off until HW_Cpu_IrqEnable().
  */
void HW_Cpu_IrqDisable(void);

/**
  * @brief  Unmasks interrupts (RIM). A tick that came due meanwhile runs now.
  */
void HW_Cpu_IrqEnable(void);

#endif /* __HW_CPU_DRIVE_H__ */
/*****************************END OF FILE****/
