/**
  ******************************************************************************
  * @file    : System_Main.h
  * @brief   : System layer - init order, slot scheduler, system state
  *
  * main.c calls only the three entry points below. Everything else in the
  * firmware is reached from here.
  *
  * Init order (spec v2.0.3 §5-2):
  *   reset cause -> HW Driver (tick first) -> Module -> App -> slot timestamps
  * A failing step does not stop init; it sets a SYS_INIT_ERR_xxx bit and the
  * App reports it later, once communication is alive.
  *
  * One pass of System_Main_Run():
  *   1 ms slot -> 10 ms slot -> state machine -> WFI
  *
  * Profile note: CR1632 battery. The loop ends in WFI (spec §13 item 2).
  * Deep sleep (Halt) is an App-driven power state and is not in the skeleton
  * yet.
  *
  * Differences from the common skeleton (spec §15):
  *   - Slot execution-time measurement is not implemented. Only run and late
  *     counts are kept. Reason: 8 KB flash, measurement needs a fine-grained
  *     timer read that is not needed to schedule. Revisit if a slot overruns.
  *   - No watchdog. The IWDG keeps running in Halt on this MCU (same choice
  *     as Surearly SMART, spec §19).
  *
  * Context: main loop only.
  ******************************************************************************
  */
#ifndef __SYSTEM_MAIN_H__
#define __SYSTEM_MAIN_H__

/* Includes ------------------------------------------------------------------*/
#include "HW_Ret_Code.h"

/* Exported types ------------------------------------------------------------*/
/* Scheduler state. Device state (idle, measuring, error) belongs to the App. */
typedef enum
{
    SYS_STATE_BOOT = 0,
    SYS_STATE_RUN,
    SYS_STATE_SAFE_STOP,
    SYS_STATE_COUNT
} System_State;

typedef enum
{
    SYS_SLOT_1MS = 0,
    SYS_SLOT_10MS,
    SYS_SLOT_COUNT
} System_Slot;

/* uint16 counters: 1 KB RAM profile (spec §12). Saturating, not wrapping. */
typedef struct
{
    uint16_t wRunCount;     /* times the slot body ran                       */
    uint16_t wLateCount;    /* times it was two or more periods late         */
} System_SlotStat;

/* Exported constants --------------------------------------------------------*/
/* Init error bits, reported through System_Main_GetInitErr(). */
#define SYS_INIT_ERR_NONE        0x00u
#define SYS_INIT_ERR_CLK         0x01u
#define SYS_INIT_ERR_TICK        0x02u
#define SYS_INIT_ERR_GPIO        0x04u
#define SYS_INIT_ERR_OPTIC       0x08u
#define SYS_INIT_ERR_LCD         0x10u
#define SYS_INIT_ERR_EEPROM      0x20u
#define SYS_INIT_ERR_PCCOMM      0x40u
#define SYS_INIT_ERR_RECORD      0x80u

#define SYS_SLOT_1MS_PERIOD_MS   1u
#define SYS_SLOT_10MS_PERIOD_MS  10u

/* API -----------------------------------------------------------------------*/
/**
  * @brief  First line of main(). Vector table setup.
  * @note   Layer: System. Blocking: no. ISR: no.
  *         Empty on this target: no bootloader, vectors are in flash at
  *         0x8000 and need no relocation.
  * @retval None
  */
void System_Main_EarlyInit(void);

/**
  * @brief  Initialises every layer in dependency order.
  * @note   Layer: System. Blocking: yes (< 2 ms, driver setup only). ISR: no.
  *         Never stops on a failing step. Details via GetInitErr().
  * @retval RET_OK, RET_ERROR
  */
Ret_t System_Main_Init(void);

/**
  * @brief  One pass of the main loop. Called from while(1) forever.
  * @note   Layer: System. Blocking: no. ISR: no.
  * @retval RET_OK
  */
Ret_t System_Main_Run(void);

/**
  * @brief  Enters SAFE_STOP: outputs off every pass, waits for reset.
  * @note   Layer: System. Blocking: no. ISR: no.
  * @retval None
  */
void System_Main_SafeStop(void);

/* Getters (cannot fail) -----------------------------------------------------*/
/**
  * @brief  Current scheduler state.
  * @note   Layer: System. Blocking: no. ISR: no.
  * @retval SYS_STATE_BOOT, SYS_STATE_RUN, SYS_STATE_SAFE_STOP
  */
System_State System_Main_GetState(void);

/**
  * @brief  Bit field of the steps that failed during init.
  * @note   Layer: System. Blocking: no. ISR: no.
  * @retval SYS_INIT_ERR_NONE or a combination of SYS_INIT_ERR_xxx
  */
uint8_t System_Main_GetInitErr(void);

/**
  * @brief  Raw reset status flags latched before they were cleared.
  * @note   Layer: System. Blocking: no. ISR: no.
  * @retval MCU reset status register value captured at init
  */
uint8_t System_Main_GetResetCause(void);

/**
  * @brief  Copies one slot's counters out.
  * @param  tSlot : slot to read
  * @param  p_out : destination, must not be NULL
  * @note   Layer: System. Blocking: no. ISR: no.
  * @retval RET_OK, RET_ERR_NULL, RET_ERR_PARAM, RET_ERR_UNSUPPORTED
  */
Ret_t System_Main_GetSlotStat(System_Slot tSlot, System_SlotStat *p_out);

#endif /* __SYSTEM_MAIN_H__ */
/*****************************END OF FILE****/
