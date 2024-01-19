/* vtty.c -- The virtual terminals driver.  */

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
#include <thix/ctype.h>
#include <thix/string.h>
#include <thix/sleep.h>
#include <thix/sched.h>
#include <thix/signal.h>
#include <thix/vtty.h>
#include <thix/errno.h>
#include <thix/gendrv.h>
#include <thix/types.h>
#include <thix/ioctl.h>
#include <thix/fcntl.h>
#include <thix/kpid.h>
#include <thix/proc/i386.h>


/*
 *      To do:
 *      - ECHOE is not implemented. I don't see any reason why this will ever
 *        be needed.
 *      - The erase & kill characters should erase/kill only the last line of
 *        input.
 *      - Whe a kill character is received (and ECHOK is set), we just switch
 *        to the next line instead of deleteing from the screen the  contents
 *        of the current line (by line we mean input line, not screen line).
 *      - We need an escape sequence to reset the  terminal  to  its  default
 *        state. When received, it should call vtty_reset(vt).
 *      - Control terminal handling is a mess.  I don't  have any information
 *        about it, about how this should look like.  Actually, if a  process
 *        doesn't have a controling terminal, it can't read / modify  a  tty.
 *        Writing is still permitted.  Stupid !
 */


#define MAX_VTTYS               0x0c

#define KEYS                    0x70

#define TAB                     0x08
#define ESC                     0x1b
#define CAPSLOCK                0
#define NUMLOCK                 1


#define KBD_OUTB	(1 << 0)
#define KBD_INPB	(1 << 1)
#define KBD_SYSF	(1 << 2)
#define KBD_CD		(1 << 3)
#define KBD_KEYL	(1 << 4)
#define KBD_AUXB	(1 << 5)
#define KBD_TIM		(1 << 6)
#define KBD_PARE	(1 << 7)

#define keyboard_inpb_empty() ((inb(0x64) & KBD_INPB) == 0)
#define keyboard_outb_empty() ((inb(0x64) & KBD_OUTB) == 0)


/*
 *  Some pseudo names for the supported capabilities (or capabilitiy groups):
 *  NI = not implemented.
 */

#define VTTY_cm      1  /* cursor move command: \E]%i%d;%dH  */

#define VTTY_cc      2  /* cursor commands: ho, ll, le, nd, up, do.  */
			/* ho = \EMH home cursor command.  */
			/* ll = \EML lower left corner command.  */
			/* le = \EME cursor left command.  */
			/* nd = \EMN cursor right command.  */
			/* up = \EMU cursor up command.  */
			/* do = \EMD cursor down command.  */

#define VTTY_vb      3  /* visible bell command: vb = \EB  */

#define VTTY_cv      4  /* cursor visibility commands: vs, vi, ve.  */
			/* vs = \EVS enhance the cursor.  */
			/* vi = \EVI make the cursor invisible.  */
			/* ve = \EVE return the cursor to normal.  */

#define VTTY_clear   5  /* clear commands: cl, cd, ce, ec.  */
			/* cl = \ECS clear the entire screen.  */
			/* cd = \ECD clear from the current line to  the
			   end of screen.  */
			/* ce = \ECE clear from the cursor to the end of
			   line.  */
			/* ec = \ECC (NI) clear N chars from cursor.
			   Don't change cursor position.  */

#define VTTY_line    6  /* insert/delete lines commands: al, dl, AL, DL.  */
			/* al = \ELA insert a blank line.  */
			/* dl = \ELD delete the current line.  */
			/* AL = \EL0 insert N blank lines.  */
			/* DL = \EL1 delete N lines.  */

#define VTTY_char    7  /* insert/delete characters commands: dc, DC.  */
			/* dc = \EDC delete the character at the cursor.  */
			/* DC = \ED0 (NI) delete N characters at the
			   cursor.  */

#define VTTY_scroll  8  /* scroll commands: sf, sr, cs.  */
			/* sf = \ESF scroll forward.  */
			/* sr = \ESR scroll reverse.  */
			/* cs = \ESD (NI) define scroll region.  */

#define VTTY_mode    9  /* standout & appereance modes commands:
			   so,mr,md,mb,me.  */
			/* so = \EAS enter standout mode (reverse mode).  */
			/* md = \EAD enter double-bright mode.  */
			/* mb = \EAB enter blinking mode.  */
			/* me = \EAE turn off all appearance modes.  */

#define VTTY_color  10  /* standard ansi color sequences.  */
			/* foreground = \E[3 *color number* m  */
			/* background = \E[4 *color number* m  */
			/* defaults   = \E[0m  */

#if 0
#define VTTY_nw         /* goto next line + clear to the end.  */
#define VTTY_ti         /* terminal init.  */
#define VTTY_te         /* terminal end.  */
#define VTTY_it         /* initial tab stop.  */
#define VTTY_ct         /* clear tab stop.  */
#define VTTY_st         /* set tab stop.  */
#define VTTY_ps         /* print screen.  */
#define VTTY_po         /* redirect further output to the printer.  */
#define VTTY_pf         /* terminate redirection to the printer.  */
#define VTTY_pO         /* redirect next N chars to the printer.  */
#endif


/*
 *  The virtual terminal structure:
 */

typedef struct
{
    int flags;
    int cx, cy;
    int lines, columns;
    int temp_cx, temp_cy;
    int esc, cap_index, capability;
    volatile int kbdhead, kbdtail, kbdcount;
    int ctrl, shift, alt, capslock, numlock, scrolllock;
    unsigned char separator, color, expected_ansi_seq;
    __pid_t pgrp;
    unsigned char cmask1;
    unsigned short cmask2;
    unsigned cmask4;
    unsigned char foreground, background, reverse, bright, blink;
    unsigned char focus, cursor, scroll;
    unsigned char *screen;
#ifdef __VTTY50L__
    unsigned char videobuf[50 * 80 * 2];
#else
    unsigned char videobuf[25 * 80 * 2];
#endif
    unsigned char kbdqueue[MAX_INPUT];
    unsigned char cap_buf[16];
    tcflag_t c_iflag;   /* flags controlling low-level input handling.  */
    tcflag_t c_oflag;   /* flags controlling low-level output handling.  */
    tcflag_t c_cflag;   /* flags controlling "serial port behaviour".  */
    tcflag_t c_lflag;   /* flags controlling high-level input handling.  */
    cc_t c_cc[NCCS];    /* special characters.  */
} vtty_struct;


static minor_info vtty_minor_info[MAX_VTTYS] =
{
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
};


static character_driver vtty_driver =
{
    "virtual terminal",
    vtty_open,
    vtty_close,
    vtty_read,
    vtty_write,
    NULL,
    vtty_ioctl,
    vtty_fcntl,
    vtty_lseek,
    NULL,               /* vtty_timer */
    VTTY_MAJOR,
    VTTY_IRQ,
    CHR_DRV,
    MAX_VTTYS,
    vtty_minor_info,
};


static char shift    = 0, ctrl    = 0, alt = 0;
static char l_shift  = 0, r_shift = 0;
static char l_ctrl   = 0, r_ctrl  = 0;
static char l_alt    = 0, r_alt   = 0;
static char capslock = 0, numlock = 0, scrolllock = 0;

static unsigned char *hardware_video_address;
static unsigned short hardware_video_offset;

static int ctrl_alt_del_pressed = 0;

/*
unsigned char keys_names[KEYS][20] =
{
    "NoKey", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    "-", "=", "Backspace", "Tab", "q", "w", "e", "r", "t", "y", "u",
    "i", "o", "p", "[", "]", "Enter", "Ctrl", "a", "s", "d", "f", "g",
    "h", "j", "k", "l", ";", "'", "`", "Shift", "\\", "z", "x", "c",
    "v", "b", "n", "m", ",", ".", "/", "Shift", "KeyPad_*", "Alt",
    "Space", "CapsLock", "F1", "F2", "F3", "F4", "F5", "F6", "F7",
    "F8", "F9", "F10", "NumLock", "ScrollLock", "KeyPad_Home", "KeyPad_Up",
    "KeyPad_PgUp", "KeyPad_-", "KeyPad_Left", "KeyPad_5", "KeyPad_Right",
    "KeyPad_+", "KeyPad_End", "KeyPad_Down", "KeyPad_PgDown", "KeyPad_Ins",
    "KeyPad_Del", "", "", "", "F11", "F12", "", "", "", "", "", "", "",
    "Break", "PrintScreen", "KeyPad_/", "Home", "Up", "PgUp", "Left",
    "Right", "End", "Down", "PgDown", "Ins", "Del", "", "", ""
};
*/

unsigned char keys_dep[KEYS] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

unsigned char keys[KEYS][4] =
{
    "", "\e", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    "-", "=", "\x8", "\x9", "q", "w", "e", "r", "t", "y", "u",
    "i", "o", "p", "[", "]", "\xd", "", "a", "s", "d", "f", "g",
    "h", "j", "k", "l", ";", "'", "`", "", "\\", "z", "x", "c",
    "v", "b", "n", "m", ",", ".", "/", "", "*", "", " ", "",
    "\e@a", "\e@b", "\e@c", "\e@d", "\e@e",
    "\e@f", "\e@g", "\e@h", "\e@i", "\e@j",
    "", "", "\eG", "\eA", "\eI", "-", "\eD",
    "\eM", "\eC", "+", "\eH", "\eB", "\eJ",
    "\eE", "\eF", "", "", "", "\e@k", "\e@l", "",
    "", "", "", "", "", "", "\eL", "\eK", "/", "\eG",
    "\eA", "\eI", "\eD", "\eC", "\eH",
    "\eB", "\eJ", "\eE", "\eF", "", "", ""
};

unsigned char shift_keys[KEYS][4] =
{
    "", "\e[0", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")",
    "_", "+", "\e[U", "\e[T", "Q", "W", "E", "R", "T", "Y", "U",
    "I", "O", "P", "{", "}", "\xd", "", "A", "S", "D", "F", "G", "H",
    "J", "K", "L", ":", "\x22", "~", "", "|", "Z", "X", "C", "V", "B",
    "N", "M", "<", ">", "?", "", "*", "", " ", "", "\e[a", "\e[b",
    "\e[c", "\e[d", "\e[e", "\e[f", "\e[g", "\e[h", "\e[i",
    "\e[j", "", "", "7", "8", "9", "-", "4", "5", "6", "+", "1", "2",
    "3", "0", ".", "", "", "", "\e[k", "\e[l", "", "", "", "", "",
    "", "", "\e[L", "\e[K", "/", "\e[G", "\e[A", "\e[I", "\e[D",
    "\e[C", "\e[H", "\e[B", "\e[J", "\e[E", "\e[F", "", "", ""
};

unsigned char ctrl_keys[KEYS][4] =
{
    "", "\e]0", "\x1", "\x0", "\x3", "\x4", "\x5", "\x1e", "\x7", "\x8",
    "\x9", "\x0", "\x1f", "\x2c", "\e]U", "\e]T", "\x11", "\x17", "\x5",
    "\x12", "\x14", "\x19", "\x15", "\x9", "\xf", "\x10", "\e", "\x1d",
    "\xd", "", "\x1", "\x13", "\x4", "\x6", "\x7", "\x8", "\xa", "\xb", "\xc",
    "\x3b", "~", "~", "", "\x1c", "\x1a", "\x18", "\x3", "\x16", "\x2",
    "\xe", "\xd", "\x3a", "\x2e", "\x2f", "", "", "", " ", "", "\e]a",
    "\e]b", "\e]c", "\e]d", "\e]e", "\e]f", "\e]g", "\e]h",
    "\e]i", "\e]j", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "\e]k", "\e]l", "", "", "", "", "", "",
    "", "\e]L", "\e]K", "", "\e]G", "\e]A", "\e]I", "\e]D",
    "\e]C", "\e]H", "\e]B", "\e]J", "\e]E", "\e]F", "", "", ""
};

unsigned char alt_keys[KEYS][4] =
{
    "", "@?0", "@1", "@2", "@3", "@4", "@5", "@6", "@7", "@8", "@9", "@0",
    "@-", "@=", "@\x7f", "@\x9", "@q", "@w", "@e", "@r", "@t", "@y", "@u",
    "@i", "@o", "@p", "@[", "@]", "@\xd", "", "@a", "@s", "@d", "@f", "@g",
    "@h", "@j", "@k", "@l", "@;", "@'", "@`", "", "@\\", "@z", "@x", "@c",
    "@v", "@b", "@n", "@m", "@,", "@.", "@/", "", "@*", "", "@ ", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "@-", "", "", "", "@+",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "@?L", "@?K", "@/", "@?G", "@?A", "@?I", "@?D", "@?C", "@?H", "@?B",
    "@?J", "@?E", "@?F", "", "", ""
};

vtty_struct vtty[MAX_VTTYS];
vtty_struct *cvtty = &vtty[0];

/* A color map for converting ANSI colors to PC ones.  */
unsigned char color_map[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };


#define seqcpy(cseq, sequence)                                  \
    *(unsigned long *)(cseq) = *(unsigned long *)(sequence)


#define cursor_move(video_address)                              \
{                                                               \
    outw(0x3d4, ((video_address) & 0xFF00)            + 0x0e);  \
    outw(0x3d4, ((unsigned char)(video_address) << 8) + 0x0f);  \
}


/*
 * Some of the keyboard initialization stuff has been stolen from Linux.
 */

int
keyboard_ready(void)
{
    int i;

    for (i = 0; i < 0x100000; i++)
	if (keyboard_inpb_empty())
	    return 1;

    return 0;
}


void
keyboard_command(unsigned char command)
{
    if (keyboard_ready())
	outb(0x64, command);
    else
	printk("VTTY: keyboard busy\n");
    
}


static void
update_leds(void)
{
    shift = l_shift | r_shift;
    ctrl  = l_ctrl  | r_ctrl;
    alt   = l_alt   | r_alt;

#if 0
    /* This code doesn't seem to work.  It completely locks the
       keyboard on some PCs.  I'll try to figure out how this should
       be done.  */
    outb(0x60, 0xed);

    if (keyboard_ready() == 0)
	printk("VTTY: keyboard busy\n");

    outb(0x60, ((capslock    & 1) << 2) |
		((numlock    & 1) << 1) |
		 (scrolllock & 1));
#endif
}


static void
alt_keys_setup(void)
{
    int key;

    for (key = 0; key < KEYS; key++)
	if (alt_keys[key][0] == '@')
	    alt_keys[key][0] = ESC;
}


static void
update_scrolllock(vtty_struct *vt, int status)
{
    if (status)
	vt->scroll = 0;
    else
    {
	vt->scroll = 1;
	wakeup(&vt->scroll);
    }

    scrolllock = status;
    update_leds();
}


void
vtty_beep(void)
{
    outb(0x61, inb(0x61) | 3);          /* start the speaker. */

    outb(0x43, 0xb6);                   /* set up the timer. */

    outb(0x42, 0x00);                   /* send the low byte to the timer. */
    outb(0x42, 0x08);                   /* send the high byte to the timer. */

    io_delay(0x400);

    outb(0x61, inb(0x61) & ~3);         /* stop that noise ! */

    timer_init();                       /* reinitialize the timer. */
}


static void
vtty_6845setup(void)
{
    outb_delay(0x3c4, 4);
    outb_delay(0x3c5, 1);
}


static inline void
vtty_hardscroll(unsigned short address)
{
    outb_delay(0x3d4, 0xc);
    outb_delay(0x3d5, address >> 8);
    outb_delay(0x3d4, 0xd);
    outb_delay(0x3d5, address & 0xFF);
}


static void
vtty_cursormove(vtty_struct *vt, int x, int y)
{
    if (x < vt->columns && y < vt->lines)
    {
	vt->cx = x;
	vt->cy = y;

	if (vt->focus)
	    cursor_move(hardware_video_offset / 2 + y * vt->columns + x);
    }
}


static int
vtty_cm(vtty_struct *vt, unsigned char c)    /* \E]%i%d;%dH */
{
    switch (vt->cap_index)
    {
	case 0:
	    if (isdigit(c))
		vt->temp_cy = c - '0';
	    else
		return 0;

	    break;

	case 1:
	    if (c == ';')
		vt->separator = 1;
	    else
		if (isdigit(c))
		    vt->temp_cy = vt->temp_cy * 10 + c - '0';
		else
		    return 0;

	    break;

	case 2:
	    if (c == ';')
		if (vt->separator)
		    return 0;
		else
		{
		    vt->separator = 1;
		    break;
		}

	    if (isdigit(c))
		if (vt->separator)
		    vt->temp_cx = vt->temp_cx * 10 + c - '0';
		else
		    vt->temp_cy = vt->temp_cy * 10 + c - '0';
	    else
		return 0;

	    break;

	case 3:
	    if (c == 'H')
		goto ok;

	    if (c == ';')
		if (vt->separator)
		    return 0;
		else
		{
		    vt->separator = 1;
		    break;
		}

	    if (isdigit(c))
		vt->temp_cx = vt->temp_cx * 10 + c - '0';
	    else
		return 0;

	    break;

	case 4:
	    if (!vt->separator)
		return 0;

	case 5:
	case 6:
	    if (c == 'H')
		goto ok;

	    if (isdigit(c))
		vt->temp_cx = vt->temp_cx * 10 + c - '0';
	    else
		return 0;

	    break;

	case 7:
	    if (c == 'H')
		goto ok;

	    return 0;

	default:
		return 0;
    }

    vt->cap_index++;
    return 1;

  ok:

    vtty_cursormove(vt, vt->temp_cx - 1, vt->temp_cy - 1);
    return 0;
}


/*
 * Implementation of cursor commands.  Currently supported:
 * ho -> home cursor
 * ll -> lower left corner
 * le -> cursor left
 * nd -> cursor right
 * up -> up one line
 * do -> down one line
 */

static int
vtty_cc(vtty_struct *vt, unsigned char c)
{
    switch (c)
    {
	case 'H':                               /* Home cursor.  */
	    vt->cx = vt->cy = 0;
	    break;

	case 'L':                               /* Lower left corner.  */
	    vt->cx = 0;
	    vt->cy = vt->lines - 1;
	    break;

	case 'E':                               /* Cursor left.  */
	    if (vt->cx == 0)
		return 0;
	    vt->cx--;
	    break;

	case 'N':                               /* Cursor right.  */
	    if (vt->cx == vt->columns - 1)
		return 0;
	    vt->cx++;
	    break;

	case 'U':                               /* Up one line.  */
	    if (vt->cy == 0)
		return 0;
	    vt->cy--;
	    break;

	case 'D':                               /* Down one line.  */
	    if (vt->cy == vt->lines - 1)
		return 0;
	    vt->cy++;
	    break;

	default:
	    return 0;
    }

    if (vt->focus)
	cursor_move(hardware_video_offset / 2 + vt->cy * vt->columns + vt->cx);

    return 0;
}


/*
 * The visible bell implementation.  Still missing :-).
 */

static void
vtty_vb(vtty_struct *vt)
{
    /* Visible bell.  Do something nice here.  */
}


/*
 * Change the cursor appereance.
 */

static int
vtty_cv(vtty_struct *vt, unsigned char c)
{
    switch(c)
    {
	case 'I':                       /* Make the cursor invisible.  */
	    outw(0x3d4, 0x200a);        /* Always 0x20.  */
	    outw(0x3d4, 0x000b);        /* Always 0.  */
	    break;

	case 'E':                       /* Return the cursor to normal.  */
#ifdef __VTTY50L__
	    /* These things are so chip dependent...  */
	    outw(0x3d4, 0x060a);        /* 6 for 80/50.  */
	    outw(0x3d4, 0x0e0b);        /* 7 for 80/50.  */
#else
	    outw(0x3d4, 0x0d0a);
	    outw(0x3d4, 0x0e0b);
#endif
	    break;

	case 'S':                       /* Enhance the cursor.  */
	    outw(0x3d4, 0x000a);        /* Always 0.  */
#ifdef __VTTY50L__
	    outw(0x3d4, 0x1f0b);        /* 0x1f for 80/50.  */
#else
	    outw(0x3d4, 0x0f0b);
#endif
	    break;

	default:
	    return 0;
    }

    vt->cursor = c;
    return 0;
}


/*
 * Clear the virtual terminal screen.
 */

static int
vtty_clear(vtty_struct *vt, unsigned char c)
{
    switch (c)
    {
	case 'S':
	    lmemset(vt->screen, vt->cmask4, (vt->columns * vt->lines) / 2);
	    vt->cx = vt->cy = 0;

	    if (vt->focus)
		cursor_move(hardware_video_offset/2+vt->cy*vt->columns+vt->cx);
	    break;

	case 'D':
	    lmemset(vt->screen + (vt->cy * vt->columns * 2), vt->cmask4,
		    (vt->lines - vt->cx) * vt->columns * 2 / 4);
	    break;

	case 'E':
	    if (vt->cx & 1)
	    {
		*(unsigned short *)(vt->screen +
			(vt->cy * vt->columns + vt->cx) * 2) = vt->cmask2;

		lmemset(vt->screen + (vt->cy * vt->columns + vt->cx + 1) * 2,
			vt->cmask4, (vt->columns - (vt->cx + 1)) / 2);
	    }
	    else
	    {
		lmemset(vt->screen + (vt->cy * vt->columns + vt->cx) * 2,
			vt->cmask4, (vt->columns - vt->cx) / 2);
	    }
	    break;

	case 'C':
	    /* Not implemented yet.  */
	    break;

	default:
	    break;
    }

    return 0;
}


static int
vtty_line(vtty_struct *vt, unsigned char c)
{
    int i;

    switch (c)
    {
	case 'A':
	    for (i = vt->lines - 1 - 1; i >= vt->cy; i--)
		memcpy(vt->screen + (i + 1) * vt->columns * 2,
		       vt->screen + i * vt->columns * 2,
		       vt->columns * 2);
	    lmemset(vt->screen + vt->cy * vt->columns * 2, vt->cmask4,
		    (vt->columns * 2) / 4);
	    break;

	case 'D':
	    memcpy(vt->screen + vt->cy * vt->columns * 2,
		   vt->screen + (vt->cy + 1) * vt->columns * 2,
		   (vt->lines - 1 - vt->cy) * vt->columns * 2);
	    lmemset(vt->screen + (vt->lines - 1) * vt->columns * 2, vt->cmask4,
		    (vt->columns * 2) / 4);
	    break;

	case '0':
	    /* Not implemented.  */
	    break;

	case '1':
	    /* Not implemented.  */
	    break;

	default:
	    break;
    }

    return 0;
}


static int
vtty_char(vtty_struct *vt, unsigned char c)
{
    switch (c)
    {
	case 'C':
	    memcpy(vt->screen + (vt->cy * vt->columns + vt->cx) * 2,
		   vt->screen + (vt->cy * vt->columns + vt->cx + 1) * 2,
		   (vt->columns - vt->cx - 1) * 2);
	    *(unsigned short *)(vt->screen +
				(vt->cy * vt->columns +
				 vt->columns - 1) * 2) = vt->cmask2;
	    break;

	case '0':
	    /* Not implemented.  */
	    break;

	default:
	    break;
    }

    return 0;
}


static int
vtty_scroll(vtty_struct *vt, unsigned char c)
{
    int i;

    switch (c)
    {
	case 'F':
	    memcpy(vt->screen, vt->screen + vt->columns * 2,
		   (vt->lines - 1) * vt->columns * 2);
	    lmemset(vt->screen + (vt->lines - 1) * vt->columns * 2, vt->cmask4,
		    (vt->columns * 2) / 4);
	    break;

	case 'R':
	    for (i = vt->lines - 1 - 1; i >= 0; i--)
		memcpy(vt->screen + (i + 1) * vt->columns * 2,
		       vt->screen + i * vt->columns * 2,
		       vt->columns * 2);
	    lmemset(vt->screen, vt->cmask4, (vt->columns * 2) / 4);
	    break;

	case 'D':
	    /* Not implemented.  */
	    break;

	default:
	    break;
    }

    return 0;
}


static void
cmask_setup(vtty_struct *vt)
{
    vt->cmask1 = vt->reverse ?
		  (vt->blink|(vt->foreground<<4)|vt->bright|vt->background) :
		  (vt->blink|(vt->background<<4)|vt->bright|vt->foreground);

    vt->cmask2 = ' ' + (vt->cmask1 << 8);
    vt->cmask4 = (vt->cmask2 << 16) + vt->cmask2;
}


static int
vtty_mode(vtty_struct *vt, unsigned char c)
{
    switch (c)
    {
	case 'S': vt->reverse = 0x01; break;
	case 'D': vt->bright  = 0x08; break;
	case 'B': vt->blink   = 0x80; break;
	case 'E': vt->bright  =
		  vt->blink   =
		  vt->reverse = 0x00; break;
	default:  break;
    }

    cmask_setup(vt);

    DEBUG(9, "fg=%x bg=%x b=%w d=%w r=%w\n",
	  vt->foreground, vt->background, vt->blink, vt->bright, vt->reverse);

    return 0;
}


static int
vtty_color(vtty_struct *vt, unsigned char c)
{
    switch (vt->cap_index)
    {
	case 0:
	    switch (c)
	    {
		case '3': vt->expected_ansi_seq = 1; break; /* foreground.  */
		case '4': vt->expected_ansi_seq = 2; break; /* background.  */
		case '0': vt->expected_ansi_seq = 3; break; /* defaults.  */
		default : return 0;			    /* error.  */
	    }

	    break;

	case 1:
	    if (vt->expected_ansi_seq != 3 && (c >= '0' && c <= '7'))
	    {
		vt->color = color_map[c - '0'];
		break;
	    }
	    else if (vt->expected_ansi_seq == 3 && c == 'm')
	    {
		/* I don't know exactly what I should do here in
		   addition to restoring the default colors.
		   `home cursor' ?  `clear screen' ?  */
		vt->foreground = color_map[7];
		vt->background = color_map[0];
		cmask_setup(vt);
	    }

	    return 0;

	case 2:
	    if (c == 'm')
	    {
		switch (vt->expected_ansi_seq)
		{
		    case 1:
			vt->foreground = vt->color;
			break;

		    case 2:
			vt->background = vt->color;
			break;
		}

		cmask_setup(vt);
	    }

	    return 0;

	default:
	    return 0;
    }

    vt->cap_index++;
    return 1;
}


static void
vtty_echo(vtty_struct *vt, unsigned char c)
{
    char *screen = vt->screen;

    switch(c)
    {
	case 7:
	    vtty_beep();
	    return;

	case 8:
	    if (vt->cx)
	    {
		screen += (vt->cy * vt->columns + --vt->cx) << 1;
		*screen = ' ';
		goto scroll;
	    }
	    else
		return;

	    break;

	case '\t':
	    vt->cx += TAB - (vt->cx % TAB);

	    if (vt->cx < vt->columns)
		goto scroll;

	case '\n':
	    vt->cy++;

	    if (!(vt->c_oflag & OPOST))
		goto scroll;

	case '\r':
	    vt->cx = 0;
	    goto scroll;

	default:
	    break;
    }

    screen += (vt->cy * vt->columns + vt->cx) << 1;
    screen[0] = c;
    screen[1] = vt->cmask1;

    if (++vt->cx == vt->columns)
    {
	vt->cx = 0;
	vt->cy++;
    }

   scroll:

    if (vt->cy >= vt->lines)
    {
	if (vt->focus)
	    if (hardware_video_offset < 0x2000)
	    {
		hardware_video_offset += vt->columns * 2;
		vt->screen            += vt->columns * 2;
		vtty_hardscroll(hardware_video_offset / 2);
	    }
	    else
	    {
		lmemcpy(hardware_video_address, vt->screen + vt->columns * 2,
			(vt->lines - 1) * vt->columns / 2);
		vt->screen = hardware_video_address;
		vtty_hardscroll(hardware_video_offset = 0);
	    }
	else
	    lmemcpy(vt->screen, vt->screen + vt->columns * 2,
		    (vt->lines - 1) * vt->columns / 2);

	/* Should this be done before scrolling for better appearance !?  */
	screen = vt->screen + (vt->lines - 1) * vt->columns * 2;
	lmemset(screen, vt->cmask4, vt->columns / 2);
	vt->cy = vt->lines - 1;
    }

    vtty_cursormove(vt, vt->cx, vt->cy);
}


static int
vtty_outchar(vtty_struct *vt, unsigned char c)
{
    if (c == 0)
	return 1;

    while (!vt->scroll)
    {
	if (vt->flags & O_NONBLOCK)
	    return -1;

	if (sleep(&vt->scroll, WAIT_TTY_OUTPUT))
	    return 0;
    }

    if (c == ESC)
    {
	vt->esc        = 1;
	vt->cap_index  = 0;
	vt->capability = 0;
	return 1;
    }

    if (vt->esc == 0)
    {
	vtty_echo(vt, c);
	return 1;
    }

    if (vt->capability)
    {
	switch (vt->capability)
	{
	    case VTTY_cm:     vt->esc = vtty_cm(vt,     c); break;
	    case VTTY_cc:     vt->esc = vtty_cc(vt,     c); break;
	    case VTTY_cv:     vt->esc = vtty_cv(vt,     c); break;
	    case VTTY_clear:  vt->esc = vtty_clear(vt,  c); break;
	    case VTTY_line:   vt->esc = vtty_line(vt,   c); break;
	    case VTTY_char:   vt->esc = vtty_char(vt,   c); break;
	    case VTTY_scroll: vt->esc = vtty_scroll(vt, c); break;
	    case VTTY_mode:   vt->esc = vtty_mode(vt,   c); break;
	    case VTTY_color:  vt->esc = vtty_color(vt,  c); break;

	    default:
		break;
	}

	return 1;
    }

    switch (c)
    {
	case ']':
	    vt->temp_cx    = 0;
	    vt->temp_cy    = 0;
	    vt->separator  = 0;
	    vt->capability = VTTY_cm;
	    break;

	case 'B':
	    vtty_vb(vt);
	    vt->esc = 0;
	    break;

	case 'M': vt->capability = VTTY_cc;     break;
	case 'V': vt->capability = VTTY_cv;     break;
	case 'C': vt->capability = VTTY_clear;  break;
	case 'L': vt->capability = VTTY_line;   break;
	case 'D': vt->capability = VTTY_char;   break;
	case 'S': vt->capability = VTTY_scroll; break;
	case 'A': vt->capability = VTTY_mode;   break;
	case '[': vt->capability = VTTY_color;  break;

	default:
	    vt->esc = 0;
	    break;
    }

    return 1;
}


static void
vtty_insertkey(vtty_struct* vt, unsigned char *sequence)
{
    unsigned char *seqp;        /* Just a pointer...  */
    unsigned char cseq[4];      /* The computed sequence.  */


    seqcpy(cseq, sequence);

    if (!(vt->c_cflag & CREAD))
	return;

    if (*cseq == '\r')
    {
	if (vt->c_iflag & IGNCR)
	    return;

	if (vt->c_iflag & ICRNL)
	    *cseq = '\n';
    }
    else
	if ((*cseq == '\n') && (vt->c_iflag & INLCR))
	    *cseq = '\r';

    if (vt->c_lflag & ISIG)
    {
	int signal = 0;

	if (*cseq == vt->c_cc[VINTR])
	    signal = SIGINT;

	if (*cseq == vt->c_cc[VQUIT])
	    signal = SIGQUIT;

#if 0
	/* Job control is not implemented yet.  */
	if (*cseq == vt->c_cc[VSUSP])
	    signal = SIGTSTP;
#endif

	if (signal)
	{
	    int kpid;

	    if ((vt->c_lflag & NOFLSH) == 0)
	    {
		/* Discard the virtual terminal input queue. */
		vt->kbdhead = vt->kbdtail = vt->kbdcount = 0;

		/* If we'll ever buffer the output, it should be discarded
		   here.  */
	    }

	    for (kpid = FIRST; kpid; kpid = NEXT(kpid))
		if (status(kpid) != PROCESS_ZOMBIE                      &&
		    u_area[kpid].ctty == makedev(VTTY_MAJOR, vt - vtty) &&
		    !u_area[kpid].leader)
		    kill_kpid(kpid, signal);

	    return;
	}
    }


    if (vt->c_lflag & ICANON)
    {
	if (*cseq == vt->c_cc[VKILL])
	{
	    vt->kbdhead = vt->kbdtail = vt->kbdcount = 0;
	    if (vt->c_lflag & ECHOK)
		if (vt->scroll)
		    vtty_echo(vt, '\n');
	    return;
	}

	if (*cseq == vt->c_cc[VERASE])
	{
	    if (vt->kbdcount)
	    {
		if (vt->kbdtail)
		    vt->kbdtail--;
		else
		    vt->kbdtail = MAX_INPUT - 1;
		vt->kbdcount--;

		if (vt->scroll)
		    vtty_echo(vt, 8);
	    }
	    return;
	}

	if (vt->c_iflag & IXON)
	{
	    if (*cseq == vt->c_cc[VSTOP])
	    {
		update_scrolllock(vt, 1);
		return;
	    }

	    if (*cseq == vt->c_cc[VSTART])
	    {
		update_scrolllock(vt, 0);
		return;
	    }
	}

	if (*cseq == 7)
	{
	    vtty_beep();
	    return;
	}
    }

    /* We should use here strlen(cseq) instead of 4.  */
    if (MAX_INPUT - vt->kbdcount >= 4)
    {
	for (seqp = cseq; *seqp; seqp++)
	{
	    if (vt->c_iflag & ISTRIP)
		*seqp &= 0x7f;

	    vt->kbdqueue[vt->kbdtail++] = *seqp;
	    vt->kbdcount++;

	    if (vt->kbdtail == MAX_INPUT)
		vt->kbdtail = 0;

	    if (vt->c_lflag & ECHO)
	    {
		if (*seqp == vt->c_cc[VEOF])
		    continue;
	    }
	    else
		if (!(vt->c_lflag & ECHONL) || *seqp != vt->c_cc[VEOL])
		    continue;

	    if (vt->scroll)
		vtty_echo(vt, *seqp);
	}

	if (vt->c_lflag & ICANON)
	    if (*cseq != vt->c_cc[VEOL] && *cseq != vt->c_cc[VEOF])
		return;

	wakeup(vt);
    }
}


static void
vtty_reset(vtty_struct *vt)
{
#ifdef __VTTY50L__
    vt->lines   = 50;
#else
    vt->lines   = 25;
#endif
    vt->columns = 80;

    vt->kbdcount = 0;
    vt->esc      = 0;

    vt->foreground = 0x07;
    vt->background = 0x00;
    vt->reverse    = 0;
    vt->bright     = 0;
    vt->blink      = 0;

    vt->cmask1 = 0x07;
    vt->cmask2 = 0x0720;
    vt->cmask4 = 0x07200720;

    vt->cursor = 'E';

    vt->c_iflag = ICRNL | IGNBRK | IXON;
    vt->c_oflag = OPOST;
    vt->c_cflag = CLOCAL | CREAD;
    vt->c_lflag = ICANON | ECHO | ECHOE | ECHOK | ISIG;

    memset(vt->c_cc, 0, sizeof(vt->c_cc));

    /* Edit characters.  */
    vt->c_cc[VEOF]   = 0x04;            /* ^D */
    vt->c_cc[VEOL]   = '\n';            /* ^J */
    vt->c_cc[VERASE] = 0x08;            /* ^H */
    vt->c_cc[VKILL]  = 0x15;            /* ^U */

    /* Start/stop characters.  */
    vt->c_cc[VSTOP]  = 0x13;            /* ^S */
    vt->c_cc[VSTART] = 0x11;            /* ^Q */

    /* Signal characters.  */
    vt->c_cc[VINTR]  = 0x03;            /* ^C */
    vt->c_cc[VQUIT]  = 0x1c;            /* ^\ */
    vt->c_cc[VSUSP]  = 0x1a;            /* ^Z */

    vt->kbdhead = 0;
    vt->kbdtail = 0;
}


static void
vtty_setup(vtty_struct* vt)
{
    vt->screen     = vt->videobuf;

    vt->cx         = 0;
    vt->cy         = 0;

    vt->flags      = 0;
    vt->focus      = 0;

    vt->ctrl       = 0;
    vt->shift      = 0;
    vt->alt        = 0;
    vt->capslock   = 0;
    vt->numlock    = 0;
    vt->scrolllock = 0;
    vt->scroll     = 1;

    vtty_reset(vt);

    lmemset(vt->videobuf, vt->cmask4, (vt->lines * vt->columns * 2) / 4);
}


static int
vtty_line_count(vtty_struct *vt)
{
    char *kbdptr;
    char *start, *end;
    char _eol_ = vt->c_cc[VEOL];
    char _eof_ = vt->c_cc[VEOF];


    if (vt->kbdhead == vt->kbdtail)
	return -1;

    if (vt->kbdhead > vt->kbdtail)
    {
	start = vt->kbdqueue + vt->kbdhead;
	end   = vt->kbdqueue + MAX_INPUT;

	for (kbdptr = start; kbdptr < end; kbdptr++)
	    if (*kbdptr == _eol_ || *kbdptr == _eof_)
		return kbdptr - start + (*kbdptr == _eol_);

	start = vt->kbdqueue;
	end   = vt->kbdqueue + vt->kbdtail;

	for (kbdptr = start; kbdptr < end; kbdptr++)
	    if (*kbdptr == _eol_ || *kbdptr == _eof_)
		return kbdptr - start + (*kbdptr == _eol_) +
		       MAX_INPUT - vt->kbdhead;
    }
    else
    {
	start = vt->kbdqueue + vt->kbdhead;
	end   = vt->kbdqueue + vt->kbdtail;

	for (kbdptr = start; kbdptr < end; kbdptr++)
	    if (*kbdptr == _eol_ || *kbdptr == _eof_)
		return kbdptr - start + (*kbdptr == _eol_);
    }

    return -1;
}


static void
discard_eof(vtty_struct *vt)
{
    if (vt->kbdcount && vt->kbdqueue[vt->kbdhead] == vt->c_cc[VEOF])
    {
	if (++vt->kbdhead == MAX_INPUT)
	    vt->kbdhead = 0;
	vt->kbdcount--;
    }
}


static int
vtty_read_data(vtty_struct* vt, char *buf, int cnt)
{
    int count, first_part;

    if (vt->c_lflag & ICANON)
    {
	while ((count = vtty_line_count(vt)) == -1)
	{
	    if (vt->flags & O_NONBLOCK)
		return -EAGAIN;

	    if (sleep(vt, WAIT_TTY_INPUT))
		return -EINTR;
	}

	cnt = min(cnt, count);
    }
    else
    {
	int _min_  = vt->c_cc[VMIN];
	int _time_ = vt->c_cc[VTIME];

	if (_min_)
	    if (_time_)
	    {
		unsigned start_ticks;
		unsigned start_count;
		unsigned timeout = _time_ * HZ / 10;

		while (vt->kbdcount < _min_)
		{
		    start_ticks = ticks;
		    start_count = vt->kbdcount;

		    while (start_count >= vt->kbdcount &&
			   ticks - start_ticks < timeout)
			reschedule();

		    if (start_count >= vt->kbdcount)
			break;
		}
	    }
	    else
	    {
		while (vt->kbdcount < _min_)
		{
		    if (vt->flags & O_NONBLOCK)
			return -EAGAIN;

		    if (sleep(vt, WAIT_TTY_INPUT))
			return -EINTR;
		}
	    }
	else
	    if (_time_)
	    {
		unsigned start_ticks = ticks;
		unsigned timeout = _time_ * HZ / 10;

		while (vt->kbdcount == 0 && ticks - start_ticks < timeout)
		    reschedule();
	    }
	    else
		if (vt->kbdcount == 0)
		    return 0;

	cnt = min(cnt, vt->kbdcount);
    }

    if (cnt > (first_part = MAX_INPUT - vt->kbdhead))
    {
	memcpy(buf, &vt->kbdqueue[vt->kbdhead], first_part);
	memcpy(buf + first_part, vt->kbdqueue, vt->kbdhead = cnt - first_part);
    }
    else
    {
	memcpy(buf, &vt->kbdqueue[vt->kbdhead], cnt);
	vt->kbdhead += cnt;
    }

    vt->kbdcount -= cnt;

    if (vt->c_lflag & ICANON)
	discard_eof(vt);

    return cnt;
}


static int
vtty_write_data(vtty_struct* vt, char *buf, int cnt)
{
    int i, done;

    for (i = 0; i < cnt; i++)
    {
	done = vtty_outchar(vt, buf[i]);

	if (done == 0)
	    return -EINTR;

	if (done < 0)
	    return -EAGAIN;
    }

    return cnt;
}


static int
vtty_keyboard_interrupt(vtty_struct* vt)
{
    unsigned char code;
    unsigned char key = 0;
    extern void display_sys_info();
    int modified = 0, vtty_no = -1;
    int old_scrolllock = scrolllock;
    static int igncnt = 0, E0_received = 0;

    /* Disable the keyboard.  */
    /* keyboard_command(0xAD); */

    if (!keyboard_ready())
    {
	printk("VTTY: keyboard input buffer full\n");
	goto end;
    }

    code = inb(0x60);

    if (!keyboard_outb_empty())
    {
	printk("VTTY: keyboard output buffer full\n");
	goto end;
    }

    if (code == 0x00 || code == 0xFF)
    {
	printk("VTTY: keyboard overflow error\n");
	goto end;
    }

    if (igncnt == 0)
	if (code == 0xe1)
	{
	    igncnt = 5;
	    key = 0x60;
	}
	else
	{
	    if (E0_received)
		switch (code)
		{
		    case 0x1d: r_ctrl = modified = 1;    break;
		    case 0x2a:                           break;
		    case 0x35: key = 0x62;               break; /* KP / */
		    case 0x36:                           break;
		    /* PrintScreen (when Ctrl is pressed) */
		    case 0x37: key = 0x61;               break;
		    case 0x38: r_alt = modified = 1;     break;
		    case 0x46: key = 0x60;               break; /* Break */
		    case 0x47: key = 0x63;               break; /* Home */
		    case 0x48: key = 0x64;               break; /* Up */
		    case 0x49: key = 0x65;               break; /* PgUp */
		    case 0x4b: key = 0x66;               break; /* Left */
		    case 0x4d: key = 0x67;               break; /* Right */
		    case 0x4f: key = 0x68;               break; /* End */
		    case 0x50: key = 0x69;               break; /* Down */
		    case 0x51: key = 0x6a;               break; /* PgDown */
		    case 0x52: key = 0x6b;               break; /* Ins */
		    case 0x53: key = 0x6c;               break; /* Del */
		    case 0x9d: r_ctrl = 0; modified = 1; break;
		    case 0xb8: r_alt  = 0; modified = 1; break;
		    case 0xfa: break;
		    default  : if (code < 0x80) key = code;
		}
	    else
		switch (code)
		{
		    case 0x1d: l_ctrl  = modified = 1;                  break;
		    case 0x2a: l_shift = modified = 1;                  break;
		    case 0x36: r_shift = modified = 1;                  break;
		    case 0x38: l_alt   = modified = 1;                  break;
		    case 0x3a: capslock   = !capslock;   update_leds(); break;
		    case 0x45: numlock    = !numlock;    update_leds(); break;
		    case 0x46: scrolllock = !scrolllock; update_leds(); break;
		    /* PrintScreen (when Alt is pressed) */
		    case 0x54: key = 0x61;                              break;
		    case 0x9d: l_ctrl  = 0; modified = 1;               break;
		    case 0xaa: l_shift = 0; modified = 1;               break;
		    case 0xb6: r_shift = 0; modified = 1;               break;
		    case 0xb8: l_alt   = 0; modified = 1;               break;
		    case 0xfa:						break;
		    default  : if (code < 0x80) key = code;
		}

	    E0_received = (code == 0xe0);
	}
    else
	igncnt--;

    if (modified)
    {
	shift = l_shift | r_shift;
	ctrl  = l_ctrl  | r_ctrl;
	alt   = l_alt   | r_alt;
    }

    if (key == 0x6c && ctrl && alt)
    {
	vt->scroll = 1;

	/* Don't send the signal twice.  */
	if (!ctrl_alt_del_pressed)
	    kill_kpid(INIT, SIGTERM);

	ctrl_alt_del_pressed = 1;
	key = 0;
    }

    if (scrolllock != old_scrolllock)
	update_scrolllock(vt, scrolllock);

    if (key == 0x61)
    {
	display_sys_info();
	key = 0;
    }

    if (key)
	if (ctrl)
	    if (keys_dep[key] == CAPSLOCK)
		vtty_insertkey(vt, ctrl_keys[key]);
	    else
		vtty_insertkey(vt, numlock ? shift_keys[key] : keys[key]);
	else if (shift)
	    if (keys_dep[key] == CAPSLOCK)
		vtty_insertkey(vt, capslock ? keys[key] : shift_keys[key]);
	    else
		vtty_insertkey(vt, numlock  ? keys[key] : shift_keys[key]);
	else if (alt)
	    if (keys_dep[key] == CAPSLOCK)
		switch (key)
		{
		    case 0x3b: case 0x3c: case 0x3d: case 0x3e:
		    case 0x3f: case 0x40: case 0x41: case 0x42:
		    case 0x43: case 0x44: case 0x57: case 0x58:
			if (key >= 0x57)
			    key -= 0x12;

			vtty_no = key - 0x3b;
			break;

		    default:
			vtty_insertkey(vt, alt_keys[key]);
			break;
		}
	    else
		vtty_insertkey(vt, numlock ? shift_keys[key] :
			                           keys[key]);
	else
	    if (keys_dep[key] == CAPSLOCK)
		vtty_insertkey(vt, capslock ? shift_keys[key] :
			                            keys[key]);
	    else
		vtty_insertkey(vt, numlock ? shift_keys[key] :
			                           keys[key]);

  end:

    /* Re-enable the keyboard.  */
    keyboard_command(0xae);

    return vtty_no;
}


static void
vtty_setfocus(vtty_struct* vt, int focus)
{
    if ((vt->focus = focus))
    {
	vt->screen = hardware_video_address;
	vtty_hardscroll(hardware_video_offset = 0);
	lmemcpy(vt->screen, vt->videobuf, vt->lines * vt->columns / 2);
	vtty_cursormove(vt, vt->cx, vt->cy);
	vtty_cv(vt, vt->cursor);
	ctrl       = vt->ctrl;
	shift      = vt->shift;
	alt        = vt->alt;
	capslock   = vt->capslock;
	numlock    = vt->numlock;
	scrolllock = vt->scrolllock;
	update_leds();
    }
    else
    {
	lmemcpy(vt->videobuf, vt->screen, vt->lines * vt->columns / 2);
	vt->screen     = vt->videobuf;
	vt->ctrl       = ctrl;
	vt->shift      = shift;
	vt->alt        = alt;
	vt->capslock   = capslock;
	vt->numlock    = numlock;
	vt->scrolllock = scrolllock;
    }
}


void
keyboard_interrupt(void)
{
    int vtty_no = vtty_keyboard_interrupt(cvtty);

    switch (vtty_no)
    {
	case -1:
	    break;

	default:
	    if (vtty_no != cvtty - vtty)
	    {
		vtty_setfocus(cvtty, 0);
		cvtty = &vtty[vtty_no];
		vtty_setfocus(cvtty, 1);
	    }
	    break;
    }
}


void
vtty_setfocus0(void)
{
    vtty_setfocus(cvtty, 0);
    cvtty = &vtty[0];
    vtty_setfocus(cvtty, 1);
}


int
vtty_open(int minor, int flags)
{
    DEBUG(9, "(%d/%d)\n", VTTY_MAJOR, minor);

    if (minor < 0 || minor >= MAX_VTTYS)
    {
	DEBUG(9, "no such device\n");
	return -ENXIO;
    }

    vtty_minor_info[minor].in_use = 1;

    if (this->leader && this->ctty == 0 && (flags & O_NOCTTY) == 0)
    {
	this->ctty = makedev(VTTY_MAJOR, minor);
	vtty[minor].pgrp = this->pgrp;
    }

    return 0;
}


int
vtty_close(int minor)
{
    DEBUG(9, "(%d/%d)\n", VTTY_MAJOR, minor);

    vtty_reset(&vtty[minor]);

    vtty_minor_info[minor].in_use = 0;

    return 0;
}


int
vtty_read(int minor, chr_request *cr)
{
    if (this->ctty == 0)
	return -EIO;

    return vtty_read_data(&vtty[minor], cr->buf, cr->count);
}


int
vtty_write(int minor, chr_request *cr)
{
    return vtty_write_data(&vtty[minor], cr->buf, cr->count);
}


int
vtty_ioctl(int minor, int cmd, void *argp)
{
    struct winsize *ws = (struct winsize *)argp;

    if (this->ctty == 0)
	return -EIO;

    switch (cmd)
    {
	case TIOCGWINSZ:
	    ws->ws_row = vtty[minor].lines;
	    ws->ws_col = vtty[minor].columns;
	    ws->ws_xpixel = 0;                  /* Unused.  */
	    ws->ws_ypixel = 0;                  /* Unused.  */
	    break;

	case TIOCSWINSZ:
	    /* Don't know what to do here!  */
	    break;

	default:
	    return -EINVAL;
    }

    return 0;
}


int
vtty_fcntl(int minor, int flags)
{
    if (this->ctty == 0)
	return -EIO;

    vtty[minor].flags = flags;

    return 0;
}


int
vtty_lseek(int minor, __off_t offset)
{
    if (minor < vtty_driver.minors)
	return -EINVAL;

    return -ESPIPE;
}


void
vtty_timer(void)
{
    /*
     * This function should be used in order to implement a screen saver.
     * This should *not* be called at every timer interrupt in order  not
     * to affect performance.  What we will probably need  is  a  way  to
     * specify a frequency to the  timer  dispatcher.  Unfortunately, the
     * timer dispatch routine is not ready to handle this yet.
     */
}


int
vtty_discard_input(int minor)
{
    vtty_struct *vt = &vtty[minor];

    if (this->ctty == 0)
	return -EIO;

    vt->kbdhead = vt->kbdtail = vt->kbdcount = 0;

    return 0;
}


int
vtty_getattr(int minor, struct termios *termios_p)
{
    vtty_struct *vt = &vtty[minor];

    if (this->ctty == 0)
	return -EIO;

    termios_p->c_iflag  = vt->c_iflag;
    termios_p->c_oflag  = vt->c_oflag;
    termios_p->c_cflag  = vt->c_cflag;
    termios_p->c_lflag  = vt->c_lflag;
    termios_p->__ispeed = B38400;               /* Not used.  */
    termios_p->__ospeed = B38400;               /* Not used.  */

    memcpy(termios_p->c_cc, vt->c_cc, NCCS * sizeof(cc_t));

    return 0;
}


int
vtty_setattr(int minor, struct termios *termios_p)
{
    vtty_struct *vt = &vtty[minor];

    if (this->ctty == 0)
	return -EIO;

    vt->c_iflag = termios_p->c_iflag;
    vt->c_oflag = termios_p->c_oflag;
    vt->c_cflag = termios_p->c_cflag;
    vt->c_lflag = termios_p->c_lflag;

    memcpy(vt->c_cc, termios_p->c_cc, NCCS * sizeof(cc_t));

    DEBUG(9, "ICANON=%z, ECHO=%z, ECHOE=%z, ECHOK=%z\n",
	  vt->c_lflag & ICANON, vt->c_lflag & ECHO,
	  vt->c_lflag & ECHOE,  vt->c_lflag & ECHOK);
    DEBUG(9, "MIN=%x, TIME=%x\n", vt->c_cc[VMIN], vt->c_cc[VTIME]);
    DEBUG(9, "OPOST=%x\n", vt->c_oflag & OPOST);
    DEBUG(9, "ICRNL=%x\n", vt->c_iflag & ICRNL);

    return 0;
}


#ifdef __DEBUG__
void
vtty_debug_info(void)
{
    int i;
    vtty_struct *vt;

    for (i = 0; i < MAX_VTTYS; i++)
    {
	vt = &vtty[i];
	printk("[%d,%d-%d,%d] ", vt->lines, vt->columns, vt->cx, vt->cy);
    }
}
#endif


int
vtty_init(void)
{
    vtty_struct *vt;
    int i, registered, color;
    void show_sys_info(void);


    color = (*(unsigned short *)0x410 & 0x30) != 0x30;

    hardware_video_address = color ? (unsigned char *)0xb8000 :
				     (unsigned char *)0xb0000;
    hardware_video_offset = 0;

    /* I wonder if this is really needed...  */
    vtty_6845setup();
    cursor_move(0);

    for (vt = vtty, i = 0; i < MAX_VTTYS; i++, vtty_setup(vt++));

    alt_keys_setup();
    vtty_setfocus(cvtty, 1);

    registered = register_driver((generic_driver *)&vtty_driver);

    show_sys_info();

    printk("VTTY: %s monitor detected, %z virtual terminals [%z/%z].\n",
	   color ? "color" : "monochrome", MAX_VTTYS,
	   cvtty->columns, cvtty->lines);

    return registered;
}
