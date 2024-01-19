/* syscalls.S -- Thix system calls.  */

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
 * Right now this contains *LOTS* of duplicated pieces of code, one for
 * each system call.  Small differences between them make it nontrivial
 * to have a "general" implementation.  One day  I'll try to reduce the
 * size of this file without speed loss.  Meanwhile...
 */

#include <thix/limits.h>


/* Don't know how to code this with gas :-( .  */
#define seg_es  .byte 0x26


SYS_CALLS = 89


.align  4
sys_call:       pushl   %ebp
		movl    %esp, %ebp

		pushl   %ds
		pushl   %es
		pushal

		movl    $0x10, %bx
		movw    %bx, %ds
		movw    %bx, %es

		cmpl    $SYS_CALLS, %eax
		jae     bad_sys_call

		pushl   $sys_call_end
		shll    $2, %eax
		movl    sys_calls_table(%eax), %eax
		lds     16(%ebp), %esi
		seg_es
		movl    _current_kpid, %ecx
		shll    $2, %ecx
		seg_es
		movl    %esi, _user_mode_ESP(%ecx)
		seg_es
		movl	%ebp, _kernel_mode_ESP(%ecx)
		jmp     %eax

.align  4
bad_sys_call:   pushl   %eax
		call    _bad_system_call
		popl    %eax

sys_call_end:   cli                     /* do we *really* need this ? */

		pushl   %eax
		pushl   $tmp_signum
		call    _issig          /* check for signals */
		addl    $4, %esp
		orl     %eax, %eax
		jz      syscall_nosigchk

		movl    $0x1022, %edx   /* 0x1022 in user mode (see crt0.s) */
		xchgl   4(%ebp), %edx   /* %edx=%eip, %eax=sigfn, 4(%ebp)=0x1022 */
		subl    $12, 16(%ebp)
		movl    tmp_signum, %ecx/* %ecx=sig */
		lds     16(%ebp), %ebx
		movl    %eax, 8(%ebx)   /* SIGNAL function */
		movl    %ecx, 4(%ebx)   /* SIGNAL number */
		movl    %edx, (%ebx)    /* %eip */

syscall_nosigchk:
		/* If a process with a higher priority waked up,
		   reschedule.  */
		seg_es
		cmpl    $0, _sched_touched
		je      continue
		movw    %es,%ax
		movw    %ax,%ds         /* restore ds */
		call    __reschedule

continue:       cli
		popl    %eax
		movw    $0x10, %bx
		movw    %bx, %ds
	
		movl    %eax, tmp_eax
		popal
		movl    tmp_eax, %eax
		popl    %es
		popl    %ds

		popl    %ebp
		iret

tmp_eax:        .long   0
tmp_signum:     .long   0

.globl          tmp_signum

.align  4
__sys_fork:     movw    %bx, %ds
		call    _sys_fork
		ret

_start_child:
		popal
		popl	%es
		popl	%ds

		popl	%ebp

		xorl	%eax, %eax		/* return 0 in the child */
	
		iret
	
.align  4
__sys_exec:     movl    12(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_exec
		addl    $0xc, %esp

		cmpl    $0, %eax
		jge     exec_ok
		ret

exec_ok:        movl    $0xFFFFFFFF - ARG_MAX + 1, %ebx
		seg_es
		subl    _KERNEL_RESERVED_SPACE, %ebx

		pushl   $0x17           /* %ss */
		pushl   %ebx            /* %esp */
		pushl   $0x200          /* %eflags */
		pushl   $0x0F           /* %cs */
		pushl   %eax            /* %eip */

		xorl    %ebx, %ebx
		movl    %ebx, %ecx
		movl    %ebx, %edx
		movl    %ebx, %esi
		movl    %ebx, %edi
		movl    %ebx, %ebp

		movl    $0x17, %eax
		movl    %ax, %ds
		movl    %ax, %es
		xor     %eax, %eax
		iret

.align  4
__sys_ustat:
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_ustat
		addl    $0x8, %esp
		ret

.align  4
__sys_utime:
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_utime
		addl    $0x8, %esp
		ret

.align  4
__sys_getpid:   movw    %bx, %ds
		call    _sys_getpid
		ret

.align  4
__sys_brk:      pushl   %esi
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_brk
		addl    $0x8, %esp
		ret

.align  4
__sys_getppid:  movw    %bx, %ds
		call    _sys_getppid
		ret

.align  4
__sys_exit:     pushl   $0
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_exit
		addl    $0x8, %esp
		ret

.align  4
__sys_umask:    pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_umask
		addl    $0x4, %esp
		ret

.align  4
__sys_fcntl:    pushl   12(%esi)
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_fcntl
		addl    $0xc, %esp
		ret

.align  4
__sys_kctrl:    movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_kctrl
		addl    $0x4, %esp
		ret

.align  4
__sys_kill:     pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_kill
		addl    $0x8, %esp
		ret

.align  4
__sys_wait:     movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_wait
		addl    $0x4, %esp
		ret

.align  4
__sys_pause:    movw    %bx, %ds
		call    _sys_pause
		ret

.align  4
__sys_statfs:   movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_statfs
		addl    $0x8, %esp
		ret

.align  4
__sys_fstatfs:  movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_fstatfs
		addl    $0x8, %esp
		ret

.align  4
__sys_uname:    movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_uname
		addl    $0x4, %esp
		ret

.align  4
__sys_stime:    movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_stime
		addl    $0x4, %esp
		ret

.align  4
__sys_time:     movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_time
		addl    $0x4, %esp
		ret

.align  4
__sys_times:    movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_times
		addl    $0x4, %esp
		ret

.align  4
__sys_alarm:    pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_alarm
		addl    $0x4, %esp
		ret

.align  4
__sys_sync:     movw    %bx, %ds
		call    _sys_sync
		ret

.align  4
__sys_chmod:    pushl   8(%esi)
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_chmod
		addl    $0x8, %esp
		ret

.align  4
__sys_open:     pushl   12(%esi)
		pushl   8(%esi)
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_open
		addl    $0xC, %esp
		ret

.align  4
__sys_close:    pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_close
		addl    $0x4, %esp
		ret

.align  4
__sys_link:     movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_link
		addl    $0x8, %esp
		ret

.align  4
__sys_unlink:   movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_unlink
		addl    $0x4, %esp
		ret

.align  4
__sys_mkdir:    pushl   8(%esi)
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_mkdir
		addl    $0x8, %esp
		ret

.align  4
__sys_chdir:    movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_chdir
		addl    $0x4, %esp
		ret

.align  4
__sys_sigend:   movw    %bx, %ds
		call    _sys_sigend
		ret

.align  4
__sys_readdir:  pushl   12(%esi)
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_readdir
		addl    $0xC, %esp
		ret

.align  4
__sys_rmdir:    movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_rmdir
		addl    $0x4, %esp
		ret

.align  4
__sys_reboot:   pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_reboot
		addl    $0x04, %esp
		ret

.align  4
__sys_lseek:    pushl   12(%esi)
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_lseek
		addl    $0xC, %esp
		ret

.align  4
__sys_read:     pushl   12(%esi)
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_read
		addl    $0xC, %esp
		ret

.align  4
__sys_write:    pushl   12(%esi)
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_write
		addl    $0xC, %esp
		ret

.align  4
__sys_dup:      pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_dup
		addl    $0x4, %esp
		ret

.align  4
__sys_stat:     movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_stat
		addl    $0x8, %esp
		ret

.align  4
__sys_fstat:    movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_fstat
		addl    $0x8, %esp
		ret

.align  4
__sys_dup2:     pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_dup2
		addl    $0x8, %esp
		ret

.align  4
__sys_mknod:    pushl   12(%esi)
		pushl   8(%esi)
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_mknod
		addl    $0xC, %esp
		ret

.align  4
__sys_pipe:     movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_pipe
		addl    $0x4, %esp
		ret

.align  4
__sys_setuid:   pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_setuid
		addl    $0x4, %esp
		ret

.align  4
__sys_setgid:   pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_setgid
		addl    $0x4, %esp
		ret

.align  4
__sys_getuid:   movw    %bx, %ds
		call    _sys_getuid
		ret

.align  4
__sys_geteuid:  movw    %bx, %ds
		call    _sys_geteuid
		ret

.align  4
__sys_getgid:   movw    %bx, %ds
		call    _sys_getgid
		ret

.align  4
__sys_getegid:  movw    %bx, %ds
		call    _sys_getegid
		ret

.align  4
__sys_sigpending:
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_sigpending
		addl    $0x4, %esp
		ret

.align  4
__sys_sigprocmask:
		movl    12(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_sigprocmask
		addl    $0xC, %esp
		ret

.align  4
__sys_sigsuspend:
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_sigsuspend
		addl    $0x4, %esp
		ret

.align  4
__sys_sigaction:
		movl    12(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_sigaction
		addl    $0xC, %esp
		ret

.align  4
__sys_chown:    pushl   12(%esi)
		pushl   8(%esi)
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_chown
		addl    $0xC, %esp
		ret

.align  4
__sys_access:   pushl   8(%esi)
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_access
		addl    $0x8, %esp
		ret

.align  4
__sys_getprocinfo:
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_getprocinfo
		addl    $0x8, %esp
		ret

.align  4
__sys_getsysinfo:
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_getsysinfo
		addl    $0x4, %esp
		ret

.align  4
__sys_ioctl:
		movl    12(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_ioctl
		addl    $0xC, %esp
		ret

.align  4
__sys_tcsendbreak:
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_tcsendbreak
		addl    $0x8, %esp
		ret

.align  4
__sys_tcdrain:
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_tcdrain
		addl    $0x4, %esp
		ret

.align  4
__sys_tcflush:
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_tcflush
		addl    $0x8, %esp
		ret

.align  4
__sys_tcflow:
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_tcflow
		addl    $0x8, %esp
		ret

.align  4
__sys_tcgetattr:
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_tcgetattr
		addl    $0x8, %esp
		ret

.align  4
__sys_tcsetattr:
		movl    12(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_tcsetattr
		addl    $0xC, %esp
		ret

.align  4
__sys_getitimer:
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_getitimer
		addl    $0x8, %esp
		ret

.align  4
__sys_setitimer:
		movl    12(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_setitimer
		addl    $0xC, %esp
		ret

.align  4
__sys_getrlimit:
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_getrlimit
		addl    $0x8, %esp
		ret

.align  4
__sys_setrlimit:
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_setrlimit
		addl    $0x8, %esp
		ret

.align  4
__sys_getrusage:
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_getrusage
		addl    $0x8, %esp
		ret

.align  4
__sys_getpriority:
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_getpriority
		addl    $0x8, %esp
		ret

.align  4
__sys_setpriority:
		pushl   12(%esi)
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_setpriority
		addl    $0xC, %esp
		ret

.align  4
__sys_mount:
		pushl	16(%esi)
		movl	12(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_mount
		addl    $0x10, %esp
		ret

.align  4
__sys_umount:
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_umount
		addl    $0x4, %esp
		ret

.align  4
__sys_waitpid:
		pushl   12(%esi)
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_waitpid
		addl    $0xC, %esp
		ret

.align  4
__sys_setsid:   movw    %bx, %ds
		call    _sys_setsid
		ret

.align  4
__sys_setpgid:
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_setpgid
		addl    $0x8, %esp
		ret

.align  4
__sys_getpgrp:  movw    %bx, %ds
		call    _sys_getpgrp
		ret

.align  4
__sys_swapon:
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_swapon
		addl    $0x4, %esp
		ret

.align  4
__sys_swapoff:
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_swapoff
		addl    $0x4, %esp
		ret

.align  4
__sys_select:   movw    %bx, %ds
		call    _sys_select
		ret

.align  4
__sys_readlink:
		movl    12(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_readlink
		addl    $0xC, %esp
		ret

.align  4
__sys_chroot:
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_chroot
		addl    $0x4, %esp
		ret

.align  4
__sys_truncate: pushl   8(%esi)
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_truncate
		addl    $0x8, %esp
		ret

.align  4
__sys_lstat:    movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_lstat
		addl    $0x8, %esp
		ret

.align  4
__sys_symlink:  movl    8(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_symlink
		addl    $0x8, %esp
		ret

.align  4
__sys_sethostname:
		pushl   8(%esi)
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_sethostname
		addl    $0x8, %esp
		ret

.align  4
__sys_setdomainname:
		pushl   8(%esi)
		movl    4(%esi), %eax
		seg_es
		addl    _KERNEL_RESERVED_SPACE, %eax
		pushl   %eax
		movw    %bx, %ds
		call    _sys_setdomainname
		addl    $0x8, %esp
		ret

.align  4
__sys_ftruncate:
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_ftruncate
		addl    $0x8, %esp
		ret

.align  4
__sys_fchown:
		pushl   12(%esi)
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_fchown
		addl    $0xC, %esp
		ret

.align  4
__sys_fchmod:
		pushl   8(%esi)
		pushl   4(%esi)
		movw    %bx, %ds
		call    _sys_fchmod
		addl    $0x8, %esp
		ret


.data

/* System calls index table:  */

.align  4
sys_calls_table:
		.long   __sys_fork              /* 0 */
		.long   __sys_exec              /* 1 */
		.long   __sys_ustat             /* 2 */
		.long   __sys_utime             /* 3 */
		.long   __sys_getpid            /* 4 */
		.long   __sys_brk               /* 5 */
		.long   __sys_getppid           /* 6 */
		.long   __sys_exit              /* 7 */
		.long   __sys_umask             /* 8 */
		.long   __sys_fcntl             /* 9 */
		.long   __sys_kctrl             /* 10 */
		.long   __sys_kill              /* 11 */
		.long   __sys_wait              /* 12 */ /* (obsolete) */
		.long   __sys_pause             /* 13 */
		.long   __sys_statfs            /* 14 */
		.long   __sys_fstatfs           /* 15 */
		.long   __sys_uname             /* 16 */
		.long   __sys_stime             /* 17 */
		.long   __sys_time              /* 18 */
		.long   __sys_times             /* 19 */ /* (obsolete) */
		.long   __sys_alarm             /* 20 */ /* (obsolete) */
		.long   __sys_sync              /* 21 */
		.long   __sys_chmod             /* 22 */
		.long   __sys_open              /* 23 */
		.long   __sys_close             /* 24 */
		.long   __sys_link              /* 25 */
		.long   __sys_unlink            /* 26 */
		.long   __sys_mkdir             /* 27 */
		.long   __sys_chdir             /* 28 */
		.long   __sys_sigend            /* 29 */
		.long   __sys_readdir           /* 30 */
		.long   __sys_rmdir             /* 31 */
		.long   __sys_reboot            /* 32 */
		.long   __sys_lseek             /* 33 */
		.long   __sys_read              /* 34 */
		.long   __sys_write             /* 35 */
		.long   __sys_dup               /* 36 */
		.long   __sys_stat              /* 37 */
		.long   __sys_fstat             /* 38 */
		.long   __sys_dup2              /* 39 */
		.long   __sys_mknod             /* 40 */
		.long   __sys_pipe              /* 41 */
		.long   __sys_setuid            /* 42 */
		.long   __sys_setgid            /* 43 */
		.long   __sys_getuid            /* 44 */
		.long   __sys_geteuid           /* 45 */
		.long   __sys_getgid            /* 46 */
		.long   __sys_getegid           /* 47 */
		.long   __sys_sigpending        /* 48 */
		.long   __sys_sigprocmask       /* 49 */
		.long   __sys_sigsuspend        /* 50 */
		.long   __sys_sigaction         /* 51 */
		.long   __sys_chown             /* 52 */
		.long   __sys_access            /* 53 */
		.long   __sys_getprocinfo       /* 54 */
		.long   __sys_getsysinfo        /* 55 */
		.long   __sys_ioctl             /* 56 */
		.long   __sys_tcsendbreak       /* 57 */
		.long   __sys_tcdrain           /* 58 */
		.long   __sys_tcflush           /* 59 */
		.long   __sys_tcflow            /* 60 */
		.long   __sys_tcgetattr         /* 61 */
		.long   __sys_tcsetattr         /* 62 */
		.long   __sys_getitimer         /* 63 */
		.long   __sys_setitimer         /* 64 */
		.long   __sys_getrlimit         /* 65 */
		.long   __sys_setrlimit         /* 66 */
		.long   __sys_getrusage         /* 67 */
		.long   __sys_getpriority       /* 68 */
		.long   __sys_setpriority       /* 69 */
		.long   __sys_mount             /* 70 */
		.long   __sys_umount            /* 71 */
		.long   __sys_waitpid           /* 72 */
		.long   __sys_setsid            /* 73 */
		.long   __sys_setpgid           /* 74 */
		.long   __sys_getpgrp           /* 75 */
		.long   __sys_swapon            /* 76 */
		.long   __sys_swapoff           /* 77 */
		.long   __sys_select            /* 78 */
		.long   __sys_readlink          /* 79 */
		.long   __sys_chroot            /* 80 */
		.long   __sys_truncate          /* 81 */
		.long   __sys_lstat             /* 82 */
		.long   __sys_symlink           /* 83 */
		.long   __sys_sethostname       /* 84 */
		.long   __sys_setdomainname     /* 85 */
		.long   __sys_ftruncate         /* 86 */
		.long   __sys_fchown            /* 87 */
		.long   __sys_fchmod            /* 88 */

.globl sys_call
.globl _start_child
