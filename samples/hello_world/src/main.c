/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

void hda_test(void);

u32_t pci_read(int bus, int dev, int func, int reg)
{
	u32_t addr = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | reg;

	sys_out32(addr, 0xcf8);
        return sys_in32(0xcfc);

}

void pci_write(int bus, int dev, int func, int reg, u32_t val)
{
	u32_t addr = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | reg;
	sys_out32(addr, 0xcf8);
        sys_out32(val, 0xcfc);
}

void probe(int bus, int dev, int func)
{
	u32_t val = pci_read(bus, dev, func, 4);
	u32_t status = val >> 16, command = val & 0xffff;
	printk("  status %xh command %xh\n", status, command);

	// FIXME: by default everything has the bus master bit cleared
	// in the command register.  Turn it on for everything here so
	// the HDA device can do DMA.  Need a general solution
	val |= 4;
	pci_write(bus, dev, func, 4, val);

	val = pci_read(bus, dev, func, 8);
	u32_t class = val >> 8, rev = val & 0xff;
	printk("  class %xh rev %xh\n", class, rev);

	val = pci_read(bus, dev, func, 12);
	printk("  BIST/HdrType/Lat/CacheSz %8x\n", val);

	for (int bar = 0; bar < 6; bar++) {
		int reg = 0x10 + (bar * 4);
		u32_t bar0 = pci_read(bus, dev, func, reg);

		if (bar0) {
			pci_write(bus, dev, func, reg, -1);

			u32_t masked = pci_read(bus, dev, func, reg);
			u32_t bytes = 1 + ~(masked & ~0xf);

			pci_write(bus, dev, func, reg, bar0);

			/* Note read-back to make sure it wasn't
			 * corrupted.  Also leaves the flag bits in
			 * the bottom without interpretation
			 */
			printk("  BAR%d %d bytes at %xh\n",
			       bar, bytes,
			       pci_read(bus, dev, func, reg));
		}
	}

	printk("  Cardbus CIS %xh\n", pci_read(bus, dev, func, 0x28));
	printk("  Subsys IDs %08xh\n", pci_read(bus, dev, func, 0x2c));
	printk("  Ex. ROM @ %xh\n", pci_read(bus, dev, func, 0x30));
	printk("  Capabilities %xh\n", pci_read(bus, dev, func, 0x34));

	val = pci_read(bus, dev, func, 0x3c);
	u32_t maxlat = val >> 24, mingnt = (val >> 16) & 0xff;
	u32_t intpin = (val >> 8) & 0xff, intline = val & 0xff;
	printk("  MaxLat %d MinGnt %d IntPin %d IntLine %d\n",
	       maxlat, mingnt, intpin, intline);
}

void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);

	for (int bus = 0; bus < 256; bus++) {
		for (int dev = 0; dev < 32; dev++) {
			for (int func = 0; func < 8; func++) {
				u32_t ids = pci_read(bus, dev, func, 0);

				if (ids != -1) {
					u32_t vid = ids & 0xffff;
					u32_t did = ids >> 16;

					printk("\nb%d d%d f%d ID %04x:%04x\n",
					       bus, dev, func, vid, did);
					probe(bus, dev, func);
				}
			}
		}
	}

	hda_test();
}
