#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(zbus_sensor_demo, LOG_LEVEL_INF);

#define STACK_SIZE 1024
#define PUBLISHER_PRIORITY 5
#define SUBSCRIBER_PRIORITY 6
#define SAMPLE_COUNT 10
#define SAMPLE_PERIOD_MS 100
#define LOG_DELAY_MS 250
#define SUBSCRIBER_QUEUE_SIZE 16

struct sensor_sample {
	unsigned int sequence;
	int temperature_c;
	int humidity_percent;
};

static void display_listener_cb(const struct zbus_channel *chan);

ZBUS_LISTENER_DEFINE(sensor_display_listener, display_listener_cb);
ZBUS_SUBSCRIBER_DEFINE(sensor_logger_subscriber, SUBSCRIBER_QUEUE_SIZE);

ZBUS_CHAN_DEFINE(sensor_data_chan,
		 struct sensor_sample,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(sensor_display_listener, sensor_logger_subscriber),
		 ZBUS_MSG_INIT(.sequence = 0,
			       .temperature_c = 0,
			       .humidity_percent = 0));

static void display_listener_cb(const struct zbus_channel *chan)
{
	if (chan != &sensor_data_chan) {
		return;
	}

	const struct sensor_sample *sample = zbus_chan_const_msg(chan);

	/* Keep synchronous listener work short for prompt display updates. */
	LOG_INF("Display update #%u: %d C, %d%% humidity", sample->sequence,
		sample->temperature_c, sample->humidity_percent);
}

static void sensor_publisher_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (unsigned int sequence = 1; sequence <= SAMPLE_COUNT; sequence++) {
		struct sensor_sample sample = {
			.sequence = sequence,
			.temperature_c = 20 + (int)(sequence % 5),
			.humidity_percent = 40 + (int)((sequence * 3) % 20),
		};

		if (zbus_chan_pub(&sensor_data_chan, &sample, K_MSEC(100)) != 0) {
			LOG_ERR("Failed to publish sensor sample #%u", sequence);
		}

		k_msleep(SAMPLE_PERIOD_MS);
	}

	LOG_INF("Sensor publisher finished");
}

static void sensor_logger_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	const struct zbus_channel *chan;

	for (unsigned int received = 0; received < SAMPLE_COUNT; received++) {
		if (zbus_sub_wait(&sensor_logger_subscriber, &chan, K_FOREVER) != 0) {
			continue;
		}

		if (chan == &sensor_data_chan) {
			struct sensor_sample sample;

			if (zbus_chan_read(chan, &sample, K_MSEC(100)) == 0) {
				LOG_INF("Slow logger received #%u: %d C, %d%% humidity",
					sample.sequence, sample.temperature_c,
					sample.humidity_percent);
			}
		}

		k_msleep(LOG_DELAY_MS);
	}

	LOG_INF("Sensor logger finished");
}

K_THREAD_DEFINE(sensor_publisher, STACK_SIZE, sensor_publisher_fn,
		NULL, NULL, NULL, PUBLISHER_PRIORITY, 0, 0);
K_THREAD_DEFINE(sensor_logger, STACK_SIZE, sensor_logger_fn,
		NULL, NULL, NULL, SUBSCRIBER_PRIORITY, 0, 0);

int main(void)
{
	LOG_INF("Starting zbus sensor demo: %u samples every %u ms",
		SAMPLE_COUNT, SAMPLE_PERIOD_MS);
	return 0;
}
