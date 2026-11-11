;*******************************************************************************
; @file    : HW_Cpu_Drive.s
; @brief   : HW Driver layer - fixed-cycle delay loop and interrupt mask
;
; Why assembly: the optical charge time is set by how long the loop runs
; between switching an LED on and starting the ADC (hs.kim Optic_Solution
; v1.2, 5-6). A C loop's cycle count depends on the compiler and its
; settings; these two instructions do not.
;
; HW_Cpu_DelayLoop(uint16_t wLoops)  - wLoops arrives in X (IAR STM8 calling
;   convention: first 16-bit argument in X; X is a scratch register).
;   One pass = DECW (1 cycle) + JRNE taken (2 cycles) = 3 cycles,
;   1.5 us at fMASTER 2 MHz. Instruction fetch alignment can add a cycle;
;   the count is fixed within one binary either way, and tuning measures in
;   loops, not in us. wLoops = 0 wraps to 65536 passes (about 98 ms):
;   callers must not pass 0.
;   Run it with interrupts masked, or an ISR stretches the delay.
;
; HW_Cpu_IrqDisable / HW_Cpu_IrqEnable - SIM / RIM, so that the Module layer
;   can mask interrupts without touching vendor headers.
;
; Code model small (MFx_Layered.ewp GenCodeModel 0): near calls, RET.
;*******************************************************************************

        MODULE  HW_Cpu_Drive

        PUBLIC  HW_Cpu_DelayLoop
        PUBLIC  HW_Cpu_IrqDisable
        PUBLIC  HW_Cpu_IrqEnable

        SECTION `.near_func.text`:CODE:NOROOT(0)
        CODE

HW_Cpu_DelayLoop:
??loop:
        DECW    X
        JRNE    ??loop
        RET

HW_Cpu_IrqDisable:
        SIM
        RET

HW_Cpu_IrqEnable:
        RIM
        RET

        END
