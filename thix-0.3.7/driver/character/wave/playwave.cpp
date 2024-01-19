#include <io.h>
#include <fcntl.h>
#include <dos.h>
#include <alloc.h>

extern "C" void far play(int speed, long length, void far *samples);

#define STEP 0xFFF0U

inline min(unsigned long l1, unsigned long l2)
{
    return l1 < l2 ? l1 : l2;
}

unsigned playdata[0x100];
long hread(int fd, void huge *buf, long len)
{
    unsigned nread = 0, toread = 0;
    for (long read = 0; read < len && nread == toread; read += nread)
	_dos_read(fd,
		  (void huge *)((char huge *)buf + read),
                  toread = min(len - read,(unsigned long)STEP),
		  &nread);
    return read;
}

void set_volume(int volume)
{
    for (int i = 0; i < 0x80; i++)
    {
	playdata[0x80 - i] = i * volume > 0x7F ? 0xFF81 : - i * volume;
	playdata[0x80 + i] = i * volume > 0x7F ? 0x007F :   i * volume;
    }
}

int play_wave(char *filename, int speed, int volume, long header, long footer)
{
    int fd = _open(filename, O_RDONLY);
    if (fd == -1) return 1;

    unsigned long flength = filelength(fd) - header - footer - 1;
    unsigned long mlength = farcoreleft();
    mlength = min(mlength, flength);
    char far *samples;
    if (!(samples = (char far *)farmalloc(mlength))) return 2;
    if (!speed)  speed  = 30;

    set_volume(volume ? volume : 10);

    lseek(fd, header, SEEK_SET);
    for(unsigned long toplay = mlength;flength && toplay == mlength;flength -= toplay)
    {
        toplay = hread(fd, samples, min(flength, mlength));
	disable();
	play(speed, toplay, samples);
    }
    enable();
    farfree(samples);
    close(fd);
    return 0;
}
