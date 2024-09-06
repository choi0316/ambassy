#ifndef __HDLCD_H__
#define __HDLCD_H__

#define HDLCD_REG_SIZE			0x250

#define SEC_HDLCD			0x7fff0000
#define SEC_HDLCD_SIZE			0x4

#define HDLCD_FB_BASE			0xfc000000
#define HDLCD_FB_SIZE			0x1000000

#define HDLCD0_BASE			0x7ff60000
#define HDLCD1_BASE			0X7ff50000

#define HDLCD_REG_VERSION               0x0000    /* ro */
#define HDLCD_REG_INT_RAWSTAT           0x0010    /* rw */
#define HDLCD_REG_INT_CLEAR             0x0014    /* wo */
#define HDLCD_REG_INT_MASK              0x0018    /* rw */
#define HDLCD_REG_INT_STATUS            0x001c    /* ro */
#define HDLCD_REG_FB_BASE               0x0100    /* rw */
#define HDLCD_REG_FB_LINE_LENGTH        0x0104    /* rw */
#define HDLCD_REG_FB_LINE_COUNT         0x0108    /* rw */
#define HDLCD_REG_FB_LINE_PITCH         0x010c    /* rw */
#define HDLCD_REG_BUS_OPTIONS           0x0110    /* rw */
#define HDLCD_REG_V_SYNC                0x0200    /* rw */
#define HDLCD_REG_V_BACK_PORCH          0x0204    /* rw */
#define HDLCD_REG_V_DATA                0x0208    /* rw */
#define HDLCD_REG_V_FRONT_PORCH         0x020c    /* rw */
#define HDLCD_REG_H_SYNC                0x0210    /* rw */
#define HDLCD_REG_H_BACK_PORCH          0x0214    /* rw */
#define HDLCD_REG_H_DATA                0x0218    /* rw */
#define HDLCD_REG_H_FRONT_PORCH         0x021c    /* rw */
#define HDLCD_REG_POLARITIES            0x0220    /* rw */
#define HDLCD_REG_COMMAND               0x0230    /* rw */
#define HDLCD_REG_PIXEL_FORMAT          0x0240    /* rw */
#define HDLCD_REG_RED_SELECT            0x0244    /* rw */
#define HDLCD_REG_GREEN_SELECT          0x0248    /* rw */
#define HDLCD_REG_BLUE_SELECT           0x024c    /* rw */

/* version */
#define HDLCD_PRODUCT_ID                0x1CDC0000
#define HDLCD_PRODUCT_MASK              0xFFFF0000
#define HDLCD_VERSION_MAJOR_MASK        0x0000FF00
#define HDLCD_VERSION_MINOR_MASK        0x000000FF

/* interrupts */
#define HDLCD_INTERRUPT_DMA_END         (1 << 0)
#define HDLCD_INTERRUPT_BUS_ERROR       (1 << 1)
#define HDLCD_INTERRUPT_VSYNC           (1 << 2)
#define HDLCD_INTERRUPT_UNDERRUN        (1 << 3)
#define HDLCD_DEBUG_INT_MASK            (HDLCD_INTERRUPT_DMA_END |  \
                                        HDLCD_INTERRUPT_BUS_ERROR | \
                                        HDLCD_INTERRUPT_UNDERRUN)

/* polarities */
#define HDLCD_POLARITY_VSYNC            (1 << 0)
#define HDLCD_POLARITY_HSYNC            (1 << 1)
#define HDLCD_POLARITY_DATAEN           (1 << 2)
#define HDLCD_POLARITY_DATA             (1 << 3)
#define HDLCD_POLARITY_PIXELCLK         (1 << 4)

/* commands */
#define HDLCD_COMMAND_DISABLE           (0 << 0)
#define HDLCD_COMMAND_ENABLE            (1 << 0)

/* pixel format */
#define HDLCD_PIXEL_FMT_LITTLE_ENDIAN   (0 << 31)
#define HDLCD_PIXEL_FMT_BIG_ENDIAN      (1 << 31)
#define HDLCD_BYTES_PER_PIXEL_MASK      (3 << 3)

/* bus options */
#define HDLCD_BUS_BURST_MASK            0x01f
#define HDLCD_BUS_MAX_OUTSTAND          0xf00
#define HDLCD_BUS_BURST_NONE            (0 << 0)
#define HDLCD_BUS_BURST_1               (1 << 0)
#define HDLCD_BUS_BURST_2               (1 << 1)
#define HDLCD_BUS_BURST_4               (1 << 2)
#define HDLCD_BUS_BURST_8               (1 << 3)
#define HDLCD_BUS_BURST_16              (1 << 4)

/* Max resolution supported is 4096x4096, 32bpp */
#define HDLCD_MAX_XRES                  4096
#define HDLCD_MAX_YRES                  4096

#define NR_PALETTE                      256

void init_hdlcd(void);
void set_secure_hdlcd(bool secure);
bool is_secure_hdlcd(void);
void get_frame_buffer(uint32_t *fb, uint64_t *offset);
void set_frame_buffer_offset(uint64_t offset);


#define HDLCD_BYTES_PER_PIXEL 	4
#define HDLCD_SCREEN_WIDTH 	1280
#define HDLCD_SCREEN_HEIGHT	800
#define HDLCD_SCREEN_SIZE	(HDLCD_SCREEN_WIDTH * HDLCD_SCREEN_HEIGHT * HDLCD_BYTES_PER_PIXEL)


#endif //< __HDLCD_H__
