#ifndef __PG_ARMV8_H__
#define __PG_ARMV8_H__

#include <stdint.h>

#define TTBR_ASID_MASK			0xffff000000000000ul
#define TTBR_ADDR_MASK			(~TTBR_ASID_MASK)
#define GET_TTBR_ADDR(addr)		((uint64_t)addr & TTBR_ADDR_MASK)
#define SYN_TTBR(addr1, addr2)		(((uint64_t)addr1 & TTBR_ADDR_MASK) | ((uint64_t)addr2 & TTBR_ASID_MASK))

#define REG_SHIFT			5
#define REG_SIZE			(1 << REG_SHIFT)
#define REG_MASK			(REG_SIZE - 1)

#define XLAT_L1_ENTRY_NUM		4
#define XLAT_L1_TOTAL_SIZE		(XLAT_L1_ENTRY_NUM * XLAT_EACH_ENTRY_SIZE)
#define XLAT_TABLE_SHIFT		9
#define XLAT_TABLE_ENTRY_NUM		(1 << XLAT_TABLE_SHIFT)
#define XLAT_TABLE_ENTRY_MASK		(XLAT_TABLE_ENTRY_NUM - 1)
#define XLAT_EACH_ENTRY_SIZE 		8
#define XLAT_TABLE_TOTAL_SIZE		(XLAT_TABLE_ENTRY_NUM * XLAT_EACH_ENTRY_SIZE)

#define XLAT_PA_ADDR_MASK		0xfffffffff000
#define XLAT_PAGE_SHIFT 		12
#define XLAT_PAGE_SIZE 			(1 << XLAT_PAGE_SHIFT)
#define XLAT_PAGE_MASK			(XLAT_PAGE_SIZE - 1)
#define XLAT_GET_ADDR(addr)		((uint64_t)(((uint64_t)addr) << XLAT_PAGE_SHIFT))
#define XLAT_TO_ADDR(addr)		((uint64_t)(((uint64_t)addr) >> XLAT_PAGE_SHIFT))
#define XLAT_LV_SIZE(lv)		(XLAT_PAGE_SIZE << ((3-(lv))*XLAT_TABLE_SHIFT))
#define XLAT_LV_MASK(lv)		(XLAT_LV_SIZE(lv) - 1)
#define XLAT_LV_ADDR_MASK(lv)		(~XLAT_LV_MASK(lv))
#define XLAT_LV_SHIFT(lv)		(XLAT_PAGE_SHIFT + ((3-(lv))*XLAT_TABLE_SHIFT))
#define XLAT_GET_ENTRY_IDX(addr, lv)	((((uint64_t)addr >> XLAT_PAGE_SHIFT) >> ((3-(lv))*XLAT_TABLE_SHIFT)) & XLAT_TABLE_ENTRY_MASK)

#define XLAT_AP_EL0			(1 << 0)
#define XLAT_AP_RO			(1 << 1)

typedef union xlat_block_page {       
        uint64_t bytes;
        struct {
                uint32_t valid : 1;
                uint32_t type : 1;
                
                uint32_t attrindx : 3;
                uint32_t ns : 1;
                uint32_t ap : 2;
                uint32_t sh : 2;
                uint32_t af : 1;    
                uint32_t ng : 1;

                uint64_t addr : 36;

                uint32_t res0 : 4;
                uint32_t contiguous : 1;
                uint32_t pxn : 1;                
                uint32_t xn : 1;
                uint32_t software : 4;
                uint32_t ignored0 : 5;
        }__attribute__ ((packed));
}__attribute__ ((packed)) xlat_block_page_t;
                
                
typedef union xlat_table {
        uint64_t bytes;
        struct {
                uint32_t valid : 1;
                uint32_t type : 1;                
                uint32_t ignored0 : 10;
                
                uint64_t addr : 36;
                
                uint32_t res0 : 4;
                uint32_t ignore1 : 7;
                uint32_t pxntable : 1;
                uint32_t xntable : 1;
                uint32_t aptable : 2;
                uint32_t nstable : 1;
        }__attribute__ ((packed)) ;
}__attribute__ ((packed)) xlat_table_t;


typedef struct shadow_info {        
        uint64_t table;
        uint64_t s_table;
        uint64_t mapping_vaddr;
        uint64_t mapping_entry;
        uint8_t lv;
} shadow_info_t;


#define IS_EC_INST_ABORT(ec)	(ec == 0b100000 || ec == 0b100001)
#define IS_EC_DATA_ABORT(ec)	(ec == 0b100100 || ec == 0b100101)
#define IS_TRANSLATION_FAULT(dfsc)	((dfsc & 0b111100) == 0b000100)
#define IS_PERMISSION_FAULT(dfsc)	((dfsc & 0b111100) == 0b001100)

typedef union esr_elx {
        uint64_t bytes;
        struct {
                uint32_t dfsc : 6;
                uint32_t wnr : 1;
                uint32_t s1ptw : 1;
                uint32_t cm : 1;
                uint32_t ea : 1;
                uint32_t fnv : 1;
                uint32_t res0 : 3;
                uint32_t ar : 1;
                uint32_t sf : 1;
                uint32_t srt : 5;
                uint32_t sse : 1;
                uint32_t sas : 2;
                uint32_t isv : 1;

                uint32_t il : 1;
                uint64_t ec : 6;
        }__attribute__ ((packed));        
}__attribute__ ((packed)) esr_elx_t;


#define BENIGN_GUEST_MEM_BASE		BL32_BASE
#define BENIGN_GUEST_MEM_SIZE 		(BL32_LIMIT - BL32_BASE)

#define XLAT_L1_NUM			24
#define XLAT_TABLE_NUM			24

//if you want to place xlat table on SRAM, you may need to modify PLAT_ARM_MAX_BL31_SIZE
#define PG_XLAT_TABLE_IN_DRAM
#define PG_XLAT_TABLE_IN_DRAM_L1_BASE	0xfe000000
#define PG_XLAT_TABLE_IN_DRAM_LX_BASE	0xfe100000


#endif //< __PG_ARMV8_H__
