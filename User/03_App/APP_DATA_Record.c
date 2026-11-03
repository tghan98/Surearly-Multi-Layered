/**
  ******************************************************************************
  * @file    : APP_DATA_Record.c
  * @brief   : Application layer (data) - EEPROM layout and stored records
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "APP_DATA_Record.h"
#include "Eeprom.h"

/* Private defines -----------------------------------------------------------*/
/* Offsets, carried over unchanged from the old FW (DM_Rom_Handl_App.c). */
#define REC_OFF_INSERT_COUNT     0u
#define REC_OFF_TEST_COUNT       1u
#define REC_OFF_ERROR_STEP       2u
#define REC_OFF_ERROR_CODE       3u
#define REC_OFF_STICK_PTR        4u
#define REC_OFF_RESULT          20u
#define REC_OFF_QC_REF         212u
#define REC_OFF_DUMP_POS       244u
#define REC_OFF_WAKE_FAIL_MIN  248u
#define REC_OFF_WAKE_FAIL_MAX  250u

#define REC_SLOT_BYTES          (APP_REC_SLOT_WORDS * 2u)        /* 32 */
#define REC_STICK_PTR_BYTES     (APP_REC_STICK_PTR_WORDS * 2u)   /* 16 */
#define REC_QC_REF_BYTES        (APP_REC_QC_REF_WORDS * 2u)      /*  8 */

/* A never-written EEPROM byte reads as this. */
#define REC_ERASED_BYTE       0xFFu

/* Dirty bits, one per mirrored block. */
#define REC_DIRTY_STATUS      0x01u
#define REC_DIRTY_STICK_PTR   0x02u
#define REC_DIRTY_RESULT      0x04u
#define REC_DIRTY_DUMP_POS    0x08u

/* Private types -------------------------------------------------------------*/
typedef struct
{
    uint8_t  abStatus[APP_REC_STATUS_BYTES];          /* insert, test, step, code */
    uint16_t awStickPtr[APP_REC_STICK_PTR_WORDS];
    uint16_t awResult[APP_REC_SLOT_WORDS];            /* staged, one test        */
    uint16_t awQcRef[APP_REC_QC_REF_WORDS];
    uint8_t  bDumpPos;
    uint8_t  bResultSlot;                             /* slot awResult goes to   */
    uint8_t  bDirty;
} Record_Ctx;

/* Private variables ---------------------------------------------------------*/
/* __near: this buffer lives in RAM above page zero. Page zero is only
   256 bytes and every variable defaults there in the small data model;
   the bulk buffers are moved out so the frequently touched state can
   stay in the short-addressed page. See EWSTM8/MFx_Layered.icf. */
__near static Record_Ctx s_rec;

/* Private function prototypes -----------------------------------------------*/
static uint8_t s_Clean(uint8_t bRaw);

/* Private functions ---------------------------------------------------------*/
/* The old FW reported an unwritten byte as 0; keep that. */
static uint8_t s_Clean(uint8_t bRaw)
{
    return (bRaw == REC_ERASED_BYTE) ? 0u : bRaw;
}

/* Functions -----------------------------------------------------------------*/
Ret_t APP_DATA_Record_Init(void)
{
    uint8_t bIdx;
    uint8_t bRaw;

    s_rec.bDirty      = 0u;
    s_rec.bResultSlot = 0u;

    (void)Eeprom_Read(REC_OFF_INSERT_COUNT, s_rec.abStatus,
                      (uint16_t)APP_REC_STATUS_BYTES);
    for (bIdx = 0u; bIdx < (uint8_t)APP_REC_STATUS_BYTES; bIdx++)
    {
        s_rec.abStatus[bIdx] = s_Clean(s_rec.abStatus[bIdx]);
    }

    (void)Eeprom_Read(REC_OFF_STICK_PTR, (uint8_t *)s_rec.awStickPtr,
                      (uint16_t)REC_STICK_PTR_BYTES);
    (void)Eeprom_Read(REC_OFF_QC_REF, (uint8_t *)s_rec.awQcRef,
                      (uint16_t)REC_QC_REF_BYTES);

    (void)Eeprom_Read(REC_OFF_DUMP_POS, &bRaw, 1u);
    bRaw = s_Clean(bRaw);
    s_rec.bDumpPos = (bRaw < (uint8_t)APP_REC_SLOT_COUNT) ? bRaw : 0u;

    return RET_OK;
}

Ret_t APP_DATA_Record_Process(void)
{
    Ret_t    tRet;
    uint16_t wFailOffset;

    /* A write that did not take: mark the block dirty again (spec §8). */
    if (Eeprom_TakeWriteFail(&wFailOffset) != 0u)
    {
        if (wFailOffset < REC_OFF_STICK_PTR)
        {
            s_rec.bDirty |= REC_DIRTY_STATUS;
        }
        else if (wFailOffset < REC_OFF_RESULT)
        {
            s_rec.bDirty |= REC_DIRTY_STICK_PTR;
        }
        else if (wFailOffset < REC_OFF_QC_REF)
        {
            s_rec.bDirty |= REC_DIRTY_RESULT;
        }
        else
        {
            s_rec.bDirty |= REC_DIRTY_DUMP_POS;
        }
    }

    if ((s_rec.bDirty == 0u) || (Eeprom_IsBusy() != 0u))
    {
        return RET_OK;
    }

    /* One block per pass, in the order they must land: the result record
       before the position that points at it. */
    if ((s_rec.bDirty & REC_DIRTY_STATUS) != 0u)
    {
        tRet = Eeprom_Write(REC_OFF_INSERT_COUNT, s_rec.abStatus,
                            (uint16_t)APP_REC_STATUS_BYTES);
        if (tRet == RET_OK)
        {
            s_rec.bDirty &= (uint8_t)~REC_DIRTY_STATUS;
        }
    }
    else if ((s_rec.bDirty & REC_DIRTY_STICK_PTR) != 0u)
    {
        tRet = Eeprom_Write(REC_OFF_STICK_PTR, (const uint8_t *)s_rec.awStickPtr,
                            (uint16_t)REC_STICK_PTR_BYTES);
        if (tRet == RET_OK)
        {
            s_rec.bDirty &= (uint8_t)~REC_DIRTY_STICK_PTR;
        }
    }
    else if ((s_rec.bDirty & REC_DIRTY_RESULT) != 0u)
    {
        tRet = Eeprom_Write((uint16_t)(REC_OFF_RESULT
                                       + ((uint16_t)s_rec.bResultSlot
                                          * (uint16_t)REC_SLOT_BYTES)),
                            (const uint8_t *)s_rec.awResult,
                            (uint16_t)REC_SLOT_BYTES);
        if (tRet == RET_OK)
        {
            s_rec.bDirty &= (uint8_t)~REC_DIRTY_RESULT;
        }
    }
    else if ((s_rec.bDirty & REC_DIRTY_DUMP_POS) != 0u)
    {
        tRet = Eeprom_Write(REC_OFF_DUMP_POS, &s_rec.bDumpPos, 1u);
        if (tRet == RET_OK)
        {
            s_rec.bDirty &= (uint8_t)~REC_DIRTY_DUMP_POS;
        }
    }
    else
    {
        /* nothing left */
    }

    return RET_OK;
}

Ret_t APP_DATA_Record_SetInsertCount(uint8_t bCount)
{
    s_rec.abStatus[REC_OFF_INSERT_COUNT] = bCount;
    s_rec.bDirty |= REC_DIRTY_STATUS;

    return RET_OK;
}

Ret_t APP_DATA_Record_SetTestCount(uint8_t bCount)
{
    s_rec.abStatus[REC_OFF_TEST_COUNT] = bCount;
    s_rec.bDirty |= REC_DIRTY_STATUS;

    return RET_OK;
}

Ret_t APP_DATA_Record_SetError(uint8_t bStep, uint8_t bCode)
{
    s_rec.abStatus[REC_OFF_ERROR_STEP] = bStep;
    s_rec.abStatus[REC_OFF_ERROR_CODE] = bCode;
    s_rec.bDirty |= REC_DIRTY_STATUS;

    return RET_OK;
}

Ret_t APP_DATA_Record_SetStickPtr(const uint16_t *p_wData)
{
    uint8_t bIdx;

    if (p_wData == NULL)
    {
        return RET_ERR_NULL;
    }

    for (bIdx = 0u; bIdx < (uint8_t)APP_REC_STICK_PTR_WORDS; bIdx++)
    {
        s_rec.awStickPtr[bIdx] = p_wData[bIdx];
    }
    s_rec.bDirty |= REC_DIRTY_STICK_PTR;

    return RET_OK;
}

Ret_t APP_DATA_Record_SaveResult(const uint16_t *p_wData)
{
    uint8_t bIdx;

    if (p_wData == NULL)
    {
        return RET_ERR_NULL;
    }
    if ((s_rec.bDirty & (REC_DIRTY_RESULT | REC_DIRTY_DUMP_POS)) != 0u)
    {
        return RET_BUSY;
    }

    for (bIdx = 0u; bIdx < (uint8_t)APP_REC_SLOT_WORDS; bIdx++)
    {
        s_rec.awResult[bIdx] = p_wData[bIdx];
    }

    /* The slot and the position that points at it are decided together here,
       the only place that computes them, so the two cannot drift apart. */
    s_rec.bResultSlot = (uint8_t)((s_rec.bDumpPos + 1u)
                                  % (uint8_t)APP_REC_SLOT_COUNT);
    s_rec.bDumpPos    = s_rec.bResultSlot;
    s_rec.bDirty |= (uint8_t)(REC_DIRTY_RESULT | REC_DIRTY_DUMP_POS);

    return RET_OK;
}

Ret_t APP_DATA_Record_UpdateWakeFail(uint16_t wAdc)
{
    uint16_t wMin = 0u;
    uint16_t wMax = 0u;

    (void)APP_DATA_Record_GetWakeFail(&wMin, &wMax);

    /* Written straight through: these only move on a wake-gate failure, and
       the value is not held anywhere else. */
    if (wAdc < wMin)
    {
        (void)Eeprom_Write(REC_OFF_WAKE_FAIL_MIN, (const uint8_t *)&wAdc, 2u);
    }
    else if (wAdc > wMax)
    {
        (void)Eeprom_Write(REC_OFF_WAKE_FAIL_MAX, (const uint8_t *)&wAdc, 2u);
    }
    else
    {
        /* inside the stored range */
    }

    return RET_OK;
}

uint8_t APP_DATA_Record_GetInsertCount(void)
{
    return s_rec.abStatus[REC_OFF_INSERT_COUNT];
}

uint8_t APP_DATA_Record_GetTestCount(void)
{
    return s_rec.abStatus[REC_OFF_TEST_COUNT];
}

uint8_t APP_DATA_Record_GetDumpPos(void)
{
    return s_rec.bDumpPos;
}

Ret_t APP_DATA_Record_GetResult(uint8_t bSlot, uint16_t *p_wData)
{
    if (p_wData == NULL)
    {
        return RET_ERR_NULL;
    }
    if (bSlot >= (uint8_t)APP_REC_SLOT_COUNT)
    {
        return RET_ERR_PARAM;
    }

    return Eeprom_Read((uint16_t)(REC_OFF_RESULT
                                  + ((uint16_t)bSlot * (uint16_t)REC_SLOT_BYTES)),
                       (uint8_t *)p_wData, (uint16_t)REC_SLOT_BYTES);
}

Ret_t APP_DATA_Record_GetQcRef(const uint16_t **p_wData)
{
    if (p_wData == NULL)
    {
        return RET_ERR_NULL;
    }

    *p_wData = s_rec.awQcRef;

    return RET_OK;
}

Ret_t APP_DATA_Record_GetStatus(uint16_t *p_wOut)
{
    uint8_t bIdx;

    if (p_wOut == NULL)
    {
        return RET_ERR_NULL;
    }

    for (bIdx = 0u; bIdx < (uint8_t)APP_REC_STATUS_BYTES; bIdx++)
    {
        p_wOut[bIdx] = (uint16_t)s_rec.abStatus[bIdx];
    }

    return RET_OK;
}

Ret_t APP_DATA_Record_GetWakeFail(uint16_t *p_wMin, uint16_t *p_wMax)
{
    if ((p_wMin == NULL) || (p_wMax == NULL))
    {
        return RET_ERR_NULL;
    }

    (void)Eeprom_Read(REC_OFF_WAKE_FAIL_MIN, (uint8_t *)p_wMin, 2u);
    (void)Eeprom_Read(REC_OFF_WAKE_FAIL_MAX, (uint8_t *)p_wMax, 2u);

    return RET_OK;
}

const uint16_t *APP_DATA_Record_GetStickPtr(void)
{
    return s_rec.awStickPtr;
}

uint8_t APP_DATA_Record_IsBusy(void)
{
    return ((s_rec.bDirty != 0u) || (Eeprom_IsBusy() != 0u)) ? 1u : 0u;
}

/*****************************END OF FILE****/
