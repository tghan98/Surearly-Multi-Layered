/**
  ******************************************************************************
  * @file    : HW_UART_Drive.c
  * @brief   : HW Driver layer - serial link to the QC program
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "HW_UART_Drive.h"
#include "HW_Tick_Drive.h"
#include "stm8l15x.h"
#include "stm8l15x_clk.h"
#include "stm8l15x_syscfg.h"
#include "stm8l15x_usart.h"

/* Private defines -----------------------------------------------------------*/
#if ((HW_UART_RX_BUF_SIZE & (HW_UART_RX_BUF_SIZE - 1u)) != 0u)
#error "HW_UART_RX_BUF_SIZE must be a power of two"
#endif
#define HW_UART_RX_MASK          (HW_UART_RX_BUF_SIZE - 1u)

/* Private variables ---------------------------------------------------------*/
static volatile uint8_t  s_abRx[HW_UART_RX_BUF_SIZE];
static volatile uint8_t  s_bRxHead = 0u;   /* written by the ISR  */
static volatile uint8_t  s_bRxTail = 0u;   /* written by the main loop */
static HW_UART_Stat      s_stat;

/* Functions -----------------------------------------------------------------*/
Ret_t HW_UART_Init(void)
{
    CLK_PeripheralClockConfig(CLK_Peripheral_USART1, ENABLE);

    /* USART1 defaults to PC3/PC2; this board wires it to PA2/PA3. */
    SYSCFG_REMAPPinConfig(REMAP_Pin_USART1TxRxPortA, ENABLE);

    USART_DeInit(USART1);
    USART_Init(USART1, HW_UART_BAUD, USART_WordLength_8b, USART_StopBits_1,
               USART_Parity_No,
               (USART_Mode_TypeDef)(USART_Mode_Tx | USART_Mode_Rx));

    s_bRxHead = 0u;
    s_bRxTail = 0u;

    /* Buffer is ready before the interrupt can fire (spec v2.0.3 §10). */
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);

    return RET_OK;
}

Ret_t HW_UART_Write(HW_UART_Id tId, const uint8_t *p_bBuf, uint16_t wLen)
{
    uint16_t wIdx;
    uint32_t dwStart;

    if (p_bBuf == NULL)
    {
        return RET_ERR_NULL;
    }
    if ((uint8_t)tId >= (uint8_t)HW_UART_COUNT)
    {
        return RET_ERR_PARAM;
    }

    for (wIdx = 0u; wIdx < wLen; wIdx++)
    {
        dwStart = HW_Tick_Get();
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        {
            if (HW_Tick_Elapsed(dwStart) > (uint32_t)HW_UART_TX_TIMEOUT_MS)
            {
                if (s_stat.wTxTimeout < 0xFFFFu)
                {
                    s_stat.wTxTimeout++;
                }
                return RET_TIMEOUT;
            }
        }

        USART_SendData8(USART1, p_bBuf[wIdx]);
    }

    /* Let the last byte leave the shift register before returning, so a
       caller may power the port down straight away. */
    dwStart = HW_Tick_Get();
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
    {
        if (HW_Tick_Elapsed(dwStart) > (uint32_t)HW_UART_TX_TIMEOUT_MS)
        {
            if (s_stat.wTxTimeout < 0xFFFFu)
            {
                s_stat.wTxTimeout++;
            }
            return RET_TIMEOUT;
        }
    }

    return RET_OK;
}

Ret_t HW_UART_TryWriteByte(HW_UART_Id tId, uint8_t bData)
{
    if ((uint8_t)tId >= (uint8_t)HW_UART_COUNT)
    {
        return RET_ERR_PARAM;
    }
    if (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
        return RET_BUSY;
    }

    USART_SendData8(USART1, bData);

    return RET_OK;
}

Ret_t HW_UART_Read(HW_UART_Id tId, uint8_t *p_bBuf, uint16_t wMax,
                   uint16_t *p_wLen)
{
    uint16_t wCount = 0u;
    uint8_t  bHead;
    uint8_t  bTail;

    if ((p_bBuf == NULL) || (p_wLen == NULL))
    {
        return RET_ERR_NULL;
    }
    if ((uint8_t)tId >= (uint8_t)HW_UART_COUNT)
    {
        return RET_ERR_PARAM;
    }

    /* Snapshot both indices once. Reading two volatiles in one expression
       leaves the order to the compiler (Pa082), and this call is defined to
       take only what had arrived when it started: bytes that land during the
       copy are read on the next call. */
    bHead = s_bRxHead;
    bTail = s_bRxTail;

    while ((wCount < wMax) && (bTail != bHead))
    {
        p_bBuf[wCount] = s_abRx[bTail];
        bTail = (uint8_t)((bTail + 1u) & HW_UART_RX_MASK);
        wCount++;
    }

    s_bRxTail = bTail;
    *p_wLen   = wCount;

    return RET_OK;
}

Ret_t HW_UART_GetStat(HW_UART_Stat *p_out)
{
    if (p_out == NULL)
    {
        return RET_ERR_NULL;
    }

    *p_out = s_stat;

    return RET_OK;
}

void HW_UART_Isr(void)
{
    uint8_t bData;
    uint8_t bNext;

    if (USART_GetITStatus(USART1, USART_IT_RXNE) == RESET)
    {
        return;
    }

    /* Reading the data register clears RXNE. */
    bData = USART_ReceiveData8(USART1);

    bNext = (uint8_t)((s_bRxHead + 1u) & HW_UART_RX_MASK);
    if (bNext == s_bRxTail)
    {
        /* Full: drop the byte and count it, never block in an ISR. */
        if (s_stat.wRxDropped < 0xFFFFu)
        {
            s_stat.wRxDropped++;
        }
        return;
    }

    s_abRx[s_bRxHead] = bData;
    s_bRxHead = bNext;

    if (s_stat.wRxBytes < 0xFFFFu)
    {
        s_stat.wRxBytes++;
    }
}

/*****************************END OF FILE****/
