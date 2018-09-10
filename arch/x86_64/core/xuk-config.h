#ifndef _XUK_CONFIG_H
#define _XUK_CONFIG_H

/* This file defines "kconfig" variables used by the xuk layer only in
 * unit test situations where we aren't using pulling in the true
 * autoconf.h
 */
#ifndef CONFIG_X86_64

/* #define CONFIG_XUK_DEBUG 1 */

/* The APIC timer will run 2^X times slower than the TSC. (X = 0-7) */
#define CONFIG_XUK_APIC_TSC_SHIFT 6

#define CONFIG_MP_NUM_CPUS 2

#endif /* CONFIG_X86_64 */
#endif /* _XUK_CONFIG_H */
