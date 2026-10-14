#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(race_demo, LOG_LEVEL_INF);

#define STACK_SIZE 1024
#define ITERATIONS 10000
#define THREAD_PRIORITY 5

/* Volatile keeps the deliberately split read and write observable. */
static volatile unsigned int race_counter;
static unsigned int protected_counter;
static unsigned int threads_done;

K_MUTEX_DEFINE(counter_mutex);
K_MUTEX_DEFINE(done_mutex);

static void counter_thread(void *thread_id, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	const unsigned int id = (unsigned int)(uintptr_t)thread_id;

	for (unsigned int i = 0; i < ITERATIONS; i++) {
		/* Split the read-modify-write so the peer can overwrite this update. */
		unsigned int value = race_counter;

		k_yield();
		race_counter = value + 1;
	}

	LOG_INF("Thread %u finished the unsynchronized increments", id);

	for (unsigned int i = 0; i < ITERATIONS; i++) {
		k_mutex_lock(&counter_mutex, K_FOREVER);
		protected_counter++;
		k_mutex_unlock(&counter_mutex);
	}

	LOG_INF("Thread %u finished the mutex-protected increments", id);

	k_mutex_lock(&done_mutex, K_FOREVER);
	threads_done++;
	if (threads_done == 2) {
		LOG_INF("Race result: %u (expected %u)", race_counter,
			2 * ITERATIONS);
		LOG_INF("Mutex result: %u (expected %u)", protected_counter,
			2 * ITERATIONS);
	}
	k_mutex_unlock(&done_mutex);
}

K_THREAD_DEFINE(counter_thread_a, STACK_SIZE, counter_thread,
		(void *)(uintptr_t)1, NULL, NULL, THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(counter_thread_b, STACK_SIZE, counter_thread,
		(void *)(uintptr_t)2, NULL, NULL, THREAD_PRIORITY, 0, 0);

int main(void)
{
	LOG_INF("Starting two equal-priority counter threads (%u increments each)",
		ITERATIONS);
	return 0;
}
