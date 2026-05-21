/* prodcon.c - 生产者消费者竞态条件演示 */
/*
 * use_lock = 0 → race (unlocked),  use_lock = 1 → correct (locked)
 * 锁机制: 通过系统调用使用 sync.c 的内核互斥锁 (SYSCALL_MUTEX_LOCK/UNLOCK)
 */

void api_putstr0(char *s);
void api_end(void);
void api_mutex_init(void *m);
void api_mutex_lock(void *m);
void api_mutex_unlock(void *m);

void int_to_str(int n, char *s)
{
    int i = 0, j;
    char c;
    if (n == 0) { s[0] = '0'; s[1] = 0; return; }
    while (n > 0) { s[i++] = '0' + (n % 10); n /= 10; }
    s[i] = 0;
    for (j = 0; j < i / 2; j++) {
        c = s[j]; s[j] = s[i - 1 - j]; s[i - 1 - j] = c;
    }
}

/* ==================== 配置 ==================== */
#define PRODUCE_TOTAL   12
#define BUF_SIZE         5
#define DELAY_LOOP       10000000

static int use_lock = 0;  /* 0=竞态 1=正确 */

/* ==================== 用户空间内核锁结构体 ==================== */
/* 必须与 sync.h 的 struct MUTEX 布局一致 (164 bytes) */
struct USER_MUTEX {
    int locked;
    int owner;
    int fifo_buf;       /* FIFO32.buf */
    int fifo_p, fifo_q, fifo_size, fifo_free, fifo_flags;
    int fifo_task;
    int wait_buf[32];
};

/* ==================== 共享缓冲区 ==================== */
static int buf[BUF_SIZE];
static int head  = 0;
static int tail  = 0;
static int count = 0;

static int produced_total = 0;
static int consumed_total = 0;

/* ==================== 主程序 ==================== */
void HariMain(void)
{
    int i, k, item, consumed_item;
    char s[32];
    struct USER_MUTEX mutex;

    if (use_lock)
        api_mutex_init(&mutex);

    /* 紧凑版表头 */
    api_putstr0("= ProdCons Race Demo =\n");
    api_putstr0(use_lock ? "LOCKED  " : "NO LOCK ");
    api_putstr0("buf="); int_to_str(BUF_SIZE, s); api_putstr0(s);
    api_putstr0(" N="); int_to_str(PRODUCE_TOTAL, s); api_putstr0(s);
    api_putstr0("\n\n");

    for (i = 0; i < PRODUCE_TOTAL; i++)
    {
        item = (i + 1) * 10;

        if (use_lock)
        {
            /* ---- 生产者临界区 (内核锁保护) ---- */
            api_mutex_lock(&mutex);

            int p_head      = head;
            int p_count     = count;
            int can_produce = (p_count < BUF_SIZE);

            for (k = 0; k < DELAY_LOOP; k++) ;

            if (can_produce) {
                buf[p_head] = item;
                head  = (p_head + 1) % BUF_SIZE;
                count = p_count + 1;
                produced_total++;
            }

            api_mutex_unlock(&mutex);

            /* ---- 消费者临界区 (内核锁保护) ---- */
            api_mutex_lock(&mutex);

            int c_tail      = tail;
            int c_count     = count;
            int can_consume = (c_count > 0);

            for (k = 0; k < DELAY_LOOP; k++) ;

            if (can_consume) {
                consumed_item = buf[c_tail];
                tail  = (c_tail + 1) % BUF_SIZE;
                count = c_count - 1;
                consumed_total++;
            }

            api_mutex_unlock(&mutex);
        }
        else
        {
            /* ---- 不加锁: 读-读-写-写 模拟竞态 ---- */
            int p_head = head, p_count = count;
            int can_produce = (p_count < BUF_SIZE);

            for (k = 0; k < DELAY_LOOP; k++) ;

            int c_tail = tail, c_count = count;
            int can_consume = (c_count > 0);

            for (k = 0; k < DELAY_LOOP; k++) ;

            if (can_produce) {
                buf[p_head] = item;
                head  = (p_head + 1) % BUF_SIZE;
                count = p_count + 1;
                produced_total++;
            }

            if (can_consume) {
                consumed_item = buf[c_tail];
                tail  = (c_tail + 1) % BUF_SIZE;
                count = c_count - 1;
                consumed_total++;
            }
        }
    }

    /* 验证 */
    api_putstr0("produced="); int_to_str(produced_total, s); api_putstr0(s);
    api_putstr0(" consumed="); int_to_str(consumed_total, s); api_putstr0(s);
    api_putstr0(" buf="); int_to_str(count, s); api_putstr0(s);
    api_putstr0("\n(expect 12 12 0)\n\n");

    if (produced_total == PRODUCE_TOTAL &&
        consumed_total == PRODUCE_TOTAL && count == 0)
    {
        api_putstr0(">>> PASS <<<\n");
    }
    else
    {
        api_putstr0(">>> FAIL (race) <<<\n");
        if (produced_total != consumed_total) {
            api_putstr0("lost=");
            int_to_str(produced_total - consumed_total, s);
            api_putstr0(s); api_putstr0("\n");
        }
    }

    api_end();
}
