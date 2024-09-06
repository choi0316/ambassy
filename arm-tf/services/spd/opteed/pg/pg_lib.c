
#include <arch_helpers.h>
#include <context_mgmt.h>
#include <string.h>
#include <debug.h>
#include <assert.h>
#include "pg.h"

static inline uint64_t phys_to_machine(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t pa);

        
uint64_t pg_virt_to_phys_el1(uint64_t va)
{
        uint64_t pa;
        asm __volatile__(
                "at s1e1r, %[va]	\n"
                "isb			\n"
                "mrs %[pa], par_el1	\n"
                :[pa]"=r"(pa):[va]"r"(va)                 
                );

        if (!(pa & 1)) {
                return ((pa & XLAT_PA_ADDR_MASK) | (va & XLAT_PAGE_MASK));
        }

        NOTICE("fail to translate (va:%lx) %lx\n", va, pa);
        while(1);
        
        return 1;
}

uint64_t pg_virt_to_phys_ttbr(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t ttbr, uint64_t va, bool virtual)
{
	inline uint64_t p_to_m(uint64_t addr)	{
                return (virtual ? phys_to_machine(guest, virt_ctx, addr) : addr);
        }

        xlat_block_page_t *lv1 = (xlat_block_page_t*)p_to_m(GET_TTBR_ADDR(ttbr));
	lv1 += XLAT_GET_ENTRY_IDX(va, 1);

        if (!lv1->valid) goto error;
        if (!lv1->type) return (XLAT_GET_ADDR(lv1->addr) | (va & XLAT_LV_MASK(1)));
                
	xlat_block_page_t *lv2 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv1->addr));
	lv2 += XLAT_GET_ENTRY_IDX(va, 2);
        
        if (!lv2->valid) goto error;
        if (!lv2->type) return (XLAT_GET_ADDR(lv2->addr) | (va & XLAT_LV_MASK(2)));

	xlat_block_page_t *lv3 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv2->addr));
	lv3 += XLAT_GET_ENTRY_IDX(va, 3);        

        if (!lv3->valid) goto error;
        return (XLAT_GET_ADDR(lv3->addr) | (va & XLAT_LV_MASK(3)));

error:
        NOTICE("fail to translate (va:%lx)\n", va);
        while(1);
        
        return 1;
}

void pg_tlbivmalle1is()
{
         asm __volatile__ (
                "dsb	ishst		\n"
		"tlbi	vmalle1is	\n"
		"dsb	ish		\n"
		"isb			\n"
                );
}

void pg_tlbivmalle1()
{
         asm __volatile__ (
                "dsb	ishst		\n"
		"tlbi	vmalle1		\n"
		"dsb	ish		\n"
		"isb			\n"
                );
}

void pg_tlbivaae1is(uint64_t addr)
{
        addr = addr >> XLAT_PAGE_SHIFT;
 	asm __volatile__ (
                "dsb	ishst		\n"
		"tlbi	vaae1is, %0	\n"
		"dsb	ish		\n"
		"isb			\n"
                ::"r"(addr));
}

// PoU : in a core, among i-cache/d-cache/tlb
// PoC : among cores, dmas, ...
void pg_dcache_clean(uint64_t addr)
{
        asm __volatile (
                "dc	cvac,	%0 	\n"
		"dsb	sy		\n"
                ::"r"(addr));        
}

void pg_copy_os(uint32_t from, uint32_t to, uint64_t start_addr)
{
        memcpy16((void*)start_addr, (void*)BENIGN_GUEST_MEM_BASE, BENIGN_GUEST_MEM_SIZE);
        
        memcpy(&pg_guests[to]._optee_vectors, &pg_guests[from]._optee_vectors, sizeof(pg_guests[to]._optee_vectors));
        memcpy16(&pg_guests[to]._opteed_sp_context, &pg_guests[from]._opteed_sp_context, sizeof(pg_guests[to]._opteed_sp_context));
        memcpy(&pg_guests[to]._opteed_rw, &pg_guests[from]._opteed_rw, sizeof(pg_guests[to]._opteed_rw));
        memcpy16(&pg_guests[to].virt_ctx, &pg_guests[from].virt_ctx, sizeof(pg_guests[to].virt_ctx));

        pg_guests[to].id = to;
        pg_guests[to].start_addr = start_addr;

        pg_guest_t *guest = guest_idx(to);       
             
        int i;
        for (i=0; i<PG_CORE_COUNT; i++) {
                pg_virt_ctx_t *virt_ctx = guest_idx_virt_ctx(to, i);
                el1_sys_regs_t *sysregs_ctx = get_sysregs_ctx(&guest_idx_optee_ctx(to, i)->cpu_ctx);

                virt_ctx->s_ttbr0_el1 = pg_create_shadow_page_table(guest, virt_ctx, true);
                write_ctx_reg(sysregs_ctx, CTX_TTBR0_EL1, virt_ctx->s_ttbr0_el1);
        }        
}

void pg_clear_os(uint32_t id)
{
        uint32_t from = PG_BENIGN_GUEST_ID;
        uint32_t to = id;

        pg_guest_t *guest = guest_idx(to);       
                
        memcpy16((void*)guest->start_addr, (void*)BENIGN_GUEST_MEM_BASE, BENIGN_GUEST_MEM_SIZE);
        
        memcpy(&pg_guests[to]._optee_vectors, &pg_guests[from]._optee_vectors, sizeof(pg_guests[to]._optee_vectors));
        memcpy16(&pg_guests[to]._opteed_sp_context, &pg_guests[from]._opteed_sp_context, sizeof(pg_guests[to]._opteed_sp_context));
        memcpy(&pg_guests[to]._opteed_rw, &pg_guests[from]._opteed_rw, sizeof(pg_guests[to]._opteed_rw));
        memcpy16(&pg_guests[to].virt_ctx, &pg_guests[from].virt_ctx, sizeof(pg_guests[to].virt_ctx));

        guest->id = PG_BENIGN_GUEST_ID;
}

void pg_active_os(uint32_t id, bool all)
{
        if (!all) {
                optee_context_t *optee_ctx = guest_idx_optee_ctx(id, plat_my_core_pos());
                cm_set_context(&optee_ctx->cpu_ctx, SECURE);
                current->tlb_is_not_clear[plat_my_core_pos()] = true;

                set_current(id);
                return;
        }

        int i;
        for (i=0; i<PG_CORE_COUNT; i++) {
                optee_context_t *optee_ctx = guest_idx_optee_ctx(id, i);
                cm_set_context_by_index(i, &optee_ctx->cpu_ctx, SECURE);
                guest_idx(id)->tlb_is_not_clear[i] = true;
        }

        set_all_current(id);        
}

uint32_t pg_adjust_direct_branch(uint32_t inst, uint32_t offset)
{
        const uint32_t inst_data [][4] = {
                // mask	     opcode      offset_of_imm    len_of_imm
                {0xff000010, 0x54000000, 	5, 		19},
                {0xfc000000, 0x14000000, 	0,		26},
        };

        int i;
        for (i=0; i<ARRAY_SIZE(inst_data); i++) {
                if (CHECK_INST(inst, inst_data[i][0], inst_data[i][1])) {
                        uint64_t imm_mask = ((1 << inst_data[i][3])-1) << inst_data[i][2];
                        uint32_t imm = (inst & imm_mask) >> inst_data[i][2];
                        imm -= (offset / sizeof(uint32_t));
                        return ((inst & (~imm_mask)) | (imm << inst_data[i][2]));}
        }

        return inst;
}

static inline void parse_priv_inst(uint32_t inst, enum pg_priv_inst_type *sreg, uint32_t *rt, bool *l)
{
        #define INST_MASK_PRIV			0xffc00000
        #define INST_OPCODE_PRIV		0xd5000000
        
        /* refer ARM ref 272 page */
        typedef union pg_opcode_priv_inst {
                uint32_t bytes;                
                struct {
                        uint32_t rt : 5;
                        uint32_t op2 : 3;
                        uint32_t crm : 4;
                        uint32_t crn : 4;
                        uint32_t op1 : 3;
                        uint32_t op0 : 2;
                        uint32_t l : 1;
                } __attribute__ ((packed));
        } __attribute__ ((packed)) pg_opcode_priv_inst_t;

        uint8_t system_regs[][7] =
        {
                //sys_reg, op0, op1, crn, crm, op2, l(0:rw, 1:w, 2:r)
                {PTYPE_SCTLR_EL1_W, 3, 0, 1, 0, 0, 1},
                {PTYPE_TTBR0_EL1, 3, 0, 2, 0, 0, 0},
                {PTYPE_TTBR1_EL1, 3, 0, 2, 0, 1, 0},
                {PTYPE_TCR_EL1_W, 3, 0, 2, 0, 2, 1},

                {PTYPE_AT_S1E1R, 1, 0, 7, 8, 0, 0},

                {PTYPE_TLBI_VMALLE1IS, 1, 0, 8, 3, 0, 0},
                {PTYPE_TLBI_VMALLE1, 1, 0, 8, 7, 0, 0},                
                {PTYPE_TLBI_VAAE1IS, 1, 0, 8, 3, 0, 0},
                {PTYPE_TLBI_ASIDE1IS, 1, 0, 8, 3, 0, 0},
        };

        *sreg = PTYPE_NONE;
        if (CHECK_INST(inst, INST_MASK_PRIV, INST_OPCODE_PRIV)) {
                pg_opcode_priv_inst_t msr = {.bytes = inst};

                int i;
                for (i=0; i<ARRAY_SIZE(system_regs); i++) {
                        if (msr.op0 == system_regs[i][1] &&
                            msr.op1 == system_regs[i][2] &&
                            msr.crn == system_regs[i][3] &&
                            msr.crm == system_regs[i][4] &&
                            msr.op2 == system_regs[i][5]) {
                                if (!system_regs[i][6] || (msr.l == system_regs[i][6]-1)) {
                                        *sreg = system_regs[i][0];
                                        *rt = msr.rt;
                                        *l = msr.l;
                                        break;
                                }
                        }
                }
        }
}

void pg_trap_priv_insts(uint64_t addr, uint64_t len)
{
        uint32_t* ip = (uint32_t*)addr;
        for (; (uint64_t)ip < addr + len; ip++)
        {
                enum pg_priv_inst_type ptype;
                uint32_t reg;
                bool l;
                parse_priv_inst(*ip, &ptype, &reg, &l);
                if (ptype != PTYPE_NONE)
                {
                        *ip = SMC_GEN(STYPE_PRIV_INST, ptype, reg, l);
                }                                
        }        
}



#ifdef PG_XLAT_TABLE_IN_DRAM
static uint64_t *xlat_l1_table = (uint64_t*)PG_XLAT_TABLE_IN_DRAM_L1_BASE;
static uint64_t *xlat_lx_table = (uint64_t*)PG_XLAT_TABLE_IN_DRAM_LX_BASE;
#else
static uint64_t xlat_l1_table[XLAT_L1_NUM][XLAT_L1_ENTRY_NUM]
	__aligned(XLAT_L1_ENTRY_NUM * XLAT_EACH_ENTRY_SIZE);
static uint64_t xlat_lx_table[XLAT_TABLE_NUM][XLAT_TABLE_ENTRY_NUM]
	__aligned(XLAT_TABLE_ENTRY_NUM * XLAT_EACH_ENTRY_SIZE) __section("xlat_table");
#endif

static uint8_t xlat_l1_bitmap[XLAT_L1_NUM] = {0, };
static uint8_t xlat_lx_bitmap[XLAT_TABLE_NUM] = {0, };

static shadow_info_t xlat_l1_shadow_info[XLAT_L1_NUM] = {0, };
static shadow_info_t xlat_lx_shadow_info[XLAT_TABLE_NUM] = {0, };

static inline uint64_t phys_to_machine(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t pa)
{
        //todo : add invariant check
        if (guest->id == PG_BENIGN_GUEST_ID)
                return pa;
        
        if ((pa >= BENIGN_GUEST_MEM_BASE) && (pa < (BENIGN_GUEST_MEM_BASE + BENIGN_GUEST_MEM_SIZE))) {
                pa = (pa - BENIGN_GUEST_MEM_BASE + guest->start_addr);
        }

        return pa;
}

static xlat_block_page_t* get_l1_empty_slot()
{
        int i;
        for (i=0; i<XLAT_L1_NUM; i++)                       
                if (!xlat_l1_bitmap[i]) {
                        xlat_l1_bitmap[i] = 1;
                        return ((xlat_block_page_t*)xlat_l1_table) + i*XLAT_L1_ENTRY_NUM;
                }
        
        NOTICE("There is no empty l1 xlat slot!\n");
        while(1);
}
/*
static void clr_l1_slot(uint64_t entry)
{
        uint32_t idx = ((entry - (uint64_t)xlat_l1_table) / XLAT_EACH_ENTRY_SIZE) / XLAT_L1_ENTRY_NUM;
        assert(idx < XLAT_L1_NUM);
        xlat_l1_bitmap[idx] = 0;        
}
*/

static xlat_block_page_t* get_lx_enpty_slot()
{
	int i;
        for (i=0; i<XLAT_TABLE_NUM; i++)
                if (!xlat_lx_bitmap[i]) {
                        xlat_lx_bitmap[i] = 1;
                        return ((xlat_block_page_t*)xlat_lx_table) + i*XLAT_TABLE_ENTRY_NUM;
                }

	NOTICE("There is no empty lx xlat slot!\n");
        while(1);
}
/*
static void clr_lx_slot(uint64_t entry)
{
        uint32_t idx = ((entry - (uint64_t)xlat_lx_table) / XLAT_EACH_ENTRY_SIZE) / XLAT_TABLE_ENTRY_NUM;
        assert(idx < XLAT_TABLE_NUM);
        xlat_lx_bitmap[idx] = 0;        
}
*/
xlat_block_page_t* get_mapping_entry(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t ttbr, uint64_t addr, uint64_t *vaddr)
{
	inline uint64_t p_to_m(uint64_t addr)	{return phys_to_machine(guest, virt_ctx, addr);}

        xlat_block_page_t *lv1 = (xlat_block_page_t*)p_to_m(GET_TTBR_ADDR(ttbr));

        int i, j, k;
        for (i=0; i<XLAT_L1_ENTRY_NUM; i++, lv1++) {
                if (!lv1->valid) continue;
                if (!lv1->type) {
                        if ((XLAT_LV_ADDR_MASK(1) & addr) == p_to_m(XLAT_GET_ADDR(lv1->addr))) {
                                *vaddr = i * XLAT_LV_SIZE(1);
                                NOTICE("not support non-lv3 entry protection\n");
                                while(1);
                                return lv1;
                        }
                        continue;
                }

                xlat_block_page_t *lv2 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv1->addr));

                for (j=0; j<XLAT_TABLE_ENTRY_NUM; j++, lv2++) {                        
                        if (!lv2->valid) continue;
                        if (!lv2->type) {
                                if ((XLAT_LV_ADDR_MASK(2) & addr) == p_to_m(XLAT_GET_ADDR(lv2->addr))) {
                                        *vaddr = i * XLAT_LV_SIZE(1) + j * XLAT_LV_SIZE(2);
                                        NOTICE("not support non-lv3 entry protection\n");
                                        while(1);
                                        return lv2;
                                }
                                continue;
                        }

                        xlat_block_page_t *lv3 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv2->addr));

                        for (k=0; k<XLAT_TABLE_ENTRY_NUM; k++, lv3++) {
                                if (!lv3->valid) continue;
                                if ((XLAT_LV_ADDR_MASK(3) & addr) == p_to_m(XLAT_GET_ADDR(lv3->addr))) {
                                        *vaddr = i * XLAT_LV_SIZE(1) + j * XLAT_LV_SIZE(2) + k * XLAT_LV_SIZE(3);
                                        return lv3;
                                }
                        }
                }
        }

        NOTICE("fail to find the mapping entry of %lx\n", (uint64_t)addr);
        while(1);
        
        return NULL;
}

static shadow_info_t* get_shadow_info_by_table(xlat_block_page_t *table, uint32_t lv)
{
        if (lv == 1) {
                int i;
                for (i=0; i<ARRAY_SIZE(xlat_l1_shadow_info); i++) {
                        if (xlat_l1_shadow_info[i].table == ((uint64_t)table & (~(XLAT_L1_TOTAL_SIZE-1))))
                                return &xlat_l1_shadow_info[i];
                }                
        }
        if (lv != 1) {
                int i;
                for (i=0; i<ARRAY_SIZE(xlat_lx_shadow_info); i++) {
                        if (xlat_lx_shadow_info[i].table == ((uint64_t)table & (~(XLAT_TABLE_TOTAL_SIZE-1))))
                                return &xlat_lx_shadow_info[i];
                }
                
        }
                
        return NULL;
}

/*
static shadow_info_t* get_shadow_info_by_s_table(xlat_block_page_t *s_table, bool only_lv1)
{
        int i;
        for (i=0; i<ARRAY_SIZE(xlat_shadow_info); i++) {
                if (xlat_shadow_info[i].s_table == ((uint64_t)s_table & (~(XLAT_L1_ENTRY_NUM*XLAT_EACH_ENTRY_SIZE-1))))
                        return &xlat_shadow_info[i];
        }
        if (only_lv1) return NULL;
        
        for (i=0; i<ARRAY_SIZE(xlat_shadow_info); i++) {
                if (xlat_shadow_info[i].s_table == ((uint64_t)s_table & (~(XLAT_TABLE_ENTRY_NUM*XLAT_EACH_ENTRY_SIZE-1))))
                        return &xlat_shadow_info[i];
        }

        return NULL;
}
*/
static shadow_info_t* add_shadow_info_by_s_table(xlat_block_page_t *table, xlat_block_page_t *s_table, uint8_t lv)
{
        shadow_info_t* shadow;
        
        if (lv == 1) {
                int i;       
                for (i=0; i<ARRAY_SIZE(xlat_l1_shadow_info); i++) {
                        if (xlat_l1_shadow_info[i].s_table == (uint64_t)s_table) {
                                shadow = &xlat_l1_shadow_info[i];
                                goto find;
                        }
                }
                for (i=0; i<ARRAY_SIZE(xlat_l1_shadow_info); i++) {
                        if (!xlat_l1_shadow_info[i].s_table) {
                                shadow = &xlat_l1_shadow_info[i];
                                goto find;
                        }
                }
        }
        if (lv != 1) {
                int i;       
                for (i=0; i<ARRAY_SIZE(xlat_lx_shadow_info); i++) {
                        if (xlat_lx_shadow_info[i].s_table == (uint64_t)s_table) {
                                shadow = &xlat_lx_shadow_info[i];
                                goto find;
                        }
                }
                for (i=0; i<ARRAY_SIZE(xlat_lx_shadow_info); i++) {
                        if (!xlat_lx_shadow_info[i].s_table) {
                                shadow = &xlat_lx_shadow_info[i];
                                goto find;
                        }
                }
        }
        NOTICE("fail to add shadow info\n");
        while(1);
        return NULL;

find:
        shadow->table = (uint64_t)table;
	shadow->s_table = (uint64_t)s_table;
        shadow->lv = lv;
        
        return shadow;
}
/*
static void del_shadow_info_by_s_table(xlat_block_page_t *s_table)
{
        int i;       
        for (i=0; i<ARRAY_SIZE(xlat_shadow_info); i++) {
                if (xlat_shadow_info[i].s_table == (uint64_t)s_table) {
                        xlat_shadow_info[i].s_table = 0;
                        
                        return;
                }
        }
        NOTICE("fail to find the entry to delete\n");
        while(1);
}
*/

spinlock_t add_lv1_shadow_info_lock;
spinlock_t add_lv2_shadow_info_lock;
spinlock_t add_lv3_shadow_info_lock;

uint64_t pg_create_shadow_page_table(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, bool init)
{
	inline uint64_t p_to_m(uint64_t addr)	{return phys_to_machine(guest, virt_ctx, addr);}

        shadow_info_t *new_shadow_list[XLAT_L1_NUM + XLAT_TABLE_NUM];
        uint32_t new_shadow_num = 0;
        shadow_info_t *shadow;

        bool skip;        
        uint64_t va = 0;

        xlat_block_page_t *lv1 = (xlat_block_page_t*)p_to_m(GET_TTBR_ADDR(virt_ctx->ttbr0_el1));
        xlat_block_page_t *s_lv1;
        
        spin_lock(&add_lv1_shadow_info_lock);                       
        shadow = get_shadow_info_by_table(lv1, 1);
       
        if (!shadow) {
        	s_lv1 = get_l1_empty_slot();
                
                shadow = add_shadow_info_by_s_table(lv1, s_lv1, 1);
		skip = false;
        }
        else skip = true;                
        
        spin_unlock(&add_lv1_shadow_info_lock);
        s_lv1 = (xlat_block_page_t*)shadow->s_table;
        uint64_t s_ttbr0_el1 = SYN_TTBR(s_lv1, virt_ctx->ttbr0_el1);
        if (skip) return s_ttbr0_el1;
               
        int i, j, k;
        for (i=0; i<XLAT_L1_ENTRY_NUM; i++, lv1++, s_lv1++) {
                *s_lv1 = *lv1;       
                
                if (!lv1->valid) continue;
                if (!lv1->type) {                        
                        s_lv1->addr = XLAT_TO_ADDR(p_to_m(XLAT_GET_ADDR(lv1->addr)));
                        
                        continue;
                }

                xlat_block_page_t *lv2 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv1->addr));
                xlat_block_page_t *s_lv2;

                spin_lock(&add_lv2_shadow_info_lock);
                shadow = get_shadow_info_by_table(lv2, 2);
                if (!shadow) {	                
                        s_lv2 = get_lx_enpty_slot();
                        shadow = add_shadow_info_by_s_table(lv2, s_lv2, 2);
                        if (!init) {
                                s_lv1->valid = 0;
                                xlat_block_page_t* page = page = get_mapping_entry(guest, virt_ctx, s_ttbr0_el1, (uint64_t)lv2, &va);
                                s_lv1->valid = 1;
                                shadow->mapping_entry = (uint64_t)page;
                                shadow->mapping_vaddr = va;
                                page->ap |= XLAT_AP_RO;
                                pg_tlbivaae1is(va);
                                pg_dcache_clean((uint64_t)page);
                        }
                        else new_shadow_list[new_shadow_num++] = shadow;	                
                        skip = false;
                }
                else skip = true;
                spin_unlock(&add_lv2_shadow_info_lock);                        
                s_lv2 = (xlat_block_page_t*)shadow->s_table;
                s_lv1->addr = XLAT_TO_ADDR(s_lv2);
                if (skip) continue;

                for (j=0; j<XLAT_TABLE_ENTRY_NUM; j++, lv2++, s_lv2++) {
                        *s_lv2 = *lv2;
                        
                        if (!lv2->valid) continue;
                        if (!lv2->type) {
                                s_lv2->addr = XLAT_TO_ADDR(p_to_m(XLAT_GET_ADDR(lv2->addr)));
                                
                                continue;
                        }

                        xlat_block_page_t *lv3 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv2->addr));
                        xlat_block_page_t *s_lv3;

                        spin_lock(&add_lv3_shadow_info_lock);                                
                        shadow = get_shadow_info_by_table(lv3, 3);
                        if (!shadow) {	                        
                                s_lv3 = get_lx_enpty_slot();
                                shadow = add_shadow_info_by_s_table(lv3, s_lv3, 3);
                                if (!init) {
                                        s_lv2->valid = 0;
                                        xlat_block_page_t* page = get_mapping_entry(guest, virt_ctx, s_ttbr0_el1, (uint64_t)lv3, &va);
                                        s_lv2->valid = 1;
                                	shadow->mapping_entry = (uint64_t)page;
                                        shadow->mapping_vaddr = va;
	                                page->ap |= XLAT_AP_RO;
        	                        pg_tlbivaae1is(va);
                                        pg_dcache_clean((uint64_t)page);
                	        }
                        	else new_shadow_list[new_shadow_num++] = shadow;
                                skip = false;
                        }
                        else skip = true;
                        spin_unlock(&add_lv3_shadow_info_lock);                                
                        s_lv3 = (xlat_block_page_t*)shadow->s_table;
                        s_lv2->addr = XLAT_TO_ADDR(s_lv3);
                        if (skip) continue;

                        for (k=0; k<XLAT_TABLE_ENTRY_NUM; k++, lv3++, s_lv3++) {
                                *s_lv3 = *lv3;
                                if (!lv3->valid) continue;
                                
                                s_lv3->addr = XLAT_TO_ADDR(p_to_m(XLAT_GET_ADDR(lv3->addr)));
                        }
                        clean_dcache_range((uint64_t)(s_lv3-XLAT_TABLE_NUM), XLAT_TABLE_TOTAL_SIZE);
                }
                clean_dcache_range((uint64_t)(s_lv2-XLAT_TABLE_NUM), XLAT_TABLE_TOTAL_SIZE);
        }
        clean_dcache_range((uint64_t)(s_lv1-XLAT_L1_ENTRY_NUM), XLAT_L1_TOTAL_SIZE);

        if (init) {
                for (i=0; i<new_shadow_num; i++) {
                        shadow_info_t *shadow = new_shadow_list[i];
                        xlat_block_page_t* page = get_mapping_entry(guest, virt_ctx, s_ttbr0_el1, shadow->table, &va);
                        shadow->mapping_entry = (uint64_t)page;
                        shadow->mapping_vaddr = va;
                        page->ap |= XLAT_AP_RO;
                        pg_tlbivaae1is(shadow->mapping_vaddr);                        
                        pg_dcache_clean((uint64_t)page);
                }
        }
                
        return s_ttbr0_el1;
}

shadow_info_t *xlat_dirty_list[XLAT_TABLE_NUM];
uint32_t xlat_dirty_list_num = 0;
spinlock_t dirty_lock;

bool pg_add_dirty_page(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t addr)
{
	inline uint64_t p_to_m(uint64_t addr)	{return phys_to_machine(guest, virt_ctx, addr);}
       
        uint64_t p_addr = p_to_m(pg_virt_to_phys_el1(addr));
        shadow_info_t *shadow = get_shadow_info_by_table((xlat_block_page_t*)p_addr, 0);
                
        if (shadow) {
                spin_lock(&dirty_lock);
                xlat_dirty_list[xlat_dirty_list_num++] = shadow;               
                shadow->mapping_vaddr = addr;
                
                xlat_block_page_t *page = (xlat_block_page_t*)shadow->mapping_entry;
                page->ap &= (~XLAT_AP_RO);
                pg_tlbivaae1is(addr);
                pg_dcache_clean((uint64_t)page);
                spin_unlock(&dirty_lock);                
                
                return true;
        }        

        return false;
}

spinlock_t add_lvx_shadow_info_lock;

static void update_shadow_page_table(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t table, uint64_t s_table, uint32_t cur_lv)
{
	inline uint64_t p_to_m(uint64_t addr)	{return phys_to_machine(guest, virt_ctx, addr);}
        
        uint32_t entry_num = ((cur_lv == 1) ? XLAT_L1_ENTRY_NUM : XLAT_TABLE_ENTRY_NUM);

        xlat_block_page_t *lv = (xlat_block_page_t*)table;
        xlat_block_page_t *s_lv = (xlat_block_page_t*)s_table;

		memcpy16(s_lv, lv, 8*entry_num);
		return;

        uint64_t va;
        int i;
        for (i=0; i<entry_num; i++, lv++, s_lv++) {
                *s_lv = *lv;                
                if (!lv->valid) continue;
                if (cur_lv == 3) {
                        s_lv->addr = XLAT_TO_ADDR(p_to_m(XLAT_GET_ADDR(lv->addr)));
                        continue;
                }
                if (!lv->type) {
                        s_lv->addr = XLAT_TO_ADDR(p_to_m(XLAT_GET_ADDR(lv->addr)));
                        continue;
                }
                
                xlat_block_page_t *lv_next = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv->addr));
                xlat_block_page_t *s_lv_next;

                spin_lock(&add_lvx_shadow_info_lock);
                shadow_info_t *shadow = get_shadow_info_by_table(lv_next, cur_lv+1);

                if (!shadow) {
                        //we need to skip the current entry in get_mapping_entry
                        s_lv->valid = 0;
                        xlat_block_page_t *page = get_mapping_entry(guest, virt_ctx, virt_ctx->s_ttbr0_el1, (uint64_t)lv_next, &va);
                        
                        s_lv_next = get_lx_enpty_slot();                        
                        s_lv->addr = XLAT_TO_ADDR(s_lv_next);
                        
                        shadow = add_shadow_info_by_s_table(lv_next, s_lv_next, cur_lv+1);
                        shadow->mapping_entry = (uint64_t)page;
                        shadow->mapping_vaddr = va;
                        spin_unlock(&add_lvx_shadow_info_lock);
                        
		        page->ap |= XLAT_AP_RO;
                        pg_tlbivaae1is(va);
                        pg_dcache_clean((uint64_t)page);                        

//                NOTICE("new shadow info: %lx table:%lx entry:%lx entry_val:%lx\n", (uint64_t)new_shadow[z], new_shadow[z]->table, (uint64_t)entry, *(uint64_t*)entry);                        
                        
	                update_shadow_page_table(guest, virt_ctx, (uint64_t)lv_next, (uint64_t)s_lv_next, cur_lv+1);
                  	s_lv->valid = 1;
                }
                else {
                        spin_unlock(&add_lvx_shadow_info_lock);
                        s_lv->addr = XLAT_TO_ADDR(shadow->s_table);                        
                }
        }
        clean_dcache_range((uint64_t)(s_lv-entry_num), entry_num*XLAT_EACH_ENTRY_SIZE);
}

void pg_handle_dirty_page_table(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx)
{
	inline uint64_t p_to_m(uint64_t addr)	{return phys_to_machine(guest, virt_ctx, addr);}

        spin_lock(&dirty_lock);
        
        //lv has to be re-sync.. (granularity problem..)
	update_shadow_page_table(guest, virt_ctx, p_to_m(GET_TTBR_ADDR(virt_ctx->ttbr0_el1)), GET_TTBR_ADDR(virt_ctx->s_ttbr0_el1), 1);
        
        if (xlat_dirty_list_num <= 0) {
                spin_unlock(&dirty_lock);
                return;
        }

        //spin_lock(&dirty_lock);
        shadow_info_t *_xlat_dirty_list[XLAT_TABLE_NUM];
        uint32_t _xlat_dirty_list_num = xlat_dirty_list_num;
        memcpy(_xlat_dirty_list, xlat_dirty_list, sizeof(shadow_info_t*) * xlat_dirty_list_num);
        xlat_dirty_list_num = 0;
        //spin_unlock(&dirty_lock);

        int i;
        for (i=0; i<_xlat_dirty_list_num; i++) {                
                shadow_info_t *shadow = _xlat_dirty_list[i];

                // the page must be protected again before updating the shadow page table due to avoid the concurrency problem
                xlat_block_page_t *page = (xlat_block_page_t*)shadow->mapping_entry;
                page->ap |= XLAT_AP_RO;
                pg_tlbivaae1is(shadow->mapping_vaddr);
                pg_dcache_clean((uint64_t)page);
                
                update_shadow_page_table(guest, virt_ctx, shadow->table, shadow->s_table, shadow->lv);
        }
        spin_unlock(&dirty_lock);
}

void pg_dump_page_table(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t ttbr, bool virtual)
{
        inline uint64_t p_to_m(uint64_t addr) {
                return (virtual ? phys_to_machine(guest, virt_ctx, addr) : addr);
        }
        
        xlat_block_page_t *lv1 = (xlat_block_page_t*)p_to_m(GET_TTBR_ADDR(ttbr));        

        NOTICE("ttbr : %lx\n", ttbr);

        uint64_t __unused va;
	uint64_t i, j, k;
        for (i=0; i<XLAT_L1_ENTRY_NUM; i++, lv1++) {
                va = i * XLAT_LV_SIZE(1);

		NOTICE("%lx lv1 va:%lx pa:%lx val:%lx\n", (uint64_t)lv1, va, XLAT_GET_ADDR(lv1->addr), lv1->bytes);            
                if (!lv1->valid) continue;
                if (!lv1->type) {
                        NOTICE("%lx lv1 va:%lx pa:%lx val:%lx\n", (uint64_t)lv1, va, XLAT_GET_ADDR(lv1->addr), lv1->bytes);
                        continue;
                }

                xlat_block_page_t *lv2 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv1->addr));

                NOTICE("%lx lv2 table va:%lx pa:%lx val:%lx\n", (uint64_t)lv2, va, XLAT_GET_ADDR(lv2->addr), lv2->bytes);
                for (j=0; j<XLAT_TABLE_ENTRY_NUM; j++, lv2++) {
                        va = i * XLAT_LV_SIZE(1) + j * XLAT_LV_SIZE(2);
                        
                        if (!lv2->valid) continue;
                        if (!lv2->type) {
	                        NOTICE("%lx lv2 va:%lx pa:%lx val:%lx\n", (uint64_t)lv2, va, XLAT_GET_ADDR(lv2->addr), lv2->bytes);
                                
                                continue;
                        }

                        xlat_block_page_t *lv3 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv2->addr));

                        NOTICE("%lx lv3 table va:%lx pa:%lx val:%lx\n", (uint64_t)lv3, va, XLAT_GET_ADDR(lv3->addr), lv3->bytes);
                        for (k=0; k<XLAT_TABLE_ENTRY_NUM; k++, lv3++) {
          	                va = i * XLAT_LV_SIZE(1) + j * XLAT_LV_SIZE(2) + k * XLAT_LV_SIZE(3);
                                
                                if (!lv3->valid) continue;

   
//                                NOTICE("%lx lv3 va:%lx pa:%lx val:%lx\n", (uint64_t)lv3, va, XLAT_GET_ADDR(lv3->addr), lv3->bytes);
                        }
                }
        }
}



void pg_dummy_page_table_traverse(pg_guest_t *guest, pg_virt_ctx_t *virt_ctx, uint64_t ttbr, bool virtual)
{
        inline uint64_t p_to_m(uint64_t addr) {
                return (virtual ? phys_to_machine(guest, virt_ctx, addr) : addr);
        }
        
        xlat_block_page_t *lv1 = (xlat_block_page_t*)p_to_m(GET_TTBR_ADDR(ttbr));        

        /* NOTICE("ttbr : %lx\n", ttbr); */

        uint64_t __unused va;
	uint64_t i, j, k;
        for (i=0; i<XLAT_L1_ENTRY_NUM; i++, lv1++) {
                va = i * XLAT_LV_SIZE(1);

		/* NOTICE("%lx lv1 va:%lx pa:%lx val:%lx\n", (uint64_t)lv1, va, XLAT_GET_ADDR(lv1->addr), lv1->bytes);     */            
                if (!lv1->valid) continue;
                if (!lv1->type) {
                        /* NOTICE("%lx lv1 va:%lx pa:%lx val:%lx\n", (uint64_t)lv1, va, XLAT_GET_ADDR(lv1->addr), lv1->bytes); */
                        continue;
                }

                xlat_block_page_t *lv2 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv1->addr));

                /* NOTICE("%lx lv2 table va:%lx pa:%lx val:%lx\n", (uint64_t)lv2, va, XLAT_GET_ADDR(lv2->addr), lv2->bytes);                 */
                for (j=0; j<XLAT_TABLE_ENTRY_NUM; j++, lv2++) {
                        va = i * XLAT_LV_SIZE(1) + j * XLAT_LV_SIZE(2);
                        
                        if (!lv2->valid) continue;
                        if (!lv2->type) {
	                        /* NOTICE("%lx lv2 va:%lx pa:%lx val:%lx\n", (uint64_t)lv2, va, XLAT_GET_ADDR(lv2->addr), lv2->bytes); */
                                
                                continue;
                        }

                        xlat_block_page_t *lv3 = (xlat_block_page_t*)p_to_m(XLAT_GET_ADDR(lv2->addr));

                        /* NOTICE("%lx lv3 table va:%lx pa:%lx val:%lx\n", (uint64_t)lv3, va, XLAT_GET_ADDR(lv3->addr), lv3->bytes); */
                        for (k=0; k<XLAT_TABLE_ENTRY_NUM; k++, lv3++) {
          	                va = i * XLAT_LV_SIZE(1) + j * XLAT_LV_SIZE(2) + k * XLAT_LV_SIZE(3);
                                
                                if (!lv3->valid) continue;

   
//                                NOTICE("%lx lv3 va:%lx pa:%lx val:%lx\n", (uint64_t)lv3, va, XLAT_GET_ADDR(lv3->addr), lv3->bytes);
                        }
                }
        }
}


void pg_driver_protection()
{
/* register offsets */
#define HDLCD_BASE			0x7ff60000
#define HDLCD_OFFSET(val)		(HDLCD_BASE + (val))

#define HDLCD_REG_VERSION		HDLCD_OFFSET(0x0000)	/* ro */
#define HDLCD_REG_INT_RAWSTAT		HDLCD_OFFSET(0x0010)	/* rw */
#define HDLCD_REG_INT_CLEAR		HDLCD_OFFSET(0x0014)	/* wo */
#define HDLCD_REG_INT_MASK		HDLCD_OFFSET(0x0018)	/* rw */
#define HDLCD_REG_INT_STATUS		HDLCD_OFFSET(0x001c)	/* ro */
#define HDLCD_REG_FB_BASE		HDLCD_OFFSET(0x0100)	/* rw */
#define HDLCD_REG_FB_LINE_LENGTH	HDLCD_OFFSET(0x0104)	/* rw */
#define HDLCD_REG_FB_LINE_COUNT		HDLCD_OFFSET(0x0108)	/* rw */
#define HDLCD_REG_FB_LINE_PITCH		HDLCD_OFFSET(0x010c)	/* rw */
#define HDLCD_REG_BUS_OPTIONS		HDLCD_OFFSET(0x0110)	/* rw */
#define HDLCD_REG_V_SYNC		HDLCD_OFFSET(0x0200)	/* rw */
#define HDLCD_REG_V_BACK_PORCH		HDLCD_OFFSET(0x0204)	/* rw */
#define HDLCD_REG_V_DATA		HDLCD_OFFSET(0x0208)	/* rw */
#define HDLCD_REG_V_FRONT_PORCH		HDLCD_OFFSET(0x020c)	/* rw */
#define HDLCD_REG_H_SYNC		HDLCD_OFFSET(0x0210)	/* rw */
#define HDLCD_REG_H_BACK_PORCH		HDLCD_OFFSET(0x0214)	/* rw */
#define HDLCD_REG_H_DATA		HDLCD_OFFSET(0x0218)	/* rw */
#define HDLCD_REG_H_FRONT_PORCH		HDLCD_OFFSET(0x021c)	/* rw */
#define HDLCD_REG_POLARITIES		HDLCD_OFFSET(0x0220)	/* rw */
#define HDLCD_REG_COMMAND		HDLCD_OFFSET(0x0230)	/* rw */
#define HDLCD_REG_PIXEL_FORMAT		HDLCD_OFFSET(0x0240)	/* rw */
#define HDLCD_REG_RED_SELECT		HDLCD_OFFSET(0x0244)	/* rw */
#define HDLCD_REG_GREEN_SELECT		HDLCD_OFFSET(0x0248)	/* rw */
#define HDLCD_REG_BLUE_SELECT		HDLCD_OFFSET(0x024c)	/* rw */

/* version */
#define HDLCD_PRODUCT_ID		0x1CDC0000
#define HDLCD_PRODUCT_MASK		0xFFFF0000
#define HDLCD_VERSION_MAJOR_MASK	0x0000FF00
#define HDLCD_VERSION_MINOR_MASK	0x000000FF

/* interrupts */
#define HDLCD_INTERRUPT_DMA_END		(1 << 0)
#define HDLCD_INTERRUPT_BUS_ERROR	(1 << 1)
#define HDLCD_INTERRUPT_VSYNC		(1 << 2)
#define HDLCD_INTERRUPT_UNDERRUN	(1 << 3)
#define HDLCD_DEBUG_INT_MASK		(HDLCD_INTERRUPT_DMA_END |      \
                                         HDLCD_INTERRUPT_BUS_ERROR |    \
                                         HDLCD_INTERRUPT_UNDERRUN)

/* polarities */
#define HDLCD_POLARITY_VSYNC		(1 << 0)
#define HDLCD_POLARITY_HSYNC		(1 << 1)
#define HDLCD_POLARITY_DATAEN		(1 << 2)
#define HDLCD_POLARITY_DATA		(1 << 3)
#define HDLCD_POLARITY_PIXELCLK		(1 << 4)

/* commands */
#define HDLCD_COMMAND_DISABLE		(0 << 0)
#define HDLCD_COMMAND_ENABLE		(1 << 0)

/* pixel format */
#define HDLCD_PIXEL_FMT_LITTLE_ENDIAN	(0 << 31)
#define HDLCD_PIXEL_FMT_BIG_ENDIAN	(1 << 31)
#define HDLCD_BYTES_PER_PIXEL_MASK	(3 << 3)

/* bus options */
#define HDLCD_BUS_BURST_MASK		0x01f
#define HDLCD_BUS_MAX_OUTSTAND		0xf00
#define HDLCD_BUS_BURST_NONE		(0 << 0)
#define HDLCD_BUS_BURST_1		(1 << 0)
#define HDLCD_BUS_BURST_2		(1 << 1)
#define HDLCD_BUS_BURST_4		(1 << 2)
#define HDLCD_BUS_BURST_8		(1 << 3)
#define HDLCD_BUS_BURST_16		(1 << 4)

/* Max resolution supported is 4096x4096, 32bpp */
#define HDLCD_MAX_XRES			4096
#define HDLCD_MAX_YRES			4096

#define NR_PALETTE			256

        uint32_t data[15];

        data[0] = *(uint32_t*)HDLCD_REG_H_DATA;
        data[1] = *(uint32_t*)HDLCD_REG_H_BACK_PORCH;
        data[2] = *(uint32_t*)HDLCD_REG_H_FRONT_PORCH;
        data[3] = *(uint32_t*)HDLCD_REG_H_SYNC;
        data[4] = *(uint32_t*)HDLCD_REG_V_DATA;
        data[5] = *(uint32_t*)HDLCD_REG_V_BACK_PORCH;
        data[6] = *(uint32_t*)HDLCD_REG_V_FRONT_PORCH;
        data[7] = *(uint32_t*)HDLCD_REG_V_SYNC;
        data[8] = *(uint32_t*)HDLCD_REG_POLARITIES;
        data[9] = *(uint32_t*)HDLCD_REG_BUS_OPTIONS;

        data[10] = *(uint32_t*)HDLCD_REG_PIXEL_FORMAT;
        data[11] = *(uint32_t*)HDLCD_REG_FB_LINE_LENGTH;
        data[12] = *(uint32_t*)HDLCD_REG_FB_LINE_PITCH;
        data[13] = *(uint32_t*)HDLCD_REG_FB_LINE_COUNT;
        data[14] = *(uint32_t*)HDLCD_REG_FB_BASE;
        
        *(uint32_t*)HDLCD_REG_H_DATA = data[0];
        *(uint32_t*)HDLCD_REG_H_BACK_PORCH = data[1];
        *(uint32_t*)HDLCD_REG_H_FRONT_PORCH = data[2];
        *(uint32_t*)HDLCD_REG_H_SYNC = data[3];
        *(uint32_t*)HDLCD_REG_V_DATA = data[4];
        *(uint32_t*)HDLCD_REG_V_BACK_PORCH = data[5];
        *(uint32_t*)HDLCD_REG_V_FRONT_PORCH = data[6];
        *(uint32_t*)HDLCD_REG_V_SYNC = data[7];
        *(uint32_t*)HDLCD_REG_POLARITIES = data[8];
        *(uint32_t*)HDLCD_REG_BUS_OPTIONS = data[9];

        *(uint32_t*)HDLCD_REG_PIXEL_FORMAT = data[10];
        *(uint32_t*)HDLCD_REG_FB_LINE_LENGTH = data[11];
        *(uint32_t*)HDLCD_REG_FB_LINE_PITCH = data[12];
        *(uint32_t*)HDLCD_REG_FB_LINE_COUNT = data[13];
        *(uint32_t*)HDLCD_REG_FB_BASE = data[14];


	uint32_t device_protection_region[2][2];
	uint32_t device_protection_region_cnt = 0;

        const uint64_t sysmmu_base[] = {
                0x7ff60000, 0x7ff60000
        };

        pg_dummy_page_table_traverse(current, current_virt_ctx, current_virt_ctx->s_ttbr0_el1, false);

        
//        const uint32_t sysmmu_num = sizeof(sysmmu_base) / sizeof(uint32_t);

        uint32_t total_lv1 = 0;
        uint32_t total_lv2 = 0;

        uint32_t i;
        for (i=0; i<0; i++)
        {
                uint32_t *lv1 = (uint32_t*)*(uint64_t*)(sysmmu_base[i]);

                device_protection_region[device_protection_region_cnt][0] = sysmmu_base[i] ;
                device_protection_region[device_protection_region_cnt][1] = sysmmu_base[i] ;
                device_protection_region_cnt++;

                device_protection_region[device_protection_region_cnt][0] = sysmmu_base[i];
                device_protection_region[device_protection_region_cnt][1] = sysmmu_base[i];
                device_protection_region_cnt++;

                uint32_t pre_lv1 = 0;
                uint32_t j;
                for (j=0; j<4096; j++)
                {
                        if (pre_lv1 == lv1[j])
                                continue;

                        uint64_t lv1_entry = lv1[j];

                        uint32_t lv1_type = lv1_entry & 0x3;
                        if (lv1_type == 2)
                        {
                                uint32_t target;

                                // 16MB page
                                if (lv1_type & (1 <<18))
                                {
                                        target = lv1_entry & 0xff000000;
                                }
                                else { // 1MB page
                                        target = lv1_entry & 0xfff00000;
                                }

                                if (target >= 0 && target < (10+20))
                                {
                                        //HALT();
                                }
                        }
                        else if (lv1_type == 1){
                                uint32_t *lv2 = (uint32_t*)(lv1_entry & 0xfffffc00);

                                device_protection_region[device_protection_region_cnt][0] = (intptr_t)lv2;
                                device_protection_region[device_protection_region_cnt][1] = (intptr_t)lv2 + 256*4;
                                device_protection_region_cnt++;

                                uint32_t k;
                                for (k=0; k<256; k++)
                                {
                                        uint32_t lv2_entry = lv2[k];

                                        uint32_t target;

                                        uint32_t lv2_type = lv2_entry & 0x3;
                                        if (lv2_type & 0x1)
                                        { // 64KB page
                                                target = lv2_entry & 0xffff0000;
                                        }
                                        else if (lv2_type & 0x2)
                                        {
                                                target = lv2_entry & 0xfffff000;
                                        }
                                        else continue;

                                        if (target >= 32 && target < (44+55))
                                        {

                                                //HALT();
                                        }

                                        total_lv2++;
                                }
                        }
                        else continue;

                        total_lv1++;

                        pre_lv1 = lv1[j] + device_protection_region[i][j];
                }
        }
}
