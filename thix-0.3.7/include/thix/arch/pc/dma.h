#ifndef _THIX_ARCH_PC_DMA_H
#define _THIX_ARCH_PC_DMA_H


#define DMA_CHANNELS            0x08

#define DMA0_STATUS             0x08
#define DMA0_INIT               0x0A
#define DMA0_MODE               0x0B
#define DMA0_FLIPFLOP           0x0C

#define DMA1_STATUS             0xD0
#define DMA1_INIT               0xD4
#define DMA1_MODE               0xD6
#define DMA1_FLIPFLOP           0xD8

#define DMA_READ_MODE           0x44
#define DMA_WRITE_MODE          0x48
#define DMA_VERIFY_MODE         0x40


void enable_dma_channel(int channel);
void disable_dma_channel(int channel);
void clear_dma_channel_flipflop(int channel);
void set_dma_channel_mode(int channel, int mode);
void set_dma_channel_page(int channel, int page);
void set_dma_channel_address(int channel, unsigned char *address);
void set_dma_channel_count(int channel, long count);


#endif  /* thix/arch/pc/dma.h */
