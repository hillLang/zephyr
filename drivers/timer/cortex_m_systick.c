/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <drivers/system_timer.h>
#include <sys_clock.h>
#include <spinlock.h>
#include <arch/arm/cortex_m/cmsis.h>

void _ExcExit(void);

/* Minimum cycles in the future to try to program. */
#define MIN_DELAY 512

#define COUNTER_MAX 0x00ffffff
#define TIMER_STOPPED 0xff000000

#define CYC_PER_TICK (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC	\
		      / CONFIG_SYS_CLOCK_TICKS_PER_SEC)
#define MAX_TICKS ((COUNTER_MAX / CYC_PER_TICK) - 1)
#define MAX_CYCLES (MAX_TICKS * CYC_PER_TICK)

static struct k_spinlock lock;

/* Last value placed in LOAD register (else "TIMER_STOPPED") */
static u32_t last_load;

/* Running count of accumulated cycles */
static u32_t cycle_count;

static u32_t announced_cycles;

// FIXME: need to add this to cycle_count whenever we reset the
// counter, right now it only does it for expired timeouts.
static u32_t delay_adj;

/* OR-accumulated cache of the CTRL register for testing overflow */
static volatile u32_t ctrl_cache;

/* The control register clears on read, necessitating this cache so
 * that elapsed() returns correctly the second+ time after an
 * overflow
 */
void reset_overflow(void)
{
	ctrl_cache = SysTick->CTRL;
	ctrl_cache = 0;
}

/* Callout out of platform assembly, not hooked via IRQ_CONNECT... */
void _timer_int_handler(void *arg)
{
	ARG_UNUSED(arg);
	u32_t dticks;
	k_spinlock_key_t key = k_spin_lock(&lock);

	cycle_count += last_load + delay_adj;
	dticks = (cycle_count - announced_cycles) / CYC_PER_TICK;
	announced_cycles += dticks * CYC_PER_TICK;
	reset_overflow();
	k_spin_unlock(&lock, key);

	z_clock_announce(IS_ENABLED(CONFIG_TICKLESS_KERNEL) ? dticks : 1);
	_ExcExit();
}

int z_clock_driver_init(struct device *device)
{
	NVIC_SetPriority(SysTick_IRQn, _IRQ_PRIO_OFFSET);
	SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk |
			  SysTick_CTRL_TICKINT_Msk |
			  SysTick_CTRL_CLKSOURCE_Msk);

	last_load = IS_ENABLED(CONFIG_TICKLESS_KERNEL) ?
		MAX_CYCLES : CYC_PER_TICK;
	SysTick->LOAD = last_load;
	SysTick->VAL = 0; /* resets timer to last_load */
	return 0;
}

void z_clock_set_timeout(s32_t ticks, bool idle)
{
	/* Fast CPUs and a 24 bit counter mean that even idle systems
	 * need to wake up multiple times per second.  If the kernel
	 * allows us to miss tick announcements in idle, then shut off
	 * the counter. (Note: we can assume if idle==true that
	 * interrupts are already disabled)
	 */
	if (IS_ENABLED(CONFIG_TICKLESS_IDLE) && idle && ticks == K_FOREVER) {
		SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
		last_load = TIMER_STOPPED;
		return;
	}

#ifdef CONFIG_TICKLESS_KERNEL
	u32_t val0, val1, delay, now, ll0;

	ticks = min(MAX_TICKS, max(ticks, 1));

	/* Desired delay in the future */
	delay = (ticks == 0) ? MIN_DELAY : ticks * CYC_PER_TICK;

	k_spinlock_key_t key = k_spin_lock(&lock);

	/* Get current time as soon as we take the lock */
	val0 = SysTick->VAL & COUNTER_MAX;
	// FIXME: can roll over, also "now" should just be c_c
	now = ((last_load - val0) & COUNTER_MAX) + cycle_count;

	/* Expresed as a delta from last announcement */
	delay = delay + (now - announced_cycles);

	/* Round up to nearest tick boundary */
	delay = ((delay + CYC_PER_TICK - 1) / CYC_PER_TICK) * CYC_PER_TICK;

	/* Back to delta from now */
	last_load = delay - (now - announced_cycles);

	ll0 = last_load;
	reset_overflow();
	cycle_count = now;

	compiler_barrier();
	val1 = SysTick->VAL & COUNTER_MAX;
	SysTick->LOAD = last_load;
	SysTick->VAL = 0; /* resets timer to last_load */
	compiler_barrier();

	/* We check time at the end to account for lost cycles during
	 * this computation that the clock didn't "see".  Keep the
	 * delta computed for this timeout, but add the adjustment
	 * back to the cycle counter when it expires.  Note that the
	 * count may have rolled over while we worked, the hardware
	 * clock doesn't honor spinlocks!
	 */
	delay_adj += val0 > val1 ? val0 - val1 : ll0 - (val1 - val0);
	k_spin_unlock(&lock, key);
#endif
}

static u32_t elapsed(void)
{
	u32_t val = SysTick->VAL & COUNTER_MAX;
	u32_t cyc = cycle_count - announced_cycles;

	ctrl_cache |= SysTick->CTRL;
	u32_t ov = (ctrl_cache & SysTick_CTRL_COUNTFLAG_Msk) ? last_load : 0;

	return (last_load - val) + cyc + ov;
}

u32_t z_clock_elapsed(void)
{
	if (!IS_ENABLED(CONFIG_TICKLESS_KERNEL)) {
		return 0;
	}

	k_spinlock_key_t key = k_spin_lock(&lock);
	u32_t ret = elapsed() + cycle_count - announced_cycles;

	k_spin_unlock(&lock, key);
	return ret / CYC_PER_TICK;
}

u32_t _timer_cycle_get_32(void)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	u32_t ret = elapsed() + cycle_count;

	k_spin_unlock(&lock, key);
	return ret;
}

void z_clock_idle_exit(void)
{
	if (last_load == TIMER_STOPPED) {
		SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
	}
}

void sys_clock_disable(void)
{
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
}
