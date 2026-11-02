/**
  ******************************************************************************
  * @file    : HW_CLK_Drive.h
  * @brief   : HW Driver layer - system clock source and speed
  *
  * Hardware facts:
  *   HSI (internal 16 MHz RC) divided by 8 -> fMASTER 2 MHz nominal.
  *   There is no crystal on this board, so the real rate varies with the part
  *   and with temperature. The old FW measured it about 4 % low; see the
  *   TEMPORARY note in HW_Tick_Drive.h.
  *
  * Responsibility boundary: this file sets the system clock once at boot.
  * Per-peripheral clock gating is NOT here - each driver enables and disables
  * its own peripheral clock when it needs it (spec v2.0.3 §13 item 3).
  * Collecting the gating here would make every driver depend on this one.
  *
  * Context: main loop only, called once from System_Main_Init().
  ******************************************************************************
  */
#ifndef __HW_CLK_DRIVE_H__
#define __HW_CLK_DRIVE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported constants --------------------------------------------------------*/
#define HW_CLK_FMASTER_HZ        2000000uL   /* nominal, see note above */

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Selects HSI/8 as the system clock.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  *         Must run before any other driver init: every later timing depends
  *         on fMASTER.
  * @retval RET_OK
  */
Ret_t HW_CLK_Init(void);

#endif /* __HW_CLK_DRIVE_H__ */
/*****************************END OF FILE****/
