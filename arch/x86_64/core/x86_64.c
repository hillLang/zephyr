#include <kernel_internal.h>
#include <kernel_structs.h>
#include <tracing.h>
#include <ksched.h>
#include "xuk.h"

struct device;

void _new_thread(struct k_thread *t, k_thread_stack_t *stack,
                 size_t sz, k_thread_entry_t entry,
                 void *p1, void *p2, void *p3,
                 int prio, unsigned int opts)
{
	void *args[] = { p1, p2, p3 };
	int nargs = 3;
	int eflags = 0;
	char *base = K_THREAD_STACK_BUFFER(stack);
	char *top = base + sz;

	_new_thread_init(t, base, sz, prio, opts);

	t->switch_handle = (void *)xuk_setup_stack((long) top,
						   (void *)entry,
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

typedef void (*cpu_init_fn_t)(int, void*);
static cpu_init_fn_t cpu_init_fns[CONFIG_MP_NUM_CPUS];
static void *cpu_init_args[CONFIG_MP_NUM_CPUS];

/* Called from Zephyr initialization */
void _arch_start_cpu(int cpu_num, k_thread_stack_t *stack, int sz,
		     void (*fn)(int, void *), void *arg)
{
	/* We ignore the stack, having already configured it.  See
	 * explanation below.
	 */
	ARG_UNUSED(stack);
	ARG_UNUSED(sz);
	cpu_init_fns[cpu_num] = fn;
	cpu_init_args[cpu_num] = arg;
}

/* Called from xuk layer on actual CPU start */
void _cpu_start(int cpu)
{
	xuk_set_f_ptr(cpu, &_kernel.cpus[cpu]);

	if (cpu <= 0) {
		for (int i = 0; i < CONFIG_MP_NUM_CPUS; i++) {
			cpu_init_fns[i] = 0;
		}

		_Cstart();
	} else if (cpu < CONFIG_MP_NUM_CPUS) {
		/* SMP initialization.  First spin, waiting for
		 * _arch_start_cpu() to be called from the main CPU
		 */
		while(!cpu_init_fns[cpu]) {
		}

		/* Enter Zephyr, which will switch away and never return */
		cpu_init_fns[cpu](0, cpu_init_args[cpu]);
	}

	/* Spin forever as a fallback */
	while (1) {
	}
}

/* Returns the initial stack to use for CPU startup on auxilliary (not
 * cpu 0) processors to the xuk layer.  We just use the interrupt
 * stack, which is sort of an impedance mismatch.  The original SMP
 * API wants to pass the stack to _arch_start_cpu(), but with xuk it's
 * much simpler to let the lower level do it on its own and "request"
 * the stack from us here.
 */
unsigned int _init_cpu_stack(int cpu)
{
	extern k_thread_stack_t _interrupt_stack1[];
	extern k_thread_stack_t _interrupt_stack2[];
	extern k_thread_stack_t _interrupt_stack3[];
	void *base = 0;

	if (cpu == 1) {
		base = &_interrupt_stack1[0];
	} else if(cpu == 2) {
		base = &_interrupt_stack2[0];
	} else if(cpu == 3) {
		base = &_interrupt_stack3[0];
	}

	return (int)(long)(base + CONFIG_ISR_STACK_SIZE);
}
