#ifndef ZEPHYR_SYS_CLOCK_H__
#define ZEPHYR_SYS_CLOCK_H__

// FIXME: should be using this kconfig directly
#define sys_clock_ticks_per_sec CONFIG_SYS_CLOCK_TICKS_PER_SEC

// Why is Zephyr in the business of defining 300 year old SI nomenclature standards?
#define MSEC_PER_SEC 1000
#define USEC_PER_MSEC 1000
#define USEC_PER_SEC 1000000

// FIXME: clumsy API, this should be exposed by the driver and not #ifdefed here
#if defined(CONFIG_TIMER_READS_ITS_FREQUENCY_AT_RUNTIME)
extern int sys_clock_hw_cycles_per_sec;
#else
#define sys_clock_hw_cycles_per_sec CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC
#endif

// FIXME: dumb and dangerous on low precicion clocks
extern int sys_clock_hw_cycles_per_tick;

////////////////////////////////////////////////////////////////////////
// Tickless:

// FIXME: craziness
extern int _sys_clock_always_on;

void _enable_sys_clock(void);

extern volatile u64_t _sys_clock_tick_count;

#endif /* ZEPHYR_SYS_CLOCK_H__ */
