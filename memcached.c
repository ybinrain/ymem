#include "memcached.h"


/* void (*signal(int sig, void (*func)(int)))(int); */
#include <signal.h>

/* void setbuf(FILE *restrict stream, char *restrict buf); */
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>


static pthread_mutex_t lru_maintainer_lock = PTHREAD_MUTEX_INITIALIZER;

int
main(int argc, char *argv[]) 
{

    // 关闭错误输出的缓冲(参考apue.3e p126)
    setbuf(stderr, NULL);

    char buf[BUFSIZ];

    // int mode = fwide(stdout, 1);
    // printf("%d\n", mode);
    // setbuf(stdout, buf);
    // setvbuf(stdout, NULL, _IONBF, 0);
    // setvbuf(stdout, buf, _IOFBF, 1);
    // setvbuf(stdout, buf, _IOLBF, 4);
    setvbuf(stdout, NULL, _IOFBF, 4);
    
    fprintf(stdout, "1\n");
    // fflush(stdout);
    fprintf(stdout, "2");

    sleep(3);
    return 0;
}
