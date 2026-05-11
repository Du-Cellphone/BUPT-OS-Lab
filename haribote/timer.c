/* タイマ関係 */

#include "bootpack.h"

#define PIT_CTRL 0x0043
#define PIT_CNT0 0x0040

struct TIMERCTL timerctl;

#define TIMER_FLAGS_ALLOC 1 /* 確保した状態 */
#define TIMER_FLAGS_USING 2 /* タイマ作動中 */

void init_pit(void)
{
    int           i;
    struct TIMER *t;
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timerctl.count = 0;
    for (i = 0; i < MAX_TIMER; i++)
    {
        timerctl.timers0[i].flags = 0; /* 未使用 */
    }
    t             = timer_alloc(); /* 一つもらってくる */
    t->timeout    = 0xffffffff;
    t->flags      = TIMER_FLAGS_USING;
    t->next       = 0;          /* 一番うしろ */
    timerctl.t0   = t;          /* 今は番兵しかいないので先頭でもある */
    timerctl.next = 0xffffffff; /* 番兵しかいないので番兵の時刻 */
    return;
}

struct TIMER *timer_alloc(void)
{
    int i;
    for (i = 0; i < MAX_TIMER; i++)
    {
        if (timerctl.timers0[i].flags == 0)
        {
            timerctl.timers0[i].flags  = TIMER_FLAGS_ALLOC;
            timerctl.timers0[i].flags2 = 0;
            return &timerctl.timers0[i];
        }
    }
    return 0; /* 見つからなかった */
}

void timer_free(struct TIMER *timer)
{
    timer->flags = 0; /* 未使用 */
    return;
}

void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data)
{
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_settime(struct TIMER *timer, unsigned int timeout)
{
    int           e;
    struct TIMER *t, *s;
    e = io_load_eflags();
    io_cli();
    if (timer->flags == TIMER_FLAGS_USING)
    {
        // 从?表中摘掉
        if (timer == timerctl.t0)
        {
            // 是?表?
            timerctl.t0   = timer->next;
            timerctl.next = timerctl.t0->timeout;
        }
        else
        {
            // 在?表中?或尾部，需要找到前?
            t = timerctl.t0;
            for (;;)
            {
                if (t->next == timer)
                {
                    break;
                }
                t = t->next;
            }
            t->next = timer->next; // 前?跳?timer
        }
    }
    timer->timeout = timeout + timerctl.count;
    timer->flags   = TIMER_FLAGS_USING;
    t              = timerctl.t0;
    if (timer->timeout <= t->timeout)
    {
        /* ????????? */
        timerctl.t0   = timer;
        timer->next   = t; /* ????t */
        timerctl.next = timer->timeout;
        io_store_eflags(e);
        return;
    }
    /* ?????????????????T?? */
    for (;;)
    {
        s = t;
        t = t->next;
        if (timer->timeout <= t->timeout)
        {
            /* s??t????????? */
            s->next     = timer; /* s?????timer */
            timer->next = t;     /* timer?????t */
            io_store_eflags(e);
            return;
        }
    }
}

void inthandler20(int *esp)
{
    struct TIMER *timer;
    char          ts = 0;
    io_out8(PIC0_OCW2, 0x60);
    timerctl.count++;

    timer = timerctl.t0;
    for (;;)
    {
        if (timer->timeout > timerctl.count)
        {
            break;
        }
        timer->flags = TIMER_FLAGS_ALLOC;
        if (timer != task_timer)
        {
            fifo32_put(timer->fifo, timer->data);
        }
        else
        {
            ts = 1;
        }
        timer = timer->next;
    }
    timerctl.t0   = timer;
    timerctl.next = timer->timeout;
    if (preempt_needed)
    {
        preempt_needed = 0;
        ts             = 1;
    }
    if (ts != 0)
    {
        task_switch();
    }
    return;
}

int timer_cancel(struct TIMER *timer)
{
    int           e;
    struct TIMER *t;
    e = io_load_eflags();
    io_cli(); /* 設定中にタイマの状態が変化しないようにするため */
    if (timer->flags == TIMER_FLAGS_USING)
    { /* 取り消し処理は必要か？ */
        if (timer == timerctl.t0)
        {
            /* 先頭だった場合の取り消し処理 */
            t             = timer->next;
            timerctl.t0   = t;
            timerctl.next = t->timeout;
        }
        else
        {
            /* 先頭以外の場合の取り消し処理 */
            /* timerの一つ前を探す */
            t = timerctl.t0;
            for (;;)
            {
                if (t->next == timer)
                {
                    break;
                }
                t = t->next;
            }
            t->next = timer->next; /* 「timerの直前」の次が、「timerの次」を指すようにする */
        }
        timer->flags = TIMER_FLAGS_ALLOC;
        io_store_eflags(e);
        return 1; /* キャンセル処理成功 */
    }
    io_store_eflags(e);
    return 0; /* キャンセル処理は不要だった */
}

void timer_cancelall(struct FIFO32 *fifo)
{
    int           e, i;
    struct TIMER *t;
    e = io_load_eflags();
    io_cli(); /* 設定中にタイマの状態が変化しないようにするため */
    for (i = 0; i < MAX_TIMER; i++)
    {
        t = &timerctl.timers0[i];
        if (t->flags != 0 && t->flags2 != 0 && t->fifo == fifo)
        {
            timer_cancel(t);
            timer_free(t);
        }
    }
    io_store_eflags(e);
    return;
}
