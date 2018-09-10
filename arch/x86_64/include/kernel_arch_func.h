#ifndef _KERNEL_ARCH_FUNC_H
#define _KERNEL_ARCH_FUNC_H

#include <irq.h>
#include <xuk-switch.h>

static inline void kernel_arch_init(void)
{
	/* This is a noop, we already took care of things before
	 * _Cstart() is entered
	 */
}

static inline struct _cpu *_arch_curr_cpu(void)
{
	long long ret, off = 0;

	/* The struct _cpu pointer for the current CPU lives at the
	 * start of the the FS segment
	 */
	__asm__("movq %%fs:(%1), %0" : "=r"(ret) : "r"(off));
	return (void *)(long)ret;
}

static inline unsigned int _arch_irq_lock(void)
{
	unsigned long long key;

	__asm__ volatile("pushfq; cli; popq %0" : "=r"(key));
	return (int)key;
}

static inline void _arch_irq_unlock(unsigned int key)
{
	if (key & 0x200) {
		__asm__ volatile("sti");
	}
}

void _arch_irq_disable(unsigned int irq);
void _arch_irq_enable(unsigned int irq);

static inline unsigned int _arch_k_cycle_get_32(void)
{
	unsigned int hi, lo;

	__asm__ volatile("rdtsc" : "=d"(hi), "=a"(lo));
	return lo;
}

/* Not a standard Zephyr function, but probably will be */
static inline unsigned long long _arch_k_cycle_get_64(void)
{
	unsigned int hi, lo;

	__asm__ volatile("rdtsc" : "=d"(hi), "=a"(lo));
	return (((unsigned long long)hi) << 32) | lo;
}

static inline void _IntLibInit(void)
{
}

#define _is_in_isr() (_arch_curr_cpu()->nested != 0)

static inline void _arch_switch(void *switch_to, void **switched_from)
{
	xuk_switch(switch_to, switched_from);
}

#endif /* _KERNEL_ARCH_FUNC_H */
