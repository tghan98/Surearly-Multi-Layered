/**
  ******************************************************************************
  * @file    : PcComm.h
  * @brief   : Module layer - command framing and reply streaming over the UART
  *
  * Frame on the wire (unchanged from the old FW, the QC program depends on it):
  *   one command letter, then CR or LF. Anything else cancels the pending
  *   letter. The letters in use are M, R and Q, upper or lower case.
  *
  * Responsibility boundary: this module recognises the frame shape and queues
  * the raw letter. What M means is an App decision (spec v2.0.3 §8: the
  * command gate is the only place that knows command keys).
  *
  * Replies are streamed: the App pushes bytes into a small queue and this
  * module hands them to the driver only as fast as the transmitter accepts
  * them. A full dump is about 250 bytes, which is 260 ms on the wire at
  * 9600 baud - far too long to wait for inside one pass.
  *
  * Differences from the old FW (ISR in User_Main.c:141):
  *   The old interrupt handler held the pending letter and called the dump
  *   routine directly from interrupt context. Parsing and acting both move
  *   out of the ISR (spec §10).
  *
  * Context: main loop only. Call PcComm_Process() from the 10 ms slot.
  ******************************************************************************
  */
#ifndef __PCCOMM_H__
#define __PCCOMM_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported constants --------------------------------------------------------*/
#define PCCOMM_TX_BUF_SIZE       32u   /* power of two, see the .c */
#define PCCOMM_TX_PER_PASS       4u    /* bytes handed over per Process() */
#define PCCOMM_CMD_NONE          0u

/* Exported types ------------------------------------------------------------*/
typedef struct
{
    uint16_t wCmdCount;     /* complete commands parsed  */
    uint16_t wCmdDropped;   /* parsed but nobody took it */
    uint16_t wTxDropped;    /* reply bytes lost, queue full */
} PcComm_Stat;

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Brings the UART up and clears both queues.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t PcComm_Init(void);

/**
  * @brief  Drains the receive buffer, parses frames, pushes queued reply bytes.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t PcComm_Process(void);

/**
  * @brief  Takes the oldest complete command; reading clears it.
  * @param  p_bCmd : out, the command letter as received
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         Only one command is held: a second one that arrives before the
  *         first is taken replaces it and is counted in wCmdDropped.
  * @retval 1 if a command was waiting, 0 otherwise
  */
uint8_t PcComm_GetCmd(uint8_t *p_bCmd);

/**
  * @brief  Queues one reply byte.
  * @param  bData : byte
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_FULL
  */
Ret_t PcComm_PutByte(uint8_t bData);

/**
  * @brief  Queues one 16-bit value, high byte first (old USART_SendWord).
  * @param  wData : value
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         Either both bytes are queued or neither is.
  * @retval RET_OK, RET_ERR_FULL
  */
Ret_t PcComm_PutWord(uint16_t wData);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Free space in the reply queue, in bytes.
  * @note   Layer: Module. Blocking: no. ISR: no.
  *         The App checks this before pushing the next part of a long reply.
  * @retval free bytes
  */
uint8_t PcComm_GetTxFree(void);

/**
  * @brief  Whether reply bytes are still waiting to go out.
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval 1 busy, 0 idle
  */
uint8_t PcComm_IsBusy(void);

/**
  * @brief  Copies the statistics out.
  * @param  p_out : destination, must not be NULL
  * @note   Layer: Module. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL
  */
Ret_t PcComm_GetStat(PcComm_Stat *p_out);

#endif /* __PCCOMM_H__ */
/*****************************END OF FILE****/
