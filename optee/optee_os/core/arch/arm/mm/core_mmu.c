/*
 * Copyright (c) 2016, Linaro Limited
 * Copyright (c) 2014, STMicroelectronics International N.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <arm.h>
#include <assert.h>
#include <kernel/cache_helpers.h>
#include <kernel/generic_boot.h>
#include <kernel/linker.h>
#include <kernel/panic.h>
#include <kernel/tlb_helpers.h>
#include <kernel/tee_l2cc_mutex.h>
#include <kernel/tee_misc.h>
#include <kernel/tee_ta_manager.h>
#include <kernel/thread.h>
#include <kernel/tz_ssvce_pl310.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <mm/mobj.h>
#include <mm/pgt_cache.h>
#include <mm/tee_mmu.h>
#include <mm/tee_pager.h>
#include <platform_config.h>
#include <stdlib.h>
#include <trace.h>
#include <util.h>

#include "core_mmu_private.h"

#define MAX_MMAP_REGIONS	13
#define RES_VASPACE_SIZE	(CORE_MMU_PGDIR_SIZE * 10)
#define SHM_VASPACE_SIZE	(1024 * 1024 * 32)

/*
 * These variables are initialized before .bss is cleared. To avoid
 * resetting them when .bss is cleared we're storing them in .data instead,
 * even if they initially are zero.
 */

/* Default NSec shared memory allocated from NSec world */
unsigned long default_nsec_shm_size;
unsigned long default_nsec_shm_paddr;

static struct tee_mmap_region
	static_memory_map[MAX_MMAP_REGIONS + 1];
static bool mem_map_inited;

/* Define the platform's memory layout. */
struct memaccess_area {
	paddr_t paddr;
	size_t size;
};
#define MEMACCESS_AREA(a, s) { .paddr = a, .size = s }

static struct memaccess_area secure_only[] = {
#ifdef TZSRAM_BASE
	MEMACCESS_AREA(TZSRAM_BASE, TZSRAM_SIZE),
#endif
	MEMACCESS_AREA(TZDRAM_BASE, TZDRAM_SIZE),
};

static struct memaccess_area nsec_shared[] = {
	MEMACCESS_AREA(CFG_SHMEM_START, CFG_SHMEM_SIZE),
};

#ifdef CFG_TEE_SDP_MEM_BASE
register_sdp_mem(CFG_TEE_SDP_MEM_BASE, CFG_TEE_SDP_MEM_SIZE);
#endif

#ifdef CFG_CORE_RWDATA_NOEXEC
register_phys_mem(MEM_AREA_TEE_RAM_RX, VCORE_UNPG_RX_PA, VCORE_UNPG_RX_SZ);
register_phys_mem(MEM_AREA_TEE_RAM_RO, VCORE_UNPG_RO_PA, VCORE_UNPG_RO_SZ);
register_phys_mem(MEM_AREA_TEE_RAM_RW, VCORE_UNPG_RW_PA, VCORE_UNPG_RW_SZ);
#ifdef CFG_WITH_PAGER
register_phys_mem(MEM_AREA_TEE_RAM_RX, VCORE_INIT_RX_PA, VCORE_INIT_RX_SZ);
register_phys_mem(MEM_AREA_TEE_RAM_RO, VCORE_INIT_RO_PA, VCORE_INIT_RO_SZ);
#endif
#else
register_phys_mem(MEM_AREA_TEE_RAM, CFG_TEE_RAM_START, CFG_TEE_RAM_PH_SIZE);
#endif

register_phys_mem(MEM_AREA_TA_RAM, CFG_TA_RAM_START, CFG_TA_RAM_SIZE);
register_phys_mem(MEM_AREA_NSEC_SHM, CFG_SHMEM_START, CFG_SHMEM_SIZE);

static bool _pbuf_intersects(struct memaccess_area *a, size_t alen,
			     paddr_t pa, size_t size)
{
	size_t n;

	for (n = 0; n < alen; n++)
		if (core_is_buffer_intersect(pa, size, a[n].paddr, a[n].size))
			return true;
	return false;
}
#define pbuf_intersects(a, pa, size) \
	_pbuf_intersects((a), ARRAY_SIZE(a), (pa), (size))

static bool _pbuf_is_inside(struct memaccess_area *a, size_t alen,
			    paddr_t pa, size_t size)
{
	size_t n;

	for (n = 0; n < alen; n++)
		if (core_is_buffer_inside(pa, size, a[n].paddr, a[n].size))
			return true;
	return false;
}
#define pbuf_is_inside(a, pa, size) \
	_pbuf_is_inside((a), ARRAY_SIZE(a), (pa), (size))

static bool pa_is_in_map(struct tee_mmap_region *map, paddr_t pa)
{
	if (!map)
		return false;
	return (pa >= map->pa && pa <= (map->pa + map->size - 1));
}

static bool va_is_in_map(struct tee_mmap_region *map, vaddr_t va)
{
	if (!map)
		return false;
	return (va >= map->va && va <= (map->va + map->size - 1));
}

/* check if target buffer fits in a core default map area */
static bool pbuf_inside_map_area(unsigned long p, size_t l,
				 struct tee_mmap_region *map)
{
	return core_is_buffer_inside(p, l, map->pa, map->size);
}

static struct tee_mmap_region *find_map_by_type(enum teecore_memtypes type)
{
	struct tee_mmap_region *map;

	for (map = static_memory_map; !core_mmap_is_end_of_table(map); map++)
		if (map->type == type)
			return map;
	return NULL;
}

static struct tee_mmap_region *find_map_by_type_and_pa(
			enum teecore_memtypes type, paddr_t pa)
{
	struct tee_mmap_region *map;

	for (map = static_memory_map; !core_mmap_is_end_of_table(map); map++) {
		if (map->type != type)
			continue;
		if (pa_is_in_map(map, pa))
			return map;
	}
	return NULL;
}

static struct tee_mmap_region *find_map_by_va(void *va)
{
	struct tee_mmap_region *map = static_memory_map;
	unsigned long a = (unsigned long)va;

	while (!core_mmap_is_end_of_table(map)) {
		if ((a >= map->va) && (a <= (map->va - 1 + map->size)))
			return map;
		map++;
	}
	return NULL;
}

static struct tee_mmap_region *find_map_by_pa(unsigned long pa)
{
	struct tee_mmap_region *map = static_memory_map;

	while (!core_mmap_is_end_of_table(map)) {
		if ((pa >= map->pa) && (pa < (map->pa + map->size)))
			return map;
		map++;
	}
	return NULL;
}

static bool pbuf_is_special_mem(paddr_t pbuf, size_t len,
				const struct core_mmu_phys_mem *start,
				const struct core_mmu_phys_mem *end)
{
	const struct core_mmu_phys_mem *mem;

	for (mem = start; mem < end; mem++) {
		if (core_is_buffer_inside(pbuf, len, mem->addr, mem->size))
			return true;
	}

	return false;
}

#ifdef CFG_DT
static void carve_out_phys_mem(struct core_mmu_phys_mem **mem, size_t *nelems,
			       paddr_t pa, size_t size)
{
	struct core_mmu_phys_mem *m = *mem;
	size_t n = 0;

	while (true) {
		if (n >= *nelems) {
			DMSG("No need to carve out %#" PRIxPA " size %#zx",
			     pa, size);
			return;
		}
		if (core_is_buffer_inside(pa, size, m[n].addr, m[n].size))
			break;
		if (!core_is_buffer_outside(pa, size, m[n].addr, m[n].size))
			panic();
		n++;
	}

	if (pa == m[n].addr && size == m[n].size) {
		/* Remove this entry */
		(*nelems)--;
		memmove(m + n, m + n + 1, sizeof(*m) * (*nelems - n));
		m = realloc(m, sizeof(*m) * *nelems);
		if (!m)
			panic();
		*mem = m;
	} else if (pa == m[n].addr) {
		m[n].addr += size;
	} else if ((pa + size) == (m[n].addr + m[n].size)) {
		m[n].size -= size;
	} else {
		/* Need to split the memory entry */
		m = realloc(m, sizeof(*m) * (*nelems + 1));
		if (!m)
			panic();
		*mem = m;
		memmove(m + n + 1, m + n, sizeof(*m) * (*nelems - n));
		(*nelems)++;
		m[n].size = pa - m[n].addr;
		m[n + 1].size -= size + m[n].size;
		m[n + 1].addr = pa + size;
	}
}

static void check_phys_mem_is_outside(struct core_mmu_phys_mem *start,
				      size_t nelems,
				      struct tee_mmap_region *map)
{
	size_t n;

	for (n = 0; n < nelems; n++) {
		if (!core_is_buffer_outside(start[n].addr, start[n].size,
					    map->pa, map->size)) {
			EMSG(
"Non-sec mem (%#" PRIxPA ":%#zx) overlaps map (type %d %#" PRIxPA ":%#zx)",
			     start[n].addr, start[n].size,
			     map->type, map->pa, map->size);
			panic();
		}
	}
}

static const struct core_mmu_phys_mem *discovered_nsec_ddr_start;
static size_t discovered_nsec_ddr_nelems;

static int cmp_pmem_by_addr(const void *a, const void *b)
{
	return ((const struct core_mmu_phys_mem *)a)->addr -
	       ((const struct core_mmu_phys_mem *)b)->addr;
}

void core_mmu_set_discovered_nsec_ddr(struct core_mmu_phys_mem *start,
				      size_t nelems)
{
	struct core_mmu_phys_mem *m = start;
	size_t num_elems = nelems;
	struct tee_mmap_region *map = static_memory_map;
	const struct core_mmu_phys_mem __maybe_unused *pmem;

	assert(!discovered_nsec_ddr_start);
	assert(m && num_elems);

	qsort(m, num_elems, sizeof(*m), cmp_pmem_by_addr);

	/*
	 * Non-secure shared memory and also secure data
	 * path memory are supposed to reside inside
	 * non-secure memory. Since NSEC_SHM and SDP_MEM
	 * are used for a specific purpose make holes for
	 * those memory in the normal non-secure memory.
	 *
	 * This has to be done since for instance QEMU
	 * isn't aware of which memory range in the
	 * non-secure memory is used for NSEC_SHM.
	 */

#ifdef CFG_SECURE_DATA_PATH
	for (pmem = &__start_phys_sdp_mem_section;
	     pmem < &__end_phys_sdp_mem_section; pmem++)
		carve_out_phys_mem(&m, &num_elems, pmem->addr, pmem->size);
#endif

	for (map = static_memory_map; core_mmap_is_end_of_table(map); map++) {
		if (map->type == MEM_AREA_NSEC_SHM)
			carve_out_phys_mem(&m, &num_elems, map->pa, map->size);
		else
			check_phys_mem_is_outside(m, num_elems, map);
	}

	discovered_nsec_ddr_start = m;
	discovered_nsec_ddr_nelems = num_elems;
}

static bool get_discovered_nsec_ddr(const struct core_mmu_phys_mem **start,
				    const struct core_mmu_phys_mem **end)
{
	if (!discovered_nsec_ddr_start)
		return false;

	*start = discovered_nsec_ddr_start;
	*end = discovered_nsec_ddr_start + discovered_nsec_ddr_nelems;

	return true;
}
#else /*!CFG_DT*/
static bool
get_discovered_nsec_ddr(const struct core_mmu_phys_mem **start __unused,
			const struct core_mmu_phys_mem **end __unused)
{
	return false;
}
#endif /*!CFG_DT*/

static bool pbuf_is_nsec_ddr(paddr_t pbuf, size_t len)
{
	const struct core_mmu_phys_mem *start;
	const struct core_mmu_phys_mem *end;

	if (!get_discovered_nsec_ddr(&start, &end)) {
		start = &__start_phys_nsec_ddr_section;
		end = &__end_phys_nsec_ddr_section;
	}

	return pbuf_is_special_mem(pbuf, len, start, end);
}

bool core_mmu_nsec_ddr_is_defined(void)
{
	const struct core_mmu_phys_mem *start;
	const struct core_mmu_phys_mem *end;

	if (!get_discovered_nsec_ddr(&start, &end)) {
		start = &__start_phys_nsec_ddr_section;
		end = &__end_phys_nsec_ddr_section;
	}

	return start != end;
}

#define MSG_MEM_INSTERSECT(pa1, sz1, pa2, sz2) \
	EMSG("[%" PRIxPA " %" PRIxPA "] intersecs [%" PRIxPA " %" PRIxPA "]", \
			pa1, pa1 + sz1, pa2, pa2 + sz2)

#ifdef CFG_SECURE_DATA_PATH
static bool pbuf_is_sdp_mem(paddr_t pbuf, size_t len)
{
	return pbuf_is_special_mem(pbuf, len, &__start_phys_sdp_mem_section,
				    &__end_phys_sdp_mem_section);
}

struct mobj **core_sdp_mem_create_mobjs(void)
{
	const struct core_mmu_phys_mem *mem;
	struct mobj **mobj_base;
	struct mobj **mobj;
	int cnt = &__end_phys_sdp_mem_section - &__start_phys_sdp_mem_section;

	/* SDP mobjs table must end with a NULL entry */
	mobj_base = calloc(cnt + 1, sizeof(struct mobj *));
	if (!mobj_base)
		panic("Out of memory");

	for (mem = &__start_phys_sdp_mem_section, mobj = mobj_base;
	     mem < &__end_phys_sdp_mem_section; mem++, mobj++) {
		*mobj = mobj_phys_alloc(mem->addr, mem->size,
					TEE_MATTR_CACHE_CACHED,
					CORE_MEM_SDP_MEM);
		if (!*mobj)
			panic("can't create SDP physical memory object");
	}
	return mobj_base;
}

static void check_sdp_intersection_with_nsec_ddr(void)
{
	const struct core_mmu_phys_mem *sdp_start =
		&__start_phys_sdp_mem_section;
	const struct core_mmu_phys_mem *sdp_end = &__end_phys_sdp_mem_section;
	const struct core_mmu_phys_mem *ddr_start =
		&__start_phys_nsec_ddr_section;
	const struct core_mmu_phys_mem *ddr_end = &__end_phys_nsec_ddr_section;
	const struct core_mmu_phys_mem *sdp;
	const struct core_mmu_phys_mem *nsec_ddr;

	if (sdp_start == sdp_end || ddr_start == ddr_end)
		return;

	for (sdp = sdp_start; sdp < sdp_end; sdp++) {
		for (nsec_ddr = ddr_start; nsec_ddr < ddr_end; nsec_ddr++) {
			if (core_is_buffer_intersect(sdp->addr, sdp->size,
					     nsec_ddr->addr, nsec_ddr->size)) {
				MSG_MEM_INSTERSECT(sdp->addr, sdp->size,
						   nsec_ddr->addr,
						   nsec_ddr->size);
				panic("SDP <-> NSEC DDR memory intersection");
			}
		}
	}
}

#else /* CFG_SECURE_DATA_PATH */
static bool pbuf_is_sdp_mem(paddr_t pbuf __unused, size_t len __unused)
{
	return false;
}

#endif /* CFG_SECURE_DATA_PATH */

/* Check special memories comply with registered memories */
static void verify_special_mem_areas(struct tee_mmap_region *mem_map,
				     size_t len,
				     const struct core_mmu_phys_mem *start,
				     const struct core_mmu_phys_mem *end,
				     const char *area_name __maybe_unused)
{
	const struct core_mmu_phys_mem *mem;
	const struct core_mmu_phys_mem *mem2;
	struct tee_mmap_region *mmap;
	size_t n;

	if (start == end) {
		DMSG("No %s memory area defined", area_name);
		return;
	}

	for (mem = start; mem < end; mem++)
		DMSG("%s memory [%" PRIxPA " %" PRIxPA "]",
		     area_name, mem->addr, mem->addr + mem->size);

	/* Check memories do not intersect each other */
	for (mem = start; mem < end - 1; mem++) {
		for (mem2 = mem + 1; mem2 < end; mem2++) {
			if (core_is_buffer_intersect(mem2->addr, mem2->size,
						     mem->addr, mem->size)) {
				MSG_MEM_INSTERSECT(mem2->addr, mem2->size,
						   mem->addr, mem->size);
				panic("Special memory intersection");
			}
		}
	}

	/*
	 * Check memories do not intersect any mapped memory.
	 * This is called before reserved VA space is loaded in mem_map.
	 *
	 * Only exception is with MEM_AREA_RAM_NSEC and MEM_AREA_NSEC_SHM,
	 * which may overlap since they are used for the same purpose
	 * except that MEM_AREA_NSEC_SHM is always mapped and
	 * MEM_AREA_RAM_NSEC only uses a dynamic mapping.
	 */
	for (mem = start; mem < end; mem++) {
		for (mmap = mem_map, n = 0; n < len; mmap++, n++) {
			if (mem->type == MEM_AREA_RAM_NSEC &&
			    mmap->type == MEM_AREA_NSEC_SHM)
				continue;
			if (core_is_buffer_intersect(mem->addr, mem->size,
						     mmap->pa, mmap->size)) {
				MSG_MEM_INSTERSECT(mem->addr, mem->size,
						   mmap->pa, mmap->size);
				panic("Special memory intersection");
			}
		}
	}
}

static void add_phys_mem(struct tee_mmap_region *memory_map, size_t num_elems,
			 const struct core_mmu_phys_mem *mem, size_t *last)
{
	size_t n = 0;
	paddr_t pa;
	size_t size;

	/*
	 * If some ranges of memory of the same type do overlap
	 * each others they are coalesced into one entry. To help this
	 * added entries are sorted by increasing physical.
	 *
	 * Note that it's valid to have the same physical memory as several
	 * different memory types, for instance the same device memory
	 * mapped as both secure and non-secure. This will probably not
	 * happen often in practice.
	 */
	DMSG("%s type %s 0x%08" PRIxPA " size 0x%08zx",
	     mem->name, teecore_memtype_name(mem->type), mem->addr, mem->size);
	while (true) {
		if (n >= (num_elems - 1)) {
			EMSG("Out of entries (%zu) in memory_map", num_elems);
			panic();
		}
		if (n == *last)
			break;
		pa = memory_map[n].pa;
		size = memory_map[n].size;
		if (mem->type == memory_map[n].type &&
		    ((mem->addr >= pa && mem->addr <= (pa + (size - 1))) ||
		    (pa >= mem->addr && pa <= (mem->addr + (mem->size - 1))))) {
			DMSG("Physical mem map overlaps 0x%" PRIxPA, mem->addr);
			memory_map[n].pa = MIN(pa, mem->addr);
			memory_map[n].size = MAX(size, mem->size) +
					     (pa - memory_map[n].pa);
			return;
		}
		if (mem->type < memory_map[n].type ||
		    (mem->type == memory_map[n].type && mem->addr < pa))
			break; /* found the spot where to inseart this memory */
		n++;
	}

	memmove(memory_map + n + 1, memory_map + n,
		sizeof(struct tee_mmap_region) * (*last - n));
	(*last)++;
	memset(memory_map + n, 0, sizeof(memory_map[0]));
	memory_map[n].type = mem->type;
	memory_map[n].pa = mem->addr;
	memory_map[n].size = mem->size;
}

static void add_va_space(struct tee_mmap_region *memory_map, size_t num_elems,
			 enum teecore_memtypes type, size_t size, size_t *last)
{
	size_t n = 0;

	DMSG("type %s size 0x%08zx", teecore_memtype_name(type), size);
	while (true) {
		if (n >= (num_elems - 1)) {
			EMSG("Out of entries (%zu) in memory_map", num_elems);
			panic();
		}
		if (n == *last)
			break;
		if (type < memory_map[n].type)
			break;
		n++;
	}

	memmove(memory_map + n + 1, memory_map + n,
		sizeof(struct tee_mmap_region) * (*last - n));
	(*last)++;
	memset(memory_map + n, 0, sizeof(memory_map[0]));
	memory_map[n].type = type;
	memory_map[n].size = size;
}

uint32_t core_mmu_type_to_attr(enum teecore_memtypes t)
{
	const uint32_t attr = TEE_MATTR_VALID_BLOCK | TEE_MATTR_GLOBAL;
	const uint32_t cached = TEE_MATTR_CACHE_CACHED << TEE_MATTR_CACHE_SHIFT;
	const uint32_t noncache = TEE_MATTR_CACHE_NONCACHE <<
				  TEE_MATTR_CACHE_SHIFT;

	switch (t) {
	case MEM_AREA_TEE_RAM:
		return attr | TEE_MATTR_SECURE | TEE_MATTR_PRWX | cached;
	case MEM_AREA_TEE_RAM_RX:
		return attr | TEE_MATTR_SECURE | TEE_MATTR_PRX | cached;
	case MEM_AREA_TEE_RAM_RO:
		return attr | TEE_MATTR_SECURE | TEE_MATTR_PR | cached;
	case MEM_AREA_TEE_RAM_RW:
		return attr | TEE_MATTR_SECURE | TEE_MATTR_PRW | cached;
	case MEM_AREA_TA_RAM:
		return attr | TEE_MATTR_SECURE | TEE_MATTR_PRW | cached;
	case MEM_AREA_NSEC_SHM:
		return attr | TEE_MATTR_PRW | cached;
	case MEM_AREA_IO_NSEC:
		return attr | TEE_MATTR_PRW | noncache;
	case MEM_AREA_IO_SEC:
		return attr | TEE_MATTR_SECURE | TEE_MATTR_PRW | noncache;
	case MEM_AREA_RAM_NSEC:
		return attr | TEE_MATTR_PRW | cached;
	case MEM_AREA_RAM_SEC:
		return attr | TEE_MATTR_SECURE | TEE_MATTR_PRW | cached;
	case MEM_AREA_RES_VASPACE:
	case MEM_AREA_SHM_VASPACE:
		return 0;
	//jwseo
	case MEM_AREA_IO_SEC_U:
		return attr | TEE_MATTR_URW | noncache;
	default:
		panic("invalid type");
	}
}

static bool __maybe_unused map_is_tee_ram(const struct tee_mmap_region *mm)
{
	switch (mm->type) {
	case MEM_AREA_TEE_RAM:
	case MEM_AREA_TEE_RAM_RX:
	case MEM_AREA_TEE_RAM_RO:
	case MEM_AREA_TEE_RAM_RW:
		return true;
	default:
		return false;
	}
}

static bool map_is_flat_mapped(const struct tee_mmap_region *mm)
{
	return map_is_tee_ram(mm);
}

static bool __maybe_unused map_is_secure(const struct tee_mmap_region *mm)
{
	return !!(core_mmu_type_to_attr(mm->type) & TEE_MATTR_SECURE);
}

static bool __maybe_unused map_is_pgdir(const struct tee_mmap_region *mm)
{
	return mm->region_size == CORE_MMU_PGDIR_SIZE;
}

static int cmp_mmap_by_lower_va(const void *a, const void *b)
{
	const struct tee_mmap_region *mm_a = a;
	const struct tee_mmap_region *mm_b = b;

	return mm_a->va - mm_b->va;
}

static int __maybe_unused cmp_mmap_by_secure_attr(const void *a, const void *b)
{
	const struct tee_mmap_region *mm_a = a;
	const struct tee_mmap_region *mm_b = b;

	/* unmapped areas are special */
	if (!core_mmu_type_to_attr(mm_a->type) ||
	    !core_mmu_type_to_attr(mm_b->type))
		return 0;

	return map_is_secure(mm_b) - map_is_secure(mm_a);
}

static int cmp_mmap_by_bigger_region_size(const void *a, const void *b)
{
	const struct tee_mmap_region *mm_a = a;
	const struct tee_mmap_region *mm_b = b;

	return mm_b->region_size - mm_a->region_size;
}

static void dump_mmap_table(struct tee_mmap_region *memory_map)
{
	struct tee_mmap_region *map;

	for (map = memory_map; !core_mmap_is_end_of_table(map); map++) {
		vaddr_t __maybe_unused vstart;

		vstart = map->va + ((vaddr_t)map->pa & (map->region_size - 1));
		DMSG("type %-12s va 0x%08" PRIxVA "..0x%08" PRIxVA
		     " pa 0x%08" PRIxPA "..0x%08" PRIxPA " size 0x%08zx (%s)",
		     teecore_memtype_name(map->type), vstart,
		     vstart + map->size - 1, map->pa,
		     (paddr_t)(map->pa + map->size - 1), map->size,
		     map->region_size == SMALL_PAGE_SIZE ? "smallpg" : "pgdir");
	}
}

static void init_mem_map(struct tee_mmap_region *memory_map, size_t num_elems)
{
	const struct core_mmu_phys_mem *mem;
	struct tee_mmap_region *map;
	size_t last = 0;
	size_t __maybe_unused count = 0;
	vaddr_t va;
	vaddr_t __maybe_unused end;
	bool __maybe_unused va_is_secure = true; /* any init value fits */

	for (mem = &__start_phys_mem_map_section;
	     mem < &__end_phys_mem_map_section; mem++) {
		struct core_mmu_phys_mem m = *mem;

		/* Discard null size entries */
		if (!m.size)
			continue;

		/* Only unmapped virtual range may have a null phys addr */
		assert(m.addr || !core_mmu_type_to_attr(m.type));

		if (m.type == MEM_AREA_IO_NSEC || m.type == MEM_AREA_IO_SEC) {
			m.addr = ROUNDDOWN(m.addr, CORE_MMU_PGDIR_SIZE);
			m.size = ROUNDUP(m.size + (mem->addr - m.addr),
					 CORE_MMU_PGDIR_SIZE);
		}
		add_phys_mem(memory_map, num_elems, &m, &last);
	}

#ifdef CFG_SECURE_DATA_PATH
	verify_special_mem_areas(memory_map, num_elems,
				 &__start_phys_sdp_mem_section,
				 &__end_phys_sdp_mem_section, "SDP");

	check_sdp_intersection_with_nsec_ddr();
#endif

	verify_special_mem_areas(memory_map, num_elems,
				 &__start_phys_nsec_ddr_section,
				 &__end_phys_nsec_ddr_section, "NSEC DDR");

	add_va_space(memory_map, num_elems, MEM_AREA_RES_VASPACE,
		     RES_VASPACE_SIZE, &last);

	add_va_space(memory_map, num_elems, MEM_AREA_SHM_VASPACE,
		     RES_VASPACE_SIZE, &last);

	memory_map[last].type = MEM_AREA_END;

	/*
	 * Assign region sizes, note that MEM_AREA_TEE_RAM always uses
	 * SMALL_PAGE_SIZE if paging is enabled.
	 */
	for (map = memory_map; !core_mmap_is_end_of_table(map); map++) {
		paddr_t mask = map->pa | map->size;

		if (!(mask & CORE_MMU_PGDIR_MASK))
			map->region_size = CORE_MMU_PGDIR_SIZE;
		else if (!(mask & SMALL_PAGE_MASK))
			map->region_size = SMALL_PAGE_SIZE;
		else
			panic("Impossible memory alignment");

#ifdef CFG_WITH_PAGER
		if (map_is_tee_ram(map))
			map->region_size = SMALL_PAGE_SIZE;
#endif
	}

	/*
	 * To ease mapping and lower use of xlat tables, sort mapping
	 * description moving small-page regions after the pgdir regions.
	 */
	qsort(memory_map, last, sizeof(struct tee_mmap_region),
		cmp_mmap_by_bigger_region_size);

#if !defined(CFG_WITH_LPAE)
	/*
	 * 32bit MMU descriptors cannot mix secure and non-secure mapping in
	 * the same level2 table. Hence sort secure mapping from non-secure
	 * mapping.
	 */
	for (count = 0, map = memory_map; map_is_pgdir(map); count++, map++)
		;

	qsort(memory_map + count, last - count, sizeof(struct tee_mmap_region),
		cmp_mmap_by_secure_attr);
#endif

	/*
	 * Map flat mapped addresses first.
	 * 'va' will store the lower address of the flat-mapped areas to later
	 * setup the virtual mapping of the non flat-mapped areas.
	 */
	va = (vaddr_t)~0UL;
	end = 0;
	for (map = memory_map; !core_mmap_is_end_of_table(map); map++) {
		if (!map_is_flat_mapped(map))
			continue;

		map->attr = core_mmu_type_to_attr(map->type);
		map->va = map->pa;
		va = MIN(va, ROUNDDOWN(map->va, map->region_size));
		end = MAX(end, ROUNDUP(map->va + map->size, map->region_size));
	}
	assert(va >= CFG_TEE_RAM_START);
	assert(end <= CFG_TEE_RAM_START + CFG_TEE_RAM_VA_SIZE);

	if (core_mmu_place_tee_ram_at_top(va)) {
		/* Map non-flat mapped addresses below flat mapped addresses */
		for (map = memory_map; !core_mmap_is_end_of_table(map); map++) {
			if (map_is_flat_mapped(map))
				continue;

#if !defined(CFG_WITH_LPAE)
			if (va_is_secure != map_is_secure(map)) {
				va_is_secure = !va_is_secure;
				va = ROUNDDOWN(va, CORE_MMU_PGDIR_SIZE);
			}
#endif
			map->attr = core_mmu_type_to_attr(map->type);
			va -= map->size;
			va = ROUNDDOWN(va, map->region_size);
#if !defined(CFG_WITH_LPAE)
			/* Mapping does not yet support sharing L2 tables */
			va = ROUNDDOWN(va, CORE_MMU_PGDIR_SIZE);
#endif
			map->va = va;
		}
	} else {
		/* Map non-flat mapped addresses above flat mapped addresses */
		va = ROUNDUP(va + CFG_TEE_RAM_VA_SIZE, CORE_MMU_PGDIR_SIZE);
		for (map = memory_map; !core_mmap_is_end_of_table(map); map++) {
			if (map_is_flat_mapped(map))
				continue;

#if !defined(CFG_WITH_LPAE)
			if (va_is_secure != map_is_secure(map)) {
				va_is_secure = !va_is_secure;
				va = ROUNDUP(va, CORE_MMU_PGDIR_SIZE);
			}
#endif
			map->attr = core_mmu_type_to_attr(map->type);
			va = ROUNDUP(va, map->region_size);
#if !defined(CFG_WITH_LPAE)
			/* Mapping does not yet support sharing L2 tables */
			va = ROUNDUP(va, CORE_MMU_PGDIR_SIZE);
#endif
			map->va = va;
			va += map->size;
		}
	}

	qsort(memory_map, last, sizeof(struct tee_mmap_region),
		cmp_mmap_by_lower_va);

	dump_mmap_table(memory_map);
}

/*
 * core_init_mmu_map - init tee core default memory mapping
 *
 * this routine sets the static default tee core mapping.
 *
 * If an error happend: core_init_mmu_map is expected to reset.
 */
void core_init_mmu_map(void)
{
	struct tee_mmap_region *map;
	size_t n;

	for (n = 0; n < ARRAY_SIZE(secure_only); n++) {
		if (pbuf_intersects(nsec_shared, secure_only[n].paddr,
				    secure_only[n].size))
			panic("Invalid memory access config: sec/nsec");
	}

	if (!mem_map_inited)
		init_mem_map(static_memory_map, ARRAY_SIZE(static_memory_map));

	map = static_memory_map;
	while (!core_mmap_is_end_of_table(map)) {
		switch (map->type) {
		case MEM_AREA_TEE_RAM:
		case MEM_AREA_TEE_RAM_RX:
		case MEM_AREA_TEE_RAM_RO:
		case MEM_AREA_TEE_RAM_RW:
			if (!pbuf_is_inside(secure_only, map->pa, map->size))
				panic("TEE_RAM can't fit in secure_only");
			break;
		case MEM_AREA_TA_RAM:
			if (!pbuf_is_inside(secure_only, map->pa, map->size))
				panic("TA_RAM can't fit in secure_only");
			break;
		case MEM_AREA_NSEC_SHM:
			if (!pbuf_is_inside(nsec_shared, map->pa, map->size))
				panic("NS_SHM can't fit in nsec_shared");
			break;
		case MEM_AREA_IO_SEC:
		case MEM_AREA_IO_NSEC:
		case MEM_AREA_RAM_SEC:
		case MEM_AREA_RAM_NSEC:
		case MEM_AREA_RES_VASPACE:
		case MEM_AREA_SHM_VASPACE:
			break;
		//jwseo
		case MEM_AREA_IO_SEC_U:
			break;
		default:
			EMSG("Uhandled memtype %d", map->type);
			panic();
		}
		map++;
	}

	core_init_mmu_tables(static_memory_map);
}

bool core_mmu_mattr_is_ok(uint32_t mattr)
{
	/*
	 * Keep in sync with core_mmu_lpae.c:mattr_to_desc and
	 * core_mmu_v7.c:mattr_to_texcb
	 */

	switch ((mattr >> TEE_MATTR_CACHE_SHIFT) & TEE_MATTR_CACHE_MASK) {
	case TEE_MATTR_CACHE_NONCACHE:
	case TEE_MATTR_CACHE_CACHED:
		return true;
	default:
		return false;
	}
}

/*
 * test attributes of target physical buffer
 *
 * Flags: pbuf_is(SECURE, NOT_SECURE, RAM, IOMEM, KEYVAULT).
 *
 */
bool core_pbuf_is(uint32_t attr, paddr_t pbuf, size_t len)
{
	struct tee_mmap_region *map;

	/* Empty buffers complies with anything */
	if (len == 0)
		return true;

	switch (attr) {
	case CORE_MEM_SEC:
		return pbuf_is_inside(secure_only, pbuf, len);
	case CORE_MEM_NON_SEC:
		return pbuf_is_inside(nsec_shared, pbuf, len) ||
			pbuf_is_nsec_ddr(pbuf, len);
	case CORE_MEM_TEE_RAM:
		return core_is_buffer_inside(pbuf, len, CFG_TEE_RAM_START,
							CFG_TEE_RAM_PH_SIZE);
	case CORE_MEM_TA_RAM:
		return core_is_buffer_inside(pbuf, len, CFG_TA_RAM_START,
							CFG_TA_RAM_SIZE);
	case CORE_MEM_NSEC_SHM:
		return core_is_buffer_inside(pbuf, len, CFG_SHMEM_START,
							CFG_SHMEM_SIZE);
	case CORE_MEM_SDP_MEM:
		return pbuf_is_sdp_mem(pbuf, len);
	case CORE_MEM_CACHED:
		map = find_map_by_pa(pbuf);
		if (map == NULL || !pbuf_inside_map_area(pbuf, len, map))
			return false;
		return map->attr >> TEE_MATTR_CACHE_SHIFT ==
		       TEE_MATTR_CACHE_CACHED;
	default:
		return false;
	}
}

/* test attributes of target virtual buffer (in core mapping) */
bool core_vbuf_is(uint32_t attr, const void *vbuf, size_t len)
{
	paddr_t p;

	/* Empty buffers complies with anything */
	if (len == 0)
		return true;

	p = virt_to_phys((void *)vbuf);
	if (!p)
		return false;

	return core_pbuf_is(attr, p, len);
}

/* core_va2pa - teecore exported service */
static int __maybe_unused core_va2pa_helper(void *va, paddr_t *pa)
{
	struct tee_mmap_region *map;

	map = find_map_by_va(va);
	if (!va_is_in_map(map, (vaddr_t)va))
		return -1;

	/*
	 * We can calculate PA for static map. Virtual address ranges
	 * reserved to core dynamic mapping return a 'match' (return 0;)
	 * together with an invalid null physical address.
	 */
	if (map->pa)
		*pa = map->pa + (vaddr_t)va  - map->va;
	else
		*pa = 0;

	return 0;
}

static void *map_pa2va(struct tee_mmap_region *map, paddr_t pa)
{
	if (!pa_is_in_map(map, pa))
		return NULL;

	return (void *)(map->va + pa - map->pa);
}

/*
 * teecore gets some memory area definitions
 */
void core_mmu_get_mem_by_type(unsigned int type, vaddr_t *s, vaddr_t *e)
{
	struct tee_mmap_region *map = find_map_by_type(type);

	if (map) {
		*s = map->va;
		*e = map->va + map->size;
	} else {
		*s = 0;
		*e = 0;
	}
}

enum teecore_memtypes core_mmu_get_type_by_pa(paddr_t pa)
{
	struct tee_mmap_region *map = find_map_by_pa(pa);

	if (!map)
		return MEM_AREA_MAXTYPE;
	return map->type;
}

int __deprecated core_tlb_maintenance(int op, unsigned long a)
{
	switch (op) {
	case TLBINV_UNIFIEDTLB:
		tlbi_all();
		break;
	case TLBINV_CURRENT_ASID:
#ifdef ARM32
		tlbi_asid(read_contextidr());
#endif
#ifdef ARM64
		tlbi_asid(read_contextidr_el1());
#endif
		break;
	case TLBINV_BY_ASID:
		tlbi_asid(a);
		break;
	case TLBINV_BY_MVA:
		panic();
	default:
		return 1;
	}
	return 0;
}

void tlbi_mva_range(vaddr_t va, size_t size, size_t granule)
{
	size_t sz = size;

	assert(granule == CORE_MMU_PGDIR_SIZE || granule == SMALL_PAGE_SIZE);

	dsb_ishst();
	while (sz) {
		tlbi_mva_allasid_nosync(va);
		if (sz < granule)
			break;
		sz -= granule;
		va += granule;
	}
	dsb_ish();
	isb();
}

TEE_Result cache_op_inner(enum cache_op op, void *va, size_t len)
{
	switch (op) {
	case DCACHE_CLEAN:
		dcache_op_all(DCACHE_OP_CLEAN);
		break;
	case DCACHE_AREA_CLEAN:
		dcache_clean_range(va, len);
		break;
	case DCACHE_INVALIDATE:
		dcache_op_all(DCACHE_OP_INV);
		break;
	case DCACHE_AREA_INVALIDATE:
		dcache_inv_range(va, len);
		break;
	case ICACHE_INVALIDATE:
		icache_inv_all();
		break;
	case ICACHE_AREA_INVALIDATE:
		icache_inv_range(va, len);
		break;
	case DCACHE_CLEAN_INV:
		dcache_op_all(DCACHE_OP_CLEAN_INV);
		break;
	case DCACHE_AREA_CLEAN_INV:
		dcache_cleaninv_range(va, len);
		break;
	default:
		return TEE_ERROR_NOT_IMPLEMENTED;
	}
	return TEE_SUCCESS;
}

#ifdef CFG_PL310
TEE_Result cache_op_outer(enum cache_op op, paddr_t pa, size_t len)
{
	TEE_Result ret = TEE_SUCCESS;
	uint32_t exceptions = thread_mask_exceptions(THREAD_EXCP_FOREIGN_INTR);

	tee_l2cc_mutex_lock();
	switch (op) {
	case DCACHE_INVALIDATE:
		arm_cl2_invbyway(pl310_base());
		break;
	case DCACHE_AREA_INVALIDATE:
		if (len)
			arm_cl2_invbypa(pl310_base(), pa, pa + len - 1);
		break;
	case DCACHE_CLEAN:
		arm_cl2_cleanbyway(pl310_base());
		break;
	case DCACHE_AREA_CLEAN:
		if (len)
			arm_cl2_cleanbypa(pl310_base(), pa, pa + len - 1);
		break;
	case DCACHE_CLEAN_INV:
		arm_cl2_cleaninvbyway(pl310_base());
		break;
	case DCACHE_AREA_CLEAN_INV:
		if (len)
			arm_cl2_cleaninvbypa(pl310_base(), pa, pa + len - 1);
		break;
	default:
		ret = TEE_ERROR_NOT_IMPLEMENTED;
	}

	tee_l2cc_mutex_unlock();
	thread_set_exceptions(exceptions);
	return ret;
}
#endif /*CFG_PL310*/

void core_mmu_set_entry(struct core_mmu_table_info *tbl_info, unsigned idx,
			paddr_t pa, uint32_t attr)
{
	assert(idx < tbl_info->num_entries);
	core_mmu_set_entry_primitive(tbl_info->table, tbl_info->level,
				     idx, pa, attr);
}

void core_mmu_get_entry(struct core_mmu_table_info *tbl_info, unsigned idx,
			paddr_t *pa, uint32_t *attr)
{
	assert(idx < tbl_info->num_entries);
	core_mmu_get_entry_primitive(tbl_info->table, tbl_info->level,
				     idx, pa, attr);
}

static void set_region(struct core_mmu_table_info *tbl_info,
		struct tee_mmap_region *region)
{
	unsigned end;
	unsigned idx;
	paddr_t pa;

	/* va, len and pa should be block aligned */
	assert(!core_mmu_get_block_offset(tbl_info, region->va));
	assert(!core_mmu_get_block_offset(tbl_info, region->size));
	assert(!core_mmu_get_block_offset(tbl_info, region->pa));

	idx = core_mmu_va2idx(tbl_info, region->va);
	end = core_mmu_va2idx(tbl_info, region->va + region->size);
	pa = region->pa;

	while (idx < end) {
		core_mmu_set_entry(tbl_info, idx, pa, region->attr);
		idx++;
		pa += 1 << tbl_info->shift;
	}
}

static void set_pg_region(struct core_mmu_table_info *dir_info,
			struct tee_ta_region *region, struct pgt **pgt,
			struct core_mmu_table_info *pg_info)
{
	struct tee_mmap_region r = {
		.va = region->va,
		.size = region->size,
		.attr = region->attr,
	};
	vaddr_t end = r.va + r.size;
	uint32_t pgt_attr = (r.attr & TEE_MATTR_SECURE) | TEE_MATTR_TABLE;

	while (r.va < end) {
		if (!pg_info->table ||
		     r.va >= (pg_info->va_base + CORE_MMU_PGDIR_SIZE)) {
			/*
			 * We're assigning a new translation table.
			 */
			unsigned int idx;

			assert(*pgt); /* We should have alloced enough */

			/* Virtual addresses must grow */
			assert(r.va > pg_info->va_base);

			idx = core_mmu_va2idx(dir_info, r.va);
			pg_info->table = (*pgt)->tbl;
			pg_info->va_base = core_mmu_idx2va(dir_info, idx);
#ifdef CFG_PAGED_USER_TA
			assert((*pgt)->vabase == pg_info->va_base);
#endif
			*pgt = SLIST_NEXT(*pgt, link);

			core_mmu_set_entry(dir_info, idx,
					   virt_to_phys(pg_info->table),
					   pgt_attr);
		}

		r.size = MIN(CORE_MMU_PGDIR_SIZE - (r.va - pg_info->va_base),
			     end - r.va);
		if (!mobj_is_paged(region->mobj)) {
			size_t granule = BIT(pg_info->shift);
			size_t offset = r.va - region->va + region->offset;

			if (mobj_get_pa(region->mobj, offset, granule,
					&r.pa) != TEE_SUCCESS)
				panic("Failed to get PA of unpaged mobj");
			set_region(pg_info, &r);
		}
		r.va += r.size;
	}
}

TEE_Result core_mmu_map_pages(vaddr_t vstart, paddr_t *pages, size_t num_pages,
			      enum teecore_memtypes memtype)
{
	TEE_Result ret;
	struct core_mmu_table_info tbl_info;
	struct tee_mmap_region *mm;
	unsigned int idx;
	uint32_t old_attr;
	vaddr_t vaddr = vstart;
	size_t i;

	assert(!(core_mmu_type_to_attr(memtype) & TEE_MATTR_PX));

	if (vaddr & SMALL_PAGE_MASK)
		return TEE_ERROR_BAD_PARAMETERS;

	mm = find_map_by_va((void *)vaddr);
	if (!mm || !va_is_in_map(mm, vaddr + num_pages * SMALL_PAGE_SIZE - 1))
		panic("VA does not belong to any known mm region");

	if (!core_mmu_is_dynamic_vaspace(mm))
		panic("Trying to map into static region");

	for (i = 0; i < num_pages; i++) {
		if (pages[i] & SMALL_PAGE_MASK) {
			ret = TEE_ERROR_BAD_PARAMETERS;
			goto err;
		}

		while (true) {
			if (!core_mmu_find_table(vaddr, UINT_MAX, &tbl_info))
				panic("Can't find pagetable for vaddr ");

			idx = core_mmu_va2idx(&tbl_info, vaddr);
			if (tbl_info.shift == SMALL_PAGE_SHIFT)
				break;

			/* This is supertable. Need to divide it. */
			if (!core_mmu_divide_block(&tbl_info, idx))
				panic("Could not divide block into smaller tables");
		}

		core_mmu_get_entry(&tbl_info, idx, NULL, &old_attr);
		if (old_attr)
			panic("Page is already mapped");

		core_mmu_set_entry(&tbl_info, idx, pages[i],
				   core_mmu_type_to_attr(memtype));
		vaddr += SMALL_PAGE_SIZE;
	}

	return TEE_SUCCESS;
err:
	if (i)
		core_mmu_unmap_pages(vstart, i);

	return ret;
}

void core_mmu_unmap_pages(vaddr_t vstart, size_t num_pages)
{
	struct core_mmu_table_info tbl_info;
	struct tee_mmap_region *mm;
	size_t i;
	unsigned int idx;

	mm = find_map_by_va((void *)vstart);
	if (!mm || !va_is_in_map(mm, vstart + num_pages * SMALL_PAGE_SIZE - 1))
		panic("VA does not belong to any known mm region");

	if (!core_mmu_is_dynamic_vaspace(mm))
		panic("Trying to unmap static region");

	for (i = 0; i < num_pages; i++, vstart += SMALL_PAGE_SIZE) {
		if (!core_mmu_find_table(vstart, UINT_MAX, &tbl_info))
			panic("Can't find pagetable");

		if (tbl_info.shift != SMALL_PAGE_SHIFT)
			panic("Invalid pagetable level");

		idx = core_mmu_va2idx(&tbl_info, vstart);
		core_mmu_set_entry(&tbl_info, idx, 0, 0);
	}
	tlbi_all();
}

void core_mmu_populate_user_map(struct core_mmu_table_info *dir_info,
				struct user_ta_ctx *utc)
{
	struct core_mmu_table_info pg_info;
	struct pgt_cache *pgt_cache = &thread_get_tsd()->pgt_cache;
	struct pgt *pgt;
	size_t n;

	/* Find the last valid entry */
	n = ARRAY_SIZE(utc->mmu->regions);
	while (true) {
		n--;
		if (utc->mmu->regions[n].size)
			break;
		if (!n)
			return;	/* Nothing to map */
	}

	/*
	 * Allocate all page tables in advance.
	 */
	pgt_alloc(pgt_cache, &utc->ctx, utc->mmu->regions[0].va,
		  utc->mmu->regions[n].va + utc->mmu->regions[n].size - 1);
	pgt = SLIST_FIRST(pgt_cache);

	core_mmu_set_info_table(&pg_info, dir_info->level + 1, 0, NULL);

	for (n = 0; n < ARRAY_SIZE(utc->mmu->regions); n++)
		mobj_update_mapping(utc->mmu->regions[n].mobj, utc,
				    utc->mmu->regions[n].va);

	for (n = 0; n < ARRAY_SIZE(utc->mmu->regions); n++) {
		if (!utc->mmu->regions[n].size)
			continue;
		set_pg_region(dir_info, utc->mmu->regions + n, &pgt, &pg_info);
	}
}

bool core_mmu_add_mapping(enum teecore_memtypes type, paddr_t addr, size_t len)
{
	struct core_mmu_table_info tbl_info;
	struct tee_mmap_region *map;
	size_t n;
	size_t granule;
	paddr_t p;
	size_t l;

	if (!len)
		return true;

	/* Check if the memory is already mapped */
	map = find_map_by_type_and_pa(type, addr);
	if (map && pbuf_inside_map_area(addr, len, map))
		return true;

	/* Find the reserved va space used for late mappings */
	map = find_map_by_type(MEM_AREA_RES_VASPACE);
	if (!map)
		return false;

	if (!core_mmu_find_table(map->va, UINT_MAX, &tbl_info))
		return false;

	granule = 1 << tbl_info.shift;
	p = ROUNDDOWN(addr, granule);
	l = ROUNDUP(len + addr - p, granule);
	/*
	 * Something is wrong, we can't fit the va range into the selected
	 * table. The reserved va range is possibly missaligned with
	 * granule.
	 */
	if (core_mmu_va2idx(&tbl_info, map->va + len) >= tbl_info.num_entries)
		return false;

	/* Find end of the memory map */
	n = 0;
	while (!core_mmap_is_end_of_table(static_memory_map + n))
		n++;

	if (n < (ARRAY_SIZE(static_memory_map) - 1)) {
		/* There's room for another entry */
		static_memory_map[n].va = map->va;
		static_memory_map[n].size = l;
		static_memory_map[n + 1].type = MEM_AREA_END;
		map->va += l;
		map->size -= l;
		map = static_memory_map + n;
	} else {
		/*
		 * There isn't room for another entry, steal the reserved
		 * entry as it's not useful for anything else any longer.
		 */
		map->size = l;
	}
	map->type = type;
	map->region_size = granule;
	map->attr = core_mmu_type_to_attr(type);
	map->pa = p;

	set_region(&tbl_info, map);
	return true;
}

static bool arm_va2pa_helper(void *va, paddr_t *pa)
{
	uint32_t exceptions = thread_mask_exceptions(THREAD_EXCP_ALL);
	paddr_t par;
	paddr_t par_pa_mask;
	bool ret = false;

#ifdef ARM32
	write_ats1cpr((vaddr_t)va);
	isb();
#ifdef CFG_WITH_LPAE
	par = read_par64();
	par_pa_mask = PAR64_PA_MASK;
#else
	par = read_par32();
	par_pa_mask = PAR32_PA_MASK;
#endif
#endif /*ARM32*/

#ifdef ARM64
	write_at_s1e1r((vaddr_t)va);
	isb();
	par = read_par_el1();
	par_pa_mask = PAR_PA_MASK;
#endif
	if (par & PAR_F)
		goto out;
	*pa = (par & (par_pa_mask << PAR_PA_SHIFT)) |
		((vaddr_t)va & ((1 << PAR_PA_SHIFT) - 1));

	ret = true;
out:
	thread_unmask_exceptions(exceptions);
	return ret;
}

#ifdef CFG_WITH_PAGER
static vaddr_t get_linear_map_end(void)
{
	/* this is synced with the generic linker file kern.ld.S */
	return (vaddr_t)__heap2_end;
}
#endif

#if defined(CFG_TEE_CORE_DEBUG)
static void check_pa_matches_va(void *va, paddr_t pa)
{
	TEE_Result res;
	vaddr_t v = (vaddr_t)va;
	paddr_t p = 0;

	if (core_mmu_user_va_range_is_defined()) {
		vaddr_t user_va_base;
		size_t user_va_size;

		core_mmu_get_user_va_range(&user_va_base, &user_va_size);
		if (v >= user_va_base &&
		    v <= (user_va_base - 1 + user_va_size)) {
			if (!core_mmu_user_mapping_is_active()) {
				if (pa)
					panic("issue in linear address space");
				return;
			}

			res = tee_mmu_user_va2pa_helper(
				to_user_ta_ctx(tee_mmu_get_ctx()), va, &p);
			if (res == TEE_SUCCESS && pa != p)
				panic("bad pa");
			if (res != TEE_SUCCESS && pa)
				panic("false pa");
			return;
		}
	}
#ifdef CFG_WITH_PAGER
	if (v >= CFG_TEE_LOAD_ADDR && v < get_linear_map_end()) {
		if (v != pa)
			panic("issue in linear address space");
		return;
	}
	if (v >= (CFG_TEE_LOAD_ADDR & ~CORE_MMU_PGDIR_MASK) &&
	    v <= (CFG_TEE_LOAD_ADDR | CORE_MMU_PGDIR_MASK)) {
		struct core_mmu_table_info *ti = &tee_pager_tbl_info;
		uint32_t a;

		/*
		 * Lookups in the page table managed by the pager is
		 * dangerous for addresses in the paged area as those pages
		 * changes all the time. But some ranges are safe,
		 * rw-locked areas when the page is populated for instance.
		 */
		core_mmu_get_entry(ti, core_mmu_va2idx(ti, v), &p, &a);
		if (a & TEE_MATTR_VALID_BLOCK) {
			paddr_t mask = ((1 << ti->shift) - 1);

			p |= v & mask;
			if (pa != p)
				panic();
		} else
			if (pa)
				panic();
		return;
	}
#endif
	if (!core_va2pa_helper(va, &p)) {
		/* Verfiy only the static mapping (case non null phys addr) */
		if (p && pa != p)
			panic();
	} else {
		if (pa)
			panic();
	}
}
#else
static void check_pa_matches_va(void *va __unused, paddr_t pa __unused)
{
}
#endif

paddr_t virt_to_phys(void *va)
{
	paddr_t pa;

	if (!arm_va2pa_helper(va, &pa))
		pa = 0;
	check_pa_matches_va(va, pa);
	return pa;
}

#if defined(CFG_TEE_CORE_DEBUG)
static void check_va_matches_pa(paddr_t pa, void *va)
{
	if (va && virt_to_phys(va) != pa)
		panic();
}
#else
static void check_va_matches_pa(paddr_t pa __unused, void *va __unused)
{
}
#endif

static void *phys_to_virt_ta_vaspace(paddr_t pa)
{
	TEE_Result res;
	void *va = NULL;

	if (!core_mmu_user_mapping_is_active())
		return NULL;

	res = tee_mmu_user_pa2va_helper(to_user_ta_ctx(tee_mmu_get_ctx()),
					pa, &va);
	if (res != TEE_SUCCESS)
		return NULL;
	return va;
}

#ifdef CFG_WITH_PAGER
static void *phys_to_virt_tee_ram(paddr_t pa)
{
	struct core_mmu_table_info *ti = &tee_pager_tbl_info;
	unsigned idx;
	unsigned end_idx;
	uint32_t a;
	paddr_t p;

	if (pa >= CFG_TEE_LOAD_ADDR && pa < get_linear_map_end())
		return (void *)(vaddr_t)pa;

	end_idx = core_mmu_va2idx(ti, CFG_TEE_RAM_START +
				      CFG_TEE_RAM_VA_SIZE);
	/* Most addresses are mapped lineary, try that first if possible. */
	idx = core_mmu_va2idx(ti, pa);
	if (idx >= core_mmu_va2idx(ti, CFG_TEE_RAM_START) &&
	    idx < end_idx) {
		core_mmu_get_entry(ti, idx, &p, &a);
		if ((a & TEE_MATTR_VALID_BLOCK) && p == pa)
			return (void *)core_mmu_idx2va(ti, idx);
	}

	for (idx = core_mmu_va2idx(ti, CFG_TEE_RAM_START);
	     idx < end_idx; idx++) {
		core_mmu_get_entry(ti, idx, &p, &a);
		if ((a & TEE_MATTR_VALID_BLOCK) && p == pa)
			return (void *)core_mmu_idx2va(ti, idx);
	}

	return NULL;
}
#else
static void *phys_to_virt_tee_ram(paddr_t pa)
{
	struct tee_mmap_region *mmap;

	mmap = find_map_by_type_and_pa(MEM_AREA_TEE_RAM, pa);
	if (!mmap)
		mmap = find_map_by_type_and_pa(MEM_AREA_TEE_RAM_RW, pa);
	if (!mmap)
		mmap = find_map_by_type_and_pa(MEM_AREA_TEE_RAM_RO, pa);
	if (!mmap)
		mmap = find_map_by_type_and_pa(MEM_AREA_TEE_RAM_RX, pa);

	return map_pa2va(mmap, pa);
}
#endif

void *phys_to_virt(paddr_t pa, enum teecore_memtypes m)
{
	void *va;

	switch (m) {
	case MEM_AREA_TA_VASPACE:
		va = phys_to_virt_ta_vaspace(pa);
		break;
	case MEM_AREA_TEE_RAM:
	case MEM_AREA_TEE_RAM_RX:
	case MEM_AREA_TEE_RAM_RO:
	case MEM_AREA_TEE_RAM_RW:
		va = phys_to_virt_tee_ram(pa);
		break;
	default:
		va = map_pa2va(find_map_by_type_and_pa(m, pa), pa);
	}
	check_va_matches_pa(pa, va);
	return va;
}

void *phys_to_virt_io(paddr_t pa)
{
	struct tee_mmap_region *map;
	void *va;

	map = find_map_by_type_and_pa(MEM_AREA_IO_SEC, pa);
	if (!map)
		map = find_map_by_type_and_pa(MEM_AREA_IO_NSEC, pa);
	if (!map)
		return NULL;
	va = map_pa2va(map, pa);
	check_va_matches_pa(pa, va);
	return va;
}

bool cpu_mmu_enabled(void)
{
	uint32_t sctlr;

#ifdef ARM32
	sctlr =  read_sctlr();
#else
	sctlr =  read_sctlr_el1();
#endif

	return sctlr & SCTLR_M ? true : false;
}

vaddr_t core_mmu_get_va(paddr_t pa, enum teecore_memtypes type)
{
	if (cpu_mmu_enabled())
		return (vaddr_t)phys_to_virt(pa, type);

	return (vaddr_t)pa;
}

void main_reg_init(void)
{
	int i = 0;

	XHCI_reg[0]=	0x00000000	;	//	CAPLENGTH
	XHCI_reg[1]=	0x00000004	;	//	HCSPARAMS1
	XHCI_reg[2]=	0x00000008	;	//	HCSPARAMS2
	XHCI_reg[3]=	0x0000000C	;	//	HCSPARAMS3
	XHCI_reg[4]=	0x00000010	;	//	HCCPARAMS1
	XHCI_reg[5]=	0x00000014	;	//	DBOFF
	XHCI_reg[6]=	0x00000018	;	//	RTSOFF
	XHCI_reg[7]=	0x0000001C	;	//	HCCPARAMS2
	XHCI_reg[8]=	0x00000020	;	//	USBCMD
	XHCI_reg[9]=	0x00000024	;	//	USBSTS
	XHCI_reg[10]=	0x00000028	;	//	PAGESIZE
	XHCI_reg[11]=	0x00000034	;	//	DNCTRL
	XHCI_reg[12]=	0x00000038	;	//	CRCR_LO
	XHCI_reg[13]=	0x0000003C	;	//	CRCR_HI
	XHCI_reg[14]=	0x00000050	;	//	DCBAAP_LO
	XHCI_reg[15]=	0x00000054	;	//	DCBAAP_HI
	XHCI_reg[16]=	0x00000058	;	//	CONFIG
	XHCI_reg[17]=	0x00000420	;	//	PORTSC_20
	XHCI_reg[18]=	0x00000424	;	//	PORTPMSC_20
	XHCI_reg[19]=	0x00000428	;	//	PORTLI_20
	XHCI_reg[20]=	0x0000042C	;	//	PORTHLPMC_20
	XHCI_reg[21]=	0x00000430	;	//	PORTSC_30
	XHCI_reg[22]=	0x00000434	;	//	PORTPMSC_30
	XHCI_reg[23]=	0x00000438	;	//	PORTLI_30
	XHCI_reg[24]=	0x0000043C	;	//	PORTHLPMC_30
	XHCI_reg[25]=	0x00000440	;	//	MFINDEX
	XHCI_reg[26]=	0x00000444	;	//	RsvdZ
	XHCI_reg[27]=	0x00000460	;	//	IMAN_0
	XHCI_reg[28]=	0x00000464	;	//	IMOD_0
	XHCI_reg[29]=	0x00000468	;	//	ERSTSZ_0
	XHCI_reg[30]=	0x0000046C	;	//	RsvdP_0
	XHCI_reg[31]=	0x00000470	;	//	ERSTBA_LO_0
	XHCI_reg[32]=	0x00000474	;	//	ERSTBA_HI_0
	XHCI_reg[33]=	0x00000478	;	//	ERDP_LO_0
	XHCI_reg[34]=	0x0000047C	;	//	ERDP_HI_0
	XHCI_reg[35]=	0x00000480	;	//	IMAN_1
	XHCI_reg[36]=	0x00000484	;	//	IMOD_1
	XHCI_reg[37]=	0x00000488	;	//	ERSTSZ_1
	XHCI_reg[38]=	0x0000048C	;	//	RsvdP_1
	XHCI_reg[39]=	0x00000490	;	//	ERSTBA_LO_1
	XHCI_reg[40]=	0x00000494	;	//	ERSTBA_HI_1
	XHCI_reg[41]=	0x00000498	;	//	ERDP_LO_1
	XHCI_reg[42]=	0x0000049C	;	//	ERDP_HI_1
	XHCI_reg[43]=	0x000004A0	;	//	IMAN_2
	XHCI_reg[44]=	0x000004A4	;	//	IMOD_2
	XHCI_reg[45]=	0x000004A8	;	//	ERSTSZ_2
	XHCI_reg[46]=	0x000004AC	;	//	RsvdP_2
	XHCI_reg[47]=	0x000004B0	;	//	ERSTBA_LO_2
	XHCI_reg[48]=	0x000004B4	;	//	ERSTBA_HI_2
	XHCI_reg[49]=	0x000004B8	;	//	ERDP_LO_2
	XHCI_reg[50]=	0x000004BC	;	//	ERDP_HI_2
	XHCI_reg[51]=	0x000004C0	;	//	IMAN_3
	XHCI_reg[52]=	0x000004C4	;	//	IMOD_3
	XHCI_reg[53]=	0x000004C8	;	//	ERSTSZ_3
	XHCI_reg[54]=	0x000004CC	;	//	RsvdP_3
	XHCI_reg[55]=	0x000004D0	;	//	ERSTBA_LO_3
	XHCI_reg[56]=	0x000004D4	;	//	ERSTBA_HI_3
	XHCI_reg[57]=	0x000004D8	;	//	ERDP_LO_3
	XHCI_reg[58]=	0x000004DC	;	//	ERDP_HI_3
	XHCI_reg[59]=	0x000004E0	;	//	DB0
	XHCI_reg[60]=	0x000004E4	;	//	DB1
	XHCI_reg[61]=	0x000004E8	;	//	DB2
	XHCI_reg[62]=	0x000004EC	;	//	DB3
	XHCI_reg[63]=	0x000004F0	;	//	DB4
	XHCI_reg[64]=	0x000004F4	;	//	DB5
	XHCI_reg[65]=	0x000004F8	;	//	DB6
	XHCI_reg[66]=	0x000004FC	;	//	DB7
	XHCI_reg[67]=	0x00000500	;	//	DB8
	XHCI_reg[68]=	0x00000504	;	//	DB9
	XHCI_reg[69]=	0x00000508	;	//	DB10
	XHCI_reg[70]=	0x0000050C	;	//	DB11
	XHCI_reg[71]=	0x00000510	;	//	DB12
	XHCI_reg[72]=	0x00000514	;	//	DB13
	XHCI_reg[73]=	0x00000518	;	//	DB14
	XHCI_reg[74]=	0x0000051C	;	//	DB15
	XHCI_reg[75]=	0x00000520	;	//	DB16
	XHCI_reg[76]=	0x00000524	;	//	DB17
	XHCI_reg[77]=	0x00000528	;	//	DB18
	XHCI_reg[78]=	0x0000052C	;	//	DB19
	XHCI_reg[79]=	0x00000530	;	//	DB20
	XHCI_reg[80]=	0x00000534	;	//	DB21
	XHCI_reg[81]=	0x00000538	;	//	DB22
	XHCI_reg[82]=	0x0000053C	;	//	DB23
	XHCI_reg[83]=	0x00000540	;	//	DB24
	XHCI_reg[84]=	0x00000544	;	//	DB25
	XHCI_reg[85]=	0x00000548	;	//	DB26
	XHCI_reg[86]=	0x0000054C	;	//	DB27
	XHCI_reg[87]=	0x00000550	;	//	DB28
	XHCI_reg[88]=	0x00000554	;	//	DB29
	XHCI_reg[89]=	0x00000558	;	//	DB30
	XHCI_reg[90]=	0x0000055C	;	//	DB31
	XHCI_reg[91]=	0x00000560	;	//	DB32
	XHCI_reg[92]=	0x00000564	;	//	DB33
	XHCI_reg[93]=	0x00000568	;	//	DB34
	XHCI_reg[94]=	0x0000056C	;	//	DB35
	XHCI_reg[95]=	0x00000570	;	//	DB36
	XHCI_reg[96]=	0x00000574	;	//	DB37
	XHCI_reg[97]=	0x00000578	;	//	DB38
	XHCI_reg[98]=	0x0000057C	;	//	DB39
	XHCI_reg[99]=	0x00000580	;	//	DB40
	XHCI_reg[100]=	0x00000584	;	//	DB41
	XHCI_reg[101]=	0x00000588	;	//	DB42
	XHCI_reg[102]=	0x0000058C	;	//	DB43
	XHCI_reg[103]=	0x00000590	;	//	DB44
	XHCI_reg[104]=	0x00000594	;	//	DB45
	XHCI_reg[105]=	0x00000598	;	//	DB46
	XHCI_reg[106]=	0x0000059C	;	//	DB47
	XHCI_reg[107]=	0x000005A0	;	//	DB48
	XHCI_reg[108]=	0x000005A4	;	//	DB49
	XHCI_reg[109]=	0x000005A8	;	//	DB50
	XHCI_reg[110]=	0x000005AC	;	//	DB51
	XHCI_reg[111]=	0x000005B0	;	//	DB52
	XHCI_reg[112]=	0x000005B4	;	//	DB53
	XHCI_reg[113]=	0x000005B8	;	//	DB54
	XHCI_reg[114]=	0x000005BC	;	//	DB55
	XHCI_reg[115]=	0x000005C0	;	//	DB56
	XHCI_reg[116]=	0x000005C4	;	//	DB57
	XHCI_reg[117]=	0x000005C8	;	//	DB58
	XHCI_reg[118]=	0x000005CC	;	//	DB59
	XHCI_reg[119]=	0x000005D0	;	//	DB60
	XHCI_reg[120]=	0x000005D4	;	//	DB61
	XHCI_reg[121]=	0x000005D8	;	//	DB62
	XHCI_reg[122]=	0x000005DC	;	//	DB63
	XHCI_reg[123]=	0x000008E0	;	//	USBLEGSUP
	XHCI_reg[124]=	0x000008E4	;	//	USBLEGCTLSTS
	XHCI_reg[125]=	0x000008F0	;	//	SUPTPRT2_DW0
	XHCI_reg[126]=	0x000008F4	;	//	SUPTPRT2_DW1
	XHCI_reg[127]=	0x000008F8	;	//	SUPTPRT2_DW2
	XHCI_reg[128]=	0x000008FC	;	//	SUPTPRT2_DW3
	XHCI_reg[129]=	0x00000900	;	//	SUPTPRT3_DW0
	XHCI_reg[130]=	0x00000904	;	//	SUPTPRT3_DW1
	XHCI_reg[131]=	0x00000908	;	//	SUPTPRT3_DW2
	XHCI_reg[132]=	0x0000090C	;	//	SUPTPRT3_DW3
	XHCI_reg[133]=	0x00000910	;	//	DCID
	XHCI_reg[134]=	0x00000914	;	//	DCDB
	XHCI_reg[135]=	0x00000918	;	//	DCERSTSZ
	XHCI_reg[136]=	0x00000920	;	//	DCERSTBA_LO
	XHCI_reg[137]=	0x00000924	;	//	DCERSTBA_HI
	XHCI_reg[138]=	0x00000928	;	//	DCERDP_LO
	XHCI_reg[139]=	0x0000092C	;	//	DCERDP_HI
	XHCI_reg[140]=	0x00000930	;	//	DCCTRL
	XHCI_reg[141]=	0x00000934	;	//	DCST
	XHCI_reg[142]=	0x00000938	;	//	DCPORTSC
	XHCI_reg[143]=	0x00000940	;	//	DCCP_LO
	XHCI_reg[144]=	0x00000944	;	//	DCCP_HI
	XHCI_reg[145]=	0x00000948	;	//	DCDDI1
	XHCI_reg[146]=	0x0000094C	;	//	DCDDI2
	XHCI_reg[147]=	0x0000C100	;	//	GSBUSCFG0
	XHCI_reg[148]=	0x0000C104	;	//	GSBUSCFG1
	XHCI_reg[149]=	0x0000C108	;	//	GTXTHRCFG
	XHCI_reg[150]=	0x0000C10C	;	//	GRXTHRCFG
	XHCI_reg[151]=	0x0000C110	;	//	GCTL
	XHCI_reg[152]=	0x0000C114	;	//	GPMSTS
	XHCI_reg[153]=	0x0000C118	;	//	GSTS
	XHCI_reg[154]=	0x0000C11C	;	//	GUCTL1
	XHCI_reg[155]=	0x0000C120	;	//	GSNPSID
	XHCI_reg[156]=	0x0000C124	;	//	GGPIO
	XHCI_reg[157]=	0x0000C128	;	//	GUID
	XHCI_reg[158]=	0x0000C12C	;	//	GUCTL
	XHCI_reg[159]=	0x0000C130	;	//	GBUSERRADDRLO
	XHCI_reg[160]=	0x0000C134	;	//	GBUSERRADDRHI
	XHCI_reg[161]=	0x0000C138	;	//	GPRTBIMAPLO
	XHCI_reg[162]=	0x0000C13C	;	//	GPRTBIMAPHI
	XHCI_reg[163]=	0x0000C140	;	//	GHWPARAMS0
	XHCI_reg[164]=	0x0000C144	;	//	GHWPARAMS1
	XHCI_reg[165]=	0x0000C148	;	//	GHWPARAMS2
	XHCI_reg[166]=	0x0000C14C	;	//	GHWPARAMS3
	XHCI_reg[167]=	0x0000C150	;	//	GHWPARAMS4
	XHCI_reg[168]=	0x0000C154	;	//	GHWPARAMS5
	XHCI_reg[169]=	0x0000C158	;	//	GHWPARAMS6
	XHCI_reg[170]=	0x0000C15C	;	//	GHWPARAMS7
	XHCI_reg[171]=	0x0000C160	;	//	GDBGFIFOSPACE
	XHCI_reg[172]=	0x0000C164	;	//	GDBGLTSSM
	XHCI_reg[173]=	0x0000C168	;	//	GDBGLNMCC
	XHCI_reg[174]=	0x0000C16C	;	//	GDBGBMU
	XHCI_reg[175]=	0x0000C170	;	//	GDBGLSPMUX_HST
	XHCI_reg[176]=	0x0000C174	;	//	GDBGLSP
	XHCI_reg[177]=	0x0000C178	;	//	GDBGEPINFO0
	XHCI_reg[178]=	0x0000C17C	;	//	GDBGEPINFO1
	XHCI_reg[179]=	0x0000C180	;	//	GPRTBIMAP_HSLO
	XHCI_reg[180]=	0x0000C184	;	//	GPRTBIMAP_HSHI
	XHCI_reg[181]=	0x0000C188	;	//	GPRTBIMAP_FSLO
	XHCI_reg[182]=	0x0000C18C	;	//	GPRTBIMAP_FSHI
	XHCI_reg[183]=	0x0000C200	;	//	GUSB2PHYCFG
	XHCI_reg[184]=	0x0000C280	;	//	GUSB2PHYACC_ULPI
	XHCI_reg[185]=	0x0000C2C0	;	//	GUSB3PIPECTL
	XHCI_reg[186]=	0x0000C300	;	//	GTXFIFOSIZ0
	XHCI_reg[187]=	0x0000C304	;	//	GTXFIFOSIZ1
	XHCI_reg[188]=	0x0000C308	;	//	GTXFIFOSIZ2
	XHCI_reg[189]=	0x0000C30C	;	//	GTXFIFOSIZ3
	XHCI_reg[190]=	0x0000C310	;	//	GTXFIFOSIZ4
	XHCI_reg[191]=	0x0000C314	;	//	GTXFIFOSIZ5
	XHCI_reg[192]=	0x0000C380	;	//	GRXFIFOSIZ0
	XHCI_reg[193]=	0x0000C384	;	//	GRXFIFOSIZ1
	XHCI_reg[194]=	0x0000C388	;	//	GRXFIFOSIZ2
	XHCI_reg[195]=	0x0000C400	;	//	GEVNTADRLO_0
	XHCI_reg[196]=	0x0000C404	;	//	GEVNTADRHI_0
	XHCI_reg[197]=	0x0000C408	;	//	GEVNTSIZ_0
	XHCI_reg[198]=	0x0000C40C	;	//	GEVNTCOUNT_0
	XHCI_reg[199]=	0x0000C410	;	//	GEVNTADRLO_1
	XHCI_reg[200]=	0x0000C414	;	//	GEVNTADRHI_1
	XHCI_reg[201]=	0x0000C418	;	//	GEVNTSIZ_1
	XHCI_reg[202]=	0x0000C41C	;	//	GEVNTCOUNT_1
	XHCI_reg[203]=	0x0000C420	;	//	GEVNTADRLO_2
	XHCI_reg[204]=	0x0000C424	;	//	GEVNTADRHI_2
	XHCI_reg[205]=	0x0000C428	;	//	GEVNTSIZ_2
	XHCI_reg[206]=	0x0000C42C	;	//	GEVNTCOUNT_2
	XHCI_reg[207]=	0x0000C430	;	//	GEVNTADRLO_3
	XHCI_reg[208]=	0x0000C434	;	//	GEVNTADRHI_3
	XHCI_reg[209]=	0x0000C438	;	//	GEVNTSIZ_3
	XHCI_reg[210]=	0x0000C43C	;	//	GEVNTCOUNT_3
	XHCI_reg[211]=	0x0000C600	;	//	GHWPARAMS8
	XHCI_reg[212]=	0x0000C610	;	//	GTXFIFOPRIDEV
	XHCI_reg[213]=	0x0000C618	;	//	GTXFIFOPRIHST
	XHCI_reg[214]=	0x0000C61C	;	//	GRXFIFOPRIHST
	XHCI_reg[215]=	0x0000C620	;	//	GFIFOPRIDBC
	XHCI_reg[216]=	0x0000C624	;	//	GDMAHLRATIO
	XHCI_reg[217]=	0x0000C630	;	//	GFLADJ
	XHCI_reg[218]=	0x0000C700	;	//	DCFG
	XHCI_reg[219]=	0x0000C704	;	//	DCTL
	XHCI_reg[220]=	0x0000C708	;	//	DEVTEN
	XHCI_reg[221]=	0x0000C70C	;	//	DSTS
	XHCI_reg[222]=	0x0000C710	;	//	DGCMDPAR
	XHCI_reg[223]=	0x0000C714	;	//	DGCMD
	XHCI_reg[224]=	0x0000C720	;	//	DALEPENA
	XHCI_reg[225]=	0x0000C800	;	//	DEPCMDPAR2_0
	XHCI_reg[226]=	0x0000C804	;	//	DEPCMDPAR1_0
	XHCI_reg[227]=	0x0000C808	;	//	DEPCMDPAR0_0
	XHCI_reg[228]=	0x0000C80C	;	//	DEPCMD_0
	XHCI_reg[229]=	0x0000C810	;	//	DEPCMDPAR2_1
	XHCI_reg[230]=	0x0000C814	;	//	DEPCMDPAR1_1
	XHCI_reg[231]=	0x0000C818	;	//	DEPCMDPAR0_1
	XHCI_reg[232]=	0x0000C81C	;	//	DEPCMD_1
	XHCI_reg[233]=	0x0000C820	;	//	DEPCMDPAR2_2
	XHCI_reg[234]=	0x0000C824	;	//	DEPCMDPAR1_2
	XHCI_reg[235]=	0x0000C828	;	//	DEPCMDPAR0_2
	XHCI_reg[236]=	0x0000C82C	;	//	DEPCMD_2
	XHCI_reg[237]=	0x0000C830	;	//	DEPCMDPAR2_3
	XHCI_reg[238]=	0x0000C834	;	//	DEPCMDPAR1_3
	XHCI_reg[239]=	0x0000C838	;	//	DEPCMDPAR0_3
	XHCI_reg[240]=	0x0000C83C	;	//	DEPCMD_3
	XHCI_reg[241]=	0x0000C840	;	//	DEPCMDPAR2_4
	XHCI_reg[242]=	0x0000C844	;	//	DEPCMDPAR1_4
	XHCI_reg[243]=	0x0000C848	;	//	DEPCMDPAR0_4
	XHCI_reg[244]=	0x0000C84C	;	//	DEPCMD_4
	XHCI_reg[245]=	0x0000C850	;	//	DEPCMDPAR2_5
	XHCI_reg[246]=	0x0000C854	;	//	DEPCMDPAR1_5
	XHCI_reg[247]=	0x0000C858	;	//	DEPCMDPAR0_5
	XHCI_reg[248]=	0x0000C85C	;	//	DEPCMD_5
	XHCI_reg[249]=	0x0000C860	;	//	DEPCMDPAR2_6
	XHCI_reg[250]=	0x0000C864	;	//	DEPCMDPAR1_6
	XHCI_reg[251]=	0x0000C868	;	//	DEPCMDPAR0_6
	XHCI_reg[252]=	0x0000C86C	;	//	DEPCMD_6
	XHCI_reg[253]=	0x0000C870	;	//	DEPCMDPAR2_7
	XHCI_reg[254]=	0x0000C874	;	//	DEPCMDPAR1_7
	XHCI_reg[255]=	0x0000C878	;	//	DEPCMDPAR0_7
	XHCI_reg[256]=	0x0000C87C	;	//	DEPCMD_7
	XHCI_reg[257]=	0x0000C880	;	//	DEPCMDPAR2_8
	XHCI_reg[258]=	0x0000C884	;	//	DEPCMDPAR1_8
	XHCI_reg[259]=	0x0000C888	;	//	DEPCMDPAR0_8
	XHCI_reg[260]=	0x0000C88C	;	//	DEPCMD_8
	XHCI_reg[261]=	0x0000C890	;	//	DEPCMDPAR2_9
	XHCI_reg[262]=	0x0000C894	;	//	DEPCMDPAR1_9
	XHCI_reg[263]=	0x0000C898	;	//	DEPCMDPAR0_9
	XHCI_reg[264]=	0x0000C89C	;	//	DEPCMD_9
	XHCI_reg[265]=	0x0000C8A0	;	//	DEPCMDPAR2_10
	XHCI_reg[266]=	0x0000C8A4	;	//	DEPCMDPAR1_10
	XHCI_reg[267]=	0x0000C8A8	;	//	DEPCMDPAR0_10
	XHCI_reg[268]=	0x0000C8AC	;	//	DEPCMD_10
	XHCI_reg[269]=	0x0000C8B0	;	//	DEPCMDPAR2_11
	XHCI_reg[270]=	0x0000C8B4	;	//	DEPCMDPAR1_11
	XHCI_reg[271]=	0x0000C8B8	;	//	DEPCMDPAR0_11
	XHCI_reg[272]=	0x0000C8BC	;	//	DEPCMD_11
	XHCI_reg[273]=	0x0000CC00	;	//	OCFG
	XHCI_reg[274]=	0x0000CC04	;	//	OCTL
	XHCI_reg[275]=	0x0000CC08	;	//	OEVT
	XHCI_reg[276]=	0x0000CC0C	;	//	OEVTEN
	XHCI_reg[277]=	0x0000CC10	;	//	OSTS
	XHCI_reg[278]=	0x0000CC28	;	//	ADPEVT

	DP_reg[0]=	0x00000000	;	//	DP_LINK_BW_SET
	DP_reg[1]=	0x00000004	;	//	DP_LANE_COUNT_SET
	DP_reg[2]=	0x00000008	;	//	DP_ENHANCED_FRAME_EN
	DP_reg[3]=	0x0000000C	;	//	DP_TRAINING_PATTERN_SET
	DP_reg[4]=	0x00000010	;	//	DP_LINK_QUAL_PATTERN_SET
	DP_reg[5]=	0x00000014	;	//	DP_SCRAMBLING_DISABLE
	DP_reg[6]=	0x00000018	;	//	DP_DOWNSPREAD_CTRL
	DP_reg[7]=	0x0000001C	;	//	DP_SOFTWARE_RESET
	DP_reg[8]=	0x00000020	;	//	DP_COMP_PATTERN_80BIT_1
	DP_reg[9]=	0x00000024	;	//	DP_COMP_PATTERN_80BIT_2
	DP_reg[10]=	0x00000028	;	//	DP_COMP_PATTERN_80BIT_3
	DP_reg[11]=	0x00000080	;	//	DP_TRANSMITTER_ENABLE
	DP_reg[12]=	0x00000084	;	//	DP_MAIN_STREAM_ENABLE
	DP_reg[13]=	0x000000C0	;	//	DP_FORCE_SCRAMBLER_RESET
	DP_reg[14]=	0x000000F8	;	//	DP_VERSION_REGISTER
	DP_reg[15]=	0x000000FC	;	//	DP_CORE_ID
	DP_reg[16]=	0x00000100	;	//	DP_AUX_COMMAND_REGISTER
	DP_reg[17]=	0x00000104	;	//	DP_AUX_WRITE_FIFO
	DP_reg[18]=	0x00000108	;	//	DP_AUX_ADDRESS
	DP_reg[19]=	0x0000010C	;	//	DP_AUX_CLOCK_DIVIDER
	DP_reg[20]=	0x00000110	;	//	DP_TX_USER_FIFO_OVERFLOW
	DP_reg[21]=	0x00000130	;	//	DP_INTERRUPT_SIGNAL_STATE
	DP_reg[22]=	0x00000134	;	//	DP_AUX_REPLY_DATA
	DP_reg[23]=	0x00000138	;	//	DP_AUX_REPLY_CODE
	DP_reg[24]=	0x0000013C	;	//	DP_AUX_REPLY_COUNT
	DP_reg[25]=	0x00000148	;	//	DP_REPLY_DATA_COUNT
	DP_reg[26]=	0x0000014C	;	//	DP_REPLY_STATUS
	DP_reg[27]=	0x00000150	;	//	DP_HPD_DURATION
	DP_reg[28]=	0x00000180	;	//	DP_MAIN_STREAM_HTOTAL
	DP_reg[29]=	0x00000184	;	//	DP_MAIN_STREAM_VTOTAL
	DP_reg[30]=	0x00000188	;	//	DP_MAIN_STREAM_POLARITY
	DP_reg[31]=	0x0000018C	;	//	DP_MAIN_STREAM_HSWIDTH
	DP_reg[32]=	0x00000190	;	//	DP_MAIN_STREAM_VSWIDTH
	DP_reg[33]=	0x00000194	;	//	DP_MAIN_STREAM_HRES
	DP_reg[34]=	0x00000198	;	//	DP_MAIN_STREAM_VRES
	DP_reg[35]=	0x0000019C	;	//	DP_MAIN_STREAM_HSTART
	DP_reg[36]=	0x000001A0	;	//	DP_MAIN_STREAM_VSTART
	DP_reg[37]=	0x000001A4	;	//	DP_MAIN_STREAM_MISC0
	DP_reg[38]=	0x000001A8	;	//	DP_MAIN_STREAM_MISC1
	DP_reg[39]=	0x000001AC	;	//	DP_MAIN_STREAM_M_VID
	DP_reg[40]=	0x000001B0	;	//	DP_MSA_TRANSFER_UNIT_SIZE
	DP_reg[41]=	0x000001B4	;	//	DP_MAIN_STREAM_N_VID
	DP_reg[42]=	0x000001B8	;	//	DP_USER_PIX_WIDTH
	DP_reg[43]=	0x000001BC	;	//	DP_USER_DATA_COUNT_PER_LANE
	DP_reg[44]=	0x000001C4	;	//	DP_MIN_BYTES_PER_TU
	DP_reg[45]=	0x000001C8	;	//	DP_FRAC_BYTES_PER_TU
	DP_reg[46]=	0x000001CC	;	//	DP_INIT_WAIT
	DP_reg[47]=	0x00000200	;	//	DP_PHY_RESET
	DP_reg[48]=	0x00000230	;	//	DP_TRANSMIT_PRBS7
	DP_reg[49]=	0x00000234	;	//	DP_PHY_CLOCK_SELECT
	DP_reg[50]=	0x00000238	;	//	DP_TX_PHY_POWER_DOWN
	DP_reg[51]=	0x0000024C	;	//	DP_PHY_PRECURSOR_LANE_0
	DP_reg[52]=	0x00000250	;	//	DP_PHY_PRECURSOR_LANE_1
	DP_reg[53]=	0x00000280	;	//	DP_PHY_STATUS
	DP_reg[54]=	0x00000300	;	//	DP_TX_AUDIO_CONTROL
	DP_reg[55]=	0x00000304	;	//	DP_TX_AUDIO_CHANNELS
	DP_reg[56]=	0x00000308	;	//	DP_TX_AUDIO_INFO_DATA0
	DP_reg[57]=	0x0000030C	;	//	DP_TX_AUDIO_INFO_DATA1
	DP_reg[58]=	0x00000310	;	//	DP_TX_AUDIO_INFO_DATA2
	DP_reg[59]=	0x00000314	;	//	DP_TX_AUDIO_INFO_DATA3
	DP_reg[60]=	0x00000318	;	//	DP_TX_AUDIO_INFO_DATA4
	DP_reg[61]=	0x0000031C	;	//	DP_TX_AUDIO_INFO_DATA5
	DP_reg[62]=	0x00000320	;	//	DP_TX_AUDIO_INFO_DATA6
	DP_reg[63]=	0x00000324	;	//	DP_TX_AUDIO_INFO_DATA7
	DP_reg[64]=	0x00000328	;	//	DP_TX_M_AUD
	DP_reg[65]=	0x0000032C	;	//	DP_TX_N_AUD
	DP_reg[66]=	0x00000330	;	//	DP_TX_AUDIO_EXT_DATA0
	DP_reg[67]=	0x00000334	;	//	DP_TX_AUDIO_EXT_DATA1
	DP_reg[68]=	0x00000338	;	//	DP_TX_AUDIO_EXT_DATA2
	DP_reg[69]=	0x0000033C	;	//	DP_TX_AUDIO_EXT_DATA3
	DP_reg[70]=	0x00000340	;	//	DP_TX_AUDIO_EXT_DATA4
	DP_reg[71]=	0x00000344	;	//	DP_TX_AUDIO_EXT_DATA5
	DP_reg[72]=	0x00000348	;	//	DP_TX_AUDIO_EXT_DATA6
	DP_reg[73]=	0x0000034C	;	//	DP_TX_AUDIO_EXT_DATA7
	DP_reg[74]=	0x00000350	;	//	DP_TX_AUDIO_EXT_DATA8
	DP_reg[75]=	0x000003A0	;	//	DP_INT_STATUS
	DP_reg[76]=	0x000003A4	;	//	DP_INT_MASK
	DP_reg[77]=	0x000003A8	;	//	DP_INT_EN
	DP_reg[78]=	0x000003AC	;	//	DP_INT_DS
	DP_reg[79]=	0x0000A000	;	//	V_BLEND_BG_CLR_0
	DP_reg[80]=	0x0000A004	;	//	V_BLEND_BG_CLR_1
	DP_reg[81]=	0x0000A008	;	//	V_BLEND_BG_CLR_2
	DP_reg[82]=	0x0000A00C	;	//	V_BLEND_SET_GLOBAL_ALPHA_REG
	DP_reg[83]=	0x0000A014	;	//	V_BLEND_OUTPUT_VID_FORMAT
	DP_reg[84]=	0x0000A018	;	//	V_BLEND_LAYER0_CONTROL
	DP_reg[85]=	0x0000A01C	;	//	V_BLEND_LAYER1_CONTROL
	DP_reg[86]=	0x0000A020	;	//	V_BLEND_RGB2YCBCR_COEFF0
	DP_reg[87]=	0x0000A024	;	//	V_BLEND_RGB2YCBCR_COEFF1
	DP_reg[88]=	0x0000A028	;	//	V_BLEND_RGB2YCBCR_COEFF2
	DP_reg[89]=	0x0000A02C	;	//	V_BLEND_RGB2YCBCR_COEFF3
	DP_reg[90]=	0x0000A030	;	//	V_BLEND_RGB2YCBCR_COEFF4
	DP_reg[91]=	0x0000A034	;	//	V_BLEND_RGB2YCBCR_COEFF5
	DP_reg[92]=	0x0000A038	;	//	V_BLEND_RGB2YCBCR_COEFF6
	DP_reg[93]=	0x0000A03C	;	//	V_BLEND_RGB2YCBCR_COEFF7
	DP_reg[94]=	0x0000A040	;	//	V_BLEND_RGB2YCBCR_COEFF8
	DP_reg[95]=	0x0000A044	;	//	V_BLEND_IN1CSC_COEFF0
	DP_reg[96]=	0x0000A048	;	//	V_BLEND_IN1CSC_COEFF1
	DP_reg[97]=	0x0000A04C	;	//	V_BLEND_IN1CSC_COEFF2
	DP_reg[98]=	0x0000A050	;	//	V_BLEND_IN1CSC_COEFF3
	DP_reg[99]=	0x0000A054	;	//	V_BLEND_IN1CSC_COEFF4
	DP_reg[100]=	0x0000A058	;	//	V_BLEND_IN1CSC_COEFF5
	DP_reg[101]=	0x0000A05C	;	//	V_BLEND_IN1CSC_COEFF6
	DP_reg[102]=	0x0000A060	;	//	V_BLEND_IN1CSC_COEFF7
	DP_reg[103]=	0x0000A064	;	//	V_BLEND_IN1CSC_COEFF8
	DP_reg[104]=	0x0000A068	;	//	V_BLEND_LUMA_IN1CSC_OFFSET
	DP_reg[105]=	0x0000A06C	;	//	V_BLEND_CR_IN1CSC_OFFSET
	DP_reg[106]=	0x0000A070	;	//	V_BLEND_CB_IN1CSC_OFFSET
	DP_reg[107]=	0x0000A074	;	//	V_BLEND_LUMA_OUTCSC_OFFSET
	DP_reg[108]=	0x0000A078	;	//	V_BLEND_CR_OUTCSC_OFFSET
	DP_reg[109]=	0x0000A07C	;	//	V_BLEND_CB_OUTCSC_OFFSET
	DP_reg[110]=	0x0000A080	;	//	V_BLEND_IN2CSC_COEFF0
	DP_reg[111]=	0x0000A084	;	//	V_BLEND_IN2CSC_COEFF1
	DP_reg[112]=	0x0000A088	;	//	V_BLEND_IN2CSC_COEFF2
	DP_reg[113]=	0x0000A08C	;	//	V_BLEND_IN2CSC_COEFF3
	DP_reg[114]=	0x0000A090	;	//	V_BLEND_IN2CSC_COEFF4
	DP_reg[115]=	0x0000A094	;	//	V_BLEND_IN2CSC_COEFF5
	DP_reg[116]=	0x0000A098	;	//	V_BLEND_IN2CSC_COEFF6
	DP_reg[117]=	0x0000A09C	;	//	V_BLEND_IN2CSC_COEFF7
	DP_reg[118]=	0x0000A0A0	;	//	V_BLEND_IN2CSC_COEFF8
	DP_reg[119]=	0x0000A0A4	;	//	V_BLEND_LUMA_IN2CSC_OFFSET
	DP_reg[120]=	0x0000A0A8	;	//	V_BLEND_CR_IN2CSC_OFFSET
	DP_reg[121]=	0x0000A0AC	;	//	V_BLEND_CB_IN2CSC_OFFSET
	DP_reg[122]=	0x0000A1D0	;	//	V_BLEND_CHROMA_KEY_ENABLE
	DP_reg[123]=	0x0000A1D4	;	//	V_BLEND_CHROMA_KEY_COMP1
	DP_reg[124]=	0x0000A1D8	;	//	V_BLEND_CHROMA_KEY_COMP2
	DP_reg[125]=	0x0000A1DC	;	//	V_BLEND_CHROMA_KEY_COMP3
	DP_reg[126]=	0x0000B000	;	//	AV_BUF_FORMAT
	DP_reg[127]=	0x0000B008	;	//	AV_BUF_NON_LIVE_LATENCY
	DP_reg[128]=	0x0000B010	;	//	AV_CHBUF0
	DP_reg[129]=	0x0000B014	;	//	AV_CHBUF1
	DP_reg[130]=	0x0000B018	;	//	AV_CHBUF2
	DP_reg[131]=	0x0000B01C	;	//	AV_CHBUF3
	DP_reg[132]=	0x0000B020	;	//	AV_CHBUF4
	DP_reg[133]=	0x0000B024	;	//	AV_CHBUF5
	DP_reg[134]=	0x0000B02C	;	//	AV_BUF_STC_CONTROL
	DP_reg[135]=	0x0000B030	;	//	AV_BUF_STC_INIT_VALUE0
	DP_reg[136]=	0x0000B034	;	//	AV_BUF_STC_INIT_VALUE1
	DP_reg[137]=	0x0000B038	;	//	AV_BUF_STC_ADJ
	DP_reg[138]=	0x0000B03C	;	//	AV_BUF_STC_VIDEO_VSYNC_TS_REG0
	DP_reg[139]=	0x0000B040	;	//	AV_BUF_STC_VIDEO_VSYNC_TS_REG1
	DP_reg[140]=	0x0000B044	;	//	AV_BUF_STC_EXT_VSYNC_TS_REG0
	DP_reg[141]=	0x0000B048	;	//	AV_BUF_STC_EXT_VSYNC_TS_REG1
	DP_reg[142]=	0x0000B04C	;	//	AV_BUF_STC_CUSTOM_EVENT_TS_REG0
	DP_reg[143]=	0x0000B050	;	//	AV_BUF_STC_CUSTOM_EVENT_TS_REG1
	DP_reg[144]=	0x0000B054	;	//	AV_BUF_STC_CUSTOM_EVENT2_TS_REG0
	DP_reg[145]=	0x0000B058	;	//	AV_BUF_STC_CUSTOM_EVENT2_TS_REG1
	DP_reg[146]=	0x0000B060	;	//	AV_BUF_STC_SNAPSHOT0
	DP_reg[147]=	0x0000B064	;	//	AV_BUF_STC_SNAPSHOT1
	DP_reg[148]=	0x0000B070	;	//	AV_BUF_OUTPUT_AUDIO_VIDEO_SELECT
	DP_reg[149]=	0x0000B074	;	//	AV_BUF_HCOUNT_VCOUNT_INT0
	DP_reg[150]=	0x0000B078	;	//	AV_BUF_HCOUNT_VCOUNT_INT1
	DP_reg[151]=	0x0000B07C	;	//	AV_BUF_DITHER_CONFIG
	DP_reg[152]=	0x0000B080	;	//	DITHER_CONFIG_SEED0
	DP_reg[153]=	0x0000B084	;	//	DITHER_CONFIG_SEED1
	DP_reg[154]=	0x0000B088	;	//	DITHER_CONFIG_SEED2
	DP_reg[155]=	0x0000B08C	;	//	DITHER_CONFIG_MAX
	DP_reg[156]=	0x0000B090	;	//	DITHER_CONFIG_MIN
	DP_reg[157]=	0x0000B100	;	//	PATTERN_GEN_SELECT
	DP_reg[158]=	0x0000B104	;	//	AUD_PATTERN_SELECT1
	DP_reg[159]=	0x0000B108	;	//	AUD_PATTERN_SELECT2
	DP_reg[160]=	0x0000B120	;	//	AV_BUF_AUD_VID_CLK_SOURCE
	DP_reg[161]=	0x0000B124	;	//	AV_BUF_SRST_REG
	DP_reg[162]=	0x0000B128	;	//	AV_BUF_AUDIO_RDY_INTERVAL
	DP_reg[163]=	0x0000B12C	;	//	AV_BUF_AUDIO_CH_CONFIG
	DP_reg[164]=	0x0000B200	;	//	AV_BUF_GRAPHICS_COMP0_SCALE_FACTOR
	DP_reg[165]=	0x0000B204	;	//	AV_BUF_GRAPHICS_COMP1_SCALE_FACTOR
	DP_reg[166]=	0x0000B208	;	//	AV_BUF_GRAPHICS_COMP2_SCALE_FACTOR
	DP_reg[167]=	0x0000B20C	;	//	AV_BUF_VIDEO_COMP0_SCALE_FACTOR
	DP_reg[168]=	0x0000B210	;	//	AV_BUF_VIDEO_COMP1_SCALE_FACTOR
	DP_reg[169]=	0x0000B214	;	//	AV_BUF_VIDEO_COMP2_SCALE_FACTOR
	DP_reg[170]=	0x0000B218	;	//	AV_BUF_LIVE_VIDEO_COMP0_SF
	DP_reg[171]=	0x0000B21C	;	//	AV_BUF_LIVE_VIDEO_COMP1_SF
	DP_reg[172]=	0x0000B220	;	//	AV_BUF_LIVE_VIDEO_COMP2_SF
	DP_reg[173]=	0x0000B224	;	//	AV_BUF_LIVE_VID_CONFIG
	DP_reg[174]=	0x0000B228	;	//	AV_BUF_LIVE_GFX_COMP0_SF
	DP_reg[175]=	0x0000B22C	;	//	AV_BUF_LIVE_GFX_COMP1_SF
	DP_reg[176]=	0x0000B230	;	//	AV_BUF_LIVE_GFX_COMP2_SF
	DP_reg[177]=	0x0000B234	;	//	AV_BUF_LIVE_GFX_CONFIG
	DP_reg[178]=	0x0000C000	;	//	AUDIO_MIXER_VOLUME_CONTROL
	DP_reg[179]=	0x0000C004	;	//	AUDIO_MIXER_META_DATA
	DP_reg[180]=	0x0000C008	;	//	AUD_CH_STATUS_REG0
	DP_reg[181]=	0x0000C00C	;	//	AUD_CH_STATUS_REG1
	DP_reg[182]=	0x0000C010	;	//	AUD_CH_STATUS_REG2
	DP_reg[183]=	0x0000C014	;	//	AUD_CH_STATUS_REG3
	DP_reg[184]=	0x0000C018	;	//	AUD_CH_STATUS_REG4
	DP_reg[185]=	0x0000C01C	;	//	AUD_CH_STATUS_REG5
	DP_reg[186]=	0x0000C020	;	//	AUD_CH_A_DATA_REG0
	DP_reg[187]=	0x0000C024	;	//	AUD_CH_A_DATA_REG1
	DP_reg[188]=	0x0000C028	;	//	AUD_CH_A_DATA_REG2
	DP_reg[189]=	0x0000C02C	;	//	AUD_CH_A_DATA_REG3
	DP_reg[190]=	0x0000C030	;	//	AUD_CH_A_DATA_REG4
	DP_reg[191]=	0x0000C034	;	//	AUD_CH_A_DATA_REG5
	DP_reg[192]=	0x0000C038	;	//	AUD_CH_B_DATA_REG0
	DP_reg[193]=	0x0000C03C	;	//	AUD_CH_B_DATA_REG1
	DP_reg[194]=	0x0000C040	;	//	AUD_CH_B_DATA_REG2
	DP_reg[195]=	0x0000C044	;	//	AUD_CH_B_DATA_REG3
	DP_reg[196]=	0x0000C048	;	//	AUD_CH_B_DATA_REG4
	DP_reg[197]=	0x0000C04C	;	//	AUD_CH_B_DATA_REG5
	DP_reg[198]=	0x0000CC00	;	//	AUDIO_SOFT_RESET
	DP_reg[199]=	0x0000CC10	;	//	PATGEN_CRC_R
	DP_reg[200]=	0x0000CC14	;	//	PATGEN_CRC_G
	DP_reg[201]=	0x0000CC18	;	//	PATGEN_CRC_B

	DPDMA_reg[0]=	0x00000000	;	//	DPDMA_ERR_CTRL
	DPDMA_reg[1]=	0x00000004	;	//	DPDMA_ISR
	DPDMA_reg[2]=	0x00000008	;	//	DPDMA_IMR
	DPDMA_reg[3]=	0x0000000C	;	//	DPDMA_IEN
	DPDMA_reg[4]=	0x00000010	;	//	DPDMA_IDS
	DPDMA_reg[5]=	0x00000014	;	//	DPDMA_EISR
	DPDMA_reg[6]=	0x00000018	;	//	DPDMA_EIMR
	DPDMA_reg[7]=	0x0000001C	;	//	DPDMA_EIEN
	DPDMA_reg[8]=	0x00000020	;	//	DPDMA_EIDS
	DPDMA_reg[9]=	0x00000100	;	//	DPDMA_CNTL
	DPDMA_reg[10]=	0x00000104	;	//	DPDMA_GBL
	DPDMA_reg[11]=	0x00000108	;	//	DPDMA_ALC0_CNTL
	DPDMA_reg[12]=	0x0000010C	;	//	DPDMA_ALC0_STATUS
	DPDMA_reg[13]=	0x00000110	;	//	DPDMA_ALC0_MAX
	DPDMA_reg[14]=	0x00000114	;	//	DPDMA_ALC0_MIN
	DPDMA_reg[15]=	0x00000118	;	//	DPDMA_ALC0_ACC
	DPDMA_reg[16]=	0x0000011C	;	//	DPDMA_ALC0_ACC_TRAN
	DPDMA_reg[17]=	0x00000120	;	//	DPDMA_ALC1_CNTL
	DPDMA_reg[18]=	0x00000124	;	//	DPDMA_ALC1_STATUS
	DPDMA_reg[19]=	0x00000128	;	//	DPDMA_ALC1_MAX
	DPDMA_reg[20]=	0x0000012C	;	//	DPDMA_ALC1_MIN
	DPDMA_reg[21]=	0x00000130	;	//	DPDMA_ALC1_ACC
	DPDMA_reg[22]=	0x00000134	;	//	DPDMA_ALC1_ACC_TRAN
	DPDMA_reg[23]=	0x00000200	;	//	DPDMA_CH0_DSCR_STRT_ADDRE
	DPDMA_reg[24]=	0x00000204	;	//	DPDMA_CH0_DSCR_STRT_ADDR
	DPDMA_reg[25]=	0x00000218	;	//	DPDMA_CH0_CNTL
	DPDMA_reg[26]=	0x00000300	;	//	DPDMA_CH1_DSCR_STRT_ADDRE
	DPDMA_reg[27]=	0x00000304	;	//	DPDMA_CH1_DSCR_STRT_ADDR
	DPDMA_reg[28]=	0x00000318	;	//	DPDMA_CH1_CNTL
	DPDMA_reg[29]=	0x00000400	;	//	DPDMA_CH2_DSCR_STRT_ADDRE
	DPDMA_reg[30]=	0x00000404	;	//	DPDMA_CH2_DSCR_STRT_ADDR
	DPDMA_reg[31]=	0x00000418	;	//	DPDMA_CH2_CNTL
	DPDMA_reg[32]=	0x00000500	;	//	DPDMA_CH3_DSCR_STRT_ADDRE
	DPDMA_reg[33]=	0x00000504	;	//	DPDMA_CH3_DSCR_STRT_ADDR
	DPDMA_reg[34]=	0x00000518	;	//	DPDMA_CH3_CNTL
	DPDMA_reg[35]=	0x00000600	;	//	DPDMA_CH4_DSCR_STRT_ADDRE
	DPDMA_reg[36]=	0x00000604	;	//	DPDMA_CH4_DSCR_STRT_ADDR
	DPDMA_reg[37]=	0x00000618	;	//	DPDMA_CH4_CNTL
	DPDMA_reg[38]=	0x00000700	;	//	DPDMA_CH5_DSCR_STRT_ADDRE
	DPDMA_reg[39]=	0x00000704	;	//	DPDMA_CH5_DSCR_STRT_ADDR
	DPDMA_reg[40]=	0x00000718	;	//	DPDMA_CH5_CNTL

	for(i = 0; i<XHCI_NUM; i++){
		XHCI_check[i] = 1;
	}
	for(i = 0; i<DP_NUM; i++){
		DP_check[i] = 1;
	}
	for(i = 0; i<DPDMA_NUM; i++){
		DPDMA_check[i] = 1;
	}

	/* add registers which can be ignored here */
//	XHCI_check[25] = 0;
//	DP_check[22] = 0;
//	DP_check[146] = 0;

}
void main_reg_set(void)
{
            DMSG("main_reg_set================================\n");
            vaddr_t reg_base;


            reg_base = (vaddr_t)phys_to_virt(XHCI_BASE, MEM_AREA_IO_SEC);
	    for(int i=0; i<XHCI_NUM; i++){
		    XHCI_known_reg[i] = read32(reg_base + XHCI_reg[i]);    
	    } 
	    reg_base = (vaddr_t)phys_to_virt(DP_BASE, MEM_AREA_IO_SEC);
	    for(int i=0; i<DP_NUM; i++){
		     DP_known_reg[i] = read32(reg_base + DP_reg[i]);    
	    } 
	    reg_base = (vaddr_t)phys_to_virt(DPDMA_BASE, MEM_AREA_IO_SEC);
	    for(int i=0; i<DPDMA_NUM; i++){
		     DPDMA_known_reg[i] = read32(reg_base + DPDMA_reg[i]);    
	    } 
            
	    for(int j=1; j<100; j++){
            	    reg_base = (vaddr_t)phys_to_virt(XHCI_BASE, MEM_AREA_IO_SEC);
		    for(int i=0; i<XHCI_NUM; i++){
			     //DMSG("----%x, %x\n", reg_base + XHCI_reg[i], read32(reg_base + XHCI_reg[i]));
			    if(XHCI_known_reg[i] != read32(reg_base + XHCI_reg[i]) && XHCI_check[i]){
				   // XHCI_check[i] = 0;
			    	    DMSG("Don't check XHCI_check[%d] %x != %x\n", i, XHCI_known_reg[i],  read32(reg_base + XHCI_reg[i]) );
			    	    XHCI_known_reg[i] = read32(reg_base + XHCI_reg[i]);    
			    }
		    } 
		    reg_base = (vaddr_t)phys_to_virt(DP_BASE, MEM_AREA_IO_SEC);
		    for(int i=0; i<DP_NUM; i++){
			     //DMSG("----%x, %x\n", reg_base + DP_reg[i], read32(reg_base + DP_reg[i]));
			    if(DP_known_reg[i] != read32(reg_base + DP_reg[i]) && DP_check[i]){
				   // DP_check[i] = 0;
			    	    DMSG("Don't check DP_check[%d] %x != %x\n", i, DP_known_reg[i],  read32(reg_base + DP_reg[i]) );
			    	    DP_known_reg[i] = read32(reg_base + DP_reg[i]);    
			    }
		    } 
		    reg_base = (vaddr_t)phys_to_virt(DPDMA_BASE, MEM_AREA_IO_SEC);
		    for(int i=0; i<DPDMA_NUM; i++){
			     //DMSG("--+--%x, %x\n", reg_base + DPDMA_reg[i], read32(reg_base + DPDMA_reg[i]));
			    if(DPDMA_known_reg[i] != read32(reg_base + DPDMA_reg[i]) && DPDMA_check[i]){
				   // DPDMA_check[i] = 0;
			    	    DMSG("Don't check DPDMA_check[%d] %x != %x\n", i, DPDMA_known_reg[i],  read32(reg_base + DPDMA_reg[i]) );
			    	    DPDMA_known_reg[i] = read32(reg_base + DPDMA_reg[i]);    
			    }
		    } 
	    }
	
	/* Get mouse buffer address */
	/*
	vaddr_t erdp;
	vaddr_t trb;
	reg_base = (vaddr_t)phys_to_virt(XHCI_BASE, MEM_AREA_IO_SEC);
	erdp = read32(reg_base + 0x0478);
	DMSG("-----ERDP: %x\n", erdp); 
	reg_base = (vaddr_t)phys_to_virt(erdp - 0x10, MEM_AREA_NSEC_SHM);
	trb = read32(reg_base);
	DMSG("-----TRB: %x\n", trb); 
	reg_base = (vaddr_t)phys_to_virt(trb, MEM_AREA_NSEC_SHM);
	embassy_mouse_buffer = read32(reg_base);
	DMSG("-----mouse buffer: %x\n", embassy_mouse_buffer); 
*/
    
}
int main_reg_compare(void)
{
            vaddr_t reg_base;
            DMSG("main_reg_compare================================\n");

            reg_base = (vaddr_t)phys_to_virt(XHCI_BASE, MEM_AREA_IO_SEC);
            for(int i=0; i<XHCI_NUM; i++){
                     DMSG("----=-%x, %x\n", XHCI_known_reg[i], read32(reg_base + XHCI_reg[i]));
	 	     if(XHCI_check[i])
		     	     if(XHCI_known_reg[i] != read32(reg_base + XHCI_reg[i])){
			//	     return 0;
			     }
            } 
            reg_base = (vaddr_t)phys_to_virt(DP_BASE, MEM_AREA_IO_SEC);
            for(int i=0; i<DP_NUM; i++){
                     DMSG("----=-%x, %x\n", DP_known_reg[i], read32(reg_base + DP_reg[i]));
	 	     if(DP_check[i])
			     if(DP_known_reg[i] != read32(reg_base + DP_reg[i])){
				//     return 0;
				}
	    } 
            reg_base = (vaddr_t)phys_to_virt(DPDMA_BASE, MEM_AREA_IO_SEC);
            for(int i=0; i<DPDMA_NUM; i++){
                     DMSG("----=-%x, %x\n", DPDMA_known_reg[i], read32(reg_base + DPDMA_reg[i]));
	 	     if(DPDMA_check[i])
			     if(DPDMA_known_reg[i] != read32(reg_base + DPDMA_reg[i])){
				 //     return 0;
				}
            } 

}
