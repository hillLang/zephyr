SECTIONS {
	. = 0x100000;

	.text : {
	      *(.xuk_stub32)
	      . = ALIGN(16);
	      *(.xuk_start64*)
	      *(.text*)
	}
	.rodata : { *(.rodata*) }
	.data : { *(.data*) }
	.bss : { *(.bss*) *(COMMON) }
}
