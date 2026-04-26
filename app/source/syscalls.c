#include <sys/stat.h>

extern int end;
static char *heap_end = 0;

void *_sbrk(int incr)
{
    char *prev_heap_end;

    if (heap_end == 0)
        heap_end = (char *)&end;

    prev_heap_end = heap_end;
    heap_end += incr;

    return (void *)prev_heap_end;
}

int _close(int file) { return -1; }
int _fstat(int file, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { return 1; }
int _lseek(int file, int ptr, int dir) { return 0; }
int _read(int file, char *ptr, int len) { return 0; }
int _write(int file, char *ptr, int len) { return len; }
void _exit(int status) { while (1); }
