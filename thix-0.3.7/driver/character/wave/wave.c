/* wave.c -- The wave player driver.  */

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


#include <thix/vm.h>
#include <thix/klib.h>
#include <thix/errno.h>
#include <thix/fcntl.h>
#include <thix/string.h>
#include <thix/irq.h>
#include <thix/wave.h>
#include <thix/gendrv.h>


character_driver wave_driver =
{
    "wave player",
    wave_open,
    wave_close,
    wave_read,
    wave_write,
    NULL,
    NULL,
    NULL,
    NULL,
    WAVE_MAJOR,
    NOIRQ,
    CHR_DRV,
    1,		/* ??? */
    NULL,
};


unsigned short wavedata[0x100];


/*
 * Driver for the line printer.
 */


void wave_set_volume(int volume)
{
    int i;

    for (i = 0; i < 0x80; i++)
    {
	wavedata[0x80 - i] = i * volume > 0x7F ? 0xFF81 : - i * volume;
	wavedata[0x80 + i] = i * volume > 0x7F ? 0x007F :   i * volume;
    }
}


int
wave_open(int minor, int flags)
{
    if ((flags & 3) != O_WRONLY)
	return -EINVAL;

    if (wavecnt[minor] == 0)
    {
	wavecnt[minor] = 1;
	return 0;
    }
    else
	return -EBUSY;
}


int
wave_close(int minor)
{
    wavecnt[minor] = 0;
    return 0;
}


int
wave_read(int minor, chr_request *cr)
{
    /* not reached. */
    return -EPERM;
}


int
wave_write(int minor, chr_request *cr)
{
    char *buf = cr->buf;
    int count = cr->count - 1;

    outb_delay(0x43, 0xB8);
    outb_delay(0x42, 0x01);
    outb_delay(0x42, 0x00);

    outb(0x61, inb(0x61) & 0xfc);

		xor     di,di
		xor     dx,dx

sample:         mov     bl,es:[si]
		inc	si
		xor     bh,bh
		shl     bx,1
		mov     bx,ds:_playdata[bx]
		mov	cx,[bp+6]

out0:           add     di,bx
                jge     out1
		and	al,0FCh
		sub	di,0FF81h
		out     61h,al
                loop    out0
                jmp     short next
out1:           or      al,3
		sub	di,7Fh
		out     61h,al
                loop    out0

next:           sub     word ptr [bp+8],1
		sbb	word ptr [bp+0Ah],0
                jc      exit1
		or      si,si
                jnz     sample
		mov	si,es
		add	si,1000h
		mov	es,si
		xor     si,si
                jmp     short sample

exit1:          in      al,61h
		and	al,0FCh
		out     61h,al
exit0:          pop     si
		pop	di
		pop     bp
		ret
    return 0;
}


int
init_wave(void)
{
    wave_set_volume(10);

    return register_driver((generic_driver *)&wave_driver);
}


/*
		push	bp
		mov	bp,sp
		push	di
		push	si
		mov	ax,[bp+8]
		or	ax,[bp+0Ah]
		jz	exit0
		sub	word ptr [bp+8],1
		sbb	word ptr [bp+0Ah],0
		cmp	word ptr [bp+6],1
                jb      exit0
		cmp	word ptr [bp+6],4B0h
                ja      exit0
		les     si,dword ptr [bp+0Ch]

		mov	al,0B8h
                out     043h,al
                out     0E0h,al
		mov	al,1
                out     042h,al
                out     0E0h,al
                mov     al,0
                out     042h,al
                out     0E0h,al
		in      al,61h
		and	al,0FCh
		out     61h,al

		xor     di,di
		xor     dx,dx

sample:         mov     bl,es:[si]
		inc	si
		xor     bh,bh
		shl     bx,1
		mov     bx,ds:_playdata[bx]
		mov	cx,[bp+6]

out0:           add     di,bx
                jge     out1
		and	al,0FCh
		sub	di,0FF81h
		out     61h,al
                loop    out0
                jmp     short next
out1:           or      al,3
		sub	di,7Fh
		out     61h,al
                loop    out0

next:           sub     word ptr [bp+8],1
		sbb	word ptr [bp+0Ah],0
                jc      exit1
		or      si,si
                jnz     sample
		mov	si,es
		add	si,1000h
		mov	es,si
		xor     si,si
                jmp     short sample

exit1:          in      al,61h
		and	al,0FCh
		out     61h,al
exit0:          pop     si
		pop	di
		pop     bp
		ret
*/
