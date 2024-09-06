#ifndef __PG_PRIV_INST_H__
#define __PG_PRIV_INST_H__


/*
 * smc instruction
 * 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 * ------------------------------------------------
 *|   TYPE    | L|     priv_type    |      rt      |
 * ------------------------------------------------
 */

typedef union smc_imm {       
        uint32_t bytes;
        struct {
                uint32_t rt : 5;
        	uint32_t ptype : 6;
               	uint32_t l : 1;
                uint32_t type : 4;
        }__attribute__ ((packed));
}__attribute__ ((packed)) smc_imm_t;

enum pg_smc_type
{
        STYPE_NONE = 0,
        
        STYPE_PRIV_INST,       
        STYPE_MEM_FAULT
};

enum pg_priv_inst_type
{
        PTYPE_NONE = 0,
        
       	PTYPE_SCTLR_EL1_W,
        PTYPE_TTBR0_EL1,
        PTYPE_TTBR1_EL1,
        PTYPE_TCR_EL1_W,

        PTYPE_AT_S1E1R,
        
        PTYPE_TLBI_VMALLE1IS,
        PTYPE_TLBI_VMALLE1,        
        PTYPE_TLBI_VAAE1IS,
        PTYPE_TLBI_ASIDE1IS,
};

#define CHECK_INST(inst, mask, opcode)	((inst & mask) == opcode)

static inline uint32_t SMC_GEN(enum pg_smc_type type, enum pg_priv_inst_type ptype, uint32_t reg, bool l)
{
	#define SMC_OPCODE		0xd4000003
	#define SMC_IMM_SHIFT		5
        
        smc_imm_t imm = {.bytes = 0};
        imm.rt = reg;
        imm.ptype = ptype;
        imm.l = l;
        imm.type = type;

        return (SMC_OPCODE | (imm.bytes << SMC_IMM_SHIFT));
}

static inline uint32_t SMC_IMM_GET_REG(uint32_t smc_imm)
{
        smc_imm_t imm = {.bytes = smc_imm};
        return imm.rt;
}

static inline enum pg_priv_inst_type SMC_IMM_GET_PTYPE(uint32_t smc_imm)
{
        smc_imm_t imm = {.bytes = smc_imm};
        return imm.ptype;
}

static inline bool SMC_IMM_GET_L(uint32_t smc_imm)
{
        smc_imm_t imm = {.bytes = smc_imm};
        return imm.l;
}

static inline enum pg_smc_type SMC_IMM_GET_TYPE(uint32_t smc_imm)
{
        smc_imm_t imm = {.bytes = smc_imm};
        return imm.type;
}


#endif //< __PG_SMC_H__
