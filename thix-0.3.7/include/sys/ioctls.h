#define B0 0x00000000
#define B110 0x00000003
#define B1200 0x00000009
#define B134 0x00000004
#define B150 0x00000005
#define B1800 0x0000000a
#define B19200 0x0000000e
#define B200 0x00000006
#define B2400 0x0000000b
#define B300 0x00000007
#define B38400 0x0000000f
#define B4800 0x0000000c
#define B50 0x00000001
#define B600 0x00000008
#define B75 0x00000002
#define B9600 0x0000000d
#define BRKINT 0x00000002
#define BS0 0x00000000
#define BS1 0x00002000
#define BSDLY 0x00002000
#define CBAUD 0x0000000f
#define CIBAUD 0x000f0000
#define CLOCAL 0x00000800
#define CR0 0x00000000
#define CR1 0x00000200
#define CR2 0x00000400
#define CR3 0x00000600
#define CRDLY 0x00000600
#define CREAD 0x00000080
#define CRTSCTS 0x80000000
#define CS5 0x00000000
#define CS6 0x00000010
#define CS7 0x00000020
#define CS8 0x00000030
#define CSIZE 0x00000030
#define CSTOPB 0x00000040
#define ECHO 0x00000008
#define ECHOCTL 0x00000200
#define ECHOE 0x00000010
#define ECHOK 0x00000020
#define ECHOKE 0x00000800
#define ECHONL 0x00000040
#define ECHOPRT 0x00000400
#define EXTA 0x0000000e
#define EXTB 0x0000000f
#define FF0 0x00000000
#define FF1 0x00004000
#define FFDLY 0x00004000
#define FIOASYNC 0x00005452
#define FIOCLEX 0x00005451
#define FIONBIO 0x00005421
#define FIONCLEX 0x00005450
#define FIONREAD 0x0000541b
#define FLUSHO 0x00001000
#define HUPCL 0x00000400
#define ICANON 0x00000002
#define ICRNL 0x00000100
#define IEXTEN 0x00008000
#define IGNBRK 0x00000001
#define IGNCR 0x00000080
#define IGNPAR 0x00000004
#define IMAXBEL 0x00002000
#define INLCR 0x00000040
#define INPCK 0x00000010
#define IOCCMD_MASK 0x0000ffff
#define IOCCMD_SHIFT 0x00000000
#define IOCSIZE_MASK 0x3fff0000
#define IOCSIZE_SHIFT 0x00000010
#define IOC_IN 0x40000000
#define IOC_INOUT 0xc0000000
#define IOC_OUT 0x80000000
#define IOC_VOID 0x00000000
#define ISIG 0x00000001
#define ISTRIP 0x00000020
#define IUCLC 0x00000200
#define IXANY 0x00000800
#define IXOFF 0x00001000
#define IXON 0x00000400
#define NCC 0x00000008
#define NCCS 0x00000013
#define NL0 0x00000000
#define NL1 0x00000100
#define NLDLY 0x00000100
#define NOFLSH 0x00000080
#define N_MOUSE 0x00000002
#define N_SLIP 0x00000001
#define N_TTY 0x00000000
#define OCRNL 0x00000008
#define OFDEL 0x00000080
#define OFILL 0x00000040
#define OLCUC 0x00000002
#define ONLCR 0x00000004
#define ONLRET 0x00000020
#define ONOCR 0x00000010
#define OPOST 0x00000001
#define PARENB 0x00000100
#define PARMRK 0x00000008
#define PARODD 0x00000200
#define PENDIN 0x00004000
#define TAB0 0x00000000
#define TAB1 0x00000800
#define TAB2 0x00001000
#define TAB3 0x00001800
#define TABDLY 0x00001800
#define TCFLSH 0x0000540b
#define TCGETA 0x00005405
#define TCGETS 0x00005401
#define TCIFLUSH 0x00000000
#define TCIOFF 0x00000002
#define TCIOFLUSH 0x00000002
#define TCION 0x00000003
#define TCOFLUSH 0x00000001
#define TCOOFF 0x00000000
#define TCOON 0x00000001
#define TCSADRAIN 0x00000001
#define TCSAFLUSH 0x00000002
#define TCSANOW 0x00000000
#define TCSBRK 0x00005409
#define TCSBRKP 0x00005425
#define TCSETA 0x00005406
#define TCSETAF 0x00005408
#define TCSETAW 0x00005407
#define TCSETS 0x00005402
#define TCSETSF 0x00005404
#define TCSETSW 0x00005403
#define TCXONC 0x0000540a
#define TIOCCONS 0x0000541d
#define TIOCEXCL 0x0000540c
#define TIOCGETD 0x00005424
#define TIOCGLCKTRMIOS 0x00005456
#define TIOCGPGRP 0x0000540f
#define TIOCGSERIAL 0x0000541e
#define TIOCGSOFTCAR 0x00005419
#define TIOCGWINSZ 0x00005413
#define TIOCINQ 0x0000541b
#define TIOCLINUX 0x0000541c
#define TIOCMBIC 0x00005417
#define TIOCMBIS 0x00005416
#define TIOCMGET 0x00005415
#define TIOCMSET 0x00005418
#define TIOCM_CAR 0x00000040
#define TIOCM_CD 0x00000040
#define TIOCM_CTS 0x00000020
#define TIOCM_DSR 0x00000100
#define TIOCM_DTR 0x00000002
#define TIOCM_LE 0x00000001
#define TIOCM_RI 0x00000080
#define TIOCM_RNG 0x00000080
#define TIOCM_RTS 0x00000004
#define TIOCM_SR 0x00000010
#define TIOCM_ST 0x00000008
#define TIOCNOTTY 0x00005422
#define TIOCNXCL 0x0000540d
#define TIOCOUTQ 0x00005411
#define TIOCPKT 0x00005420
#define TIOCPKT_DOSTOP 0x00000010
#define TIOCPKT_FLUSHREAD 0x00000001
#define TIOCPKT_FLUSHWRITE 0x00000002
#define TIOCPKT_NOSTOP 0x00000020
#define TIOCPKT_START 0x00000008
#define TIOCPKT_STOP 0x00000004
#define TIOCSCTTY 0x0000540e
#define TIOCSERCONFIG 0x00005453
#define TIOCSERGWILD 0x00005454
#define TIOCSERSWILD 0x00005455
#define TIOCSETD 0x00005423
#define TIOCSLCKTRMIOS 0x00005457
#define TIOCSPGRP 0x00005410
#define TIOCSSERIAL 0x0000541f
#define TIOCSSOFTCAR 0x0000541a
#define TIOCSTI 0x00005412
#define TIOCSWINSZ 0x00005414
#define TOSTOP 0x00000100
#define VDISCARD 0x0000000d
#define VEOF 0x00000004
#define VEOL 0x0000000b
#define VEOL2 0x00000010
#define VERASE 0x00000002
#define VINTR 0x00000000
#define VKILL 0x00000003
#define VLNEXT 0x0000000f
#define VMIN 0x00000006
#define VQUIT 0x00000001
#define VREPRINT 0x0000000c
#define VSTART 0x00000008
#define VSTOP 0x00000009
#define VSUSP 0x0000000a
#define VSWTC 0x00000007
#define VT0 0x00000000
#define VT1 0x00004000
#define VTDLY 0x00004000
#define VTIME 0x00000005
#define VWERASE 0x0000000e
#define XCASE 0x00000004
#define XTABS 0x00001800
