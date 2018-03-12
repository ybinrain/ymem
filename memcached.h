
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <pthread.h>
#include <event.h>
#include <unistd.h>

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif



/* 系统设置信息, 由命令行提供 */
struct settings {
    size_t maxbytes;
    int port;
};

/* 全局变量 */
extern struct settings settings;
