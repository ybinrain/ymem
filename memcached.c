#include "memcached.h"


/* void (*signal( sig, void (*func)(int)))(int); */
#include <signal.h>
/* int getrlimit(int resource, struct rlimit *rlp); */
#include <sys/resource.h>

/* void setbuf(FILE *restrict stream, char *restrict buf); */
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
/* void exit(int status); */
#include <stdlib.h>

/* char* strsignal(int sig); */
#include <string.h>

#include <fcntl.h>

#include <sysexits.h>

/* 函数声明 */
static void settings_init(void);

/* 全局变量 */
struct settings settings;


/**/
conn **conns;


/**/
static conn *listen_conn = NULL;
static int max_fds;
static struct event_base *main_base;

/**/
static int lru_maintainer_initialized = 0;
static pthread_mutex_t lru_maintainer_lock = PTHREAD_MUTEX_INITIALIZER;


/* 初始化设置 */
static void settings_init(void) {
    settings.maxbytes = 64 * 1024 * 1024; /* 默认64M */
    settings.port = 11211; /* 默认端口 */
    settings.socketpath = NULL; /* 默认使用 unix socket*/
    settings.num_threads = 2; /* N works */
    settings.inter = NULL;
    settings.backlog = 1024;
    settings.maxconns = 1024;
    settings.verbose = 1;
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

/**/
static void drive_machine(conn *c) {
    bool stop = false;
    int sfd;
    socklen_t addrlen;
    struct sockaddr_storage addr;
    static int use_accept4 = 0;

    printf("c->state: %d\n", c->state);
    assert(c != NULL);
    char buf[1024];

    while (!stop) {

        switch(c->state) {
            case conn_listening:
                sfd = accept(c->sfd, (struct sockaddr *)&addr, &addrlen);
                
                if (sfd == -1) {
                    break;
                }

                if (!use_accept4) {
                    if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL) | O_NONBLOCK) < 0) {
                        perror("setting O_NONBLOCK");
                        close(sfd);
                        break;
                    }
                }

                dispatch_conn_new(sfd, conn_new_cmd, EV_READ | EV_PERSIST,
                                  DATA_BUFFER_SIZE, c->transport);
                stop = true;
                break;
            case conn_waiting:
                stop = true;
                break;
            case conn_read:
                stop = true;
                break;
            case conn_parse_cmd:
                stop = true;
                break;
            case conn_new_cmd:
                read(c->sfd, buf, 1024);
                printf("conn_new_cmd: %d\t%s\n", c->sfd, buf);
                stop = true;
                break;
            case conn_nread:
                stop = true;
                break;
            case conn_swallow:
                stop = true;
                break;
            case conn_write:
                stop = true;
                break;
            case conn_mwrite:
                stop = true;
                break;
            case conn_closing:
                stop = true;
                break;
            case conn_closed:
                stop = true;
                break;
            case conn_watch:
                stop = true;
                break;
            case conn_max_state:
                stop = true;
                break;
        }
    }

    return;
}

/* 事件处理 */
void event_handler(const int fd, const short which, void *arg) {
    conn *c;
    printf("event_handler: %d\n", fd);

    c = (conn *)arg;
    assert(c != NULL);

    c->which = which;

    if (fd != c->sfd) {
        fprintf(stderr, "Catastrophic: event fd doesn't match conn fd!\n");
        return;
    }
    
    drive_machine(c);

    return;
}

/* 初始化连接 */
static void conn_init(void) {
    int next_fd = dup(1);
    int headroom = 10;
    struct rlimit rl;

    max_fds = settings.maxconns + headroom + next_fd;

    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        max_fds = rl.rlim_max;
    } else {
        fprintf(stderr, "Failed to query maximum file descriptor; "
                        "failing back to maxconns\n");
    }

    close(next_fd);

    if ((conns = calloc(max_fds, sizeof(conn *))) == NULL) {
        fprintf(stderr, "Failed to allocate connection structures\n");
        exit(1);
    }
}

/**/
conn *conn_new(const int sfd, enum conn_states init_state,
               const int event_flags,
               const int read_buffer_size, enum network_transport transport,
               struct event_base *base) {
    conn *c;
    assert(sfd >= 0 && sfd < max_fds);
    c = conns[sfd];

    if (NULL == c) {
        if (!(c = (conn *)calloc(1, sizeof(conn)))) {

            fprintf(stderr, "Failed to allocate connection object\n");
            return NULL;
        }
        c->sfd = sfd;
        conns[sfd] = c;
    }

    c->transport = transport;

    c->state = init_state;

    event_set(&c->event, sfd, event_flags, event_handler, (void *)c);
    event_base_set(base, &c->event);
    c->ev_flags = event_flags;

    if (event_add(&c->event, 0) == -1) {
        perror("event_add");
        return NULL;
    }

    return c;
}

/**
 * 创建 socket
 */
static int new_socket(struct addrinfo *ai) {
    int sfd;
    int flags;

    if ((sfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) {
        return -1;
    }

    if ((flags = fcntl(sfd, F_GETFL, 0)) < 0 ||
        fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("settings O_NONBLOCK");
        close(sfd);
        return -1;
    }
    return sfd;
}

/**/
static int server_socket(const char *interface,
                         int port,
                         enum network_transport transport,
                         FILE *portnumber_file) {
    int sfd;
    struct linger ling = {0, 0};
    struct addrinfo *ai;
    struct addrinfo *next;
    struct addrinfo hints = { .ai_flags = AI_PASSIVE,
                              .ai_family = AF_INET,
                              .ai_socktype = SOCK_STREAM };

    char port_buf[NI_MAXSERV];
    int error;
    int success = 0;
    int flags = 1;

    hints.ai_socktype = IS_UDP(transport) ? SOCK_DGRAM : SOCK_STREAM;

    if (port == -1) {
        port = 0;
    }
    snprintf(port_buf, sizeof(port_buf), "%d", port);
    error = getaddrinfo(interface, port_buf, &hints, &ai);
    if (error != 0) {
        if (error != EAI_SYSTEM) {
            fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(error));
        } else {
            perror("getaddrinfo()");
        }
        return 1;
    }

    for (next = ai; next; next = next->ai_next) {
        conn *listen_conn_add;
        if ((sfd = new_socket(next)) == -1) {
            continue;
        }
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));

        if (bind(sfd, next->ai_addr, next->ai_addrlen) == -1) {
            close(sfd);
            continue;
        } else {
            success++;
            if (!IS_UDP(transport) && listen(sfd, settings.backlog) == -1) {
                perror("listen()");
                close(sfd);
                freeaddrinfo(ai);
                return 1;
            }
        }

        if (!(listen_conn_add = conn_new(sfd, conn_listening,
                                         EV_READ | EV_PERSIST, 1,
                                         transport, main_base))) {
            fprintf(stderr, "failed to create listening connection\n");
            exit(EXIT_FAILURE);
        }
        listen_conn_add->next = listen_conn;
        listen_conn = listen_conn_add;
    }

    freeaddrinfo(ai);

    return success == 0;
}


/**
 * 创建服务
 */
static int server_sockets(int port, enum network_transport transport, 
                          FILE *portnumber_file) {
    if (settings.inter == NULL) {
        return server_socket(settings.inter, port, transport, portnumber_file);
    } else {
        return 0;
    }
}

int
main(int argc, char *argv[]) 
{
    /* 输入参数 */
    int c;
    /**/
    bool tcp_specified = false;
    /**/
    struct rlimit rlim;

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

    /* rlimit */
    if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
        fprintf(stderr, "failed to getrlimit number of files.\n");
        exit(EX_OSERR);
    } else {
        rlim.rlim_cur = settings.maxconns;
        rlim.rlim_max = settings.maxconns;
        if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
            fprintf(stderr, "failed to set limit for open files. Try starting as root or requesting smaller maxconns value.\n");
            exit(EX_OSERR);
        }
    }

    /**/
    main_base = event_base_new();

    /* Init */
    conn_init();

    /* 初始化 work 线程*/
    memcached_thread_init(settings.num_threads, NULL);

    if (settings.socketpath == NULL) {
        const char *portnumber_filename = getenv("MEMCACHED_PORT_FILENAME");
        char *temp_portnumber_filename = NULL;
        size_t len;
        FILE *portnumber_file = NULL;

        errno = 0;
        if (settings.port && server_sockets(settings.port, tcp_transport,
                                            portnumber_file)) {
            fprintf(stderr, "server_sockets error");
            exit(EX_OSERR);
        }
    }

    if (event_base_loop(main_base, 0) != 0) {
        retval = EXIT_FAILURE;
    }

    event_base_free(main_base);
    return retval;
}












