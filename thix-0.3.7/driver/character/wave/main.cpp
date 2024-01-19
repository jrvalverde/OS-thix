#include <io.h>
#include <stdlib.h>

int play_wave(char *filename, int speed, int volume, long header, long footer);

int main (int argc, char *argv[])
{
    if (argc < 2)
    return write(1, "Usage:\n\tPLAY filename.ext [speed(1..300)] [volume(1..130)] [header] [footer]\n", 77);
    return play_wave(argv[1], atoi(argv[2]), atoi(argv[3]), atol(argv[4]), atol(argv[5]));
}
