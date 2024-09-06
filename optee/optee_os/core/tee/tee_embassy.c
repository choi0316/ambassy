#include <tee/tee_embassy.h>
#include <stdlib.h>
#include <tee_api_defines.h>
#include <trace.h>
#include <kernel/tee_ta_manager.h>
#include <tee_api.h>
#include <string.h>
#include <time.h>

#include <mm/core_memprot.h>
#include <io.h>
#include "tomcrypt.h"

#define BASE_ADDRESS    0xA0000000
#define ISR 		0x0
#define IER 		0x4
#define TDFR 		0x8
#define TDFV 		0xc
#define TDFD 		0x10
#define TLR 		0x14
#define RDFR 		0x18
#define RDFO 		0x1c
#define RDFD 		0x20
#define RLR 		0x24
#define SRR 		0x28
#define TDR 		0x2c
#define RDR 		0x30

struct mem_region_t {
    vaddr_t base;
    vaddr_t mask;
    vaddr_t size;
};
static struct {
    struct mem_region_t c;
    struct mem_region_t r2; // TZPC??
    struct mem_region_t r3; // ICAP??
    struct mem_region_t r4; //XMPU??
    //XXX: very dirty...............
} regions;

static uint8_t regions_init = 0;



int obtain_mem_regions(void) {
    regions.c.base = (vaddr_t)phys_to_virt(BASE_ADDRESS, MEM_AREA_IO_SEC);
    regions.c.mask = 0xFFFF0000;
    regions.r2.base = (vaddr_t)phys_to_virt(0x80000000, MEM_AREA_IO_SEC);
    regions.r2.mask = 0xFFFF0000;
    regions.r3.base = (vaddr_t)phys_to_virt(0xFFCA3000, MEM_AREA_IO_SEC);
    regions.r3.mask = 0xFFFF0000;
    regions.r4.base = (vaddr_t)phys_to_virt(0xFD000000, MEM_AREA_IO_SEC);
    regions.r4.size = 0x60000;

    regions_init = 1;
    return 0;
}


static inline int do_check(vaddr_t addr) {
    int in_region[4];
    in_region[0] = (addr & regions.c.mask) == regions.c.base; 
    in_region[1] = (addr & regions.r2.mask) == regions.r2.base; 
    in_region[2] = (addr & regions.r3.mask) == (regions.r3.base & regions.r3.mask); //XXX: this is correct but dirty 
    in_region[3] = (addr >= regions.r4.base) && (addr < regions.r4.base + regions.r4.size);
    int ret = 0;
    ret = ret == 1 ||
        in_region[0] == 1 ||
        in_region[1] == 1 ||
        in_region[2] == 1 ||
        in_region[3] == 1;
    return ret;

}
#ifdef AMBASSY_CBMC
void* phys_to_virt(paddr_t pa, enum teecore_memtypes m) {
    return (void*)pa;
}

static inline void in_a_region(vaddr_t addr) {
    uint8_t in_region[4];
    in_region[0] = (addr & regions.c.mask) == regions.c.base; 
    in_region[1] = (addr & regions.r2.mask) == regions.r2.base; 
    in_region[2] = (addr & regions.r3.mask) == (regions.r3.base & regions.r3.mask); //XXX: this is correct but dirty 
    in_region[3] = (addr >= regions.r4.base) && (addr < regions.r4.base + regions.r4.size);
    uint8_t ret = 0;
    ret = ret == 1 ||
        in_region[0] == 1 ||
        in_region[1] == 1 ||
        in_region[2] == 1 ||
        in_region[3] == 1;

//    if(ret) DMSG("in_a_regigon error: %x\n", addr);
    assert(ret == 1);
}
#else
static inline void in_a_region(vaddr_t addr) {
    // hope compiler understands..
}
#endif
static inline void write32_wrapper(uint32_t val, vaddr_t addr) {
//    if (!do_check(addr)) return;
    in_a_region(addr);
    write32(val, addr);
}

static inline uint32_t read32_wrapper(vaddr_t addr) {
  //  if (!do_check(addr)) return;
    in_a_region(addr);
    read32(addr);
}



int write_reg(int reg_address, unsigned int value){
	write32_wrapper(value, regions.c.base + reg_address);
	return 0;
}
int read_reg(int reg_address){
    DMSG("Reg 0x%x: 0x%x\n",(BASE_ADDRESS + reg_address), read32_wrapper(regions.c.base + reg_address));
    return 0;
}

/*
#define TEE_PLAYGROUND_CMD_SET_ICAP 0
#define TEE_PLAYGROUND_CMD_UNSET_ICAP 1
#define TEE_PLAYGROUND_CMD_WRITE_ICAP_SETUP 2
#define TEE_PLAYGROUND_CMD_WRITE_ICAP_TRANSMIT 3
#define TEE_PLAYGROUND_CMD_WRITE_ICAP_RESET 4
TODO:
1. Can we load more TAs while we have an active TA already?
2. We should verify the image and generate an attestation. We should implement
an attestation mechanism as it affects the performance.
*/


TEE_Result syscall_embassy_cmd(unsigned long cmd, unsigned long arg0, unsigned long arg1, unsigned long  arg2, unsigned long arg3)
{
    /*
     * 1. obtain the bases for the regions we'll access. 
     * Could affect the performance. If so, we should fix.
     * needed for verification with cbmc (spec-writing...).
    */
	TEE_Result res;
	vaddr_t psu_base;
        int ret = obtain_mem_regions();
        if (ret != 0) {
            DMSG("failed to obtain bases\n");
            return TEE_ERROR_GENERIC;
        }
	int i;
	unsigned char tmp[32];
	hash_state md;
	unsigned char *mapped_base;
	unsigned int nonce;
	unsigned int file_size;
	unsigned char *nonce_file = "Hello World!";
        unsigned int known_value[317] = {0};

        switch (cmd) {
		case TEE_PLAYGROUND_CMD_SET_ICAP:
		      DMSG("syscall_embassy: set accesses======================\n");
		      
       		      psu_base = regions.r2.base;
        	      if(arg0 == 0) {
                                //XXX
                                vaddr_t tested = psu_base + 0x200 + 0x20 * arg1;
                                if(!do_check(tested))
                                    return TEE_ERROR_BAD_PARAMETERS;
		      		write32_wrapper(arg2, tested);
                                tested = psu_base + 0x200 + 0x20 * arg1 + 0x10;
                                if(!do_check(tested))
                                    return TEE_ERROR_BAD_PARAMETERS;
		      		write32_wrapper(arg3,tested);
		      }
		      else {
                          vaddr_t tested = psu_base + 0x280 + 0x10 * arg1;
                          if(!do_check(tested))
                              return TEE_ERROR_BAD_PARAMETERS;
                          write32_wrapper(arg2, tested);
		      }	  

          	      break;
	      	case TEE_PLAYGROUND_CMD_WRITE_ICAP_TRANSMIT:
 
		      DMSG("syscall_embassy: set icap=========================\n");
		      psu_base = regions.r3.base;

		      write32_wrapper(0,psu_base+8);
      
		      DMSG("dihwang---------------- Configure XMPU: %x \n", embassy_mouse_buffer);
		      psu_base = regions.r4.base;

			for(i = 1; i<5; i++){
				//DMSG("Loop %d================\n", i);
				write32_wrapper(embassy_mouse_buffer>>12, psu_base + i*0x10000 +  0x100 );  //start address
				write32_wrapper((embassy_mouse_buffer + 0x100000)>>12, psu_base + i*0x10000 + 0x104 ); //end address
				
				write32_wrapper(0x7, psu_base + i*0x10000 + 0x10c );
				write32_wrapper(0x8, psu_base + i*0x10000 );

			//	if(i==4) write32_wrapper(0x1 << 20, psu_base + i*0x10000 + 0xc ); //Configuring poison makes system stops
				write32_wrapper(0xf, psu_base + i*0x10000 + 0x18 );
				write32_wrapper(0xf, psu_base + i*0x10000 + 0x10 );
			}

		        DMSG("dihwang---------------- Configuring EControl \n");

			DMSG("============dihwang XMPU reconfiguration done\n");

	//	break;
        
	//	case TEE_PLAYGROUND_CMD_WRITE_ICAP_SETUP:
			   DMSG("syscall_embassy: write icap=========================\n");
			   
			   //Power-up/Reset read of Register values
			   DMSG("Power-up/Reset read of Register Values\n");
			   read_reg(ISR);
			   write_reg(ISR, 0xffffffff); 
			   read_reg(ISR);
			   read_reg(IER);
			   read_reg(TDFV);
			   read_reg(RDFO);

			   //Transmit a packet
			   DMSG("Transmit a packet\n");
			   write_reg(IER, 0x0c000000);
			   read_reg(IER);
			   write_reg(TDR, 0x00000002);
	//	break;
        //	case TEE_PLAYGROUND_CMD_WRITE_ICAP_TRANSMIT:
			DMSG("syscall_embassy: Transmit=========================, %x, %d\n", arg0, arg1);
		
                        /*
                         * Special care needed due to the loop depth.
                         *
                         */
                       in_a_region(regions.c.base + ISR);
                       in_a_region(regions.c.base + TDFD);
                       in_a_region(regions.c.base + TLR);
			for (i = 0; i < (int)arg1 / 4; i++){
				write_reg(ISR, 0xffffffff); 
				write_reg(TDFD, *((unsigned int*)arg0 + i));
				write_reg(TLR, 4);//
				//DMSG("%d, %x----------\n",i , *((unsigned int*)arg0 + i) );
			}

	//	break;	
        //	case TEE_PLAYGROUND_CMD_WRITE_ICAP_RESET:
			   read_reg(TDFV);
		  	   read_reg(ISR);
			   write_reg(ISR, 0xffffffff);
			   read_reg(TDFV);
			   read_reg(ISR);
	//	case TEE_PLAYGROUND_CMD_UNSET_ICAP:
		      DMSG("syscall_embassy: unset icap=========================\n");
		      psu_base = regions.r3.base; 
		      
		      //DMSG("DIHWANG ==========pcap_ctrl before write: 0x%x\n", read32_wrapper(psu_base+8));
		      write32_wrapper(1,psu_base+8);
		      //DMSG("DIHWANG ==========pcap_ctrl after write: 0x%x\n", read32_wrapper(psu_base+8));
                break;
        
		case TEE_PLAYGROUND_CMD_ATTEST:
			DMSG("syscall_embassy: Attest====+=====================, %x, %x\n", arg0, arg1, arg2, arg3);
			
			sha256_init(&md);
			sha256_process(&md, (char*)arg0, arg1);//may be problem
			sha256_done(&md, tmp);
			
			for(i=0;i<32;i++){
				((char*)arg2)[i] = tmp[i];
			}

			
		break;	
		case TEE_PLAYGROUND_CMD_REG_CHECK:
		      DMSG("syscall_embassy: Check registers=========================\n");
		      res = TEE_SUCCESS;			
		      res = main_reg_compare();
		 break;
		case TEE_PLAYGROUND_CMD_REG_DISCONNECTED:
		      DMSG("syscall_embassy: Set configure registers=========================\n");
		      res = TEE_SUCCESS;			
		      main_reg_set();
		break;

		case TEE_PLAYGROUND_CMD_SET_IDENTITY:
		      // called after loading the TA successfully
		      DMSG("syscall_embassy: Set identity=========================\n");
		      res = TEE_SUCCESS;			
		      
		      psu_base = regions.r2.base; 
                    write32_wrapper(*(uint32_t*)arg1, psu_base + 0x90);
		      write32_wrapper(*(uint32_t*)arg2, psu_base + 0xa0);
		      
		break;
		case TEE_PLAYGROUND_CMD_GET_IDENTITY:
		      // called when remote server requests identity
		      DMSG("syscall_embassy: Get identity=========================\n");
		      res = TEE_SUCCESS;			
		      
		      psu_base = regions.r2.base;
		      
		      *(uint32_t*)arg1 = read32_wrapper(psu_base + 0x90);
		      *(uint32_t*)arg2 = read32_wrapper(psu_base + 0xa0);
		      
		break;


        default:                
                break;
        }

        return TEE_SUCCESS;
}

