#include <kernel_structs.h>
#include <system_timer.h>
#include <syscall_handler.h>

struct device;

int sys_clock_hw_cycles_per_tick;

// FIXME: these variables should be wrapped behind a timeslicing API or something
s32_t _time_slice_duration;
s32_t _time_slice_elapsed;
int _time_slice_prio_ceiling;

void _sys_clock_tick_announce(void)
{
}

void _sys_clock_final_tick_announce(void)
{
}

void _add_thread_timeout(struct k_thread *th, _wait_q_t *wq, s32_t ticks)
{
}

int _abort_thread_timeout(struct k_thread *th)
{
    return 0;
}

void _init_thread_timeout(struct _thread_base *th)
{
}

void _add_timeout(struct k_thread *th, struct _timeout *to,
                  _wait_q_t *wait_q, s32_t ticks)
{
}

int _abort_timeout(struct _timeout *to)
{
    return 0;
}

void _init_timeout(struct _timeout *to, _timeout_func_t fn)
{
}

s32_t _get_next_timeout_expiry(void)
{
    return 0;
}

u32_t _impl_k_uptime_get_32(void)
{
    return 0;
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(k_uptime_get_32)
{
        return _impl_k_uptime_get_32();
}
#endif

////////////////////////////////////////////////////////////////////////
// Tickless:

// Crazy flag used to track calls to
// k_en/disable_sys_clock_always_on(), which is only used as a guard
// around k_uptime_get_32()...
int _sys_clock_always_on;

#ifdef CONFIG_TIMER_READS_ITS_FREQUENCY_AT_RUNTIME
int sys_clock_hw_cycles_per_sec;
#endif

s32_t _sys_idle_elapsed_ticks;

s64_t _tick_get(void)
{
    return 0;
}

volatile u64_t _sys_clock_tick_count;

