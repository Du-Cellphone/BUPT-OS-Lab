/* prodcons.c - 生产者消费者同步场景实现 */

#include "bootpack.h"
#include "sync.h"
#include <stdio.h>

/*
 * 生产者消费者问题：
 *
 * 竞争条件：
 * 1. 生产者和消费者同时访问共享缓冲区
 * 2. 多个生产者同时写入
 * 3. 多个消费者同时读取
 *
 * 解决方案：
 * 1. 使用互斥锁保护缓冲区访问
 * 2. 使用信号量控制缓冲区的空/满状态
 *    - empty信号量：表示空槽数量（初始=BUFFER_SIZE）
 *    - full信号量：表示满槽数量（初始=0）
 */

/* 全局共享缓冲区 */
static struct SHARED_BUFFER g_buffer;
static int g_buffer_initialized = 0;

/* 初始化共享缓冲区 */
void buffer_init(struct SHARED_BUFFER *buf)
{
    int i;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    for (i = 0; i < BUFFER_SIZE; i++) {
        buf->data[i] = 0;
    }
    mutex_init(&buf->mutex);
    sem_init(&buf->empty, BUFFER_SIZE, BUFFER_SIZE);  /* 初始全空 */
    sem_init(&buf->full, 0, BUFFER_SIZE);             /* 初始无数据 */
}

/* 生产者：向缓冲区写入数据 */
void buffer_produce(struct SHARED_BUFFER *buf, int item)
{
    sem_wait(&buf->empty);
    mutex_lock(&buf->mutex);

    buf->data[buf->head] = item;
    buf->head = (buf->head + 1) % BUFFER_SIZE;
    buf->count++;

    mutex_unlock(&buf->mutex);
    sem_signal(&buf->full);
}

/* 消费者：从缓冲区读取数据 */
int buffer_consume(struct SHARED_BUFFER *buf)
{
    int item;

    sem_wait(&buf->full);
    mutex_lock(&buf->mutex);

    item = buf->data[buf->tail];
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;
    buf->count--;

    mutex_unlock(&buf->mutex);
    sem_signal(&buf->empty);

    return item;
}

/* 获取全局缓冲区 */
struct SHARED_BUFFER *get_global_buffer(void)
{
    if (!g_buffer_initialized) {
        buffer_init(&g_buffer);
        g_buffer_initialized = 1;
    }
    return &g_buffer;
}

/* 扩展系统调用处理 */
int *hrb_api_sync(int edx, int eax, int ebx, int ecx, int *reg, int ds_base)
{
    struct SHARED_BUFFER *buf;
    struct MUTEX *mutex;
    struct SEMAPHORE *sem;

    switch (edx) {
    case SYSCALL_MUTEX_INIT:
        mutex = (struct MUTEX *)(eax + ds_base);
        mutex_init(mutex);
        break;

    case SYSCALL_MUTEX_LOCK:
        mutex = (struct MUTEX *)(eax + ds_base);
        mutex_lock(mutex);
        break;

    case SYSCALL_MUTEX_UNLOCK:
        mutex = (struct MUTEX *)(eax + ds_base);
        mutex_unlock(mutex);
        break;

    case SYSCALL_SEM_INIT:
        sem = (struct SEMAPHORE *)(eax + ds_base);
        sem_init(sem, ebx, ecx);
        break;

    case SYSCALL_SEM_WAIT:
        sem = (struct SEMAPHORE *)(eax + ds_base);
        sem_wait(sem);
        break;

    case SYSCALL_SEM_SIGNAL:
        sem = (struct SEMAPHORE *)(eax + ds_base);
        sem_signal(sem);
        break;

    case SYSCALL_BUFFER_INIT:
        buf = get_global_buffer();
        buffer_init(buf);
        reg[7] = (int)buf;  /* 返回缓冲区地址 */
        break;

    case SYSCALL_BUFFER_PRODUCE:
        buf = get_global_buffer();
        buffer_produce(buf, eax);
        break;

    case SYSCALL_BUFFER_CONSUME:
        buf = get_global_buffer();
        reg[7] = buffer_consume(buf);  /* 返回消费的数据 */
        break;

    case SYSCALL_YIELD:
        task_switch();
        break;

    default:
        return 0;
    }
    return 0;
}
