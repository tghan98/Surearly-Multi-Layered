/**
  ******************************************************************************
  * @file    : Eeprom.c
  * @brief   : Module layer - non-blocking writes to the on-chip data EEPROM
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Eeprom.h"
#include "HW_EEPROM_Drive.h"

/* Private types -------------------------------------------------------------*/
typedef struct
{
    Eeprom_State    tState;
    const uint8_t  *p_bSrc;      /* caller's buffer, not copied */
    uint16_t        wOffset;     /* where the request started   */
    uint16_t        wLen;
    uint16_t        wDone;       /* bytes programmed so far     */
    uint16_t        wFailOffset;
    uint8_t         bFailPending;
} Eeprom_Ctx;

/* Private variables ---------------------------------------------------------*/
static Eeprom_Ctx s_eeprom;

/* Functions -----------------------------------------------------------------*/
Ret_t Eeprom_Init(void)
{
    s_eeprom.tState       = EEPROM_IDLE;
    s_eeprom.p_bSrc       = NULL;
    s_eeprom.wOffset      = 0u;
    s_eeprom.wLen         = 0u;
    s_eeprom.wDone        = 0u;
    s_eeprom.wFailOffset  = 0u;
    s_eeprom.bFailPending = 0u;

    return RET_OK;
}

Ret_t Eeprom_Read(uint16_t wOffset, uint8_t *p_bBuf, uint16_t wLen)
{
    return HW_EEPROM_Read(wOffset, p_bBuf, wLen);
}

Ret_t Eeprom_Write(uint16_t wOffset, const uint8_t *p_bBuf, uint16_t wLen)
{
    if (p_bBuf == NULL)
    {
        return RET_ERR_NULL;
    }
    if (((uint32_t)wOffset + (uint32_t)wLen) > (uint32_t)HW_EEPROM_SIZE_BYTES)
    {
        return RET_ERR_RANGE;
    }
    if (s_eeprom.tState != EEPROM_IDLE)
    {
        return RET_BUSY;
    }
    if (wLen == 0u)
    {
        return RET_OK;
    }

    s_eeprom.p_bSrc  = p_bBuf;
    s_eeprom.wOffset = wOffset;
    s_eeprom.wLen    = wLen;
    s_eeprom.wDone   = 0u;
    s_eeprom.tState  = EEPROM_WRITING;

    return RET_OK;
}

Ret_t Eeprom_Process(void)
{
    uint16_t wChunk;
    Ret_t    tRet;

    if (s_eeprom.tState != EEPROM_WRITING)
    {
        return RET_OK;
    }

    wChunk = s_eeprom.wLen - s_eeprom.wDone;
    if (wChunk > (uint16_t)EEPROM_BYTES_PER_PASS)
    {
        wChunk = (uint16_t)EEPROM_BYTES_PER_PASS;
    }

    tRet = HW_EEPROM_Write((uint16_t)(s_eeprom.wOffset + s_eeprom.wDone),
                           &s_eeprom.p_bSrc[s_eeprom.wDone], wChunk);

    if (tRet != RET_OK)
    {
        s_eeprom.wFailOffset  = (uint16_t)(s_eeprom.wOffset + s_eeprom.wDone);
        s_eeprom.bFailPending = 1u;
        (void)Eeprom_Abort();
        return RET_OK;
    }

    s_eeprom.wDone += wChunk;

    if (s_eeprom.wDone >= s_eeprom.wLen)
    {
        (void)Eeprom_Abort();
    }

    return RET_OK;
}

Ret_t Eeprom_Abort(void)
{
    s_eeprom.tState = EEPROM_IDLE;
    s_eeprom.p_bSrc = NULL;
    s_eeprom.wLen   = 0u;
    s_eeprom.wDone  = 0u;

    return RET_OK;
}

uint8_t Eeprom_IsBusy(void)
{
    return (s_eeprom.tState == EEPROM_IDLE) ? 0u : 1u;
}

Eeprom_State Eeprom_GetState(void)
{
    return s_eeprom.tState;
}

uint8_t Eeprom_TakeWriteFail(uint16_t *p_wOffset)
{
    if ((p_wOffset == NULL) || (s_eeprom.bFailPending == 0u))
    {
        return 0u;
    }

    *p_wOffset = s_eeprom.wFailOffset;
    s_eeprom.bFailPending = 0u;

    return 1u;
}

/*****************************END OF FILE****/
