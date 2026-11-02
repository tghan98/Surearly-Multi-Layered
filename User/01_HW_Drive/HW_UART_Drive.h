/**
  ******************************************************************************
  * @file    : HW_UART_Drive.h
  * @brief   : HW Driver layer - serial link to the QC program
  *
  * Hardware facts (v1.1 board):
  *   USART1 remapped to PA2 (TX) / PA3 (RX). The reset default is PC3/PC2.
  *   9600 baud, 8N1. One byte takes about 1.04 ms on the wire.
  *
  * Responsibility boundary: bytes in, bytes out. The driver does not know what
  * any byte means. Command letters, frames and replies belong to the Module.
  *
  * Receive path (spec v2.0.3 §12 default): the RXNE interrupt only stores one
  * byte into a ring buffer here; upper layers poll with HW_UART_Read().
  *
  * Differences from the old FW (DM_HW_Drv_USART_* and the ISR in
  * User_Main.c):
  *   - The old interrupt handler compared the received byte against command
  *     letters and set flags. That is parsing inside an ISR (spec §10) and it
  *     moves to the Module.
  *   - The old send waited on the TC flag with no bound. Writes are now
  *     bounded by a timeout.
  *
  * Context: HW_UART_Isr() runs in interrupt context, everything else in the
  * main loop.
  ******************************************************************************
  */
#ifndef __HW_UART_DRIVE_H__
#define __HW_UART_DRIVE_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    HW_UART_PC = 0,    /* USART1, PA2/PA3 */
    HW_UART_COUNT
} HW_UART_Id;

/* Statistics. Each field has exactly one writer, noted beside it. */
typedef struct
{
    uint16_t wRxBytes;     /* ISR  */
    uint16_t wRxDropped;   /* ISR  - buffer was full, byte thrown away */
    uint16_t wTxTimeout;   /* main */
} HW_UART_Stat;

/* Exported constants --------------------------------------------------------*/
#define HW_UART_BAUD             9600uL
#define HW_UART_RX_BUF_SIZE      16u    /* power of two, see the .c */
#define HW_UART_TX_TIMEOUT_MS    5u     /* one byte is ~1.04 ms at 9600 */

/* API -----------------------------------------------------------------------*/
/**
  * @brief  Remaps and configures USART1, enables RX interrupt.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t HW_UART_Init(void);

/**
  * @brief  Sends a block of bytes, waiting for each to leave the shift register.
  * @param  tId    : port
  * @param  p_bBuf : source bytes, must not be NULL
  * @param  wLen   : how many to send
  * @note   Layer: HW Driver. Blocking: yes, bounded by
  *         HW_UART_TX_TIMEOUT_MS per byte. ISR: no.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_PARAM, RET_TIMEOUT
  */
Ret_t HW_UART_Write(HW_UART_Id tId, const uint8_t *p_bBuf, uint16_t wLen);

/**
  * @brief  Takes whatever the ring buffer holds, up to wMax bytes.
  * @param  tId     : port
  * @param  p_bBuf  : destination, must not be NULL
  * @param  wMax    : capacity of p_bBuf
  * @param  p_wLen  : out, how many bytes were copied. May be 0.
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_PARAM
  */
Ret_t HW_UART_Read(HW_UART_Id tId, uint8_t *p_bBuf, uint16_t wMax,
                   uint16_t *p_wLen);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Copies the statistics out.
  * @param  p_out : destination, must not be NULL
  * @note   Layer: HW Driver. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL
  */
Ret_t HW_UART_GetStat(HW_UART_Stat *p_out);

/**
  * @brief  USART1 RX interrupt body. Called from stm8l15x_it.c only.
  * @note   Layer: HW Driver. Blocking: no. ISR: yes.
  *         Stores one byte and returns. No parsing here.
  * @retval None
  */
void HW_UART_Isr(void);

#endif /* __HW_UART_DRIVE_H__ */
/*****************************END OF FILE****/
