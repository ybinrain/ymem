#include "memcached.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITEMS_PER_ALLOC 64

enum conn_queue_item_modes {
    queue_new_conn,
    queue_redispatch
};

typedef struct conn_queue_item CQ_ITEM;
struct conn_queue_item {
    int               sfd;
    enum conn_states  init_state;
    int               event_flags;
    int               read_buffer_size;
    enum network_transport      transport;
    enum conn_queue_item_modes  mode;
    conn *c;
    CQ_ITEM           *next;
};


/* 连接队列 */
typedef struct conn_queue CQ;
struct conn_queue {
    CQ_ITEM *head;
    CQ_ITEM *tail;
    pthread_mutex_t lock;
};

static LIBEVENT_THREAD *threads;

static int last_thread = -1;

static CQ_ITEM *cqi_freelist;

/* 初始化cq */
static void cq_init(CQ *cq) {
    cq->head = NULL;
    cq->tail = NULL;
}

/* 取队列 */
static CQ_ITEM *cq_pop(CQ *cq) {
    CQ_ITEM *item;
    
    item = cq->head;
    if (NULL != item) {
        cq->head = item->next;
        if (NULL == cq->head) {
            cq->tail = NULL;
        }
    }
    return item;
}

/* 添加连接记录*/
static void cq_push(CQ *cq, CQ_ITEM *item) {
    item->next = NULL;

    if (NULL == cq->tail) {
        cq->head = item;
    } else {
        cq->tail->next = item;
    }
    cq->tail = item;
}

/* 创建队列记录 */
static CQ_ITEM *cqi_new(void) {
    CQ_ITEM *item = NULL;

    if (NULL == item) {
        int i;

        item = malloc(sizeof(CQ_ITEM) * ITEMS_PER_ALLOC);
        if (NULL == item) {
            return NULL;
        }

        for (i = 0; i < ITEMS_PER_ALLOC; i++) {
            item[i - 1].next = &item[i];
        }

        item[ITEMS_PER_ALLOC - 1].next = cqi_freelist;
        cqi_freelist = &item[i];
    }

    return item;
}

/**
 * 分发请求
 */
void dispatch_conn_new(int sfd, enum conn_states init_state, int event_flags,
                       int read_buffer_size, enum network_transport transport) {
    CQ_ITEM *item = cqi_new();
    char buf[1];

    if (item == NULL) {
        close(sfd);
        fprintf(stderr, "Failed to allocate memory for connection object\n");
        return;
    }

    int tid = (last_thread + 1) % settings.num_threads;

    LIBEVENT_THREAD *thread = threads + tid;

    last_thread = tid;
    printf("thread_id: %lu\n", (unsigned long)thread->thread_id);

    item->sfd = sfd;
    item->init_state = init_state;
    item->event_flags = event_flags;
    item->transport = transport;
    item->mode = queue_new_conn;

    cq_push(thread->new_conn_queue, item);

    buf[0] = 'c';
    if (write(thread->notify_send_fd, buf, 1) != 1) {
        perror("Writing to thread notify pipe");
    }
}

/**
 * 事件处理
 */
static void thread_libevent_process(int fd, short which, void *arg) {
    LIBEVENT_THREAD *me = arg;
    char buf[1];
    CQ_ITEM *item;
    conn *c;

    if (read(fd, buf, 1) != 1) {
        fprintf(stderr, "Can't read from libevent pipe\n");
        return;
    }

    switch (buf[0]) {
        case 'c':
            item = cq_pop(me->new_conn_queue);
            if (NULL == item) {
                break;
            }
            switch (item->mode) {
                case queue_new_conn:
                    c = conn_new(item->sfd, item->init_state, item->event_flags, 1000, item->transport, me->base);
                    if (c == NULL) {
                        fprintf(stderr, "conn_new");
                    } else {
                        c->thread = me;
                    }
                    break;
                case queue_redispatch:
                    break;
            }
            break;
    }

}

/**
 * Worker thread: main event loop
 */
static void *worker_libevent(void *arg) {
    LIBEVENT_THREAD *me = arg;

    event_base_loop(me->base, 0);

    event_base_free(me->base);
    return NULL;
}

/* 创建线程 */
static void create_worker(void *(*func)(void *), void *arg) {
    pthread_attr_t  attr;
    int             ret;

    pthread_attr_init(&attr);

    if ((ret = pthread_create(&((LIBEVENT_THREAD*)arg)->thread_id, &attr, func, arg)) != 0) {
        fprintf(stderr, "Can't create thread: %s\n",
                strerror(ret));
        exit(1);
    }
}

/**
 * 初始化 work 线程相关信息
 *      - 注册监听事件
 */
static void setup_thread(LIBEVENT_THREAD *me) {
    me->base = event_base_new();

    if (! me->base) {
        fprintf(stderr, "Can't allocate event base\n");
        exit(1);
    }
    event_set(&me->notify_event, me->notify_receive_fd,
            EV_READ | EV_PERSIST, thread_libevent_process, me);
    event_base_set(me->base, &me->notify_event);

    if (event_add(&me->notify_event, 0) == -1) {
        fprintf(stderr, "Can't monitor libevent notify pipe\n");
        exit(1);
    }

    // 给连接队列分配内存
    me->new_conn_queue = malloc(sizeof(struct conn_queue));
    if (me->new_conn_queue == NULL) {
        perror("Failed to allocate memory for connection queue");
        exit(EXIT_FAILURE);
    }
    cq_init(me->new_conn_queue);
}

/**
 * 初始化线程
 */
void memcached_thread_init(int nthreads, void *arg) {
    int i;

    threads = calloc(nthreads, sizeof(LIBEVENT_THREAD));
    if (!threads) {
        perror("Can't allocate thread descriptors");
        exit(1);
    }

    for (i = 0; i < nthreads; i++) {
        int fds[2];
        if (pipe(fds)) {
            perror("Can't create notify pipe");
            exit(1);
        }

        threads[i].notify_receive_fd = fds[0];
        threads[i].notify_send_fd = fds[1];

        /* 注册线程触发事件 */
        setup_thread(&threads[i]);
    }

    for (i = 0; i < nthreads; i++) {
        /* 创建线程 */
        create_worker(worker_libevent, &threads[i]);
    }
}
