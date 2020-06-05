/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <platform/shim.h>

#define STACK_SIZE 2048
K_THREAD_STACK_DEFINE(stack, STACK_SIZE);

/* Weirdness with a non-array "first_dword" because -Warray-bounds is
 * too smart and flips out when we calculate the uncached mapping, no
 * matter how hard I try to hide the array.
 */
static __aligned(64) struct {
	u32_t first_dword;
	u32_t rest[15];
} cacheline;

/* Test rig to do an operation through cached/uncached pointers.  Logs
 * what is going to happen, executes it, makes sure the compiler
 * doens't muck with the results, and prints the old and new values
 */
#define CACHEOP(expr) do {						\
		printk("%s\n", #expr);					\
		u32_t u0 = *ucp, c0 = *cp;				\
		__asm__ volatile("" : : : "memory");			\
		expr;							\
		__asm__ volatile("" : : : "memory");			\
		printk(" :: *cp = %d -> %d, *ucp = %d -> %d\n", c0, *cp, u0, *ucp); \
	} while(0)

int __incoherent cached_int;
int __attribute__((__section__(".cached"))) cached2_int;

static int bss_int;

void check_cache(void)
{
        // Check linkage, make sure things are mapped where they
        // should be
        int stack_int;
	printk("&cached_int = %p\n", &cached_int);
	printk("&cached2_int = %p\n", &cached2_int);
        printk("&bss_int = %p\n", &bss_int);
        printk("&stack_int = %p\n", &stack_int);

	for (int region = 0; region < 8; region++) {
		u32_t addr = 0x20000000 * region;
		u32_t attrib;

		__asm__ volatile("rdtlb1 %0, %1" : "=r"(attrib) : "r"(addr));
		printk("region %d @0x%x cachattr 0x%x\n", region, addr, attrib & 0xf);
	}

	/* Cached and uncached pointers to the same value. */
        long addr = (long)&cacheline.first_dword;
	volatile u32_t *cp = (void *)(addr | 0x20000000);
	volatile u32_t *ucp = (void *)(addr & ~0x20000000);

	printk("cp %p ucp %p\n", cp, ucp);

	*cp = *ucp = 0;

	/* Set the cached pointer to 1, only it should show the results */
	CACHEOP(*cp = 1);

	/* Set the uncached pointer, the cached one shouldn't change */
	CACHEOP(*ucp = 2);

	/* Flush the cache from before, BOTH should become 1 */
	CACHEOP(SOC_DCACHE_FLUSH((void*)cp, sizeof(*cp)));

	/* Now prime the cache with a new value, and invalidate it */
	CACHEOP(*cp = 3);
	CACHEOP(SOC_DCACHE_INVALIDATE((void*)cp, sizeof(*cp)));

	/* Write into the underlying memory, make sure cp has it
	 * cached, write one word into the cache, write new values
	 * into the uncached RAM, then flush the cache.  Does the
	 * flush honor the mask of "actually written" memory or does
	 * it write back the previously read (and still unmodified)
	 * data?
	 */
	SOC_DCACHE_INVALIDATE((void*)cp, sizeof(*cp));
	ucp[0] = ucp[1] = 5;  /* Prime RAM with 5's */
	cp[0] = 6;            /* Write a 6 into the cache */
	ucp[0] = ucp[1] = 7;  /* Now RAM is 7's, but cache still {6, 5} */
	SOC_DCACHE_FLUSH((void*)cp, sizeof(*cp));
	if (ucp[1] == 7) {
		printk("Cache is piecewise coherent!\n");
	} else if (ucp[1] == 5) {
		/* This is the truth here.  Sigh. */
		printk("Cache is piecewise INCOHERENT!\n");
	} else {
		printk("Huh?!\n");
	}

}

/* Dumps 1kb of memory at the specified address */
void dump_mem(unsigned int addr)
{
	const int rowbytes = 32;

	for(int row = 0; row < 512/rowbytes; row++) {
		printk("@%08x ", addr + rowbytes*row);

		int *pi = (int *)(addr + row*rowbytes);
		char *pc = (char *)pi;
		for (int col = 0; col < (rowbytes / sizeof(int)); col++) {
			printk("%08x ", pi[col]);
		}
		for (int b = 0; b < rowbytes; b++) {
			char c = pc[b];
			printk("%c", c = c < ' ' ? '.' : (c > '~' ? '.' : c));
		}
		printk("\n");
	}
}

void check_write(volatile int *p)
{
	int val = *p;
	*p = 0x5a5a5a5a;
	*p = val;
}

void probe_imr(void)
{
	uint32_t addr, addr0 = 0x90000000;

	for(addr = addr0; addr < 0x90400000; addr += 4) {
		check_write((int *)addr);
	}
	printk("Probed IMR range [%p,%p]\n", (void*)addr0, (void*)(addr-4));

#if 0
	// This one fails and crashes the DSP, but recoverably (the next
	// firmware load boots OK)
	printk("Trying %p\n", addr0 - 4);
	check_write(addr0 - 4);

	// This one hangs the DSP solid and requries a power cycle to
	// recover.
	printk("Trying %p\n", (int *)addr);
	check_write((int *)addr);
#endif
}

void timecheck(void)
{
	unsigned int c0, c, n = 0;
	__asm__ volatile("rsr.ccount %0" : "=r"(c0));
	while(true) {
		__asm__ volatile("rsr.ccount %0" : "=r"(c));
		if (c - c0 > 400000000) {
			printk("%d\n", c);
			c0 = c;
			if (++n >= 5) {
				break;
			}
		}
	}
}

void busy_wait(unsigned int us)
{
	unsigned int cyc = us * 400;
	unsigned int t0, t1;
	__asm__ volatile("rsr.ccount %0" : "=r"(t0));
	do {
		__asm__ volatile("rsr.ccount %0" : "=r"(t1));
	} while (t1 - t0 < cyc);
}

/* Bounces around the power-of-two sized memory region specified doing
 * reads from random addresses (to evade any intervening caching or
 * prefetching), returning a cycle count for the 100k operations.
 * Mask must obviously have 1's in its lower bits.
 */
void membounce(unsigned int addr, unsigned int mask)
{
	unsigned int t0, t1, val = 29;
	busy_wait(10000);

	mask &= ~3;  // mask off bottom 2 bits for alignment
	__asm__ volatile("rsr.ccount %0" : "=r"(t0));
	for(int i = 0; i < 100000; i++) {
		volatile int *p = (void *)(addr + val);
		val = ((val ^ (*p)) * 1664525 + 1013904223) & mask;
	}
	__asm__ volatile("rsr.ccount %0" : "=r"(t1));

	unsigned int result = t1 - t0;
	printk("%d.%05d cyc/read\n", result / 100000, result % 100000);
}

void imr_walkbench(void)
{
	unsigned int t0, t1, val = 0, n = 1000000;
	volatile int *imr = (void *)0x90000000;

	__asm__ volatile("rsr.ccount %0" : "=r"(t0));
	for(int i=0; i<n; i++) {
		val += imr[i];
	}
	__asm__ volatile("rsr.ccount %0" : "=r"(t1));
	unsigned int result = t1 - t0;
	printk("%d.%06d cyc/read\n", result / 1000000, result % 1000000);
}

void mem_benchmark(void)
{
        printk("L1 cache bench: ");
        membounce(0xbe000000, 16*1024 - 1);
        printk("HP-SRAM bench: ");
        membounce(0x9e000000, 512*1024 - 1);
        printk("LP-SRAM bench: ");
        membounce(0x9e800000, 128*1024 - 1);

	// The IMR is behaving weirdly.  The first 64k seems reliably
	// fast, but the others can take 1000+ cycles per op.  That's
	// hundreds of refresh cycles of the underlying DRAM, it's
	// absolutely ridiculous.  It's like there's a tiny cache for
	// the first bit, but everything else... it's like it's
	// handled by a software interrupt somewhere.  Maybe there's a
	// TLB and we're taking page faults in something?  Note the
	// busy wait in membounce(), that seems to regularize the
	// behavior.
	printk("IMR bench (1st 64k): ");
	membounce(0x90000000, 0xffff);
	printk("          (2nd 64k): ");
	membounce(0x90010000, 0xffff);
	printk("          (3rd 64k): ");
	membounce(0x90020000, 0xffff);
	printk("          (1st 128k): ");
	membounce(0x90000000, 0xffff);
	printk("          (full): ");
	membounce(0x90000000, 0x3fffff);

	// Sequential walks are faster, but not super fast. Like
	// there's a prefetch buffer maybe?
	printk("IMR sequential walk: ");
	imr_walkbench();
}


void dump_misc_regs(void)
{
	extern char _memmap_vecbase_reset;
	u32_t vecbase;
	__asm__ volatile("rsr.vecbase %0" : "=r"(vecbase));
	printk("_memmap_vecbase_reset @ %p VECBASE %p\n",
	       &_memmap_vecbase_reset, (void*)vecbase);


	for(int w=0; w<4; w++) {
		printk("Window %d @%p limit offset %d\n",
		       w, *(void**)DMWBA(w), (*(int*)DMWLO(w))>>3);
	}
}


void dump_imr(void)
{
	// Dump and note that this region, unused by any DSP firmware,
        // contains precisely the CSME-iterpreted start of the rimage
        // file.  It just copies the thing blind.
        dump_mem(0xb0002000);
}

FUNC_NORETURN void cputop(void *data)
{
	printk("\n==\n== Second CPU!\n==\n");
	dump_misc_regs();
	check_cache();

	while(1) {
		for(volatile int i=0; i<10000000; i++);
		printk(".\n");
	}
}

void mp_check(void)
{
#if CONFIG_MP_NUM_CPUS > 1 && !defined(CONFIG_SMP)
        // Fire up the second CPU
	arch_start_cpu(1, stack, STACK_SIZE, cputop, NULL);
#endif
}

void main(void)
{
	k_busy_wait(500);
	printk("Hello World! %s [T%p]\n", CONFIG_BOARD, k_current_get());

	dump_misc_regs();
	//dump_imr();
        //probe_imr();
	//mem_benchmark();
        //timecheck();
	check_cache();
	mp_check();
}
