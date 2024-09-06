#include <linux/linkage.h>
#include <linux/arm-smccc.h>
#include <asm/asm-offsets.h>

    .macro SMCCC instr
    .cfi_startproc
    \instr  #0
    ldr x4, [sp]
    stp x0, x1, [x4, #ARM_SMCCC_RES_X0_OFFS]
    stp x2, x3, [x4, #ARM_SMCCC_RES_X2_OFFS]
    ret
    .cfi_endproc
    .endm

ENTRY(arm_smccc_smc)
    SMCCC smc
ENDPROC(arm_smccc_smc)

ENTRY(arm_smccc_hvc)
    SMCCC   hvc
ENDPROC(arm_smccc_hvc)

