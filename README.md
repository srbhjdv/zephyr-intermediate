# Zephyr Training Environment

Welcome to the Zephyr RTOS training! This repository includes a ready-to-use
development environment based on Zephyr **4.4.0**, which you can set up in one of
three ways:

---

## Manual Zephyr Setup

Follow the following guide:
- [Getting Started Guide](https://docs.zephyrproject.org/4.4.0/develop/getting_started/index.html#).

Make sure to select appropriate OS and to perform all steps till
[Build the Blinky Sample](https://docs.zephyrproject.org/4.4.0/develop/getting_started/index.html#build-the-blinky-sample).

## L1 Task 1: Ready, Waiting, and Yielding

The app defines three preemptive threads that log and then sleep for different
intervals: `T_HIGH` (priority 3, 100 ms), `T_MED` (priority 5, 200 ms), and
`T_LOW` (priority 7, 300 ms). In Zephyr, a lower numeric priority is higher
priority. A thread becomes **Waiting** when it sleeps, so the scheduler can run
another ready thread. When the sleeping thread's timeout expires, it becomes
**Ready**; if it has the highest priority among ready threads, it runs next.

The `T_COOP` thread uses cooperative priority -1. It performs five short busy
work iterations and calls `k_yield()` after each one. Yield gives other ready
threads an opportunity to run, but it does not put `T_COOP` into Waiting. By
contrast, `k_msleep()` suspends a thread until its timeout expires. Because the
preemptive threads sleep at different intervals, their log interleaving changes
over time; `T_HIGH` wakes most often, but priority determines which ready thread
runs, not how often a thread's sleep interval expires.

## L2 Task 1: Race Condition and Mutex

Two equal-priority threads each increment a shared counter 10,000 times. In the
first phase, each thread deliberately yields between reading and writing the
counter. This exposes a lost-update race: both threads can read the same value,
then write back the same incremented value, so the final `Race result` is
typically below the expected 20,000. The exact result depends on scheduling.

In the second phase, the threads increment a separate shared counter while
holding a Zephyr mutex. The mutex makes each read-modify-write operation
exclusive, so the `Mutex result` should be 20,000. Both results are printed
after both threads complete, avoiding an early read of a counter that is still
being updated.

## L3 Task 1: Event-Driven Workqueue and Debounce

The sensor simulator emits five events 4 ms apart, then waits before starting
the next burst. Each event increments an atomic pending-event count and
reschedules a delayed work item for 20 ms later. Rescheduling pushes the
handler's deadline out until the burst has been quiet for 20 ms, so one handler
run reports the whole five-event batch. The handler only runs in response to
those sensor events; there is no polling loop waking every 10 ms to check a
flag. Compared with polling, this avoids periodic wake-ups when no event has
arrived, and the delayed work coalesces a burst into one debounced callback.

## L4 Task 1: Sensor Data with zbus

The app defines one `sensor_data_chan` channel for a sample containing a
sequence number, temperature, and humidity. The publisher sends ten simulated
samples at 100 ms intervals. A synchronous zbus listener reads the channel's
current message and logs a fast display update during publication. A separate
thread-based subscriber waits for channel notifications and adds a 250 ms
processing delay to model slower logging. The subscriber queue can retain all
ten notifications in this finite demo; standard zbus subscribers receive a
channel reference, so a delayed read may see the latest channel value rather
than a historical copy of every publication.

## L5 Task 1: Queue Pressure and Task Watchdog

The producer sends 30 samples at 100 ms intervals into an eight-item message
queue. The consumer processes the first sample, then deliberately sleeps for
2,500 ms. During that stall, the queue reaches its 75% warning level (six of
eight slots), fills, and the producer logs dropped samples. A task-watchdog
channel with a 1,200 ms timeout monitors the consumer; its callback logs when
the consumer misses its deadline. The consumer feeds its watchdog after
receiving work and removes its channel when it exits. A separate health thread
checks queue occupancy every 100 ms and warns when it reaches 75% capacity.

## L6 Task 1: Control Response-Time Scheduling

The control thread's response-time guarantee is to begin processing each
accepted timer event within 10 ms of its timer-release timestamp. The control log
includes the event sequence, measured response time, whether the deadline was
met, and the cumulative miss count. Queue overflows also count as missed
events. Warnings are limited to the first miss and then every ten misses.

The starter priorities caused the delay: Zephyr gives priority 4 higher
urgency than priority 7, so the maintenance thread's 45 ms busy wait could run
ahead of the control thread and exceed its 10 ms deadline. The correction
raises control to priority 4 and lowers maintenance to priority 7, letting
control run as soon as an event is queued. Maintenance work remains scheduled
after the control thread blocks.

The timer emits `control_event_ready` with sequence and release time;
`control_event_complete` records sequence and response time, and
`control_deadline_miss` marks violations. The app config enables CTF tracing,
the POSIX backend, kernel thread monitoring, and the serial kernel shell for
runtime inspection. Use the shell's `kernel threads` command to inspect thread
priorities and states, then decode the POSIX backend's `channel0_0` capture
with `babeltrace2 channel0_0` from the run directory. No runtime trace or
measured before/after results are included because this submission was
prepared without running or building the firmware.
