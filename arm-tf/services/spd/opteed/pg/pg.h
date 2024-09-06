#ifndef __PG_H__
#define __PG_H__

#ifndef __ASSEMBLY__

#include <platform.h>
#include <stdbool.h>
#include <stdlib.h>
#include <spinlock.h>
//#include "umm/umm_malloc.h"

#include "pg_mem_virt.h"
#include "pg_priv_inst.h"
#include "../opteed_private.h"

#define PG_GUEST_NUM		3

#if defined(FVP_MAX_CPUS_PER_CLUSTER)
#define PG_CORE_COUNT		4
#elif defined(JUNO_CLUSTER_COUNT)
#define PG_CORE_COUNT		6
#endif
//jwseo
#define PG_CORE_COUNT		6
#define PG_BENIGN_GUEST_ID 	0

#define PG_SECURE_OS_PUB_KEY_BIT_SIZE	2048

typedef struct pg_virt_ctx {        
        uint64_t ttbr0_el1;
        uint64_t s_ttbr0_el1;
} pg_virt_ctx_t;

typedef struct pg_guest {
        uint32_t id;
        uint8_t pub_key[PG_SECURE_OS_PUB_KEY_BIT_SIZE/8] __aligned(8);
        uint32_t pub_key_bits;
        
        
        uint64_t start_addr;
        bool tlb_is_not_clear[PG_CORE_COUNT];
        
        //context
        optee_vectors_t *_optee_vectors;
        optee_context_t _opteed_sp_context[PG_CORE_COUNT] __aligned(16);
        uint32_t _opteed_rw;

        //contextes for resource virtualization
        pg_virt_ctx_t virt_ctx[PG_CORE_COUNT] __aligned(16);
} pg_guest_t;

extern pg_guest_t pg_guests[PG_GUEST_NUM];
extern uint32_t cur_guest[PG_CORE_COUNT];

#define current				(&pg_guests[cur_guest[plat_my_core_pos()]])
#define current_optee_ctx		(&current->_opteed_sp_context[plat_my_core_pos()])
#define current_virt_ctx		(&current->virt_ctx[plat_my_core_pos()])
#define guest_idx(id)			(&pg_guests[id])
#define guest_idx_optee_ctx(id, core)	(&guest_idx(id)->_opteed_sp_context[core])
#define guest_idx_virt_ctx(id, core)	(&guest_idx(id)->virt_ctx[core])
#define set_current(cur)		{cur_guest[plat_my_core_pos()] = cur;}
static inline void  set_all_current(uint32_t id) {
        int i;
        for (i=0; i<PG_CORE_COUNT; i++)
                cur_guest[i] = id;
}

#define SET_NS()	\
        uint64_t __scr_el3 = read_scr_el3(); \
        write_scr_el3(__scr_el3 | SCR_NS_BIT));	\
        isb();
#define CLEAR_NS()	\
        uint64_t __scr_el3 = read_scr_el3(); \
        write_scr_el3(__scr_el3 & (~SCR_NS_BIT));	\
        isb();
#define RESTORE_NS()	\
        write_scr_el3(__scr_el3);	\
        isb();        


//missed declares or definitions in arm-tfx
DEFINE_SYSOP_TYPE_PARAM_FUNC(tlbi, aside1is);
DEFINE_SYSOP_TYPE_FUNC(tlbi, vmalle1is);
DEFINE_RENAME_SYSREG_WRITE_FUNC(par_el1, par_el1);

/* pg_lib */
uint64_t pg_virt_to_phys_el1(uint64_t va);
uint64_t pg_virt_to_phys_ttbr(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t ttbr, uint64_t va, bool virtual);
void pg_trap_priv_insts(uint64_t addr, uint64_t len);
uint32_t pg_adjust_direct_branch(uint32_t inst, uint32_t offset);
void pg_create_check_point();
void pg_dump_page_table(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t ttbr, bool virtual);
void pg_copy_os(uint32_t from, uint32_t to, uint64_t start_addr);
void pg_active_os(uint32_t id, bool all);
void pg_tlbivaae1is(uint64_t addr);
void pg_tlbivmalle1is();
void pg_tlbivmalle1();
bool pg_add_dirty_page(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t addr);
void pg_driver_protection();
void pg_handle_dirty_page_table(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx);
         
/* pg functions */
void pg_init();
void pg_manage_memory();
void pg_secondary_init();
uint32_t pg_create_or_destroy_os(uint64_t pub_key_ptr, uint64_t pub_key_bits, uint64_t start_addr, bool create);
void pg_clear_os(uint32_t id);
void pg_switch_os();
void pg_manage_os();
void pg_enter_to_os(bool secure);

uint64_t pg_create_shadow_page_table(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, bool init);
//uint64_t pg_renew_shadow_page_table(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx);

void pg_emulate_priv_inst(uint32_t smc_imm);
void pg_enable_trap_priv_insts(pg_virt_ctx_t *virt_ctx);
void pg_enable_trap_vector_table();

#define PG_OPTEE_PATCH
#ifdef PG_OPTEE_PATCH

#define opteed_sp_context		(current->_opteed_sp_context)
#define optee_vectors			(current->_optee_vectors)
#define opteed_rw			(current->_opteed_rw)

#endif //< PG_OPTEE_PATCH

#include <runtime_svc.h>

#define	OEN_PLAYGROUND_START		47
#define	OEN_PLAYGROUND_END		47

#define ARM_SMCCC_OWNER_PLAYGROUND	47
#define TEESMC_PLAYGROUND_RV(func_num) \
		((SMC_TYPE_FAST << FUNCID_TYPE_SHIFT) | \
		 ((SMC_32) << FUNCID_CC_SHIFT) | \
		 (ARM_SMCCC_OWNER_PLAYGROUND << FUNCID_OEN_SHIFT) | \
		 ((func_num) & FUNCID_NUM_MASK))

#define TEESMC_PLAYGROUND_CREATE_SEC_OS 	TEESMC_PLAYGROUND_RV(0)
#define TEESMC_PLAYGROUND_CHANGE_SEC_OS 	TEESMC_PLAYGROUND_RV(1)
#define TEESMC_PLAYGROUND_DESTROY_SEC_OS 	TEESMC_PLAYGROUND_RV(2)


#endif //< __ASSEMBLY__
#endif //< __PG_H__
