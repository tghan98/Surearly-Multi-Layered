/**
  ******************************************************************************
  * @file    : PcComm.c
  * @brief   : Module layer - command framing and reply streaming over the UART
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "PcComm.h"
#include "HW_UART_Drive.h"

/* Private defines -----------------------------------------------------------*/
#if ((PCCOMM_TX_BUF_SIZE & (PCCOMM_TX_BUF_SIZE - 1u)) != 0u)
#error "PCCOMM_TX_BUF_SIZE must be a power of two"
#endif
#define PCCOMM_TX_MASK           (PCCOMM_TX_BUF_SIZE - 1u)

#define PCCOMM_RX_CHUNK          8u    /* bytes taken from the driver per pass */
#define PCCOMM_CR                0x0Du
#define PCCOMM_LF                0x0Au

/* Private types -------------------------------------------------------------*/
typedef struct
{
    uint8_t     bPending;                      /* letter awaiting CR/LF   */
    uint8_t     bCmd;                          /* complete command        */
    uint8_t     bCmdReady;
    uint8_t     abTx[PCCOMM_TX_BUF_SIZE];
    uint8_t     bTxHead;
    uint8_t     bTxTail;
    PcComm_Stat tStat;
} PcComm_Ctx;

/* Private variables ---------------------------------------------------------*/
static PcComm_Ctx s_pc;

/* Private function prototypes -----------------------------------------------*/
static uint8_t s_IsCmdLetter(uint8_t bData);
static void    s_Parse(uint8_t bData);
static void    s_Count(uint16_t *p_wCounter);

/* Private functions ---------------------------------------------------------*/
static void s_Count(uint16_t *p_wCounter)
{
    if (*p_wCounter < 0xFFFFu)
    {
        (*p_wCounter)++;
    }
}

/* Frame shape only. The meaning of each letter belongs to the App. */
static uint8_t s_IsCmdLetter(uint8_t bData)
{
    uint8_t bUpper;

    bUpper = ((bData >= 'a') && (bData <= 'z'))
             ? (uint8_t)(bData - ('a' - 'A')) : bData;

    return ((bUpper == 'M') || (bUpper == 'R') || (bUpper == 'Q')) ? 1u : 0u;
}

static void s_Parse(uint8_t bData)
{
    if (s_IsCmdLetter(bData) != 0u)
    {
        s_pc.bPending = bData;
    }
    else if ((bData == PCCOMM_CR) || (bData == PCCOMM_LF))
    {
        if (s_pc.bPending != PCCOMM_CMD_NONE)
        {
            if (s_pc.bCmdReady != 0u)
            {
                /* The App has not taken the previous one yet. */
                s_Count(&s_pc.tStat.wCmdDropped);
            }
            s_pc.bCmd      = s_pc.bPending;
            s_pc.bCmdReady = 1u;
            s_Count(&s_pc.tStat.wCmdCount);
            s_pc.bPending  = PCCOMM_CMD_NONE;
        }
    }
    else
    {
        /* Anything else cancels the pending letter (old behaviour). */
        s_pc.bPending = PCCOMM_CMD_NONE;
    }
}

/* Functions -----------------------------------------------------------------*/
Ret_t PcComm_Init(void)
{
    s_pc.bPending  = PCCOMM_CMD_NONE;
    s_pc.bCmd      = PCCOMM_CMD_NONE;
    s_pc.bCmdReady = 0u;
    s_pc.bTxHead   = 0u;
    s_pc.bTxTail   = 0u;

    return HW_UART_Init();
}

Ret_t PcComm_Process(void)
{
    uint8_t  abRx[PCCOMM_RX_CHUNK];
    uint16_t wLen = 0u;
    uint16_t i;
    uint8_t  bSent;

    /* 1. Receive: bounded work per pass (spec v2.0.3 §7). */
    if (HW_UART_Read(HW_UART_PC, abRx, (uint16_t)PCCOMM_RX_CHUNK, &wLen)
        == RET_OK)
    {
        for (i = 0u; i < wLen; i++)
        {
            s_Parse(abRx[i]);
        }
    }

    /* 2. Transmit: only take a byte out when the driver accepts it. */
    for (bSent = 0u; bSent < (uint8_t)PCCOMM_TX_PER_PASS; bSent++)
    {
        if (s_pc.bTxTail == s_pc.bTxHead)
        {
            break;
        }
        if (HW_UART_TryWriteByte(HW_UART_PC, s_pc.abTx[s_pc.bTxTail])
            != RET_OK)
        {
            break;
        }
        s_pc.bTxTail = (uint8_t)((s_pc.bTxTail + 1u) & PCCOMM_TX_MASK);
    }

    return RET_OK;
}

uint8_t PcComm_GetCmd(uint8_t *p_bCmd)
{
    if ((p_bCmd == NULL) || (s_pc.bCmdReady == 0u))
    {
        return 0u;
    }

    *p_bCmd        = s_pc.bCmd;
    s_pc.bCmdReady = 0u;

    return 1u;
}

Ret_t PcComm_PutByte(uint8_t bData)
{
    uint8_t bNext;

    bNext = (uint8_t)((s_pc.bTxHead + 1u) & PCCOMM_TX_MASK);
    if (bNext == s_pc.bTxTail)
    {
        s_Count(&s_pc.tStat.wTxDropped);
        return RET_ERR_FULL;
    }

    s_pc.abTx[s_pc.bTxHead] = bData;
    s_pc.bTxHead = bNext;

    return RET_OK;
}

Ret_t PcComm_PutWord(uint16_t wData)
{
    /* Both bytes or neither: a half word would shift the whole dump. */
    if (PcComm_GetTxFree() < 2u)
    {
        s_Count(&s_pc.tStat.wTxDropped);
        return RET_ERR_FULL;
    }

    (void)PcComm_PutByte((uint8_t)(wData >> 8));
    (void)PcComm_PutByte((uint8_t)(wData & 0x00FFu));

    return RET_OK;
}

uint8_t PcComm_GetTxFree(void)
{
    uint8_t bUsed;

    bUsed = (uint8_t)((s_pc.bTxHead - s_pc.bTxTail) & PCCOMM_TX_MASK);

    return (uint8_t)((PCCOMM_TX_BUF_SIZE - 1u) - bUsed);
}

uint8_t PcComm_IsBusy(void)
{
    return (s_pc.bTxTail == s_pc.bTxHead) ? 0u : 1u;
}

Ret_t PcComm_GetStat(PcComm_Stat *p_out)
{
    if (p_out == NULL)
    {
        return RET_ERR_NULL;
    }

    *p_out = s_pc.tStat;

    return RET_OK;
}

/*****************************END OF FILE****/
