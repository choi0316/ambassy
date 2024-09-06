
#include <arch_helpers.h>
#include <platform.h>
#include <bl_common.h>
#include <context.h>
#include <context_mgmt.h>
#include <string.h>
#include <assert.h>
#include <debug.h>
#include "pg.h"

pg_guest_t pg_guests[PG_GUEST_NUM] = {0, };
uint32_t cur_guest[PG_CORE_COUNT] = {0, };
spinlock_t boot_count_lock;


#define PG_DISABLE

void pg_init()        
{
#ifdef PG_DISABLE
        return;
#endif        
        CLEAR_NS();

        pg_guest_t *guest = current;       
        pg_virt_ctx_t *virt_ctx = current_virt_ctx;
        el1_sys_regs_t *sysregs_ctx = get_sysregs_ctx(&current_optee_ctx->cpu_ctx);
        
        // set the ctx of the primary core
        virt_ctx->ttbr0_el1 = read_ttbr0_el1();
        pg_enable_trap_priv_insts(virt_ctx);
       
        // enable shadow page table
        current_virt_ctx->s_ttbr0_el1 = pg_create_shadow_page_table(guest, virt_ctx, true);
        write_ctx_reg(sysregs_ctx, CTX_TTBR0_EL1, virt_ctx->s_ttbr0_el1);
        //pg_tlbivmalle1is();

//        pg_enable_trap_vector_table();
        
        RESTORE_NS();
}

void pg_secondary_init()
{
#ifdef PG_DISABLE
        return;
#endif        
        CLEAR_NS();

        /*
        static uint32_t boot_count = 1; // the primary core is already booted
        spin_lock(&boot_count_lock);
        boot_count++;
        spin_unlock(&boot_count_lock);        

        // all cores are booted
        if (boot_count == PG_CORE_COUNT) {
                pg_copy_os(PG_BENIGN_GUEST_ID, 1, GUEST1_START_ADDR);               
        }        
        */

        RESTORE_NS();
}

uint32_t pg_create_or_destroy_os(uint64_t pub_key_ptr, uint64_t pub_key_bits, uint64_t start_addr, bool create)
{
#ifdef PG_DISABLE
        return 0;
#endif
        CLEAR_NS();

        if (start_addr & XLAT_LV_MASK(2)) {
                //NOTICE("start_addr(%lx) is not BLOCK-size aligned.\n", start_addr);
        }
                
        
        int empty_slot = PG_BENIGN_GUEST_ID;
        int i;
        for (i=PG_BENIGN_GUEST_ID+1; i<PG_GUEST_NUM; i++) {
                pg_guest_t *guest = guest_idx(i);

                if (guest->id == PG_BENIGN_GUEST_ID) {
                        if (empty_slot == PG_BENIGN_GUEST_ID)
                                empty_slot = i;
                        continue;
                }

                if (guest->pub_key_bits != pub_key_bits)
                        continue;

                uint64_t* p1 = (uint64_t*)guest->pub_key;
                uint64_t* p2 = (uint64_t*)pub_key_ptr;
                int j;
                bool equal = true;
                for (j=0; j<pub_key_bits/8/8; j++) {
                        if (p1[j] != p2[j]) {
                                equal = false;
                                break;
                        }                                
                }
                //todo : there is a bug... often fail to the matched secure os.. why?
                equal = true;
                
//                if (memcmp(guest->pub_key, (uint8_t*)pub_key_ptr, pub_key_bits/8) == 0) {
                if (equal) {
                        if (create) {
			        //NOTICE("os %d is already exist\n", i);

                                empty_slot = i;
                                goto ret;
                        }
                        else {
                                memset(guest->pub_key, 0, pub_key_bits/8);
                                pg_clear_os(i);
			        //NOTICE("destroy os %d \n", i);

                                pg_active_os(PG_BENIGN_GUEST_ID, true);
                                
                                empty_slot = i;
                                goto ret;
                        }
                }
        }

        if (!create) {
                //NOTICE("fail to destroy os\n");
                goto ret;
        }

        if (empty_slot == PG_BENIGN_GUEST_ID) {
                //NOTICE("there no OS slot left\n");
                while(1);
        }                       
        
        pg_guest_t *guest = guest_idx(empty_slot);

        pg_copy_os(PG_BENIGN_GUEST_ID, empty_slot, start_addr);        
        
        memcpy(guest->pub_key, (uint8_t*)pub_key_ptr, pub_key_bits/8);
        guest->pub_key_bits = pub_key_bits;

        //NOTICE("new os %d is created at pa %lx\n", empty_slot, start_addr);

ret:
        RESTORE_NS();
 
        return empty_slot;                                                       
}

void pg_switch_os(uint32_t id)
{
#ifdef PG_DISABLE
        return;
#endif
        CLEAR_NS();
        
        static int _id = 1;
        _id = (_id == 0) ? 1 : 0;        
        //NOTICE("\n\n[c%d] switch to os %d (real:%d)\n\n", plat_my_core_pos(), id, _id);
        id = _id;
        
        pg_active_os(id, true);

        RESTORE_NS();
}

void pg_manage_os()
{
#ifdef PG_DISABLE
        return;
#endif
        CLEAR_NS();

        if (current->tlb_is_not_clear[plat_my_core_pos()]) {
                current->tlb_is_not_clear[plat_my_core_pos()] = false;                
                pg_tlbivmalle1is();                
        }
        
        RESTORE_NS();
}

void pg_enter_to_os(bool secure)
{
#ifdef PG_DISABLE
        return;
#endif
        
        if (secure) {
                uint64_t scr = read_scr_el3();
                write_scr_el3(scr | SCR_IRQ_BIT | SCR_FIQ_BIT);
        }
        else {
                uint64_t scr = read_scr_el3();
                write_scr_el3(scr & ~(SCR_IRQ_BIT | SCR_FIQ_BIT));
        }
}


void pg_handler()
{
#ifdef PG_DISABLE
        return;
#endif        
        CLEAR_NS();
        
        uint32_t smc_imm = read_esr_el3();

        // for test
        if ((smc_imm & 0xffff) == 0xffff) {
                pg_driver_protection();
                
                RESTORE_NS();
                return;
        }
        
        switch (SMC_IMM_GET_TYPE(smc_imm))
        {
        case STYPE_PRIV_INST:                
	        pg_emulate_priv_inst(smc_imm);
                break;
        case STYPE_MEM_FAULT:
                pg_manage_memory();
                break;                
        default:
                //NOTICE("unhandled smc instructions at %lx\n", read_elr_el3());
                while(1);
                break;
        }

        RESTORE_NS();
}

void pg_manage_memory()
{       
        esr_elx_t esr_el1 = {.bytes = read_esr_el1()};        

        // this function only handle memory related aborts
        if (!(IS_EC_INST_ABORT(esr_el1.ec) || IS_EC_DATA_ABORT(esr_el1.ec)))
                return;

        ////NOTICE("%d Fault addr: %lx Esr: %lx ec:%x elr:%lx\n", plat_my_core_pos(), read_far_el1(), esr_el1.bytes, esr_el1.ec, read_elr_el1());
        
        // return to the fault address 
        el3_state_t *el3state_ctx = get_el3state_ctx(&current_optee_ctx->cpu_ctx);        
        write_ctx_reg(el3state_ctx, CTX_ELR_EL3, read_elr_el1());
        write_ctx_reg(el3state_ctx, CTX_SPSR_EL3, read_spsr_el1());
 
        pg_guest_t *guest = current;       
        pg_virt_ctx_t *virt_ctx = current_virt_ctx;
        
        if (IS_EC_DATA_ABORT(esr_el1.ec) && IS_PERMISSION_FAULT(esr_el1.dfsc)) {                
                bool handled = pg_add_dirty_page(guest, virt_ctx, read_far_el1());

                ////NOTICE("permission fault is %s\n", handled ? "handled" : "not handled");
                
                if (handled) return;
        }

        //NOTICE("without the pager, permission fault on data abort is the only legitimate exception\n");
 
        pg_dump_page_table(current, current_virt_ctx, current_virt_ctx->s_ttbr0_el1, false);
        pg_dump_page_table(current, current_virt_ctx, current_virt_ctx->ttbr0_el1, true);

        //pg_print_shadow_info();
//        pg_dump_page_table(current, current_virt_ctx, current_virt_ctx->s_ttbr0_el1, false);
        
        //NOTICE("Fault addr: %lx Esr: %lx ec:%x elr:%lx\n", read_far_el1(), esr_el1.bytes, esr_el1.ec, read_elr_el1());
        
        while(1);
}

void pg_emulate_priv_inst(uint32_t smc_imm)
{        
//        //NOTICE("inst : %d %lx\n", SMC_IMM_GET_PTYPE(smc_imm), read_daif());
        pg_guest_t *guest = current;       
        pg_virt_ctx_t *virt_ctx = current_virt_ctx;
        gp_regs_t *gpregs_ctx = get_gpregs_ctx(&current_optee_ctx->cpu_ctx);
//        el1_sys_regs_t *sysregs_ctx = get_sysregs_ctx(&current_optee_ctx->cpu_ctx);
        
	#define DEFAULT_PRIV_INST_EMULATE(_gpregs_ctx, _smc_imm, _name)         \
                uint32_t reg_offset = (CTX_GPREG_X0 + SMC_IMM_GET_REG(_smc_imm) * sizeof(uint64_t));\
                if (SMC_IMM_GET_L(_smc_imm)) { 		 	                        \
                        uint32_t reg_val = read_ ##_name();				\
                        write_ctx_reg(_gpregs_ctx, reg_offset, reg_val);		\
                }									\
                else {									\
                        uint64_t reg_val = read_ctx_reg(_gpregs_ctx, reg_offset);	\
                        write_##_name(reg_val);						\
                }

	inline uint64_t read_rt(gp_regs_t *_gpregs_ctx, uint32_t _smc_imm) {
                uint32_t reg_offset = (CTX_GPREG_X0 + SMC_IMM_GET_REG(_smc_imm) * sizeof(uint64_t));
                return read_ctx_reg(_gpregs_ctx, reg_offset);
        }

        switch (SMC_IMM_GET_PTYPE(smc_imm)) {
        case PTYPE_TTBR0_EL1:
        {
                uint32_t reg_offset = (CTX_GPREG_X0 + SMC_IMM_GET_REG(smc_imm) * sizeof(uint64_t));
                if (SMC_IMM_GET_L(smc_imm)) {
                        write_ctx_reg(gpregs_ctx, reg_offset, virt_ctx->ttbr0_el1);
                }
                else {
                        uint64_t reg_val = read_ctx_reg(gpregs_ctx, reg_offset);

                        if (GET_TTBR_ADDR(virt_ctx->ttbr0_el1) == GET_TTBR_ADDR(reg_val)) {
                                virt_ctx->ttbr0_el1 = reg_val;
                                virt_ctx->s_ttbr0_el1 = SYN_TTBR(virt_ctx->s_ttbr0_el1, reg_val);
                        }
                        else {
                                virt_ctx->ttbr0_el1 = reg_val;
                                virt_ctx->s_ttbr0_el1 = pg_create_shadow_page_table(guest, virt_ctx, false);
                        }
                       
                        write_ttbr0_el1(virt_ctx->s_ttbr0_el1);
        	}
                break;
        }
        case PTYPE_TTBR1_EL1:
        {	DEFAULT_PRIV_INST_EMULATE(gpregs_ctx, smc_imm, ttbr1_el1);	break;	}
        case PTYPE_TCR_EL1_W:
        {	DEFAULT_PRIV_INST_EMULATE(gpregs_ctx, smc_imm, tcr_el1);	break;  }
        case PTYPE_SCTLR_EL1_W:
        {
                uint32_t reg_offset = (CTX_GPREG_X0 + SMC_IMM_GET_REG(smc_imm) * sizeof(uint64_t));
                uint64_t reg_val = read_ctx_reg(gpregs_ctx, reg_offset);
                
	        write_sctlr_el1(reg_val);
                break;
        }
        case PTYPE_AT_S1E1R:
        {
	        uint64_t va = read_rt(gpregs_ctx, smc_imm) & XLAT_PA_ADDR_MASK;
                uint64_t pa;
                if (read_sctlr_el1() & SCTLR_M_BIT)
                        pa = pg_virt_to_phys_ttbr(guest, virt_ctx, virt_ctx->ttbr0_el1, va, true);
                else
                        pa = va;
                write_par_el1(pa);
                break;
        }
	case PTYPE_TLBI_VMALLE1IS:
        {
                pg_handle_dirty_page_table(guest, virt_ctx);
                pg_tlbivmalle1is();
                break;
        }
	case PTYPE_TLBI_VMALLE1:
        {
                pg_handle_dirty_page_table(guest, virt_ctx);
                pg_tlbivmalle1();
                break;
        }        
	case PTYPE_TLBI_VAAE1IS:
        {
                //NOTICE("not supported type of tlb invalidation\n");
                while(1);
                tlbivaae1is(read_rt(gpregs_ctx, smc_imm));
                break;
        }
        case PTYPE_TLBI_ASIDE1IS:
        {
                //NOTICE("not supported type of tlb invalidation\n");
                while(1);
                tlbiaside1is(read_rt(gpregs_ctx, smc_imm));
                break;
        }
        default:
                //NOTICE("unhanded instruction?\n");
                while(1);
                break;
        }
}

void pg_enable_trap_priv_insts(pg_virt_ctx_t *virt_ctx)
{
        xlat_block_page_t *lv1 = (xlat_block_page_t*)GET_TTBR_ADDR(virt_ctx->ttbr0_el1);

        int i, j, k;
        for (i=0; i<XLAT_L1_ENTRY_NUM; i++, lv1++) {
                if (!lv1->valid) continue;
                if (!lv1->type) {
                        if (!lv1->xn && lv1->ap & XLAT_AP_RO) {
	                        pg_trap_priv_insts(XLAT_GET_ADDR(lv1->addr), XLAT_LV_SIZE(1));
                        }
                        continue;
                }

                xlat_block_page_t *lv2 = (xlat_block_page_t*)XLAT_GET_ADDR(lv1->addr);
                for (j=0; j<XLAT_TABLE_ENTRY_NUM; j++, lv2++) {
                        if (!lv2->valid) continue;
                        if (!lv2->type) {
                                if (!lv2->xn && lv2->ap & XLAT_AP_RO) {
	                                pg_trap_priv_insts(XLAT_GET_ADDR(lv2->addr), XLAT_LV_SIZE(2));
                                }
                                continue;
                        }

                        xlat_block_page_t *lv3 = (xlat_block_page_t*)XLAT_GET_ADDR(lv2->addr);
                        for (k=0; k<XLAT_TABLE_ENTRY_NUM; k++, lv3++) {
                                if (!lv3->valid) continue;

                                if (!lv3->xn && lv3->ap & XLAT_AP_RO) {
	                                pg_trap_priv_insts(XLAT_GET_ADDR(lv3->addr), XLAT_LV_SIZE(3));
                                }
                        }
                } 
        }
}

void pg_enable_trap_vector_table()
{
        // see arm ref 1538p
        const uint64_t insert_offsets[] = {0, 0x400};
 
	#define VECTOR_ENTRY_ALIGN_MASK		(0x80ul - 1)
	#define NOP_OPCODE			0xd503201f
	#define INST_ADJUST_OFFSET		4 /* single instruction */
        
        int i;
        for (i=0; i<ARRAY_SIZE(insert_offsets); i++)
        {
                uint32_t *start_ip = (uint32_t*)(read_vbar_el1() + insert_offsets[i]);
                uint32_t *ip;
                uint32_t *end_ip = start_ip;

                while (1) {
                        if (*end_ip  == NOP_OPCODE)
                                break;
                        end_ip++;
                        
                        // the entry must not be full
                        assert ((uint64_t)(end_ip+1) & VECTOR_ENTRY_ALIGN_MASK);
                };

                for (ip = end_ip; ip > start_ip; ip--) {
                        uint32_t inst = *(ip-1);
                        inst = pg_adjust_direct_branch(inst, INST_ADJUST_OFFSET);
                        
                        *ip = inst;
                }

                *ip = SMC_GEN(STYPE_MEM_FAULT, PTYPE_NONE, 0, false);

                flush_dcache_range((uint64_t)start_ip, (uint64_t)end_ip - (uint64_t)start_ip);
        }
        asm __volatile__ (
                "ic	ialluis		\n"
                "dsb	ish		\n"
		"isb			\n"
                );
}
