/* boot.S -- Thix boot sector.  Floppy disk only.  */

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


/*
 * WARNING:
 *
 * Right now this boot sector works *ONLY* for 1.4 Mb floppies.  In order
 * to make it work on other floppy types the SECTORS constant should be
 * changed.
 */

/*
 * FIXME: This should *REALLY* be autodetected !!!
 */


START_CYLINDER  EQU     0
START_HEAD      EQU     0


/*
 * 3.5' floppy disks: 2 heads, 18 sectors per track.
 */
HEADS           EQU     2
SECTORS         EQU     18

/*
 * 0x00 - first floppy
 * 0x01 - second floppy
 */
FD0_ID		EQU	0



/*
 * YOU SHOULD NOT NEED TO MODIFY ANYTHING BELOW THIS LINE !!!
 */

DATA_ARB        EQU     0x92
CODE_ARB        EQU     0x9A


		.org    0

.globl _main

_main:          cli
		xor     ax, ax
		mov     ss, ax
		mov     sp, #0x7C00
		mov     si, sp
		mov     ds, ax
		mov     es, ax
		sti
		cld
		mov     di, #0x600
		mov     cx, #0x100
		repnz
		movsw
		jmpi    #0x61D, #0      ; jmp far ptr 0000:061D

real_start:     mov     ax, #0xF0
		mov     ss, ax
		mov     sp, ax          ; stack at 0xF0:0xF0 = 0xFF0

		mov     ax, #0x60
		mov     ds, ax
		mov     si, #msg
		call    write_msg

recalibrate:    mov     ah, #0
		mov     dl, #FD0_ID
		int     0x13            ; recalibrate floppy
		jc      recalibrate

		mov     ax, #0x80       ; read the second part of the
		mov     es, ax          ; boot sector at address 800h
		xor     bx, bx
		mov     cx, #START_CYLINDER
		mov     dh, #START_HEAD
		mov     ah, #1 + 1
		call    read

		mov     ah, #5
		mov     di, #0xC0       ; read the first kernel sector
		mov     es, di          ; (the header) at address 0xC00 to
		call    read            ; get segment sizes

		seg     es
		mov     esi, [4]
		seg     es
		add     esi, [8]
		shr     esi, #9         ; %si = # of sectors to load

		mov     di, #0x100      ; load the kernel at address 0x1000
		mov     es, di
		jmp     L1

L3:             xorb    dh, dh
L2:             movb    ah, #1
L1:             call    read
		jnc     one_sect

		sub     si, #SECTORS
		cmp     si, #0
		jle     L4

		add     di, #0x20 * SECTORS
		mov     es, di
		jmp     multiple_sects

one_sect:       dec     si
		jz      L4

		add     di, #0x20
		mov     es, di

		inc     ah
		cmp     ah, #SECTORS + 1
		jne     L1

multiple_sects: inc     dh

		pusha
		xor     bx, bx
		mov     ax, #0xE2E
		int     0x10
		popa

		cmp     dh, #HEADS
		jne     L2

		inc     cx
		jmp     L3

L4:             mov     ah, #0x88
		int     0x15                    ; get extended memory size
		push    ax

;               mov     ax, #3
;               int     0x10
;               mov     ax, #0x1112
;               xor     bl, bl
;               int     0x10                    ; video mode 80 x 50

		cli

		mov     dx, #0x3F2
		xor     al, al
		out     dx, al                  ; stop floppy motor

		call	kbd_wait
		mov     al, #0xF3
		out     #0x60, al
		call    kbd_wait
		xor     al, al
		out     #0x60, al               ; maximum keyboard speed rate
		call    kbd_wait
		mov     al, #0xD1               ; 8042 write command
		out     #0x64, al
		call    kbd_wait
		mov     al, #0xDF               ; enable A20
		out     #0x60, al
		call    kbd_wait

		pop     ax                      ; extended memory size

		jmpi    #protected_mode, #0x60


read:           pusha                   ; cx=cylinder, dh=head
					; ah=start_sector, es:bx=address

;               cmp     ah, #1
;               je      all_sects
		mov     al, #1
		jmp     param_ready
all_sects:      mov     al, #SECTORS

param_ready:    ror     ch, #1
		ror     ch, #1
		and     ch, #0xC0
		or      ch, ah
		xchg    ch, cl
		mov     dl, #FD0_ID
		mov     ah, #0x02               ; read sectors
		push    ax
		int     0x13
		pop     ax
		jnc     end_read

		pusha
again:          mov     ah, #0
		mov     dl, #FD0_ID
		int     0x13                    ; recalibrate floppy
		jc      again
		call    error
end_read:       cmp     al, #1
		je      ok
		stc
ok:             popa
		ret

write_msg:
		pusha
loc_1:          lodsb
		cmp     al, #0
		je      loc_2
		xor     bx, bx
		mov     ah, #0xE
		int     0x10                    ; video display ah = fct 0Eh
						; write char al, teletype mode
		jmp     loc_1
loc_2:          popa
		ret

kbd_wait:					; stolen from Linux setup.S
		.word   0x00eb, 0x00eb
		in      al, #0x64               ; 8042 status port
		test	al, #1			; output buffer empty ?
		jz	no_output
		.word   0x00eb, 0x00eb
		in	al, #0x60		; read the character
		jmp	kbd_wait
no_output:
		test    al, #2                  ; input buffer full ?
		jnz     kbd_wait
		ret

error:          mov     ax, #0x60
		mov     ds, ax
		mov     si, #error
		call    write_msg
err:            jmp     err


msg:            .ascii  '[Thix 0.3] Wait:'
		.byte   0
error:          .byte   10, 13
		.ascii  '*** ERROR ***'
		.byte   0


		.org    0x1FD

		.byte   0x2                     ; sectors/block

		.org    0x1FE

		.word   0xAA55

		.org    0x200


/*
 * i386 protected mode initialization
 */

protected_mode: cli

		push    ds
		xor     ebx, ebx
		mov     ds, bx
		mov     es, bx
		mov     (ebx + 0xC10), eax      ; extended memory size at
						; address 0xC10

		cld

		mov     cx, #0x10
		mov     si, (ebx + 0x41 * 4)
		mov     ax, (ebx + 0x41 * 4 + 2)
		mov     ds, ax
		mov     di, #0xC20
		rep
		movsb                           ; first hd parameters

		mov     ds, bx                  ; Andi figured out that
						; this was missing...
		mov     cx, #0x10
		mov     si, (ebx + 0x46 * 4)
		mov     ax, (ebx + 0x46 * 4 + 2)
		mov     ds, ax
		mov     di, #0xC30
		rep
		movsb                           ; second hd parameters

		pop     ds

		lgdt    GDT_DESCRIPTOR

		mov     ax, #1                  ; still in real mode
		lmsw    ax                      ; see you later, real mode

		jmpi    0x1020, 8               ; start the 32-bit kernel


.align  4

.word   0
GDT_DESCRIPTOR: .word   3 * 8 - 1
		.long   0x600 + GDT

.align  2

GDT:
		.long   0, 0                    ;    0: dummy  descriptor

		.word   0x100, 0                ;    8: text   descriptor
		.byte   0, CODE_ARB, 0xC0, 0

		.word   0xFFFF, 0               ; 0x10: data   descriptor
		.byte   0, DATA_ARB, 0xCF, 0
