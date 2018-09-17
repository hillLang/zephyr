#ifndef ZEPHYR_SYSTEM_TIMER_H__
#define ZEPHYR_SYSTEM_TIMER_H__

/* FIXME: this header appears to be dual mode just to declare one symbol */
#ifdef _ASMLANGUAGE
GTEXT(_timer_int_handler)
#else

struct device;

int _sys_clock_driver_init(struct device *device);

void _sys_clock_tick_announce(void);
void _sys_clock_final_tick_announce(void);

void _timer_idle_enter(s32_t ticks);

void _timer_idle_exit(void);

u32_t _impl_k_uptime_get_32(void);

////////////////////////////////////////////////////////////////////////
// Tickless:

// Defined in core kernel, but only ever used INTERNAL to the drivers!
s32_t _sys_idle_elapsed_ticks;

u64_t _get_elapsed_clock_time(void);

void _set_time(u32_t time);

u32_t _get_elapsed_program_time(void);

u32_t _get_remaining_program_time(void);

#endif /* _ASMLANGUAGE -- FIXME! */

#endif /* ZEPHYR_SYSTEM_TIMER_H__ */
