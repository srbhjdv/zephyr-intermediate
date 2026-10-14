#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(pipeline_demo, LOG_LEVEL_INF);

#define STACK_SIZE 1024
#define PRODUCER_PRIORITY 5
#define CONSUMER_PRIORITY 6
#define HEALTH_PRIORITY 7
#define QUEUE_CAPACITY 8
#define PRODUCER_PERIOD_MS 100
#define PRODUCER_SAMPLES 30
#define CONSUMER_STALL_MS 2500
#define CONSUMER_WDT_TIMEOUT_MS 1200
#define HEALTH_PERIOD_MS 100
#define QUEUE_WARNING_PERCENT 75

struct sample {
	unsigned int sequence;
	int value;
};

K_MSGQ_DEFINE(sample_queue, sizeof(struct sample), QUEUE_CAPACITY, 4);

static atomic_t consumer_stalled_once;
static atomic_t consumer_finished;

K_THREAD_STACK_DEFINE(producer_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(consumer_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(health_stack, STACK_SIZE);
static struct k_thread producer_thread;
static struct k_thread consumer_thread;
static struct k_thread health_thread;

static void consumer_wdt_callback(int channel_id, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_ERR("Task watchdog expired for consumer (channel %d)", channel_id);
}

static void producer_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (unsigned int sequence = 1; sequence <= PRODUCER_SAMPLES; sequence++) {
		struct sample item = {
			.sequence = sequence,
			.value = (int)(sequence * 10),
		};

		if (k_msgq_put(&sample_queue, &item, K_NO_WAIT) != 0) {
			LOG_WRN("Queue full; dropped sample #%u", sequence);
		}

		k_msleep(PRODUCER_PERIOD_MS);
	}

	/* A zero sequence marks completion and lets the consumer exit cleanly. */
	const struct sample stop = {0};

	k_msgq_put(&sample_queue, &stop, K_FOREVER);
	LOG_INF("Producer finished");
}

static void consumer_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const int wdt_channel = task_wdt_add(CONSUMER_WDT_TIMEOUT_MS,
						    consumer_wdt_callback, NULL);

	if (wdt_channel < 0) {
		LOG_ERR("Failed to add consumer task watchdog channel (%d)",
			wdt_channel);
		atomic_set(&consumer_finished, 1);
		return;
	}

	while (1) {
		struct sample item;

		k_msgq_get(&sample_queue, &item, K_FOREVER);
		if (item.sequence == 0) {
			break;
		}

		LOG_INF("Consumer received sample #%u (value %d)", item.sequence,
			item.value);
		task_wdt_feed(wdt_channel);

		if (atomic_cas(&consumer_stalled_once, 0, 1)) {
			LOG_WRN("Simulating stuck consumer for %d ms",
				CONSUMER_STALL_MS);
			k_msleep(CONSUMER_STALL_MS);
		}
	}

	task_wdt_delete(wdt_channel);
	atomic_set(&consumer_finished, 1);
	LOG_INF("Consumer finished");
}

static void health_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	bool warning_active = false;
	const unsigned int warning_level =
		(QUEUE_CAPACITY * QUEUE_WARNING_PERCENT + 99) / 100;

	while (!atomic_get(&consumer_finished)) {
		const unsigned int used = k_msgq_num_used_get(&sample_queue);

		if (used >= warning_level && !warning_active) {
			LOG_WRN("Queue fill reached %u/%u (at least %u%%)", used,
				QUEUE_CAPACITY, QUEUE_WARNING_PERCENT);
			warning_active = true;
		} else if (used < warning_level) {
			warning_active = false;
		}

		k_msleep(HEALTH_PERIOD_MS);
	}
}

int main(void)
{
	int ret = task_wdt_init(NULL);

	if (ret != 0) {
		LOG_ERR("Failed to initialize task watchdog (%d)", ret);
		return ret;
	}

	k_thread_create(&consumer_thread, consumer_stack,
			K_THREAD_STACK_SIZEOF(consumer_stack), consumer_fn,
			NULL, NULL, NULL, CONSUMER_PRIORITY, 0, K_NO_WAIT);
	k_thread_create(&producer_thread, producer_stack,
			K_THREAD_STACK_SIZEOF(producer_stack), producer_fn,
			NULL, NULL, NULL, PRODUCER_PRIORITY, 0, K_NO_WAIT);
	k_thread_create(&health_thread, health_stack,
			K_THREAD_STACK_SIZEOF(health_stack), health_fn,
			NULL, NULL, NULL, HEALTH_PRIORITY, 0, K_NO_WAIT);

	LOG_INF("Started producer/consumer pipeline: queue capacity %u",
		QUEUE_CAPACITY);
	return 0;
}
