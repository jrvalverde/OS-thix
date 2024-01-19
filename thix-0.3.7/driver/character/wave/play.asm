_TEXT		segment byte public 'CODE'
		assume	cs:_TEXT

_play           proc    far

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

_play          	endp

_TEXT		ends

		public	_play
		extrn   _playdata:word
		end
