/* マルチタスク関係 */

#include "bootpack.h"

#define MAX_QUANTUM 64

struct TASKCTL *taskctl;
struct TIMER   *task_timer;
int             preempt_needed = 0;

struct TASK *task_now(void)
{
    struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
    return tl->tasks[tl->now];
}

void task_add(struct TASK *task)
{
    struct TASKLEVEL *tl   = &taskctl->level[task->level];
    tl->tasks[tl->running] = task;
    tl->running++;
    task->flags = 2; /* 動作中 */
    return;
}

void task_remove(struct TASK *task)
{
    int               i;
    struct TASKLEVEL *tl = &taskctl->level[task->level];

    /* taskがどこにいるかを探す */
    for (i = 0; i < tl->running; i++)
    {
        if (tl->tasks[i] == task)
        {
            /* ここにいた */
            break;
        }
    }

    tl->running--;
    if (i < tl->now)
    {
        tl->now--; /* ずれるので、これもあわせておく */
    }
    if (tl->now >= tl->running)
    {
        /* nowがおかしな値になっていたら、修正する */
        tl->now = 0;
    }
    task->flags = 1; /* スリープ中 */

    /* ずらし */
    for (; i < tl->running; i++)
    {
        tl->tasks[i] = tl->tasks[i + 1];
    }

    return;
}

void task_switchsub(void)
{
    int i;
    /* 一番上のレベルを探す */
    for (i = 0; i < MAX_TASKLEVELS; i++)
    {
        if (taskctl->level[i].running > 0)
        {
            break; /* 見つかった */
        }
    }
    taskctl->now_lv    = i;
    taskctl->lv_change = 0;
    return;
}

void task_idle(void)
{
    for (;;)
    {
        io_hlt();
    }
}

struct TASK *task_init(struct MEMMAN *memman)
{
    int                        i;
    struct TASK               *task, *idle;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;

    taskctl = (struct TASKCTL *)memman_alloc_4k(memman, sizeof(struct TASKCTL));
    for (i = 0; i < MAX_TASKS; i++)
    {
        taskctl->tasks0[i].flags    = 0;
        taskctl->tasks0[i].sel      = (TASK_GDT0 + i) * 8;
        taskctl->tasks0[i].tss.ldtr = (TASK_GDT0 + MAX_TASKS + i) * 8;
        set_segmdesc(gdt + TASK_GDT0 + i, 103, (int)&taskctl->tasks0[i].tss, AR_TSS32);
        set_segmdesc(gdt + TASK_GDT0 + MAX_TASKS + i, 15, (int)taskctl->tasks0[i].ldt, AR_LDT);
    }
    for (i = 0; i < MAX_TASKLEVELS; i++)
    {
        taskctl->level[i].running = 0;
        taskctl->level[i].now     = 0;
    }

    task           = task_alloc();
    task->flags    = 2; /* 動作中マーク */
    task->priority = 2; /* 0.02秒 */
    task->level    = 0; /* 最高レベル */
    task_add(task);
    task_switchsub(); /* レベル設定 */
    load_tr(task->sel);
    task_timer = timer_alloc();
    timer_settime(task_timer, task->priority);

    idle          = task_alloc();
    idle->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024;
    idle->tss.eip = (int)&task_idle;
    idle->tss.es  = 1 * 8;
    idle->tss.cs  = 2 * 8;
    idle->tss.ss  = 1 * 8;
    idle->tss.ds  = 1 * 8;
    idle->tss.fs  = 1 * 8;
    idle->tss.gs  = 1 * 8;
    task_run(idle, MAX_TASKLEVELS - 1, 1);

    return task;
}

struct TASK *task_alloc(void)
{
    int          i;
    struct TASK *task;
    for (i = 0; i < MAX_TASKS; i++)
    {
        if (taskctl->tasks0[i].flags == 0)
        {
            task             = &taskctl->tasks0[i];
            task->flags      = 1;          /* 使用中マーク */
            task->tss.eflags = 0x00000202; /* IF = 1; */
            task->tss.eax    = 0;          /* とりあえず0にしておくことにする */
            task->tss.ecx    = 0;
            task->tss.edx    = 0;
            task->tss.ebx    = 0;
            task->tss.ebp    = 0;
            task->tss.esi    = 0;
            task->tss.edi    = 0;
            task->tss.es     = 0;
            task->tss.ds     = 0;
            task->tss.fs     = 0;
            task->tss.gs     = 0;
            task->tss.iomap  = 0x40000000;
            task->tss.ss0    = 0;
            task->tss.cr3    = PAGE_DIR_ADDR;
            return task;
        }
    }
    return 0; /* もう全部使用中 */
}

// 在这里升高level
void task_run(struct TASK *task, int level, int priority)
{
    // 保持level不变
    if (level < 0)
    {
        level = task->level;
    }
    // 修改priority
    if (priority > 0)
    {
        task->priority = priority;
    }

    // 该task正在执行且level与当前level不同
    if (task->flags == 2 && task->level != level)
    {
        task_remove(task);
        task->level = level;
        task_add(task);
    }
    // 该task正在休眠或是一个new task，将其level调整到最高级
    else if (task->flags != 2)
    {
        task->level    = 0;
        task->priority = 2;
        task_add(task);
    }

    taskctl->lv_change = 1;
    return;
}

void task_sleep(struct TASK *task)
{
    struct TASK *now_task;
    if (task->flags == 2)
    {
        /* 動作中だったら */
        now_task = task_now();
        task_remove(task); /* これを実行するとflagsは1になる */
        if (task == now_task)
        {
            /* 自分自身のスリープだったので、タスクスイッチが必要 */
            task_switchsub();
            now_task = task_now(); /* 設定後での、「現在のタスク」を教えてもらう */
            farjmp(0, now_task->sel);
        }
    }
    return;
}

// 任务时间片用完才会被调用，时间片用完后应降低优先级
void task_switch(void)
{
    struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
    struct TASK      *new_task, *now_task = tl->tasks[tl->now];
    if (now_task->level < 9)
    {
        int new_priority = now_task->priority + (1 << now_task->level);
        if (new_priority > MAX_QUANTUM)
        {
            new_priority = MAX_QUANTUM;
        }
        task_run(now_task, now_task->level + 1, new_priority); // 当前任务时间片用完了，所以降低当前任务的优先级
    }
    else
    {
        tl->now++;
        if (tl->now == tl->running)
        {
            tl->now = 0;
        }
    }
    if (taskctl->lv_change != 0)
    {
        task_switchsub();
        tl = &taskctl->level[taskctl->now_lv];
    }
    new_task = tl->tasks[tl->now];
    timer_settime(task_timer, new_task->priority);
    if (new_task != now_task)
    {
        farjmp(0, new_task->sel);
    }
    return;
}