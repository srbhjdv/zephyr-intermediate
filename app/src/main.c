#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(workqueue_demo, LOG_LEVEL_INF);

#define STACK_SIZE 1024
#define SENSOR_THREAD_PRIORITY 5
#define EVENTS_PER_BURST 5
#define EVENT_INTERVAL_MS 4
#define DEBOUNCE_INTERVAL_MS 20
#define BURST_INTERVAL_MS 100

static atomic_t pending_events;
static atomic_t debounce_runs;

static void debounce_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	const int event_count = atomic_set(&pending_events, 0);
	const int run = atomic_inc(&debounce_runs) + 1;

	LOG_INF("Debounced work handler run %d after %d sensor events", run,
		event_count);
}

K_WORK_DELAYABLE_DEFINE(debounce_work, debounce_handler);

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		for (int event = 1; event <= EVENTS_PER_BURST; event++) {
			int total = atomic_inc(&pending_events) + 1;

			LOG_INF("Sensor event %d/%d received (pending batch: %d)",
				event, EVENTS_PER_BURST, total);
			k_work_reschedule(&debounce_work,
					  K_MSEC(DEBOUNCE_INTERVAL_MS));

			if (event < EVENTS_PER_BURST) {
				k_msleep(EVENT_INTERVAL_MS);
			}
		}

		/* Leave a quiet interval before simulating the next burst. */
		k_msleep(BURST_INTERVAL_MS);
	}
}

K_THREAD_DEFINE(sensor_sim, STACK_SIZE, sensor_sim_fn,
		NULL, NULL, NULL, SENSOR_THREAD_PRIORITY, 0, 0);

int main(void)
{
	LOG_INF("Starting event-driven sensor workqueue demo");
	return 0;
}
