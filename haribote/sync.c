/* sync.c - 同步原语实现 */

#include "bootpack.h"
#include "sync.h"

/*
 * 竞争条件分析：
 *
 * 1. 内核态竞争条件场景：
 *    - 多个任务同时访问共享数据结构（如FIFO、内存管理器）
 *    - 中断处理程序与普通代码同时访问共享数据
 *    - 任务切换时的状态不一致
 *
 * 2. 解决方案：
 *    - 关中断（io_cli/io_sti）：简单但粗暴，影响系统响应
 *    - 互斥锁（Mutex）：适合保护临界区
 *    - 信号量（Semaphore）：适合资源计数和同步
 *
 * 3. 用户态竞争条件：
 *    - 多个用户程序通过系统调用访问共享资源
 *    - 需要通过系统调用提供同步原语
 */

/* ==================== 互斥锁实现 ==================== */

void mutex_init(struct MUTEX *mutex)
{
    mutex->locked = 0;
    mutex->owner = 0;
    fifo32_init(&mutex->wait_queue, 32, mutex->wait_buf, 0);
}

void mutex_lock(struct MUTEX *mutex)
{
    int eflags;
    struct TASK *task = task_now();

    for (;;) {
        eflags = io_load_eflags();
        io_cli();

        if (mutex->locked == 0) {
            mutex->locked = 1;
            mutex->owner = task;
            io_store_eflags(eflags);
            return;
        }

        fifo32_put(&mutex->wait_queue, task->sel);
        io_store_eflags(eflags);

        task_sleep(task);
    }
}

void mutex_unlock(struct MUTEX *mutex)
{
    int eflags;
    int waiting_task_sel;
    struct TASK *task = task_now();

    eflags = io_load_eflags();
    io_cli();

    if (mutex->owner != task) {
        io_store_eflags(eflags);
        return;
    }

    mutex->locked = 0;
    mutex->owner = 0;

    if (fifo32_status(&mutex->wait_queue) > 0) {
        waiting_task_sel = fifo32_get(&mutex->wait_queue);
        int task_index = (waiting_task_sel / 8) - TASK_GDT0;
        if (task_index >= 0 && task_index < MAX_TASKS) {
            task_run(&taskctl->tasks0[task_index], -1, 0);
        }
    }

    io_store_eflags(eflags);
}

int mutex_trylock(struct MUTEX *mutex)
{
    int eflags;
    struct TASK *task = task_now();
    int result = 0;

    eflags = io_load_eflags();
    io_cli();

    if (mutex->locked == 0) {
        mutex->locked = 1;
        mutex->owner = task;
        result = 1;
    }

    io_store_eflags(eflags);
    return result;
}

/* ==================== 信号量实现 ==================== */

void sem_init(struct SEMAPHORE *sem, int initial, int max)
{
    sem->count = initial;
    sem->max_count = max;
    fifo32_init(&sem->wait_queue, 32, sem->wait_buf, 0);
}

/* P操作（等待/减少） */
void sem_wait(struct SEMAPHORE *sem)
{
    int eflags;
    struct TASK *task = task_now();

    for (;;) {
        eflags = io_load_eflags();
        io_cli();

        if (sem->count > 0) {
            sem->count--;
            io_store_eflags(eflags);
            return;
        }

        fifo32_put(&sem->wait_queue, task->sel);
        io_store_eflags(eflags);

        task_sleep(task);
    }
}

/* V操作（信号/增加） */
void sem_signal(struct SEMAPHORE *sem)
{
    int eflags;
    int waiting_task_sel;

    eflags = io_load_eflags();
    io_cli();

    if (sem->count < sem->max_count) {
        sem->count++;
    }

    if (fifo32_status(&sem->wait_queue) > 0) {
        waiting_task_sel = fifo32_get(&sem->wait_queue);
        int task_index = (waiting_task_sel / 8) - TASK_GDT0;
        if (task_index >= 0 && task_index < MAX_TASKS) {
            task_run(&taskctl->tasks0[task_index], -1, 0);
        }
    }

    io_store_eflags(eflags);
}

int sem_trywait(struct SEMAPHORE *sem)
{
    int eflags;
    int result = 0;

    eflags = io_load_eflags();
    io_cli();

    if (sem->count > 0) {
        sem->count--;
        result = 1;
    }

    io_store_eflags(eflags);
    return result;
}
