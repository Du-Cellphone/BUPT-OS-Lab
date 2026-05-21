/* sync.h - 同步原语定义 */

#ifndef SYNC_H
#define SYNC_H

#include "bootpack.h"

/* 互斥锁结构 */
struct MUTEX {
    int locked;           /* 0=未锁定, 1=已锁定 */
    struct TASK *owner;   /* 持有锁的任务 */
    struct FIFO32 wait_queue;  /* 等待队列 */
    int wait_buf[32];     /* 等待队列缓冲区 */
};

/* 信号量结构 */
struct SEMAPHORE {
    int count;            /* 信号量计数 */
    int max_count;        /* 最大计数 */
    struct FIFO32 wait_queue;  /* 等待队列 */
    int wait_buf[32];     /* 等待队列缓冲区 */
};

/* 共享缓冲区结构（用于生产者消费者） */
#define BUFFER_SIZE 8
struct SHARED_BUFFER {
    int data[BUFFER_SIZE];
    int head;             /* 写入位置 */
    int tail;             /* 读取位置 */
    int count;            /* 当前数据数量 */
    struct SEMAPHORE empty;    /* 空槽信号量 */
    struct SEMAPHORE full;     /* 满槽信号量 */
    struct MUTEX mutex;        /* 互斥锁 */
};

/* 互斥锁函数 */
void mutex_init(struct MUTEX *mutex);
void mutex_lock(struct MUTEX *mutex);
void mutex_unlock(struct MUTEX *mutex);
int mutex_trylock(struct MUTEX *mutex);

/* 信号量函数 */
void sem_init(struct SEMAPHORE *sem, int initial, int max);
void sem_wait(struct SEMAPHORE *sem);    /* P操作 */
void sem_signal(struct SEMAPHORE *sem);  /* V操作 */
int sem_trywait(struct SEMAPHORE *sem);

/* 共享缓冲区函数 */
void buffer_init(struct SHARED_BUFFER *buf);
void buffer_produce(struct SHARED_BUFFER *buf, int item);
int buffer_consume(struct SHARED_BUFFER *buf);

/* 系统调用号定义 (使用30-39，避免与main已有的20-29冲突) */
#define SYSCALL_MUTEX_INIT    30
#define SYSCALL_MUTEX_LOCK    31
#define SYSCALL_MUTEX_UNLOCK  32
#define SYSCALL_SEM_INIT      33
#define SYSCALL_SEM_WAIT      34
#define SYSCALL_SEM_SIGNAL    35
#define SYSCALL_BUFFER_INIT   36
#define SYSCALL_BUFFER_PRODUCE 37
#define SYSCALL_BUFFER_CONSUME 38
#define SYSCALL_YIELD         39

/* 系统调用分发函数（prodcons.c） */
int *hrb_api_sync(int edx, int eax, int ebx, int ecx, int *reg, int ds_base);

#endif
