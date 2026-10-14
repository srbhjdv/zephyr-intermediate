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
