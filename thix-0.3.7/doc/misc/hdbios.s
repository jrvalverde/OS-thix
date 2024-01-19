hdsk_status_1_	db	0
hdsk_count_	db	2	; numarul de hard discuri
hdsk_head_ctrl_	db	8
hdsk_ctrl_port_	db	0

hdsk_status_2_	db	58h
hdsk_error_	db	82h
hdsk_int_flags_	db	0
hdsk_options_	db	33h
hdsk0_media_st_	db	2
hdsk1_media_st_	db	7
hdsk0_start_st_	db	0
hdsk1_start_st_	db	0
hdsk0_cylinder_	db	0
hdsk1_cylinder_	db	0

				; ===============================
				; |	 HARD DISK DATA 	|
				; ===============================

hdsk_status_1	db	0	; Hard disk status
				;  00h = ok
				;  01h = bad command or parameter
				;  02h = can't find address mark
				;  03h = can't write, protected dsk
				;  04h = sector not found
				;  05h = reset failure
				;  07h = activity failure
				;  08h = DMA overrun
				;  09h = DMA attempt over 64K bound
				;  0Ah = bad sector flag
				;  0Bh = removed bad track
				;  0Dh = wrong # of sectors, format
				;  0Eh = removed control data addr mark
				;  0Fh = out of limit DMA arbitration level
				;  10h = bad CRC or ECC, disk read
				;  11h = bad ECC corrected data
				;  20h = controller failure
				;  40h = seek failure
				;  80h = timeout, no response
				;  AAh = not ready
				;  BBh = error occurred, undefined
				;  CCh = write error, selected dsk
				;  E0h = error register = 0
				;  FFh = disk sense failure
hdsk_count	db	2	; Number of hard disk drives
hdsk_head_ctrl	db	8	; Head control (XT only)
hdsk_ctrl_port	db	0	; Hard disk control port (XT only)
hdsk_status_2	db	58h	; Hard disk status
hdsk_error	db	82h	; Hard disk error
hdsk_int_flags	db	0	; Set for hard disk interrupt flag
hdsk_options	db	33h	; Bit 0 = 1 when using 1 controller
				;  card for both hard disk & floppy
hdsk0_media_st	db	2	; Media state for drive 0
hdsk1_media_st	db	7	; Media state for drive 1
				;     7      6      5      4
				;  data xfer rate  two   media
				;   00=500K bit/s  step  known
				;   01=300K bit/s
				;   10=250K bit/s
				;     3      2      1      0
				;  unused  -----state of drive-----
				;          bits floppy  drive state
				;          000=  360K in 360K, ?
				;          001=  360K in 1.2M, ?
				;          010=  1.2M in 1.2M, ?
				;          011=  360K in 360K, ok
				;          100=  360K in 1.2M, ok
				;          101=  1.2M in 1.2M, ok
				;          111=  state not defined

hdsk0_start_st	db	0	; Start state for drive 0
hdsk1_start_st	db	0	; Start state for drive 1
hdsk0_cylinder	db	0	; Track number for drive 0
hdsk1_cylinder	db	0	; Track number for drive 1


Semnificatia [bp+x]-urilor:

La intrarea in int 13h se executa:
	push	ax	-------------------------->	bp+18h
	push	cx	-------------------------->	bp+16h
	push	dx	-------------------------->	bp+14h
	push	bx	-------------------------->	bp+12h
	push	si	-------------------------->	bp+10h
	push	di	-------------------------->	bp+0Eh
	push	ds	-------------------------->	bp+0Ch
	push	es	-------------------------->	bp+0Ah
	sub	sp,8	-------------------------->	bp+2
	push	bp	-------------------------->	bp
	mov	bp,sp
	...

;=============================================================================

sub_77		proc	near
		mov	ax,100h
		assume	ds:seg_b
		mov	hdsk_status_1,ah	; bad command or parameter
		stc				; Set carry flag
		retn
sub_77		endp


;=============================================================================
; Pozitionare la 0 a controller-ului + recalibrare disc.
; ah = 0
; dl = disc
;=============================================================================

HD_RESET	proc	near
		cmp	dl,81h
		ja	short loc_ret_469	; Jump if bad disk.
loc_468:
		jmp	short loc_471

;=============================================================================

HD_RECALIBRATE:
		call	HD_SETUP
		jnc	loc_468			; Jump if carry=0

loc_ret_469:
		retn


;=============================================================================
; Citirea parametrilor curenti ai unitatii de disc
;=============================================================================

HD_GET_PARAM:
		call	sub_87
		jmp	short loc_470


;=============================================================================

HD_GET_INTERF:
		call	HD_GET_GEOMETRY
loc_470:
		mov	[bp+14h],dx		; drive & head
		mov	[bp+16h],cx		; track & sector
		retn
loc_471:
		mov	hdsk_int_flags,0	; (0040:008E=0)
		mov	dx,3F6h
		mov	al,4
		out	dx,al			; port 3F6h, hdsk0 register
						;  al = 4, reset controller
		mov	cx,24h
		call	DELAY
		mov	al,0
		out	dx,al			; port 3F6h, hdsk0 register
		call	HD_CHK_BUSY
		jc	short loc_ret_476	; Jump if carry Set
		mov	dx,1F1h
		in	al,dx			; port 1F1h, hdsk0-error regstr
		mov	hdsk_error,al		; (0040:008D=82h)
		cmp	al,1			; data address mark not found
		je	short loc_472
		mov	ah,5			; reset failure
		stc
		jmp	short loc_475		; exit with error
loc_472:
		mov	dl,80h
		mov	di,22h
		call	HD_RECALIBRATE_
		cmp	hdsk_count,2
		jb	short loc_473		; Jump if below
		mov	dl,81h
		call	HD_RECALIBRATE_
loc_473:
		mov	dl,80h
		mov	di,12h
		call	HD_SET_PARAM
		cmp	hdsk_count,2
		jb	short loc_474		; Jump if below
		mov	dl,81h
		call	HD_SET_PARAM
loc_474:
		xor	ah,ah			; no errors
loc_475:
		mov	hdsk_status_1,ah

loc_ret_476:
		retn
HD_RESET	endp


;=============================================================================
; Intoarce in al valoarea lui hdsk_status_1. In hdsk_status_1 pune 0 si
; reseteaza carry flag.
;=============================================================================

HD_GET_ERROR	proc	near
		mov	al,hdsk_status_1
		mov	ah,0
		mov	hdsk_status_1,ah
		clc				; Clear carry flag
		retn
HD_GET_ERROR	endp


;=============================================================================
; Citire sectoare
;=============================================================================


HD_READ:	proc	near
		mov	word ptr [bp+18h],200h
		call	CHECK_DMA
		jc	short loc_ret_477	; Jump if carry Set
		mov	ah,20h
		jmp	loc_499

loc_ret_477:
		retn


;=============================================================================
; Scriere sectoare
;=============================================================================

HD_WRITE:
		mov	word ptr [bp+18h],200h
		call	CHECK_DMA
		jc	short loc_ret_478	; Jump if carry Set
		mov	ah,30h
		jmp	loc_510

loc_ret_478:
		retn

;=============================================================================
; Rutina asta verifica sectoare.
;=============================================================================

HD_VERIFY:
		call	HD_SETUP
		jc	short loc_ret_479	; Jump if carry Set
		call	HD_SEND_DRV&HD
		jc	short loc_ret_479	; Jump if carry Set
		call	HD_SET_STEP
		call	HD_SEND_PARAM
		call	HD_SET_PRECOMP
		mov	al,40h			; verify read
		call	HD_SEND_CMD
		call	HD_WAIT_INTR
		jc	short loc_ret_479	; Jump if carry Set
		call	HD_CHK_ERROR

loc_ret_479:
		retn

;=============================================================================
; Rutina asta formateaza piste.
;=============================================================================

HD_FORMAT:
		call	HD_SETUP
		jc	short loc_480		; Jump if carry Set
		call	HD_SEND_DRV&HD
		jnc	short loc_481		; Jump if carry=0
loc_480:
		jmp	short loc_ret_490
		db	90h
loc_481:
		mov	di,es
		mov	si,bx
		call	HD_GET_PTBL
		call	HD_SET_STEP
		call	HD_SET_PRECOMP
		mov	al,es:[bx+0Eh]		; reserved (used as sector cnt)
		mov	[bp+3],al		; sector count
		mov	dx,1F2h
		out	dx,al			; port 1F2h, hdsk0-sector count
		jcxz	short loc_482		; Jump if cx=0
loc_482:
		jcxz	short loc_483		; Jump if cx=0
loc_483:
		mov	al,[bp+5]		; cylinder low byte
		and	al,3Fh
		mov	dx,1F3h
		out	dx,al			; port 1F3h, hdsk0-sector numbr
		jcxz	short loc_484		; Jump if cx=0
loc_484:
		jcxz	short loc_485		; Jump if cx=0
loc_485:
		mov	al,[bp+7]		; drive & head
		mov	dx,1F6h
		out	dx,al			; port 1F6h, hdsk0-siz/drv/head
		jcxz	short loc_486		; Jump if cx=0
loc_486:
		jcxz	short loc_487		; Jump if cx=0
loc_487:
		mov	al,ch
		mov	[bp+5],al		; cylinder low byte
		mov	dx,1F4h
		out	dx,al			; port 1F4h, hdsk0-cylr,lo byte
		jcxz	short loc_488		; Jump if cx=0
loc_488:
		jcxz	short loc_489		; Jump if cx=0
loc_489:
		mov	al,cl
		shr	al,6			; Shift w/zeros fill
		mov	dl,al
		mov	al,[bp+15h]		; head number
		shr	al,4			; Shift w/zeros fill
		and	al,0Ch
		or	al,dl
		mov	[bp+6],al		; cylinder hi byte
		mov	dx,1F5h
		out	dx,al			; port 1F5h, hdsk0-cylr,hi byte
		mov	al,50h 			; format track
		call	HD_SEND_CMD
		call	HD_WAITING_DATA
		jc	short loc_ret_490	; Jump if carry Set
		mov	ax,ds
		xchg	ax,di
		mov	ds,ax
		mov	cx,100h
		mov	dx,1F0h
		cli				; Disable interrupts
		cld				; Clear direction
		rep	outsw			; Rep when cx >0 Out [si] to port dx
		sti				; Enable interrupts
		mov	ds,di
		call	HD_WAIT_INTR
		jc	short loc_ret_490	; Jump if carry Set
		call	HD_CHK_ERROR

loc_ret_490:
		retn


;=============================================================================
; Aceasta functie nu e apelata decat dintr-o rutina care nu ma intereseaza.
;=============================================================================

sub_87:
		call	HD_GET_PTBL
		jnc	short loc_491		; Jump if carry=0
		mov	ah,7
		xor	cx,cx			; Zero register
		xor	dx,dx			; Zero register
		stc				; Set carry flag
		jmp	short loc_493
loc_491:
		mov	dh,es:[bx+2]
		dec	dh
		mov	ax,es:[bx]
		sub	ax,2
		cmp	ax,3FFh
		jbe	short loc_492		; Jump if below or =
		mov	ax,3FFh
loc_492:
		mov	dl,ah
		mov	ch,al
		shl	ah,6			; Shift w/zeros fill
		mov	cl,ah
		mov	ah,dl
		shl	ah,4			; Shift w/zeros fill
		and	ah,0C0h
		or	dh,ah
		mov	al,es:[bx+0Eh]
		or	cl,al
		mov	dl,hdsk_count
		mov	ah,0
loc_493:
		mov	hdsk_status_1,ah
		retn

;=============================================================================
; Rutina asta nu pare sa faca altceva decat setarea parametrilor hard discului
; drive, head, step, sector count, drive & head again, write precompensation.
; Dupa ce termina asteapta intrerupere.
; dl = 80h sau 81h (numarul hard discului)
; di = 12h ???
;=============================================================================

HD_SET_PARAM:
		call	HD_SETUP
		jc	short loc_ret_496	; Jump if carry Set
		call	HD_GET_PTBL
		jc	short loc_ret_496	; Jump if carry Set
		call	HD_SEND_DRV&HD
		jc	short loc_ret_496	; Jump if carry Set
		call	HD_SET_STEP
		mov	al,es:[bx+0Eh]
		mov	ah,dl
		mov	dx,1F2h
		mov	[bp+3],al		; sector count
		out	dx,al			; port 1F2h, hdsk0-sector count
		jcxz	short loc_494		; Jump if cx=0
loc_494:
		jcxz	short loc_495		; Jump if cx=0
loc_495:
		mov	dx,1F6h
		mov	al,[bp+7]		; drive & head
		out	dx,al			; port 1F6h, hdsk0-siz/drv/head
		mov	al,91h			; set parameters
		call	HD_SET_PRECOMP
		call	HD_SEND_CMD
		call	HD_WAIT_INTR
		jc	short loc_ret_496	; Jump if carry Set
		call	HD_CHK_ERROR

loc_ret_496:
		retn

;=============================================================================
; Citire sectoare lungi
;=============================================================================

HD_LONG_READ:
		mov	word ptr [bp+18h],204h
		call	CHECK_DMA
		jc	short loc_ret_497	; Jump if carry Set
		mov	ah,22h			; read sectors
		jmp	short loc_499

loc_ret_497:
		retn


;=============================================================================
; Scriere sectoare lungi
;=============================================================================

HD_LONG_WRITE:
		mov	word ptr [bp+18h],204h
		call	CHECK_DMA
		jc	short loc_ret_498	; Jump if carry Set
		mov	ah,32h
		jmp	short loc_510

loc_ret_498:
		retn

;===========================================================================
; ah = 20h sau 22h.
; Citire sectoare. Se sare aici din HD_READ si HD_LONG_READ pt. efectuarea
; operatiei.
;===========================================================================

loc_499:
		call	HD_SETUP
		jc	short loc_ret_509	; Jump if carry Set
		call	HD_SEND_DRV&HD
		jc	short loc_ret_509	; Jump if carry Set
		mov	di,bx
		mov	si,es
		call	HD_SET_STEP

; (XT only)	test	hdsk_head_ctrl,0C0h	; hdsk_head_ctrl = 8
;		jz	short loc_500		; Jump if zero
;		inc	ah

loc_500:
		call	HD_SEND_PARAM
		call	HD_SET_PRECOMP
		mov	al,ah
		call	HD_SEND_CMD
loc_501:
		call	HD_WAIT_INTR
		jc	short loc_ret_509	; Jump if carry Set
		call	HD_CHK_ERROR
		jnc	short loc_502		; Jump if carry=0
		cmp	hdsk_status_1,11h	; bad ECC data
		jne	short loc_508		; Jump if not equal
loc_502:
		mov	es,si
		mov	cx,100h
		mov	dx,1F0h
		cli				; Disable interrupts
		cld				; Clear direction
		rep	insw			; Rep when cx >0 Port dx to es:[di]
		sti				; Enable interrupts
		test	byte ptr [bp+8],2	; command
		jz	short loc_507		; Jump if zero
		call	HD_WAITING_DATA
		jc	short loc_ret_509	; Jump if carry Set
		mov	cx,4

locloop_503:
		in	al,dx			; port 1F0h, hdsk0-read data
		mov	es:[di],al
		inc	di
		jcxz	short loc_504		; Jump if cx=0
loc_504:
		jcxz	short loc_505		; Jump if cx=0
loc_505:
		jcxz	short loc_506		; Jump if cx=0
loc_506:
		loop	locloop_503		; Loop if cx > 0

loc_507:
		dec	byte ptr [bp+3]		; sector count
		jnz	loc_501			; Jump if not zero
loc_508:
		mov	ah,hdsk_status_1
		or	ah,ah			; Zero ?
		jz	short loc_ret_509	; Jump if zero
		stc				; Set carry flag

loc_ret_509:
		retn


;===========================================================================
; ah = 30h sau 32h.
; Scriere sectoare. Se sare aici din HD_WRITE si HD_LONG_WRITE pentru
; efectuarea operatiei.
;===========================================================================

loc_510:
		call	HD_SETUP
		jc	short loc_ret_518	; Jump if carry Set
		call	HD_SEND_DRV&HD
		jc	short loc_ret_518	; Jump if carry Set
		mov	di,es
		mov	si,bx
		call	HD_SET_STEP
		call	HD_GET_PTBL
		test	hdsk_head_ctrl,0C0h	; (0040:0076=8)
		jz	short loc_511		; Jump if zero
		inc	ah
loc_511:
		call	HD_SEND_PARAM
		call	HD_SET_PRECOMP
		mov	al,ah
		call	HD_SEND_CMD
		call	HD_WAITING_DATA
		jc	short loc_ret_518	; Jump if carry Set
loc_512:
		mov	ds,di
		mov	cx,100h
		mov	dx,1F0h
		cli				; Disable interrupts
		cld				; Clear direction
		rep	outsw			; Rep when cx >0 Out [si] to port dx
		sti				; Enable interrupts
		test	byte ptr [bp+8],2	; command
		jz	short loc_517		; Jump if zero
		push	40h
		pop	ds
		call	HD_WAITING_DATA
		jc	short loc_ret_518	; Jump if carry Set
		mov	cx,4
		mov	ds,di

locloop_513:
		mov	al,[si]
		inc	si
		out	dx,al			; port 1F0h, hdsk0-write data
		jcxz	short loc_514		; Jump if cx=0
loc_514:
		jcxz	short loc_515		; Jump if cx=0
loc_515:
		jcxz	short loc_516		; Jump if cx=0
loc_516:
		loop	locloop_513		; Loop if cx > 0

loc_517:
		push	40h
		pop	ds
		call	HD_WAIT_INTR
		jc	short loc_ret_518	; Jump if carry Set
		call	HD_CHK_ERROR
		jc	short loc_ret_518	; Jump if carry Set
		test	hdsk_status_2,8		; buffer is waiting for data ?
		jnz	loc_512			; Jump if not zero

loc_ret_518:
		retn
HD_READ		endp


;=============================================================================
; Rutina urmatoare pozitioneaza capetele discului pe o anumita pista.
;=============================================================================

HD_SEEK		proc	near
		call	HD_SETUP
		jc	short loc_ret_523	; Jump if carry Set
		call	HD_SEND_DRV&HD
		jc	short loc_ret_523	; Jump if carry Set
		call	HD_SET_STEP
		mov	al,ch
		mov	ch,dl
		mov	dx,1F4h
		mov	[bp+5],al		; cylinder low byte
		out	dx,al			; port 1F4h, hdsk0-cylr,lo byte
		jcxz	short loc_519		; Jump if cx=0
loc_519:
		jcxz	short loc_520		; Jump if cx=0
loc_520:
		mov	al,cl
		shr	al,6			; Shift w/zeros fill
		mov	dl,[bp+15h]		; head number
		shr	dl,4			; Shift w/zeros fill
		and	dl,0Ch
		or	al,dl
		mov	dx,1F5h
		mov	[bp+6],al		; cylinder hi byte
		out	dx,al			; port 1F5h, hdsk0-cylr,hi byte
		jcxz	short loc_521		; Jump if cx=0
loc_521:
		jcxz	short loc_522		; Jump if cx=0
loc_522:
		mov	al,[bp+7]		; drive & head
		mov	dx,1F6h
		out	dx,al			; port 1F6h, hdsk0-siz/drv/head
		call	HD_SET_PRECOMP
		mov	al,70h			; seek to cylinder
		call	HD_SEND_CMD
		call	HD_WAIT_INTR
		jc	short loc_ret_523
		call	HD_CHK_ERROR
		jnc	short loc_ret_523
		cmp	hdsk_status_1,40h	; seek failure ?
		stc				; Set carry flag
		jnz	short loc_ret_523
		xor	ah,ah			; no errors
		mov	hdsk_status_1,ah
loc_ret_523:
		retn
HD_SEEK		endp


;=============================================================================
; Rutina aceasta verifica daca hard discul este pregatit pentru operatie :-)
;=============================================================================

HD_CHK_READY	proc	near
		call	HD_SETUP
		jc	short loc_ret_525	; Jump if carry Set
		call	HD_CHK_BUSY
		jc	short loc_ret_525	; Jump if carry Set
		mov	dx,1F6h
		mov	al,[bp+7]		; drive & head
		out	dx,al			; port 1F6h, hdsk0-siz/drv/head
		mov	dx,1F7h
		push	cx
		mov	cx,2
		call	DELAY
		pop	cx
		in	al,dx			; port 1F7h, hdsk0-status reg
		mov	hdsk_status_2,al
		mov	ah,0
		test	al,40h			; is drive ready for r/w/s ?
		jnz	short loc_524
		mov	ah,0AAh			; not ready
		stc				; Set carry flag
loc_524:
		mov	hdsk_status_1,ah

loc_ret_525:
		retn
HD_CHK_READY	endp


;=============================================================================
; Recalibrare disk. Muta capul la pista 0.
;=============================================================================


HD_RECALIBRATE_	proc	near
		call	HD_SETUP
		jc	short loc_ret_527	; Jump if carry Set
		call	HD_SEND_DRV&HD
		jc	short loc_ret_527	; Jump if carry Set
		call	HD_SET_STEP
		mov	dx,1F6h
		mov	al,[bp+7]		; drive & head
		out	dx,al			; port 1F6h, hdsk0-siz/drv/head
		call	HD_SET_PRECOMP
		mov	al,10h			; restore to cylinder 0
		call	HD_SEND_CMD
		call	HD_WAIT_INTR
		jnc	short loc_526		; Jump if carry=0
		call	HD_WAIT_INTR
		jc	short loc_ret_527	; Jump if carry Set
loc_526:
		call	HD_CHK_ERROR

loc_ret_527:
		retn
HD_RECALIBRATE_	endp


;=============================================================================

HD_DIAGNOSE	proc	near
		call	HD_SET_STEP
		call	HD_CHK_BUSY
		jc	short loc_ret_529	; Jump if carry Set
		mov	al,90h			; diagnose
		call	HD_SEND_CMD
		call	HD_WAIT_INTR
		jc	short loc_ret_529	; Jump if carry Set
		mov	dx,1F1h
		in	al,dx			; port 1F1h, hdsk0-error regstr
		mov	hdsk_error,al		; (0040:008D=82h)
		mov	ah,20h			; ' '
		cmp	al,1
		stc				; Set carry flag
		jnz	short loc_528		; Jump if not zero
		xor	ah,ah			; Zero register
loc_528:
		mov	hdsk_status_1,ah

loc_ret_529:
		retn
HD_DIAGNOSE	endp


;=============================================================================

HD_GET_GEOMETRY	proc	near
		call	HD_SETUP
		jnc	short loc_530		; Jump if carry=0
		xor	ah,ah			; Zero register
		xor	cx,cx			; Zero register
		xor	dx,dx			; Zero register
		retn
loc_530:
		call	HD_GET_PTBL_
		mov	ax,es:[bx]		; ax = max # of cylinders
		dec	ax
		mov	cl,es:[bx+0Eh]		; cl = reserved !!!???
		mov	ch,0
		mul	cx			; dx:ax = reg * ax
		mov	cl,es:[bx+2]		; cl = max # of heads
		mul	cx			; dx:ax = reg * ax
		mov	cx,dx
		mov	dx,ax			; cx:dx=(cyl-1)*heads*reserved
		mov	ah,3
		clc				; Clear carry flag
		retn
HD_GET_GEOMETRY	endp


;=============================================================================
; Aceasta functie nu e apelata de nicaieri (aparent). Cel putin nu din ruti-
; nele care ma intereseaza pe mine. Asa ca ...
;=============================================================================

sub_96		proc	near
		call	HD_GET_PTBL
		jnc	short loc_531		; Jump if carry=0
		mov	ah,1			; bad command or parameter
		mov	hdsk_status_1,ah
		retn
loc_531:
		mov	dh,[bp+15h]		; head number
		push	dx
		mov	ax,es:[bx+0Ch]
		mov	ch,al
		mov	cl,ah
		shl	cl,6			; Shift w/zeros fill
		mov	dh,ah
		shl	dh,4			; Shift w/zeros fill
		and	dh,0C0h
		mov	[bp+15h],dh		; head number
		call	HD_SEEK
		pop	dx
		mov	[bp+15h],dh		; head number
		retn
sub_96		endp


;=============================================================================
; Aproximativ identica cu HD_GET_PGTBL_ dar verifica corectitudinea lui dl
; si pune 1 in hdsk_status_1 in caz de eroare (bad command or parameter).
;=============================================================================

HD_GET_PTBL	proc	near
		cmp	dl,81h
		jbe	short loc_532		; Jump if below or =
		mov	ah,1			; bad command or parameter
		mov	hdsk_status_1,ah
		stc				; Set carry flag
		retn
HD_GET_PTBL	endp


;=============================================================================
; dl = drive (80h sau 81h)
; Intoarce in es:bx pointer la tabela de parametri pentru hard-discul
; specificat in dl. Pentru primul hd, adresa tabelei se gaseste in locatiile
; vectorului de intreruperi 41h. Pentru cel de al doilea -> 46h.
;=============================================================================

HD_GET_PTBL_	proc	near
loc_532:
		xor	bx,bx			; Zero register
		mov	es,bx
		test	dl,1
		mov	bx,104h			; adresa vectorului int 41h
		jz	short loc_533		; Jump if zero
		mov	bx,data_25e		; (0000:0118=16h)
loc_533:					; adresa vectorului int 46h
		les	bx,dword ptr es:[bx]	; Load 32 bit ptr
		clc				; Clear carry flag
		retn
HD_GET_PTBL_	endp


;=============================================================================
; Verifica daca nu cumva a fost apelata cu un numar disc prost (in dl). Daca
; da, iese cu carry flag = 1. Daca nu, ... (nu m-am prins inca).
; dl = drive
; dh = head number
; di = (nr rutinei din tabela int-ului 13h care a apelat aceasta functie) * 2
; In octetul de la adresa [bp+2] pune numarul cilindrului de la care incepe
; write precompensation / 4.
; in octetul de la adresa [bp+7] pune numarul de capete (in nibble low) si
; 0Ah pentru primul harddisc si 0Bh pentru al doilea (in nibble high). Astfel
; [bp+7] este adus in forma in care va fi trimis la controller prin out la
; portul 1F6h.
;=============================================================================

HD_SETUP	proc	near
		push	ax
		mov	al,hdsk_count
		mov	ah,dl
		and	ah,7Fh
		cmp	al,ah
		pop	ax
		jbe	short loc_538		; exit with error if bad disk #
		cmp	di,2Ah			; called by func 15h ?
		je	short loc_537		; exit ok if true
		cmp	di,10h			; called by func 8h ?
		je	short loc_537		; exit ok if true
		push	es
		push	bx
		push	dx
		push	cx
		call	HD_GET_PTBL_
		mov	cx,es:[bx+5]		; starting wpcomp cylinder
		shr	cx,2			; imparte cx la 4 si obtine
		mov	[bp+2],cl		; un # in 0-255. (cyl<1024)
		mov	cl,0A0h
		test	dl,1
		jz	short loc_534		; Jump if first hd
		mov	cl,0B0h
loc_534:
		cmp	di,12h			; called by func 9h ?
		jne	short loc_535		; Jump if not true
		mov	dh,es:[bx+2]		; dh = number of heads
		dec	dh
		or	cl,dh
		jmp	short loc_536
loc_535:
		and	dh,0Fh
		or	cl,dh
loc_536:
		mov	[bp+7],cl		; drive & head
		pop	cx
		pop	dx
		pop	bx
		pop	es
loc_537:
		clc				; Clear carry flag
		retn
loc_538:
		mov	ah,1			; bad command or parameter
		mov	hdsk_status_1,ah
		stc				; Set carry flag
		retn
HD_SETUP	endp


;=============================================================================
; Seteaza registrul de "write precompensation" (171h).
;=============================================================================

HD_SET_PRECOMP	proc	near
		push	dx
		push	ax
		mov	al,[bp+2]		; write precompensation
		mov	dx,1F1h			; port 1F1h, hdsk0-precomp cylr
		out	dx,al
		pop	ax
		pop	dx
		retn
HD_SET_PRECOMP	endp


;=============================================================================
; Verifica daca controller-ul e busy. Daca nu e, iese. Daca e, asteapta intr-o
; bucla dupa cx sa devina not busy. Dupa  10h  cicluri  pune  in hdsk_status_2
; valoarea citita de la "status register" si in hdsk_status_1 80h (timeout).
;=============================================================================

HD_CHK_BUSY	proc	near
		sti				; Enable interrupts
		push	cx
		push	dx
		push	ax
		mov	cx,10h
		mov	dx,1F7h
		mov	ah,80h

locloop_539:
		push	cx
		xor	cx,cx			; Zero register
		call	HD_CHK_NOERR
		pop	cx
		jnc	short loc_540		; Jump if carry=0
		loop	locloop_539		; Loop if cx > 0

		in	al,dx			; port 1F7h, hdsk0-status reg
		mov	hdsk_status_2,al
		pop	ax
		mov	ah,80h
		mov	hdsk_status_1,ah	; timeout
		jmp	short loc_541
loc_540:
		pop	ax
loc_541:
		pop	dx
		pop	cx
		retn
HD_CHK_BUSY	endp


;=============================================================================
; cx = numarul de incercari
; Aceasta rutina seteaza drive-ul si head-ul dupa care asteapta ca buffer-ul
; controller-ului sa fie gata pentru operatii de read/write/seek si sa se
; termine seek-ul. Seteaza hdsk_status_1 si hdsk_status_2 in caz de timeout.
;=============================================================================

HD_SEND_DRV&HD	proc	near
		push	dx
		push	cx
		push	ax
		mov	cx,0C00h

locloop_542:
		call	HD_CHK_BUSY
		jc	short loc_544		; Jump if carry Set
		mov	dx,1F6h
		mov	al,[bp+7]		; drive & head
		out	dx,al			; port 1F6h, hdsk0-siz/drv/head
		push	cx
		mov	cx,2
		call	DELAY
		pop	cx
		mov	dx,1F7h
		in	al,dx			; port 1F7h, hdsk0-status reg
		mov	ah,0AAh			; not ready
		test	al,40h			; is drive ready for r/w/s ?
		jz	short loc_543		; Jump if not ready
		mov	ah,40h			; seek failure
		test	al,10h			; seek completed ?
		jnz	short loc_545		; Jump if completed
loc_543:
		loop	locloop_542		; Loop if cx > 0

		mov	hdsk_status_2,al
		mov	hdsk_status_1,ah
loc_544:
		pop	cx
		mov	al,cl
		pop	cx
		pop	dx
		stc				; Set carry flag
		retn
loc_545:
		pop	ax
		pop	cx
		pop	dx
		clc				; Clear carry flag
		retn
HD_SEND_DRV&HD	endp


;=============================================================================
; Rutina aceasta se intoarce numai dupa ce controller-ul a trimis o
; intrerupere sau a expirat timpul de asteptare (timeout). Multitasking
; puternic ... :-)
;=============================================================================

HD_WAIT_INTR	proc	near
		clc				; Clear carry flag
		mov	ax,9000h
		int	15h			; General services, ah=func 90h
						;  device busy, al=type
		sti				; Enable interrupts
		jc	short loc_547		; Jump if carry Set
		mov	bx,offset hdsk_int_flags; (0040:008E=0)
		mov	dx,10h
loc_546:
		xor	cx,cx			; Zero register
		call	HD_WAIT_INTR_F
		jnc	short loc_548		; Jump if carry=0
		dec	dx
		jnz	loc_546			; Jump if not zero
loc_547:
		mov	ah,80h			; timeout
		mov	hdsk_status_1,ah
		jmp	short loc_ret_549
loc_548:
		mov	hdsk_int_flags,0	; (0040:008E=0)

loc_ret_549:
		retn
HD_WAIT_INTR	endp


;=============================================================================
; Daca buffer-ul controller-ului asteapta date carry = 0, altfel carry = 1 si
; se seteaza hdsk_status_1 cu timeout si hdsk_status_2 cu valoarea portului
; 1F7h.
;=============================================================================

HD_WAITING_DATA	proc	near
		push	cx
		push	dx
		push	ax
		mov	cx,0C8h
		mov	dx,1F7h
		mov	ah,8			; is buffer waiting for data ?
		call	HD_CHK_COND
		jnc	short loc_550		; Jump if carry=0
		in	al,dx			; port 1F7h, hdsk0-status reg
		mov	hdsk_status_2,al
		pop	ax
		mov	ah,80h			; timeout
		mov	hdsk_status_1,ah
		jmp	short loc_551
loc_550:
		pop	ax
loc_551:
		pop	dx
		pop	cx
		retn
HD_WAITING_DATA	endp


;=============================================================================
; Verifica daca au aparut erori. Comentariile sunt suficiente (cred eu).
; Daca a aparut o eroare, codul ei este pus in ah si este setat carry.
; Sunt actualizate hdsk_status_1 si hdsk_status_2.
;=============================================================================

HD_CHK_ERROR	proc	near
		push	dx
		push	ax
		mov	dx,1F7h
		in	al,dx			; port 1F7h, hdsk0-status reg
		mov	hdsk_status_2,al
		mov	ah,11h			; bad ECC corected data.
		test	al,4			; ECC corected data ?
		jnz	short loc_552
		mov	ah,0CCh			; write error, selected disk
		test	al,20h			; write fault ?
		jnz	short loc_552
		mov	ah,0AAh			; not ready
		test	al,40h			; not ready ?
		jz	short loc_552
		mov	ah,40h			; seek failure
		test	al,10h			; seek completed ?
		jz	short loc_552
		mov	ah,0			; no errors
		test	al,1			; previous cmd ended in an err?
		jz	short loc_552
		mov	dx,1F1h
		in	al,dx			; port 1F1h, hdsk0-error regstr
		mov	hdsk_error,al		; (0040:008D=82h)
		mov	ah,2			; can't find address mark
		test	al,1			; data address mark not found ?
		jnz	short loc_552
		mov	ah,1			; bad command or parameter
		test	al,4			; command was aborted ?
		jnz	short loc_552
		mov	ah,4			; sector not found
		test	al,10h			; sector ID not found ?
		jnz	short loc_552
		mov	ah,10h			; bad ECC or CRC, disk read
		test	al,40h			; Uncorectable data error ?
		jnz	short loc_552
		mov	ah,40h			; seek failure
		test	al,2			; track 0 error ?
		jnz	short loc_552
		mov	ah,0Ah			; bad sector flag
		test	al,80h			; bad block ?
		jnz	short loc_552
		mov	ah,0BBh			; error occureed, undefined
loc_552:
		mov	hdsk_status_1,ah
		pop	dx
		mov	al,dl
		pop	dx
		or	ah,ah			; Zero ?
		jz	short loc_ret_553	; Jump if zero
		stc				; Set carry flag

loc_ret_553:
		retn
HD_CHK_ERROR	endp


;=============================================================================
; Rutina aceasta trimite o comanda la controller.
; Inainte de a trimite comanda se mascheaza irq2 si irq14.
;=============================================================================

HD_SEND_CMD	proc	near
		call	HD_CHK_BUSY
		cli				; Disable interrupts
		push	ax
		mov	hdsk_int_flags,0	; (0040:008E=0)
		in	al,0A1h			; port 0A1h, 8259-2 int IMR
		jcxz	short loc_554		; Jump if cx=0
loc_554:
		jcxz	short loc_555		; Jump if cx=0
loc_555:
		and	al,0BFh
		out	0A1h,al			; port 0A1h, 8259-2 int comands
		in	al,21h			; port 21h, 8259-1 int IMR
		jcxz	short loc_556		; Jump if cx=0
loc_556:
		jcxz	short loc_557		; Jump if cx=0
loc_557:
		and	al,0FBh
		out	21h,al			; port 21h, 8259-1 int comands
		jcxz	short loc_558		; Jump if cx=0
loc_558:
		jcxz	short loc_559		; Jump if cx=0
loc_559:
		pop	ax
		push	dx
		mov	dx,1F7h
		mov	[bp+8],al		; command
		out	dx,al			; port 1F7h, hdsk0-command reg
		pop	dx
		sti				; Enable interrupts
		retn
HD_SEND_CMD	endp


;=============================================================================
; es:bx = pointer la zona de date
; in [bp+18h] se gaseste lungimea zonei (200h sau 204h(pt. sectoare lungi)).
; Normalizeaza pointerul primit in es:bx si verifica daca nu cumva zona de
; date specificata depaseste o limita de 64K deoarece acest lucru nu e permis
; de catre DMA.
;=============================================================================

CHECK_DMA	proc	near
		push	dx
		push	ax
		mov	ax,bx			; Normalizare:
		shr	ax,4
		mov	dx,es
		add	ax,dx
		mov	es,ax			; es = es + bx / 16
		and	bx,0Fh			; bx = bx & 0Fh
		xor	ax,ax			; Zero register

		mov	dx,1
		sub	ax,bx
		sbb	dx,0
		div	word ptr [bp+18h]	; ax,dxrem=dx:ax/data
		mov	dl,al
		pop	ax
		cmp	dl,al
		pop	dx
		jnc	short loc_ret_560	; Jump if carry=0
		mov	ah,9			; DMA attempt over 64K bound
		mov	hdsk_status_1,ah

loc_ret_560:
		retn
CHECK_DMA	endp


;=============================================================================

HD_SET_STEP	proc	near
		push	es
		push	bx
		push	dx
		push	ax
		call	HD_GET_PTBL_
		mov	al,es:[bx+8]		; al = drive step options
		mov	ah,al
		and	al,8			; al = drive options
		mov	dx,3F6h
		out	dx,al			; port 3F6h, hdsk0 register

;(XT only)	mov	al,hdsk_head_ctrl	; (0040:0076=8)
;		and	al,0C0h
;		or	al,ah
;		mov	hdsk_head_ctrl,al	; (0040:0076=8)

		pop	ax
		pop	dx
		pop	bx
		pop	es
		retn
HD_SET_STEP	endp


;=============================================================================
; al = sector count
; cl = starting logical sector number (bits 0-5)
; cl = cylinder high (bits 6-7 are bits 8-9 of 10 bit cylinder number)
; ch = cylinder low
;=============================================================================

HD_SEND_PARAM	proc	near
		push	ax
		push	dx
		mov	dx,1F2h
		mov	[bp+3],al		; sector count
		out	dx,al			; port 1F2h, hdsk0-sector count
		jcxz	short loc_561		; Jump if cx=0
loc_561:
		jcxz	short loc_562		; Jump if cx=0
loc_562:
		mov	al,cl
		and	al,3Fh			; sterge bitii 6-7
		mov	dx,1F3h
		mov	[bp+4],al		; sector number
		out	dx,al			; port 1F3h, hdsk0-sector numbr
		jcxz	short loc_563		; Jump if cx=0
loc_563:
		jcxz	short loc_564		; Jump if cx=0
loc_564:
		mov	al,ch
		mov	dx,1F4h
		mov	[bp+5],al		; cylinder low byte
		out	dx,al			; port 1F4h, hdsk0-cylr,lo byte
		jcxz	short loc_565		; Jump if cx=0
loc_565:
		jcxz	short loc_566		; Jump if cx=0
loc_566:
		mov	al,cl
		shr	al,6			; Shift w/zeros fill
		mov	dl,[bp+15h]		; head number
		shr	dl,4			; Shift w/zeros fill
		and	dl,0Ch
		or	al,dl			; La 1F5h trimite in bitii 0-1
		mov	dx,1F5h			; hi-cyl si in 2-3 bitii 6-7
		mov	[bp+6],al		; din octetul de la [bp+15h]
		out	dx,al			; port 1F5h, hdsk0-cylr,hi byte
		jcxz	short loc_567		; Jump if cx=0
loc_567:
		jcxz	short loc_568		; Jump if cx=0
loc_568:
		mov	dx,1F6h
		mov	al,[bp+7]		; drive & head
		out	dx,al			; port 1F6h, hdsk0-siz/drv/head
		pop	dx
		pop	ax
		retn
HD_SEND_PARAM	endp


;=============================================================================
; Verifica daca controller-ul este busy. Carry = 1 daca da, 0 daca nu.
; cx = numarul de incercari, ah = masca cu care se verifica hdsk0-status reg
; Asteapta sa se puna pe 1 bitul 4 din octetul citit de la portul 61h.
; Asteapta apoi ca bitul 4 sa devina din nou 0. Practic bucla asta asteapta
; sa obtina de la portul 1F7h o valoare care sa nu aiba nici un bit setat pe
; pozitiile pe care ah are biti setati. Citirea de la portul 1F7h se face
; la tranzitiile 0 -> 1 si 1 -> 0 ale bitului 4 de la portul 61h.
; Nu stiu exact ce inseamna octetul citit de la portul 61h. Tot ce stiu
; este ca pe calculatorul meu bitul 4 trece din 0 in 1 de 32813 (aprox.
; 33000) de ori pe secunda indiferent de viteza procesorului.
;=============================================================================

HD_CHK_NOERR	proc	near
		push	ax
loc_1264:
		in	al,dx			; port 1F7h, hdsk0-status reg
		test	al,ah
		jz	short loc_1268		; exit ok if zero
loc_1265:
		in	al,61h			; port 61h, 8255 port B, read
		test	al,10h			; wait for bit 4 == 1
		jz	loc_1265		; repeat if bit 4 still 0
		dec	cx
		jz	short loc_1267		; error: too many retries

		in	al,dx			; port 1F7h, hdsk0-status reg
		test	al,ah
		jz	short loc_1268		; exit ok if zero
loc_1266:
		in	al,61h			; port 61h, 8255 port B, read
		test	al,10h			; wait for bit 4 == 0
		jnz	loc_1266		; repeat if bit 4 still 1
		dec	cx
		jnz	loc_1264		; Jump if cx != 0
loc_1267:
		stc				; ERROR
loc_1268:
		pop	ax
		retn
HD_CHK_NOERR	endp


;=============================================================================
; Aceasta rutina face exact ce face si rutina anterioara dar asteptand
; ca macar un bit dintre cei setati in ah sa fie setat si in octetul citit
; de la portul 1F7h. Din cate am vazut eu e apelata doar o data, cu ah = 8.
;=============================================================================

HD_CHK_COND	proc	near
		push	ax
loc_1271:
		in	al,dx			; port 1F7h, hdsk0-status reg
		test	al,ah
		jnz	short loc_1275		; Jump if not zero
loc_1272:
		in	al,61h			; port 61h, 8255 port B, read
		test	al,10h			; wait for bit 4 == 1
		jz	loc_1272		; repeat if bit 4 still 0
		dec	cx
		jz	short loc_1274		; error: too many retries

		in	al,dx			; port 1F7h, hdsk0-status reg
		test	al,ah
		jnz	short loc_1275		; exit ok if bit == 1
loc_1273:
		in	al,61h			; port 61h, 8255 port B, read
		test	al,10h			; wait for bit 4 == 0
		jnz	loc_1273		; repeat if bit 4 still 1
		dec	cx
		jnz	loc_1271		; Jump if cx != 0
loc_1274:
		stc				; ERROR
loc_1275:
		pop	ax
		retn
HD_CHK_COND	endp


;=============================================================================
; Tot stilul de bucla de asteptare de pana acum ... Acum asteapta ca bitul 7
; al locatiei de memorie adresata de bx sa devina 1. E apelata dintr-un singur
; loc cu bx == offset hdsk_int_flags. Rutina de intrerupere va seta bitul 7.
;=============================================================================

HD_WAIT_INTR_F	proc	near
		push	ax
loc_1278:
		test	byte ptr [bx],80h
		jnz	short loc_1282		; Jump if not zero
loc_1279:
		in	al,61h			; port 61h, 8255 port B, read
		test	al,10h			; wait for bit 4 == 1
		jz	loc_1279		; repeat if bit 4 still 0
		dec	cx
		jz	short loc_1281		; Jump if zero

		test	byte ptr [bx],80h
		jnz	short loc_1282		; Jump if not zero
loc_1280:
		in	al,61h			; port 61h, 8255 port B, read
		test	al,10h			; wait for bit 4 == 0
		jnz	loc_1280		; repeat if bit 4 still 1
		dec	cx
		jnz	loc_1278		; Jump if not zero
loc_1281:
		stc				; Set carry flag
loc_1282:
		pop	ax
		retn
HD_WAIT_INTR_F	endp


;=============================================================================
; Rutina asta este identica cu oricare din cele precedente cu exceptia teste-
; lor care nu exista. Este pur si simplu o rutina de temporizare. Numarul de
; iteratii este trimis in cx si este adunat cu 2 in interiorul rutinei din
; motive care ma depasesc ...
; Daca intr-adevar tranzitiile bitului 4 citit de la portul 61h nu depind de
; viteza procesorului atunci rutina aaceasta va realiza o pauza cu o lungime
; de (cx + 2) * 0.00003 secunde.
;=============================================================================

DELAY		proc	near
		push	ax
		add	cx,2			; mistere si tenebre ...
loc_1285:
		in	al,61h			; port 61h, 8255 port B, read
		test	al,10h			; wait for bit 4 == 1
		jz	loc_1285		; repeat if bit 4 still 0
		dec	cx
		jz	short loc_1287		; return if zero
loc_1286:
		in	al,61h			; port 61h, 8255 port B, read
		test	al,10h			; wait for bit 4 == 0
		jnz	loc_1286		; repeat if bit 4 still 1
		dec	cx
		jnz	loc_1285		; repeat if not zero
loc_1287:
		pop	ax
		retn
DELAY		endp
