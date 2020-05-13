#!/usr/bin/env python3

# ADSP devices have their RAM regions mapped twice, once in the 512MB
# region from 0x80000000-0x9fffffff and again from
# 0xa0000000-0xbfffffff.  The first mapping is set in the CPU to
# bypass the L1 cache, and so access through pointers in that region
# is coherent between CPUs (but slow).  The second region accesses the
# same memory through the L1 cache and requires careful flushing when
# used with shared data.
#
# This distinction is exposed in the linker script, where some symbols
# (e.g. stack regions) are linked into cached memory, but others
# (general kernel memory) are not.  But the rimage signing tool
# doesn't understand that and fails if regions aren't contiguous.
#
# Walk the sections in the ELF file, changing the VMA/LMA of each
# uncached section to the equivalent region in the 

import os
import sys
from elftools.elf.elffile import ELFFile

objcopy_bin = sys.argv[1]
elffile = sys.argv[2]

fixup =[]
with open(elffile, "rb") as fd:
    elf = ELFFile(fd)
    for s in elf.iter_sections():
        addr = s.header.sh_addr
        if addr >= 0x80000000 and addr < 0xa0000000:
            print(f"fix_elf_addrs.py: Moving section {s.name} to cached SRAM region")
            fixup.append(s.name)

for s in fixup:
    cmd = f"{objcopy_bin} --change-section-address {s}+0x20000000 {elffile}"
    print(cmd)
    os.system(cmd)
