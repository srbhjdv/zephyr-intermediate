#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/tracing/tracing.h>

LOG_MODULE_REGISTER(scheduling_delay, LOG_LEVEL_INF);

#define STACK_SIZE 2048
#define CONTROL_PRIORITY 4
#define MAINTENANCE_PRIORITY 7
#define EVENT_PERIOD_MS 250
#define RESPONSE_DEADLINE_MS 10
#define MAINTENANCE_LOAD_US 45000

struct control_event {
	uint32_t seq;
	uint32_t ready_ms;
};

K_MSGQ_DEFINE(control_queue, sizeof(struct control_event), 4, 4);
K_SEM_DEFINE(maintenance_start, 0, 1);

static atomic_t deadline_miss_count;

static void event_timer_expiry(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	static uint32_t sequence;
	const struct control_event event = {
		.seq = sequence++,
		.ready_ms = k_uptime_get_32(),
	};

	/* Timer callbacks run in interrupt context; never wait on the queue. */
	if (k_msgq_put(&control_queue, &event, K_NO_WAIT) != 0) {
		const uint32_t misses = atomic_inc(&deadline_miss_count) + 1;

		sys_trace_named_event("control_queue_drop", event.seq, misses);
		return;
	}

	sys_trace_named_event("control_event_ready", event.seq, event.ready_ms);
	k_sem_give(&maintenance_start);
}

K_TIMER_DEFINE(event_timer, event_timer_expiry, NULL);

static void control_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	uint32_t last_warning_count = 0;

	while (true) {
		struct control_event event;

		if (k_msgq_get(&control_queue, &event, K_FOREVER) != 0) {
			continue;
		}

		const uint32_t response_ms = k_uptime_get_32() - event.ready_ms;
		const bool missed_deadline = response_ms > RESPONSE_DEADLINE_MS;

		if (missed_deadline) {
			atomic_inc(&deadline_miss_count);
		}

		const uint32_t misses = atomic_get(&deadline_miss_count);

		sys_trace_named_event("control_event_complete", event.seq,
				      response_ms);
		if (missed_deadline) {
			sys_trace_named_event("control_deadline_miss", event.seq,
					      response_ms);
		}

		LOG_INF("[CONTROL] seq=%u response=%u ms (%s), misses=%u",
			event.seq, response_ms,
			missed_deadline ? "DEADLINE MISS" : "within deadline",
			misses);

		/* Report the first miss seen here, then summarize every ten more. */
		if (misses > last_warning_count &&
		    (last_warning_count == 0 || misses >= last_warning_count + 10)) {
			LOG_WRN("Response-time deadline is %u ms; total misses: %u",
				RESPONSE_DEADLINE_MS, misses);
			last_warning_count = misses;
		}
	}
}

static void maintenance_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&maintenance_start, K_FOREVER);
		k_busy_wait(MAINTENANCE_LOAD_US);
	}
}

K_THREAD_DEFINE(control, STACK_SIZE, control_fn,
		NULL, NULL, NULL, CONTROL_PRIORITY, 0, 0);
K_THREAD_DEFINE(maintenance, STACK_SIZE, maintenance_fn,
		NULL, NULL, NULL, MAINTENANCE_PRIORITY, 0, 0);

int main(void)
{
	LOG_INF("L6: control response-time deadline is %u ms",
		RESPONSE_DEADLINE_MS);
	LOG_INF("Control priority %d; maintenance priority %d",
		CONTROL_PRIORITY, MAINTENANCE_PRIORITY);

	k_timer_start(&event_timer, K_MSEC(500), K_MSEC(EVENT_PERIOD_MS));
	return 0;
}
