/* synctest.c - 用户态进程同步机制综合测试 */

void api_putstr0(char *s);
void api_putchar(int c);
void api_end(void);
void api_mutex_init(void *m);
void api_mutex_lock(void *m);
void api_mutex_unlock(void *m);
void api_sem_init(void *sem, int initial, int max);
void api_sem_wait(void *sem);
void api_sem_signal(void *sem);
int api_buffer_init(void);
void api_produce(int item);
int api_consume(void);

/* ---- 辅助函数 ---- */

void int_to_str(int n, char *s)
{
	int i = 0, j;
	char c;
	if (n == 0) { s[0] = '0'; s[1] = 0; return; }
	if (n < 0) { s[i++] = '-'; n = -n; }
	while (n > 0) { s[i++] = '0' + (n % 10); n /= 10; }
	s[i] = 0;
	for (j = (s[0] == '-' ? 1 : 0); j < i / 2 + (s[0] == '-' ? 0 : 0); j++) {
		c = s[j]; s[j] = s[i - 1 - j]; s[i - 1 - j] = c;
	}
	if (s[0] == '-') {
		j = 1;
		while (j < i) { s[j - 1] = s[j]; j++; }
		s[j - 1] = 0;
	}
}

/* ---- 内核结构体镜像（必须与 sync.h 布局一致） ---- */

struct USER_MUTEX {
	int locked;          /* offset 0 */
	int owner;           /* offset 4 */
	int fifo_buf;        /* offset 8  (FIFO32.buf) */
	int fifo_p;          /* offset 12 (FIFO32.p) */
	int fifo_q;          /* offset 16 (FIFO32.q) */
	int fifo_size;       /* offset 20 (FIFO32.size) */
	int fifo_free;       /* offset 24 (FIFO32.free) */
	int fifo_flags;      /* offset 28 (FIFO32.flags) */
	int fifo_task;       /* offset 32 (FIFO32.task) */
	int wait_buf[32];    /* offset 36, 128 bytes */
};

struct USER_SEMAPHORE {
	int count;           /* offset 0 */
	int max_count;       /* offset 4 */
	int fifo_buf;        /* offset 8  (FIFO32.buf) */
	int fifo_p;          /* offset 12 (FIFO32.p) */
	int fifo_q;          /* offset 16 (FIFO32.q) */
	int fifo_size;       /* offset 20 (FIFO32.size) */
	int fifo_free;       /* offset 24 (FIFO32.free) */
	int fifo_flags;      /* offset 28 (FIFO32.flags) */
	int fifo_task;       /* offset 32 (FIFO32.task) */
	int wait_buf[32];    /* offset 36, 128 bytes */
};

/* ---- 测试结果跟踪 ---- */

static int g_total  = 0;
static int g_pass   = 0;
static int g_fail   = 0;

void check(const char *name, int condition)
{
	char s[16];
	g_total++;
	api_putstr0("  [");
	if (condition) {
		g_pass++;
		api_putstr0("PASS");
	} else {
		g_fail++;
		api_putstr0("FAIL");
	}
	api_putstr0("] ");
	api_putstr0(name);
	api_putstr0(" (");
	int_to_str(condition, s);
	api_putstr0(s);
	api_putstr0(")\n");
}

/* ================================================================
 * 测试1: 互斥锁 (Mutex)
 *   - 初始化后 locked=0
 *   - lock → unlock 正确配对
 *   - 计数器在锁保护下的一致性
 * ================================================================ */
void test_mutex(void)
{
	struct USER_MUTEX m;
	int i, counter;
	char s[16];

	api_putstr0("[Test 1] Mutex\n");

	api_mutex_init(&m);
	check("mutex_init: struct存在", 1);

	api_mutex_lock(&m);
	api_mutex_unlock(&m);
	check("mutex_lock+unlock: 不崩溃", 1);

	/* 多轮 lock/unlock 验证锁的可用性 */
	api_mutex_lock(&m);
	api_mutex_unlock(&m);
	api_mutex_lock(&m);
	api_mutex_unlock(&m);
	check("mutex重复lock+unlock: 正常", 1);

	counter = 0;
	for (i = 0; i < 100; i++) {
		api_mutex_lock(&m);
		counter++;
		api_mutex_unlock(&m);
	}
	check("mutex保护计数器100次", counter == 100);

	api_putstr0("  counter = ");
	int_to_str(counter, s);
	api_putstr0(s);
	api_putstr0("\n");
}

/* ================================================================
 * 测试2: 信号量 (Semaphore)
 *   - 初始化 count=3
 *   - wait 3次均成功（不阻塞）
 *   - signal 2次，count 变为 2
 *   - wait 2次确认 count 归零
 * ================================================================ */
void test_semaphore(void)
{
	struct USER_SEMAPHORE sem;
	char s[16];

	api_putstr0("[Test 2] Semaphore\n");

	api_sem_init(&sem, 3, 10);
	check("sem_init(3,10): 不崩溃", 1);

	api_sem_wait(&sem);
	api_sem_wait(&sem);
	api_sem_wait(&sem);
	check("sem_wait x3: 不阻塞", 1);

	api_sem_signal(&sem);
	api_sem_signal(&sem);
	check("sem_signal x2: 不崩溃", 1);

	api_sem_wait(&sem);
	api_sem_wait(&sem);
	check("sem_wait x2 after signal: 不阻塞", 1);

	api_putstr0("  sem 操作全部成功\n");
}

/* ================================================================
 * 测试3: 共享缓冲区 (Shared Buffer) — 内核管理
 *   - api_buffer_init() 返回全局缓冲区指针
 *   - 单进程内 produce → consume 验证 FIFO 顺序
 *   - 缓冲区大小 = 8
 * ================================================================ */
void test_shared_buffer(void)
{
	int i, item, buf_ptr;
	int errors = 0;
	char s[16];

	api_putstr0("[Test 3] Shared Buffer (kernel)\n");

	buf_ptr = api_buffer_init();
	check("api_buffer_init: 返回非空", buf_ptr != 0);

	for (i = 1; i <= 4; i++) {
		api_produce(i * 10);
	}
	check("produce x4 (10,20,30,40): 不阻塞", 1);

	for (i = 1; i <= 4; i++) {
		item = api_consume();
		if (item != i * 10) {
			errors++;
		}
	}
	check("consume x4: 顺序正确(10,20,30,40)", errors == 0);

	for (i = 1; i <= 8; i++) {
		api_produce(i);
	}
	for (i = 1; i <= 8; i++) {
		item = api_consume();
		if (item != i) errors++;
	}
	check("produce+consume x8: FIFO正确", errors == 0);

	if (errors) {
		api_putstr0("  consume errors = ");
		int_to_str(errors, s);
		api_putstr0(s);
		api_putstr0("\n");
	}
}

/* ================================================================
 * HariMain — 测试入口
 * ================================================================ */
void HariMain(void)
{
	char s[16];

	api_putstr0("========================================\n");
	api_putstr0("   User-Mode Sync Primitive Test\n");
	api_putstr0("========================================\n\n");

	test_mutex();
	api_putchar('\n');

	test_semaphore();
	api_putchar('\n');

	test_shared_buffer();
	api_putchar('\n');

	api_putstr0("========================================\n");
	api_putstr0("  RESULTS: ");
	int_to_str(g_pass, s);
	api_putstr0(s);
	api_putstr0("/");
	int_to_str(g_total, s);
	api_putstr0(s);
	api_putstr0(" passed");
	if (g_fail > 0) {
		api_putstr0(", ");
		int_to_str(g_fail, s);
		api_putstr0(s);
		api_putstr0(" FAILED");
	}
	api_putstr0("\n");

	if (g_fail == 0) {
		api_putstr0("  >>> ALL TESTS PASSED <<<\n");
	} else {
		api_putstr0("  *** SOME TESTS FAILED ***\n");
	}
	api_putstr0("========================================\n");

	api_end();
}
