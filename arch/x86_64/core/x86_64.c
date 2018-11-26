#include <kernel_internal.h>
#include <kernel_structs.h>
#include <tracing.h>
#include <ksched.h>
#include <irq_offload.h>
#include "xuk.h"

struct device;

void _new_thread(struct k_thread *t, k_thread_stack_t *stack,
                 size_t sz, k_thread_entry_t entry,
                 void *p1, void *p2, void *p3,
                 int prio, unsigned int opts)
{
	void *args[] = { entry, p1, p2, p3 };
	int nargs = 4;
	int eflags = 0;
	char *base = K_THREAD_STACK_BUFFER(stack);
	char *top = base + sz;

	_new_thread_init(t, base, sz, prio, opts);

	t->switch_handle = (void *)xuk_setup_stack((long) top,
						   (void *)_thread_entry,
						   eflags, (long *)args,
						   nargs);
}

int _sys_clock_driver_init(struct device *device)
{
	/* No need to do anything, we're just using the TSC */
	return 0;
}

void k_cpu_idle(void)
{
	z_sys_trace_idle();
	__asm__ volatile("sti; hlt");
}

void _unhandled_vector(int vector, int err)
{
	/* FIXME: need to hook this to Zephyr fatal errors */
}

void *_isr_exit_restore_stack(void *interrupted)
{
	return _get_next_switch_handle(interrupted);
}

struct {
	void (*fn)(int, void*);
	void *arg;
	unsigned int esp;
} cpu_init[CONFIG_MP_NUM_CPUS];

/* Called from Zephyr initialization */
void _arch_start_cpu(int cpu_num, k_thread_stack_t *stack, int sz,
		     void (*fn)(int, void *), void *arg)
{
	cpu_init[cpu_num].arg = arg;
	cpu_init[cpu_num].esp = (int)(long)(sz + (char *)stack);

	/* This is our flag to the spinning CPU.  Do this last */
	cpu_init[cpu_num].fn = fn;
}

#ifdef CONFIG_IRQ_OFFLOAD
static irq_offload_routine_t offload_fn;
static void *offload_arg;

static void irq_offload_handler(void *arg, int err)
{
	ARG_UNUSED(arg);
	ARG_UNUSED(err);
	offload_fn(offload_arg);
}

void irq_offload(irq_offload_routine_t fn, void *arg)
{
	offload_fn = fn;
	offload_arg = arg;

	_apic.ICR_HI = (struct apic_icr_hi) {};
	_apic.ICR_LO = (struct apic_icr_lo) {
		.delivery_mode = FIXED,
		.vector = CONFIG_IRQ_OFFLOAD_VECTOR,
		.shorthand = SELF,
	};
}
#endif

/* Called from xuk layer on actual CPU start */
void _cpu_start(int cpu)
{
	xuk_set_f_ptr(cpu, &_kernel.cpus[cpu]);

#ifdef CONFIG_IRQ_OFFLOAD
	xuk_set_isr(XUK_INT_RAW_VECTOR(CONFIG_IRQ_OFFLOAD_VECTOR),
		    -1, irq_offload_handler, 0);
#endif

	if (cpu <= 0) {
		/* The SMP CPU startup function pointers act as init
		 * flags.  Zero them here because this code is running
		 * BEFORE .bss is zeroed!  Should probably move that
		 * out of _Cstart() for this architecture...
		 */
		for (int i = 0; i < CONFIG_MP_NUM_CPUS; i++) {
			cpu_init[i].fn = 0;
		}

		/* Enter Zephyr */
		_Cstart();

	} else if (cpu < CONFIG_MP_NUM_CPUS) {
		/* SMP initialization.  First spin, waiting for
		 * _arch_start_cpu() to be called from the main CPU
		 */
		while(!cpu_init[cpu].fn) {
		}

		/* Enter Zephyr, which will switch away and never return */
		cpu_init[cpu].fn(0, cpu_init[cpu].arg);
	}

	/* Spin forever as a fallback */
	while (1) {
	}
}

/* Returns the initial stack to use for CPU startup on auxilliary (not
 * cpu 0) processors to the xuk layer, which gets selected by the
 * non-arch Zephyr kernel and stashed by _arch_start_cpu()
 */
unsigned int _init_cpu_stack(int cpu)
{
	return cpu_init[cpu].esp;
}

void _arch_irq_disable(unsigned int irq)
{
	xuk_set_isr_mask(irq, 1);
}

void _arch_irq_enable(unsigned int irq)
{
	xuk_set_isr_mask(irq, 0);
}
