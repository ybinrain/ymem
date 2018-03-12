#include "memcached.h"


/* void (*signal( sig, void (*func)(int)))(int); */
#include <signal.h>

/* void setbuf(FILE *restrict stream, char *restrict buf); */
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
/* void exit(int status); */
#include <stdlib.h>

/* char* strsignal(int sig); */
#include <string.h>

/* 函数声明 */
static void settings_init(void);

/* 全局变量 */
struct settings settings;

/**/
static struct event_base *main_event;

/**/
static int lru_maintainer_initialized = 0;
static pthread_mutex_t lru_maintainer_lock = PTHREAD_MUTEX_INITIALIZER;


/* 初始化设置 */
static void settings_init(void) {
    settings.maxbytes = 64 * 1024 * 1024; /* 默认64M */
    settings.port = 11211; /* 默认端口 */
}

/* 信号处理 */
static void sig_handler(const int sig) {
    printf("Signal handled: %s\n", strsignal(sig));
    exit(EXIT_SUCCESS);
}

/* 初始化互斥量 lru_maintainer_initialized */
int init_lru_maintainer(void) {
    if (lru_maintainer_initialized == 0) {
        pthread_mutex_init(&lru_maintainer_lock, NULL);
        lru_maintainer_initialized = 1;
    }
    return 0;
}

/* 帮助信息 */
static void usage(void) {
    printf(PACKAGE " " VERSION "\n");
    printf("-p, --port=<num>        TCP port to listen on (default: 11211)\n"           
           "-h, --help              print this help and exit\n");
    return;
}

int
main(int argc, char *argv[]) 
{
    /* 输入参数 */
    int c;
    /**/
    bool tcp_specified = false;

    /* 退出状态码 */
    int retval = EXIT_SUCCESS;

    /* 处理 SIGINT 和 SIGTERM 信号 */
    /* SIGINT 用户按终端键(Ctrl+C)时, 终端驱动程序产生此信号并发送至前台进程组中的每个进程. */
    /* SIGTERM 由 kill 命令发送的系统默认终止信号, 由于该信号是由应用程序捕获的, 使用 SIGTERM 也让程序有机会在退出之前做好清理工作,
     * 从而优雅的终止(相对于SIGKILL 而言, SIGKILL 不能被捕获或忽略) */
    /* SIGKILL 这是两个不能被捕获或忽略信号中的一个. 它向系统管理员提供了一种可以杀死任意进程的可靠方法. */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* 初始化设置 */
    settings_init();

    /**/
    init_lru_maintainer();

    /* 关闭错误输出的缓冲(参考apue.3e p126) */
    setbuf(stderr, NULL);


    /* 命令行参数处理 */
    char *shortopts = 
        "p:"  /* TCP port number to listen on */
        "hiV" /* help, licence info, version */
        ;
#ifdef HAVE_GETOPT_LONG
    const struct option longopts[] = {
        {"port", required_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int optindex;
    while (-1 != (c = getopt_long(argc, argv, shortopts,
                    longopts, &optindex))) { 
#else
    while (-1 != (c = getopt(argc, argv, shortopts))) {
#endif
        switch (c) {
            case 'p':
                settings.port = atoi(optarg);
                tcp_specified = true;
            break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            break;
            default:
                fprintf(stderr, "Illegal argument \"%c\"\n", c);
                return 1;
        }
    }

    /**/
    main_event = event_base_new();


    event_base_free(main_event);
    return retval;
}
