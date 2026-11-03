/**
  ******************************************************************************
  * @file    : System_Main.c
  * @brief   : System layer - init order, slot scheduler, system state
  *
  * Vendor access here is limited to system-global resources (reset status,
  * core sleep instruction), which spec v2.0.3 §3 allows at this layer.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "System_Main.h"
#include "HW_CLK_Drive.h"
#include "HW_GPIO_Drive.h"
#include "HW_Tick_Drive.h"
#include "Eeprom.h"
#include "Lcd.h"
#include "PcComm.h"
#include "APP_DATA_Record.h"
#include "APP_CTRL_Sequence.h"
#include "APP_EXEC_Measure.h"
#include "Optic.h"
#include "stm8l15x.h"
#include "stm8l15x_rst.h"

/* Build switches (manual) ---------------------------------------------------*/
/* Slot run and late counters. Development instrumentation, not a feature:
   nothing in the firmware reads them, they exist for a debugger session.
   Turned off for release because this part has 8 KB of flash and the port
   needed the space (spec v2.0.3 §12 allows exactly this). Set to 1 while
   investigating a slot that overruns. (2026-11-03) */
#ifndef SYS_USE_SLOT_STAT
#define SYS_USE_SLOT_STAT        0
#endif
#if ((SYS_USE_SLOT_STAT != 0) && (SYS_USE_SLOT_STAT != 1))
#error "SYS_USE_SLOT_STAT must be 0 or 1"
#endif

#ifndef SYS_USE_WDG
#define SYS_USE_WDG              0
#endif

#if ((SYS_USE_WDG != 0) && (SYS_USE_WDG != 1))
#error "SYS_USE_WDG must be 0 or 1"
#endif

/* Private types -------------------------------------------------------------*/
typedef struct
{
    System_State tState;
    uint8_t      bInitErr;
    uint8_t      bResetCause;
    uint32_t     adwSlotLast[SYS_SLOT_COUNT];
#if (SYS_USE_SLOT_STAT == 1)
    System_SlotStat atSlotStat[SYS_SLOT_COUNT];
#endif
} System_Ctx;

/* Private variables ---------------------------------------------------------*/
static System_Ctx s_sys;

/* Private function prototypes -----------------------------------------------*/
static void s_RunSlot(System_Slot tSlot, uint32_t dwPeriodMs);
static void s_Slot1ms(void);
static void s_Slot10ms(void);
static void s_StateMachine(void);
#if (SYS_USE_SLOT_STAT == 1)
static void s_StatCount(uint16_t *p_wCounter);
#endif

/* Private functions ---------------------------------------------------------*/
#if (SYS_USE_SLOT_STAT == 1)
/* Saturating increment: a stuck counter is better than a wrapped one. */
static void s_StatCount(uint16_t *p_wCounter)
{
    if (*p_wCounter < 0xFFFFu)
    {
        (*p_wCounter)++;
    }
}
#endif

static void s_RunSlot(System_Slot tSlot, uint32_t dwPeriodMs)
{
    HW_Tick_Due tDue;

    tDue = HW_Tick_IsDue(&s_sys.adwSlotLast[tSlot], dwPeriodMs);

    if (tDue == HW_TICK_NOT_DUE)
    {
        return;
    }

#if (SYS_USE_SLOT_STAT == 1)
    s_StatCount(&s_sys.atSlotStat[tSlot].wRunCount);
    if (tDue == HW_TICK_DUE_LATE)
    {
        s_StatCount(&s_sys.atSlotStat[tSlot].wLateCount);
    }
#endif

    /* Direct calls, no function pointer table: the execution order stays
       visible in the source and the stack analysis stays exact (spec §5-3). */
    if (tSlot == SYS_SLOT_1MS)
    {
        s_Slot1ms();
    }
    else
    {
        s_Slot10ms();
    }
}

static void s_Slot1ms(void)
{
    /* Time critical modules go here. */
}

static void s_Slot10ms(void)
{
    /* Slow modules and Apps go here. Apps run in SYS_STATE_RUN only. */
    (void)Optic_Process();
    (void)Lcd_Process();
    (void)Eeprom_Process();
    (void)PcComm_Process();

    /* App: data first, it is the layer the others read from. */
    (void)APP_DATA_Record_Process();
    (void)APP_EXEC_Measure_Process();

    /* Apps run in RUN only (spec v2.0.3 §5-3). */
    if (s_sys.tState == SYS_STATE_RUN)
    {
        (void)APP_CTRL_Sequence_Process();
    }
}

static void s_StateMachine(void)
{
    switch (s_sys.tState)
    {
        case SYS_STATE_BOOT:
            s_sys.tState = SYS_STATE_RUN;
            break;

        case SYS_STATE_RUN:
            break;

        case SYS_STATE_SAFE_STOP:
            /* Outputs are forced off every pass until a reset. */
            break;

        default:
            s_sys.tState = SYS_STATE_SAFE_STOP;
            break;
    }
}

/* Functions -----------------------------------------------------------------*/
void System_Main_EarlyInit(void)
{
    /* No bootloader on this target: the vector table stays in flash at the
       reset address and needs no relocation. */
}

Ret_t System_Main_Init(void)
{
    Ret_t   tRet;
    uint8_t bSlot;

    s_sys.tState   = SYS_STATE_BOOT;
    s_sys.bInitErr = SYS_INIT_ERR_NONE;

    /* 1. Reset cause, before anything clears the flags. */
    s_sys.bResetCause = (uint8_t)RST->SR;
    RST_ClearFlag((RST_FLAG_TypeDef)((uint8_t)RST_FLAG_PORF
                                     | (uint8_t)RST_FLAG_SWIMF
                                     | (uint8_t)RST_FLAG_ILLOPF
                                     | (uint8_t)RST_FLAG_IWDGF
                                     | (uint8_t)RST_FLAG_WWDGF
                                     | (uint8_t)RST_FLAG_BORF));

    /* 2. HW Driver: clock first (every later timing depends on fMASTER),
          then the time base, then pins at a safe level. */
    tRet = HW_CLK_Init();
    if (tRet != RET_OK)
    {
        s_sys.bInitErr |= SYS_INIT_ERR_CLK;
    }

    tRet = HW_Tick_Init();
    if (tRet != RET_OK)
    {
        s_sys.bInitErr |= SYS_INIT_ERR_TICK;
    }

    tRet = HW_GPIO_Init();
    if (tRet != RET_OK)
    {
        s_sys.bInitErr |= SYS_INIT_ERR_GPIO;
    }

    /* 3. Module: outputs to a safe state first (LEDs off), then the rest. */
    tRet = Optic_Init();
    if (tRet != RET_OK)
    {
        s_sys.bInitErr |= SYS_INIT_ERR_OPTIC;
    }

    tRet = Lcd_Init();
    if (tRet != RET_OK)
    {
        s_sys.bInitErr |= SYS_INIT_ERR_LCD;
    }

    tRet = Eeprom_Init();
    if (tRet != RET_OK)
    {
        s_sys.bInitErr |= SYS_INIT_ERR_EEPROM;
    }

    tRet = PcComm_Init();
    if (tRet != RET_OK)
    {
        s_sys.bInitErr |= SYS_INIT_ERR_PCCOMM;
    }

    /* 4. App: stored data first, then the command gate, then control. */
    tRet = APP_DATA_Record_Init();
    if (tRet != RET_OK)
    {
        s_sys.bInitErr |= SYS_INIT_ERR_RECORD;
    }

    (void)APP_EXEC_Measure_Init();
    (void)APP_CTRL_Sequence_Init();


    /* 4. App: parameters, command gate, control. */

    /* 5. Slot timestamps, so the first pass is not reported as late. */
    for (bSlot = 0u; bSlot < (uint8_t)SYS_SLOT_COUNT; bSlot++)
    {
        s_sys.adwSlotLast[bSlot] = HW_Tick_Get();
    }

    enableInterrupts();

    return (s_sys.bInitErr == SYS_INIT_ERR_NONE) ? RET_OK : RET_ERROR;
}

Ret_t System_Main_Run(void)
{
    /* Every pass: communication RX parse, storage, command dispatch. */

    s_RunSlot(SYS_SLOT_1MS,  SYS_SLOT_1MS_PERIOD_MS);
    s_RunSlot(SYS_SLOT_10MS, SYS_SLOT_10MS_PERIOD_MS);

    s_StateMachine();

    /* Battery profile: sleep until the next interrupt (spec §13 item 2). */
    wfi();

    return RET_OK;
}

void System_Main_SafeStop(void)
{
    s_sys.tState = SYS_STATE_SAFE_STOP;
}

System_State System_Main_GetState(void)
{
    return s_sys.tState;
}

uint8_t System_Main_GetInitErr(void)
{
    return s_sys.bInitErr;
}

uint8_t System_Main_GetResetCause(void)
{
    return s_sys.bResetCause;
}

Ret_t System_Main_GetSlotStat(System_Slot tSlot, System_SlotStat *p_out)
{
#if (SYS_USE_SLOT_STAT == 1)
    if (p_out == NULL)
    {
        return RET_ERR_NULL;
    }
    if ((uint8_t)tSlot >= (uint8_t)SYS_SLOT_COUNT)
    {
        return RET_ERR_PARAM;
    }

    *p_out = s_sys.atSlotStat[tSlot];

    return RET_OK;
#else
    (void)tSlot;
    (void)p_out;
    return RET_ERR_UNSUPPORTED;
#endif
}

/*****************************END OF FILE****/
