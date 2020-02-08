#include <zephyr.h>

#define HDA_BAR0 0xfebf0000
#define HDA_IRQ 11

/* Size (in transfer units) of the CORB/RIRB ring buffers.  The
 * interface specification only allows 2, 16 or 256, and specific
 * hardware may implement only a subset of those (qemu supports only
 * 256)
 */
#define RING_BUF_SIZE 256

enum hda_regs {
	GCAP		= 0x00,	/* Global Capabilities (16 bit) */
	VMIN		= 0x02,	/* Minor Version (8 bit) */
	VMAJ		= 0x03,	/* Major Version (8 bit) */
	OUTPAY		= 0x04,	/* Output Payload Cap (16 bit) */
	INPAY		= 0x06,	/* Input Payload Cap (16 bit) */
	GCTL		= 0x08,	/* Global Control */
	WAKEEN		= 0x0C,	/* Wake Enable (16 bit) */
	WAKESTS		= 0x0E,	/* Wake Status (16 bit) */
	GSTS		= 0x10,	/* Global Status (16 bit) */
	OUTSTRMPAY	= 0x18,	/* Output Stream Payload Cap (16 bit) */
	INSTRMPAY	= 0x1A,	/* Input Stream Payload Cap (16 bit) */
	INTCTL		= 0x20,	/* Interrupt Control */
	INTSTS		= 0x24,	/* Interrupt Status */
	WALCLK		= 0x30,	/* Wall Clock Counter */
	SSYNC		= 0x38,	/* Stream Synchronization */
	CORBLBASE	= 0x40,	/* CORB Lower Base Address */
	CORBUBASE	= 0x44,	/* CORB Upper Base Address */
	CORBWP		= 0x48,	/* CORB Write Pointer (16 bit) */
	CORBRP		= 0x4A,	/* CORB Read Pointer (16 bit) */
	CORBCTL		= 0x4C,	/* CORB Control (8 bit) */
	CORBSTS		= 0x4D,	/* CORB Status (8 bit) */
	CORBSIZE	= 0x4E,	/* CORB Size (8 bit) */
	RIRBLBASE	= 0x50,	/* RIRB Lower Base Address */
	RIRBUBASE	= 0x54,	/* RIRB Upper Base Address */
	RIRBWP		= 0x58,	/* RIRB Write Pointer (16 bit) */
	RINTCNT		= 0x5A,	/* Response Interrupt Count (16 bit) */
	RIRBCTL		= 0x5C,	/* RIRB Control (8 bit) */
	RIRBSTS		= 0x5D,	/* RIRB Status (8 bit) */
	RIRBSIZE	= 0x5E,	/* RIRB Size (8 bit) */
	ICOI		= 0x60,	/* Imm. Command Output Interface */
	ICII		= 0x64,	/* Imm. Command Input Interface */
	ICIS		= 0x68,	/* Imm. Command Status (16 bit) */
	DPIBLBASE	= 0x70,	/* DMA Position Buffer Lower Base */
	DPIBUBASE	= 0x74,	/* DMA Position Buffer Upper Base */
};

enum codec_cmds {
	GET_NODE_PARAM = 0xf00,
};

enum node_params {
	IDS = 0,

	REVISION = 0x02,

	NODE_COUNT = 0x04,
	FUNC_GROUP_TYPE = 0x05,


	AUD_GROUP_CAP = 0x08,
	AUD_WIDGET_CAP = 0x09,
	SUPP_PCM_RATES = 0x0a,
	SUPP_FORMATS = 0x0b,
	PIN_CAP = 0x0c,
	IN_AMP_CAP = 0x0d,
	CONN_LIST_LEN = 0x0e,
	SUPP_POWER_STATES = 0x0f,
	PROC_CAP = 0x10,
	GPIO_COUNT = 0x11,
	OUT_AMP_CAP = 0x12,
	VOL_CAP = 0x13,
};

#define HDA_REG8(r)  (*((volatile  u8_t *)(long)(HDA_BAR0 + r)))
#define HDA_REG16(r) (*((volatile u16_t *)(long)(HDA_BAR0 + r)))
#define HDA_REG32(r) (*((volatile u32_t *)(long)(HDA_BAR0 + r)))

/* Hardware requires the buffers be aligned like this!  Note carefully
 * that RIRB entries are twice the size of CORB commands.
 */
static volatile u32_t __aligned(128) corb[RING_BUF_SIZE];
static volatile u64_t __aligned(128) rirb[RING_BUF_SIZE];

int last_rirb;

static void enqueue_cmd(int codec, int node, int cmd, int data)
{
	u32_t val = (codec << 28) | (node << 20) | (cmd << 8) | data;
	int wp = (HDA_REG16(CORBWP) + 1) & (RING_BUF_SIZE - 1);

	corb[wp] = val;
	HDA_REG16(CORBWP) = wp;
}

/* spins waiting on a RIRB entry, returning its index (!) in rirb[].
 * Note that qith qemu, all codec operations are perfectly
 * synchronous, they happen inside the MMIO write to CORBWP.
 */
static int await_rirb(void)
{
	while (HDA_REG16(RIRBWP) == last_rirb) {
		k_busy_wait(1);
	}

	last_rirb = (last_rirb + 1) & (RING_BUF_SIZE - 1);
	return last_rirb;
}

static u64_t sync_codec_cmd(int codec, int node, int cmd, int data)
{
	// FIXME: this is intolerant of existing commands in flight
	// (it assume's we've transmitted everything) and of
	// "unsolicited" out of band messages from the codecs.
	enqueue_cmd(codec, node, cmd, data);
	return rirb[await_rirb()];
}

void hda_test(void)
{
	printk("\n*** HDA ***\n");

	/* Reset the device by pulsing low bit of GCTL */
	HDA_REG32(GCTL) = 0;
	while ((HDA_REG32(GCTL) & 1) != 0) {
	}
	HDA_REG32(GCTL) = 1;
	while ((HDA_REG32(GCTL) & 1) == 0) {
	}

	printk("GCAP %xh VER %d.%d\n",
	       HDA_REG16(GCAP), HDA_REG8(VMAJ), HDA_REG8(VMIN));
	printk("WAKESTS %xh\n", HDA_REG16(WAKESTS));
	printk("GCTL %xh GSTS %xh INTCTL %xh INTSTS %xh\n",
	       HDA_REG32(GCTL), HDA_REG16(GSTS),
	       HDA_REG32(INTCTL), HDA_REG32(INTSTS));
	printk("ICIS %xh\n", HDA_REG16(ICIS));

	u32_t wc0 = HDA_REG32(WALCLK), cyc0 = k_cycle_get_32();
	for (int i=0; i<4; i++) {
		printk("  WALCLK %d cyc %d\n",
		       HDA_REG32(WALCLK) - wc0, k_cycle_get_32() - cyc0);
	}

	int szbits = RING_BUF_SIZE == 256 ? 2 : (RING_BUF_SIZE == 16 ? 1 : 0);

	/* Initialize the CORB, note the dance required by the spec to
	 * reset the Read Pointer register: write a 1 to the high bit,
	 * read it back, then set it back to zero and read back.  The
	 * last step enables the DMA engine.
	 */
	HDA_REG32(CORBLBASE) = (u32_t)(long)&corb[0];
	HDA_REG32(CORBUBASE) = 0;
	HDA_REG8(CORBSIZE) = szbits;
	HDA_REG16(CORBWP) = 0;
	HDA_REG16(CORBRP) = 0x8000;
	while ((HDA_REG16(CORBRP) & 0x8000) == 0) {
		k_busy_wait(1);
	}
	HDA_REG16(CORBRP) = 0;
	while (HDA_REG16(CORBRP) != 0) {
		k_busy_wait(1);
	}
	HDA_REG8(CORBCTL) = 0x2; /* Enable DMA */
	while ((HDA_REG8(CORBCTL) & 0x2) == 0) {
	}

	printk("CORB BASE %x:%xh (&corb[0] %p)\n", HDA_REG32(CORBUBASE), HDA_REG32(CORBLBASE), &corb[0]);
	printk("CORBWP %d CORBRP %d CORBCTL %xh CORBSTS %xh CORBSIZE %xh\n",
	       HDA_REG16(CORBWP), HDA_REG16(CORBRP), HDA_REG8(CORBCTL),
	       HDA_REG8(CORBSTS), HDA_REG8(CORBSIZE));

	/* The RIRB initializes similarly, but the "Read Pointer" is
	 * our side (implemented in software) so there is no register.
	 */
	HDA_REG32(RIRBLBASE) = (u32_t)(long)&rirb[0];
	HDA_REG16(RIRBUBASE) = 0;
	HDA_REG16(RIRBWP) = 0;
	HDA_REG8(RIRBSIZE) = szbits;
	HDA_REG8(RIRBCTL) = 0x2;

	/* HDA allows for an interrupt to be delivered after this many
	 * RIRB entries have been queued (presumably as a mechanism to
	 * prevent overruns).  Qemu, seemingly contrary to the docs,
	 * will actually STALL THE DEVICE when this limit is reached
	 * until something resets the interrupt status bit (i.e. until
	 * it thinks an ISR has serviced the requests).  So on qemu,
	 * it's required that this have a non-zero value, otherwise it
	 * looks "full" with its default value of zero and nothing
	 * happens.
	 */
	HDA_REG16(RINTCNT) = 0xff;

	printk("RIRBWP %d RIRBCTL %xh RIRBSTS %xh RIRBSIZE %xh\n",
	       HDA_REG16(RIRBWP), HDA_REG8(RIRBCTL),
	       HDA_REG8(RIRBSTS), HDA_REG8(RIRBSIZE));
	printk("RIRB BASE %x:%xh (&rirb[0] %p)\n", HDA_REG32(RIRBUBASE), HDA_REG32(RIRBLBASE), &rirb[0]);

	u64_t val = sync_codec_cmd(0, 0, GET_NODE_PARAM, IDS);
	printk("C0 N0 IDS: %llxh\n", val);

	val = sync_codec_cmd(0, 0, GET_NODE_PARAM, NODE_COUNT);
	printk("C0 N0 NODE_COUNT: %llxh\n", val);
}
