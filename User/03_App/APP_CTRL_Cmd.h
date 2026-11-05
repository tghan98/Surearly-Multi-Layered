/**
  ******************************************************************************
  * @file    : APP_CTRL_Cmd.h
  * @brief   : Application layer (control) - the command gate
  *
  * The only file that knows what a command letter means (spec v2.0.3 §8).
  * PcComm recognises the frame and queues the raw letter; this file decides
  * what happens.
  *
  *   M  dump every stored record to the QC program
  *   R  clear the completed-test count so the unit may be used again
  *   Q  clear all stored data and reset. Development only.
  *
  * Anything else is ignored without a reply, as the old FW did.
  *
  * The dump is 253 bytes, which is 265 ms on the wire at 9600 baud, and it
  * blocks for that time - the same as the old FW. Streaming it across passes
  * cost about 200 bytes of flash that this 8 KB part does not have. A dump is
  * a service action by the QC operator, not part of a running test, so the
  * pause does not disturb a measurement. Recorded as a deviation from the
  * common skeleton in the design document (spec v2.0.3 §7, §15).
  *
  * @par Upper App
  * - none (top layer)
  *
  * Context: main loop only. Call APP_CTRL_Cmd_Process() from the 10 ms slot.
  ******************************************************************************
  */
#ifndef __APP_CTRL_CMD_H__
#define __APP_CTRL_CMD_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Clears any dump in progress.
  * @note   Layer: App (control). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_CTRL_Cmd_Init(void);

/**
  * @brief  Takes a waiting command and pushes the next part of a reply.
  * @note   Layer: App (control). Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t APP_CTRL_Cmd_Process(void);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Whether a reply is still being streamed.
  * @note   Layer: App (control). Blocking: no. ISR: no.
  * @retval 1 busy, 0 idle
  */
uint8_t APP_CTRL_Cmd_IsBusy(void);

#endif /* __APP_CTRL_CMD_H__ */
/*****************************END OF FILE****/
