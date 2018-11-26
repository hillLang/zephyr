#include <autoconf.h> /* Seems not to get picked up automatically? */

#define _LINKER
#define _ASMLANGUAGE
#include <linker/linker-defs.h>

/* The common-ram.ld definitions are written to a sort of oddball
 * preprocessor API which we reimplement here.  The default implementation
 * is incompatible with the simpler way we handle address assignment
 * and ELF segment definitions
 */
#define SORT_BY_NAME(x) SORT(x)
#define OPTIONAL
#define SECTION_DATA_PROLOGUE(name, opts, align) name opts : align
#define SECTION_PROLOGUE(name, opts, align)      name opts : align
#define GROUP_DATA_LINK_IN(v, l) :ram
#define GROUP_LINK_IN(a)         :ram

PHDRS {
      ram PT_LOAD;
}

_start = 0x100000;
ENTRY(_start);

SECTIONS {
	. = 0x100000;

	text : {
		KEEP(*(.xuk_stub32*))
		. = ALIGN(16);
		KEEP(*(.xuk_start64*))

		*(.text_start*)
		*(.text*)
		*(.gnu.linkonce.t.*)
		*(.eh_frame)
		*(.init)
		*(.fini)
		*(.eini)
		KEEP(*(.openocd_dbg*))
	} :ram

#include <linker/common-rom.ld>

	rodata : {
		*(.rodata*)
		*(.gnu.linkonce.r.*)
	} :ram

	data : {
		*(.data*)
	} :ram

#include <linker/common-ram.ld>

	bss (NOLOAD) : {
		*(COMMON)
		*(.bss*)
	} :ram

	noinit (NOLOAD) : {
		*(.noinit*)
	} :ram

}
