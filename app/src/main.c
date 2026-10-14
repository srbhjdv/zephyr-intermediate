#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(scheduler_demo, LOG_LEVEL_INF);

#define STACK_SIZE 1024

/* Lower numeric priority values represent higher priorities in Zephyr. */
#define T_LOW_PRIORITY  7
#define T_MED_PRIORITY  5
#define T_HIGH_PRIORITY 3
#define T_COOP_PRIORITY -1

static void t_low_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		LOG_INF("T_LOW running");
		k_msleep(300);
	}
}

static void t_med_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		LOG_INF("T_MED running");
		k_msleep(200);
	}
}

static void t_high_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		LOG_INF("T_HIGH running");
		k_msleep(100);
	}
}

static void t_coop_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (int iteration = 1; iteration <= 5; iteration++) {
		volatile unsigned int work = 0;

		for (unsigned int i = 0; i < 100000; i++) {
			work += i;
		}

		LOG_INF("T_COOP busy work iteration %d complete (%u)", iteration,
			(unsigned int)work);
		k_yield();
	}

	LOG_INF("T_COOP finished");
}

K_THREAD_DEFINE(t_low, STACK_SIZE, t_low_fn,
		NULL, NULL, NULL, T_LOW_PRIORITY, 0, 0);
K_THREAD_DEFINE(t_med, STACK_SIZE, t_med_fn,
		NULL, NULL, NULL, T_MED_PRIORITY, 0, 0);
K_THREAD_DEFINE(t_high, STACK_SIZE, t_high_fn,
		NULL, NULL, NULL, T_HIGH_PRIORITY, 0, 0);
K_THREAD_DEFINE(t_coop, STACK_SIZE, t_coop_fn,
		NULL, NULL, NULL, T_COOP_PRIORITY, 0, 0);

int main(void)
{
	LOG_INF("Scheduler demo started");
	return 0;
}
