/*
 * Copyright (c) 2014-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Variables needed needed for system clock
 *
 *
 * Declare variables used by both system timer device driver and kernel
 * components that use timer functionality.
 */

#ifndef ZEPHYR_INCLUDE_SYS_CLOCK_H_
#define ZEPHYR_INCLUDE_SYS_CLOCK_H_

#include <sys/util.h>
#include <sys/dlist.h>

#include <toolchain.h>
#include <zephyr/types.h>

#include <sys/time_units.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup clock_apis
 * @{
 */

/**
 * @brief Tick precision used in timeout APIs
 *
 * This type defines the word size of the timeout values used in
 * k_timeout_t objects, and thus defines an upper bound on maximum
 * timeout length (or equivalently minimum tick duration).  Note that
 * this does not affect the size of the system uptime counter, which
 * is always a 64 bit count of ticks.
 */
#ifdef CONFIG_TIMEOUT_64BIT
typedef s64_t k_ticks_t;
#else
typedef u32_t k_ticks_t;
#endif

#define K_TICKS_FOREVER ((k_ticks_t) -1)

#ifndef CONFIG_LEGACY_TIMEOUT_API

/**
 * @brief Kernel timeout type
 *
 * Timeout arguments presented to kernel APIs are stored in this
 * opaque type, which is capable of representing times in various
 * formats and units.  It should be constructed from application data
 * using one of the macros defined for this purpose (e.g. `K_MSEC()`,
 * `K_TIMEOUT_ABS_TICKS()`, etc...), or be one of the two constants
 * K_NO_WAIT or K_FOREVER.  Applications should not inspect the
 * internal data once constructed.  Timeout values may be compared for
 * equality with the `K_TIMEOUT_EQ()` macro.
 */
typedef struct {
	k_ticks_t ticks;
} k_timeout_t;

/**
 * @brief Compare timeouts for equality
 *
 * The k_timeout_t object is an opaque struct that should not be
 * inspected by application code.  This macro exists so that users can
 * test timeout objects for equality with known constants
 * (e.g. K_NO_WAIT and K_FOREVER) when implementing their own APIs in
 * terms of Zephyr timeout constants.
 *
 * @return True if the timeout objects are identical
 */
#define K_TIMEOUT_EQ(a, b) ((a).ticks == (b).ticks)

#define Z_TIMEOUT_NO_WAIT ((k_timeout_t) {})
#define Z_TIMEOUT_TICKS(t) ((k_timeout_t) { .ticks = (t) })
#define Z_FOREVER Z_TIMEOUT_TICKS(K_TICKS_FOREVER)
#define Z_TIMEOUT_MS(t) Z_TIMEOUT_TICKS(k_ms_to_ticks_ceil32(MAX(t, 0)))
#define Z_TIMEOUT_US(t) Z_TIMEOUT_TICKS(k_us_to_ticks_ceil32(MAX(t, 0)))
#define Z_TIMEOUT_NS(t) Z_TIMEOUT_TICKS(k_ns_to_ticks_ceil32(MAX(t, 0)))
#define Z_TIMEOUT_CYC(t) Z_TIMEOUT_TICKS(k_cyc_to_ticks_ceil32(MAX(t, 0)))

/* Converts between absolute timeout expiration values (packed into
 * the negative space below K_TICKS_FOREVER) and (non-negative) delta
 * timeout values.  If the result of Z_TICK_ABS(t) is >= 0, then the
 * value was an absolute timeout with the returend expiration time.
 * Note that this macro is bidirectional: Z_TICK_ABS(Z_TICK_ABS(t)) ==
 * t for all inputs, and that the representation of K_TICKS_FOREVER is
 * the same value in both spaces!  Clever, huh?
 */
#define Z_TICK_ABS(t) (K_TICKS_FOREVER - 1 - (t))

#else

/* Legacy timeout API */
typedef s32_t k_timeout_t;
#define K_TIMEOUT_EQ(a, b) ((a) == (b))
#define Z_TIMEOUT_NO_WAIT 0
#define Z_TIMEOUT_TICKS(t) k_ticks_to_ms_ceil32(t)
#define Z_FOREVER K_TICKS_FOREVER
#define Z_TIMEOUT_MS(t) (t)
#define Z_TIMEOUT_US(t) ((t) * 1000)
#define Z_TIMEOUT_NS(t) ((t) * 1000000)
#define Z_TIMEOUT_CYC(t) k_cyc_to_ms_ceil32(MAX((t), 0))

#endif

/** @} */

#ifdef CONFIG_TICKLESS_KERNEL
extern int _sys_clock_always_on;
extern void z_enable_sys_clock(void);
#endif

#if defined(CONFIG_SYS_CLOCK_EXISTS) && \
	(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC == 0)
#error "SYS_CLOCK_HW_CYCLES_PER_SEC must be non-zero!"
#endif

/* number of nsec per usec */
#define NSEC_PER_USEC 1000U

/* number of microseconds per millisecond */
#define USEC_PER_MSEC 1000U

/* number of milliseconds per second */
#define MSEC_PER_SEC 1000U

/* number of microseconds per second */
#define USEC_PER_SEC ((USEC_PER_MSEC) * (MSEC_PER_SEC))

/* number of nanoseconds per second */
#define NSEC_PER_SEC ((NSEC_PER_USEC) * (USEC_PER_MSEC) * (MSEC_PER_SEC))


/* kernel clocks */

/*
 * We default to using 64-bit intermediates in timescale conversions,
 * but if the HW timer cycles/sec, ticks/sec and ms/sec are all known
 * to be nicely related, then we can cheat with 32 bits instead.
 */

#ifdef CONFIG_SYS_CLOCK_EXISTS

#if defined(CONFIG_TIMER_READS_ITS_FREQUENCY_AT_RUNTIME) || \
	(MSEC_PER_SEC % CONFIG_SYS_CLOCK_TICKS_PER_SEC) || \
	(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC % CONFIG_SYS_CLOCK_TICKS_PER_SEC)
#define _NEED_PRECISE_TICK_MS_CONVERSION
#endif

#endif

#define __ticks_to_ms(t) __DEPRECATED_MACRO \
	k_ticks_to_ms_floor64((u64_t)(t))
#define z_ms_to_ticks(t) \
	((s32_t)k_ms_to_ticks_ceil32((u32_t)(t)))
#define __ticks_to_us(t) __DEPRECATED_MACRO \
	((s32_t)k_ticks_to_us_floor32((u32_t)(t)))
#define z_us_to_ticks(t) __DEPRECATED_MACRO \
	((s32_t)k_us_to_ticks_ceil32((u32_t)(t)))
#define sys_clock_hw_cycles_per_tick() __DEPRECATED_MACRO \
	((int)k_ticks_to_cyc_floor32(1U))
#define SYS_CLOCK_HW_CYCLES_TO_NS64(t) __DEPRECATED_MACRO \
	k_cyc_to_ns_floor64((u64_t)(t))
#define SYS_CLOCK_HW_CYCLES_TO_NS(t) __DEPRECATED_MACRO \
	((u32_t)k_cyc_to_ns_floor64(t))

/* added tick needed to account for tick in progress */
#define _TICK_ALIGN 1

/*
 * SYS_CLOCK_HW_CYCLES_TO_NS_AVG converts CPU clock cycles to nanoseconds
 * and calculates the average cycle time
 */
#define SYS_CLOCK_HW_CYCLES_TO_NS_AVG(X, NCYCLES) \
	(u32_t)(k_cyc_to_ns_floor64(X) / NCYCLES)

/**
 * @defgroup clock_apis Kernel Clock APIs
 * @ingroup kernel_apis
 * @{
 */

/**
 * @} end defgroup clock_apis
 */

/**
 *
 * @brief Return the lower part of the current system tick count
 *
 * @return the current system tick count
 *
 */
u32_t z_tick_get_32(void);

/**
 *
 * @brief Return the current system tick count
 *
 * @return the current system tick count
 *
 */
s64_t z_tick_get(void);

#ifndef CONFIG_SYS_CLOCK_EXISTS
#define z_tick_get() (0)
#define z_tick_get_32() (0)
#endif

u64_t z_timeout_end_calc(k_timeout_t timeout);

/* timeouts */

struct _timeout;
typedef void (*_timeout_func_t)(struct _timeout *t);

struct _timeout {
	sys_dnode_t node;
	s32_t dticks;
	_timeout_func_t fn;
};

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SYS_CLOCK_H_ */
