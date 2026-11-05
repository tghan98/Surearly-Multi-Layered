/**
  ******************************************************************************
  * @file    : APP_CTRL_Cmd.c
  * @brief   : Application layer (control) - the command gate
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "APP_CTRL_Cmd.h"
#include "APP_CTRL_Sequence.h"
#include "APP_DATA_Config.h"
#include "APP_DATA_Record.h"
#include "Eeprom.h"
#include "PcComm.h"
#include "HW_PWR_Drive.h"
#include "HW_UART_Drive.h"

/* Private defines -----------------------------------------------------------*/
/* Fixed-width header. The PC-side parser reads these widths, so a short
   string is padded, never terminated (old DM_App_Main_Sq_Send_FixedStr). */
#define CMD_VER_LABEL_LEN        4u
#define CMD_VER_BUILD_LEN        8u
#define CMD_LOT_PREFIX_LEN       4u
#define CMD_LOT_SUFFIX_LEN       5u

/* Upper bound on the flush before a Q reset. Four blocks, one per pass,
   land in about 50 ms when the EEPROM answers. Ten times that, so a dead
   EEPROM cannot leave the unit sitting with no reaction at all. */
#define CMD_FLUSH_TIMEOUT_TK     HW_TICK_MS(500u)

/* Private variables ---------------------------------------------------------*/
/* C drops the terminating NUL when a string literal exactly fills the array,
   which is the fixed-width form the parser expects. */
static const char s_acVerLabel[CMD_VER_LABEL_LEN]   = APP_VER_LABEL;
static const char s_acVerBuild[CMD_VER_BUILD_LEN]   = APP_VER_BUILD;
static const char s_acLotPrefix[CMD_LOT_PREFIX_LEN] = APP_LOT_PREFIX;
static const char s_acLotSuffix[CMD_LOT_SUFFIX_LEN] = APP_LOT_SUFFIX;
static const char s_acAck[4] = { 'O', 'K', '\r', '\n' };

/* __near: kept out of the 256-byte page zero, see EWSTM8/MFx_Layered.icf. */
__near static uint16_t s_awBuf[APP_REC_SLOT_WORDS];

/* Private function prototypes -----------------------------------------------*/
static void s_Send(const uint8_t *p_bBuf, uint16_t wLen);
static void s_SendWord(uint16_t wData);
static void s_SendWords(const uint16_t *p_wSrc, uint8_t bCount);
static void s_Dump(void);

/* Private functions ---------------------------------------------------------*/
static void s_Send(const uint8_t *p_bBuf, uint16_t wLen)
{
    (void)HW_UART_Write(HW_UART_PC, p_bBuf, wLen);
}

static void s_SendWord(uint16_t wData)
{
    uint8_t ab[2];

    ab[0] = (uint8_t)(wData >> 8);
    ab[1] = (uint8_t)(wData & 0x00FFu);
    s_Send(ab, 2u);
}

static void s_SendWords(const uint16_t *p_wSrc, uint8_t bCount)
{
    uint8_t bIdx;

    for (bIdx = 0u; bIdx < bCount; bIdx++)
    {
        s_SendWord(p_wSrc[bIdx]);
    }
}

/* 253 bytes, about 265 ms on the wire at 9600 baud.
   This blocks, as the old FW did (DM_App_Main_Sq_Send_RawDump).
   Streaming it across passes cost about 200 bytes of flash, which this part
   does not have. A dump is a service action taken by the QC operator, not
   part of a running test, so the pause does not disturb a measurement.
   Recorded as a deviation in the design document. (decided 2026-11-03) */
static void s_Dump(void)
{
    const uint16_t *p_wSrc;
    uint16_t        wMin = 0u;
    uint16_t        wMax = 0u;
    uint8_t         bStart;
    uint8_t         bIdx;
    uint8_t         bSlot;

    /* 1. Version and lot, fixed width. */
    s_Send((const uint8_t *)s_acVerLabel,  (uint16_t)CMD_VER_LABEL_LEN);
    s_Send((const uint8_t *)s_acVerBuild,  (uint16_t)CMD_VER_BUILD_LEN);
    s_Send((const uint8_t *)s_acLotPrefix, (uint16_t)CMD_LOT_PREFIX_LEN);
    s_Send((const uint8_t *)s_acLotSuffix, (uint16_t)CMD_LOT_SUFFIX_LEN);

    /* 2. Counters and the last error. */
    (void)APP_DATA_Record_GetStatus(s_awBuf);
    s_SendWords(s_awBuf, (uint8_t)APP_REC_STATUS_BYTES);

    /* 3. Initial stick readings. */
    s_SendWords(APP_DATA_Record_GetStickPtr(),
                (uint8_t)APP_REC_STICK_PTR_WORDS);

    /* 4. Stored results, newest first. */
    bStart = APP_DATA_Record_GetDumpPos();
    for (bIdx = 0u; bIdx < (uint8_t)APP_REC_SLOT_COUNT; bIdx++)
    {
        bSlot = (uint8_t)((bStart + (uint8_t)APP_REC_SLOT_COUNT - bIdx)
                          % (uint8_t)APP_REC_SLOT_COUNT);
        (void)APP_DATA_Record_GetResult(bSlot, s_awBuf);
        s_SendWords(s_awBuf, (uint8_t)APP_REC_SLOT_WORDS);
    }

    /* 5. QC reference, then the wake-gate fail range. */
    (void)APP_DATA_Record_GetQcRef(&p_wSrc);
    s_SendWords(p_wSrc, (uint8_t)APP_REC_QC_REF_WORDS);

    (void)APP_DATA_Record_GetWakeFail(&wMin, &wMax);
    s_SendWord(wMin);
    s_SendWord(wMax);

    /* 5. The cut-off values this build was actually compiled with. The QC
          program used to show what the lot sheet says the unit should hold;
          these four bytes let it show what the unit does hold, so a build
          made with the wrong constants is caught before it reaches a batch.
          Appended at the end on purpose: every field before it keeps its
          offset, so the packet is the 249-byte one plus four and the parser
          only has to accept one more length. (2026-11-05) */
    s_SendWord((uint16_t)APP_T_LINE_THRESHOLD);
    s_SendWord((uint16_t)APP_C_LINE_THRESHOLD);
}

/* Functions -----------------------------------------------------------------*/
Ret_t APP_CTRL_Cmd_Init(void)
{
    return RET_OK;
}

Ret_t APP_CTRL_Cmd_Process(void)
{
    uint8_t  bCmd;
    uint8_t  bUpper;
    uint16_t wStart;

    if (PcComm_GetCmd(&bCmd) == 0u)
    {
        return RET_OK;
    }

    bUpper = ((bCmd >= 'a') && (bCmd <= 'z'))
             ? (uint8_t)(bCmd - ('a' - 'A')) : bCmd;

    switch (bUpper)
    {
        case 'M':
            s_Dump();
            break;

        case 'R':
            /* Same-layer call, listed in the design document. */
            (void)APP_CTRL_Sequence_ResetTestCount();
            break;

        case 'Q':
            (void)APP_CTRL_Sequence_RestartQc();

            /* RestartQc() only marks the counters dirty; the EEPROM write
               itself happens in APP_DATA_Record_Process(), one block per
               10 ms pass. A reset before those land throws the reset away -
               the unit comes back reading the OLD counts out of EEPROM.
               So drive the write here and wait for it, bounded.
               (found on the bench 2026-11-03, fixed the same day) */
            wStart = HW_Tick_Get();
            while ((APP_DATA_Record_IsBusy() != 0u)
                   && (HW_Tick_Elapsed(wStart) < CMD_FLUSH_TIMEOUT_TK))
            {
                (void)APP_DATA_Record_Process();
                (void)Eeprom_Process();
            }

            s_Send((const uint8_t *)s_acAck, 4u);
            /* HW_UART_Write only returns once the last byte has left the
               shift register, so the reply is on the wire before this. */
            HW_PWR_SystemReset();
            break;

        default:
            /* Unknown letters are ignored without a reply. */
            break;
    }

    return RET_OK;
}

uint8_t APP_CTRL_Cmd_IsBusy(void)
{
    /* The dump completes inside one pass, so nothing is ever outstanding. */
    return 0u;
}

/*****************************END OF FILE****/
