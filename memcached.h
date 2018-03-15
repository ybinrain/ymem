
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <event.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif


#define DATA_BUFFER_SIZE 2048

#define IS_TCP(x) (x == tcp_transport)
#define IS_UDP(x) (x == udp_transport)



/* 系统设置信息, 由命令行提供 */
struct settings {
    size_t maxbytes;
    int port;
    char *socketpath;
    int num_threads;
    char *inter;
    int backlog;
    int maxconns;
    int verbose;
};

/* 连接状态 */
enum conn_states {
    conn_listening,
    conn_new_cmd,
    conn_waiting,
    conn_read,
    conn_parse_cmd,
    conn_write,
    conn_nread,
    conn_swallow,
    conn_closing,
    conn_mwrite,
    conn_closed,
    conn_watch,
    conn_max_state
};

/* 全局变量 */
extern struct settings settings;

/* 线程结构体 */
typedef struct {
    pthread_t thread_id;
    struct event_base *base;
    struct event notify_event;
    int notify_receive_fd;
    int notify_send_fd;
    struct conn_queue *new_conn_queue;
} LIBEVENT_THREAD;

/**/
enum network_transport {
    local_transport,
    tcp_transport,
    udp_transport
};

typedef struct conn conn;
/**/
struct conn {
    int     sfd;
    struct event event;
    short ev_flags;
    short which;
    enum conn_states state;
    enum network_transport transport;
    void *item;
    conn *next;
    LIBEVENT_THREAD *thread;
};


/**/
extern conn **conns;


/**/
void memcached_thread_init(int threads, void *arg);
void dispatch_conn_new(int sfd, enum conn_states init_state, int event_flags, int read_buffer_size, enum network_transport transport);
conn *conn_new(const int sdf, enum conn_states init_state, const int event_flags, const int read_buffer_size, enum network_transport transport, struct event_base *base);

