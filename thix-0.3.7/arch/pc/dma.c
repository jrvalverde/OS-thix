/* dma.c -- PC architecture: DMA management routines.  */

/*
 * Thix Operating System
 * Copyright (C) 1993,1996 Tudor Hulubei
 *
 * This program is  free software; you can  redistribute it and/or modify
 * it under the terms  of the GNU General Public  License as published by
 * the  Free Software Foundation; either version  2,  or (at your option)
 * any later version.
 *
 * This program is distributed  in the hope that  it will be  useful, but
 * WITHOUT    ANY  WARRANTY;  without   even   the  implied  warranty  of
 * MERCHANTABILITY  or  FITNESS FOR  A PARTICULAR   PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should  have received a  copy of  the GNU  General Public  License
 * along with   this  program; if   not,  write  to   the Free   Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <thix/proc/i386.h>
#include <thix/arch/pc/dma.h>


unsigned char dma_page_reg   [8] = { 0x87,0x83,0x81,0x82,0x00,0x8B,0x89,0x8A };
unsigned char dma_address_reg[8] = { 0x00,0x02,0x04,0x06,0xC0,0xC4,0xC8,0xCC };
unsigned char dma_count_reg  [8] = { 0x01,0x03,0x05,0x07,0xC2,0xC6,0xCA,0xCE };


void
enable_dma_channel(int channel)
{
    if (channel < 4)
	outb_delay(DMA0_INIT, channel);
    else
	outb_delay(DMA1_INIT, channel & 3);
}


void
disable_dma_channel(int channel)
{
    if (channel < 4)
	outb_delay(DMA0_INIT, channel | 4);
    else
	outb_delay(DMA1_INIT, (channel & 3) | 4);
}


void
set_dma_channel_mode(int channel, int mode)
{
    if (channel < 4)
	outb_delay(DMA0_MODE, mode | channel);
    else
	outb_delay(DMA1_MODE, mode | (channel & 3));
}


void
clear_dma_channel_flipflop(int channel)
{
    outb_delay((channel < 4) ? DMA0_FLIPFLOP : DMA1_FLIPFLOP, 0);
}


void
set_dma_channel_count(int channel, long count)
{
    count--;

    if (channel < 4)
    {
	outb_delay(dma_count_reg[channel], (unsigned char)count);
	outb_delay(dma_count_reg[channel], (unsigned char)(count >> 8));
    }
    else
    {
	outb_delay(dma_count_reg[channel], (unsigned char)(count >> 1));
	outb_delay(dma_count_reg[channel], (unsigned char)(count >> 9));
    }
}


void
set_dma_channel_address(int channel, unsigned char *address)
{
    if (channel < 4)
    {
	outb_delay(dma_address_reg[channel],
		   (unsigned char)(unsigned)address);
	outb_delay(dma_address_reg[channel],
		   (unsigned char)((unsigned)address >>8));
    }
    else
    {
	outb_delay(dma_address_reg[channel],
		   (unsigned char)((unsigned)address >>1));
	outb_delay(dma_address_reg[channel],
		   (unsigned char)((unsigned)address >>9));
    }
}


void
set_dma_channel_page(int channel, int page)
{
    outb_delay(dma_page_reg[channel], (channel < 4) ? page : page & 0xFE);
}
