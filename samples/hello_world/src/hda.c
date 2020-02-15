#include <zephyr.h>

#define HDA_BAR0 0xfebf0000
#define HDA_IRQ 11

/* Size (in transfer units) of the CORB/RIRB ring buffers.  The
 * interface specification only allows 2, 16 or 256, and specific
 * hardware may implement only a subset of those (qemu supports only
 * 256)
 */
#define RING_BUF_SIZE 256

/* How many buffers per stream */
#define NUM_BUFS 3

/* How many samples per buffer */
#define BUF_SAMPLES 128

/* Maximum number of codecs in a chain that defines a stream */
#define MAX_STREAM_CODECS 8

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

enum stream_regs {
	STSCTL	= 0x0,
	LPIB	= 0x04,
	CBL	= 0x08,
	LVI	= 0x0c, /* 16 bit */
	FIFOS	= 0x10, /* 16 bit */
	FMT	= 0x12, /* 16 bit */
	BDPL	= 0x18,
	BDPU	= 0x1c,
};

enum codec_cmds {
	GET_NODE_PARAM = 0xf00,
	GET_CONN_ENTRY = 0xf02,
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

#define STREAMREG(s, r) (0x80 + (s * 32) + r)

#define HDA_STREAM_REG8(s, r) HDA_REG8(STREAMREG(s, r))
#define HDA_STREAM_REG16(s, r) HDA_REG16(STREAMREG(s, r))
#define HDA_STREAM_REG32(s, r) HDA_REG32(STREAMREG(s, r))

struct buf_desc_entry {
	u64_t addr;
	u32_t len;
	u32_t interrupt; /* 1 = "interrupt on completion" */
}; // FIXME: aligned(128)!

struct stream_desc {
	/* Abstract assigned stream identifier, should be >= 1 */
	int stream_id;

	/* Index within stream register banks */
	int stream_idx;

	/* Which codec device's widgets are we using? */
	int codec_id;

	/* List of widgets to assemble into the chain, in order from
	 * the last sink (i.e. an output pin, or an input ADC link) to
	 * the original source (e.g. DAC link, microphone pin).  Node
	 * zero is never a widget (it's always a function group) so
	 * zeros are used to terminate the list.
	 */
	int widget_chain[MAX_STREAM_CODECS];

	struct stream_bufs *bufs;
};

struct stream_bufs {
	struct buf_desc_entry __aligned(128) buf_desc_list[NUM_BUFS];
	// FIXME: misalignment right here
	unsigned short __aligned(128) input_buf[NUM_BUFS][BUF_SAMPLES];
	unsigned short __aligned(128) output_buf[NUM_BUFS][BUF_SAMPLES];
};

static struct stream_bufs out_stream_bufs;
static struct stream_desc out_stream = {
	.stream_id = 1,
	.stream_idx = 4,
	.codec_id = 0,
	.widget_chain = { 3, 2 },
	.bufs = &out_stream_bufs;
};

/* Hardware requires the buffers be aligned to 128 byte boundaries.
 * Note carefully that RIRB entries are TWICE the size of CORB
 * commands.
 */
static volatile u32_t __aligned(128) corb[RING_BUF_SIZE];
static volatile u64_t __aligned(128) rirb[RING_BUF_SIZE];

static int last_rirb;

/* Three small (256 byte) buffers each for output and input streams. */
// FIXME: move to stream_desc
static unsigned short __aligned(128) output_buf[3][128];

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
	/* NOTE: this is intolerant of existing commands in flight (it
	 * assume's we've transmitted everything) and of "unsolicited"
	 * out of band messages from the codecs.
	 */
	enqueue_cmd(codec, node, cmd, data);
	return rirb[await_rirb()];
}

/* Each output buffer implements an identical triangle wave over the
 * full buffer length.  With a 128 sample buffer at 48 kHz, that comes
 * out to a 375 Hz tone (roughly the F# above middle C).
 */
// FIXME: move to stream_desc
static void init_output_bufs(void)
{
	int val, halfway = BUF_SAMPLES / 2;

	for (int b = 0; b < NUM_BUFS; b++) {
		for (int i = 0; i < halfway; i++) {
			/* Increasing ramp */
			val = (0xffff * i) / halfway;
			output_buf[b][i] = val;
		}
		for (int i = 0; i + halfway < BUF_SAMPLES; i++) {
			/* Decreasing ramp */
			val = 0xffff - ((0xffff * i) / halfway);
			output_buf[b][halfway + i] = val;
		}
	}
}

static void enum_node(int codec, int node, int depth)
{
	int widget_type = 0;
	u64_t val;
	char prefix[16];
	for(int i=0; i<depth; i++) {
		prefix[2*i] = ' ';
		prefix[2*i+1] = ' ';
	}
	prefix[2*depth] = 0;
	printk("\n%s Codec%d Node%d (%s):\n", prefix, codec, node,
	       depth == 0 ? "root"
	       : (depth == 1 ? "function group" : "widget"));

	if (node == 0) {
		val = sync_codec_cmd(codec, node, GET_NODE_PARAM, IDS);
		printk("%s  Vendor ID = %xh\n", prefix, (int)val);
		val = sync_codec_cmd(codec, node, GET_NODE_PARAM, REVISION);
		printk("%s  Revision ID = %xh\n", prefix, (int)val);
	}

	if (depth == 1) {
		val = sync_codec_cmd(codec, node,
				     GET_NODE_PARAM, FUNC_GROUP_TYPE);
		printk("%s  Function Group Type = %xh\n", prefix, (int)val);

		val = sync_codec_cmd(codec, node,
				     GET_NODE_PARAM, AUD_GROUP_CAP);
		printk("%s  Audio Group Capabilities = %xh\n", prefix, (int)val);
	}

	if (depth > 1) {
		val = sync_codec_cmd(codec, node,
				     GET_NODE_PARAM, AUD_WIDGET_CAP);
		widget_type = (val >> 20) & 0xf;
		printk("%s  Audio Widget Capabilities = %xh (type: %xh)\n",
		       prefix, (int)val, widget_type);
	}

	if (depth != 0) {
		val = sync_codec_cmd(codec, node,
				     GET_NODE_PARAM, SUPP_PCM_RATES);
		if (val) {
			printk("%s  Supported PCM Rates = %xh\n", prefix, (int)val);
		}
		val = sync_codec_cmd(codec, node,
				     GET_NODE_PARAM, SUPP_FORMATS);
		if (val) {
			printk("%s  Supported Stream Formats = %xh\n",
			       prefix, (int)val);
		}
	}

	if (depth > 1 && widget_type == 4) {
		val = sync_codec_cmd(codec, node,
				     GET_NODE_PARAM, PIN_CAP);
		printk("%s  Pin Capabilities = %xh\n", prefix, (int)val);
	}

	for (int i = 0; depth > 0 && i < 2; i++) {
		int arg = i ? OUT_AMP_CAP : IN_AMP_CAP;
		val = sync_codec_cmd(codec, node,
				     GET_NODE_PARAM, arg);

		if (!val) {
			continue;
		}

		printk("%s  %sput Amplifier stepsz=%d nsteps=%d offset=%d%s\n",
		       prefix, i ? "Out" : "In", (int)((val >> 16) & 0x7f),
		       (int)((val >> 8) & 0x7f), (int)(val & 0x7f),
		       (val & (1 << 31)) ? " (mute works)" : "");
	}

	val = sync_codec_cmd(codec, node,
			     GET_NODE_PARAM, CONN_LIST_LEN);
	if (val > 0) {
		// FIXME: only supports fetching one connection!
		val = sync_codec_cmd(codec, node, GET_CONN_ENTRY, 0);
		printk("%s  Connections: [ %d ]\n", prefix, (int)val);
	}

	val = sync_codec_cmd(codec, node, GET_NODE_PARAM, SUPP_POWER_STATES);
	if (val) {
		printk("%s  Supported Power States: %xh\n", prefix, (int)val);
	}

	val = sync_codec_cmd(codec, node, GET_NODE_PARAM, PROC_CAP);
	if (val) {
		printk("%s  Processing Capabilities: %xh\n", prefix, (int)val);
	}

	val = sync_codec_cmd(codec, node, GET_NODE_PARAM, GPIO_COUNT);
	if (val) {
		printk("%s  GPIO Count: %xh\n", prefix, (int)val);
	}

	val = sync_codec_cmd(codec, node, GET_NODE_PARAM, VOL_CAP);
	if (val) {
		printk("%s  Volume Capabilties: %xh\n", prefix, (int)val);
	}

	/* Now enumerate children */
	val = sync_codec_cmd(codec, node, GET_NODE_PARAM, NODE_COUNT);

	int start_node = (val >> 16) & 0xff;
	int num_nodes = val & 0xff;

	for (int n = 0; n < num_nodes; n++) {
		enum_node(codec, start_node + n, depth + 1);
	}
}

static void dump_stream(int idx)
{
	printk(" STS/CTL=%xh", HDA_STREAM_REG32(idx, STSCTL));
	printk(" LPIB=%d", HDA_STREAM_REG32(idx, LPIB));
	printk(" CBL=%d", HDA_STREAM_REG32(idx, CBL));
	printk(" LVI=%d\n", HDA_STREAM_REG16(idx, LVI));
	printk(" FIFOS=%d", HDA_STREAM_REG16(idx, FIFOS));
	printk(" FMT=%xh", HDA_STREAM_REG16(idx, FMT));
	printk(" BDPL=%xh", HDA_STREAM_REG32(idx, BDPL));
	printk(" BDPU=%xh\n", HDA_STREAM_REG32(idx, BDPU));
}

void hda_test(void)
{
	/* Reset the device by pulsing low bit of GCTL */
	HDA_REG32(GCTL) = 0;
	while ((HDA_REG32(GCTL) & 1) != 0) {
	}
	HDA_REG32(GCTL) = 1;
	while ((HDA_REG32(GCTL) & 1) == 0) {
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

	/********************************************************************/
	/* Debug code to dump state: */

	printk("\n*** HDA ***\n");

	printk("GCAP %xh VER %d.%d\n",
	       HDA_REG16(GCAP), HDA_REG8(VMAJ), HDA_REG8(VMIN));
	printk("WAKESTS %xh\n", HDA_REG16(WAKESTS));
	printk("GCTL %xh GSTS %xh INTCTL %xh INTSTS %xh\n",
	       HDA_REG32(GCTL), HDA_REG16(GSTS),
	       HDA_REG32(INTCTL), HDA_REG32(INTSTS));
	printk("ICIS %xh\n", HDA_REG16(ICIS));

	/* Seems like on qemu, the WALCLK register ticks 4x slower
	 * than the TSC.
	 */
	u32_t wc0 = HDA_REG32(WALCLK), cyc0 = k_cycle_get_32();
	for (int i=0; i<4; i++) {
		printk("  WALCLK %d cyc %d\n",
		       HDA_REG32(WALCLK) - wc0, k_cycle_get_32() - cyc0);
	}

	printk("CORB BASE %x:%xh (&corb[0] %p)\n", HDA_REG32(CORBUBASE), HDA_REG32(CORBLBASE), &corb[0]);
	printk("CORBWP %d CORBRP %d CORBCTL %xh CORBSTS %xh CORBSIZE %xh\n",
	       HDA_REG16(CORBWP), HDA_REG16(CORBRP), HDA_REG8(CORBCTL),
	       HDA_REG8(CORBSTS), HDA_REG8(CORBSIZE));

	printk("RIRBWP %d RIRBCTL %xh RIRBSTS %xh RIRBSIZE %xh\n",
	       HDA_REG16(RIRBWP), HDA_REG8(RIRBCTL),
	       HDA_REG8(RIRBSTS), HDA_REG8(RIRBSIZE));
	printk("RIRB BASE %x:%xh (&rirb[0] %p)\n", HDA_REG32(RIRBUBASE), HDA_REG32(RIRBLBASE), &rirb[0]);

	/* Dump stream registers */
	u32_t gcap = HDA_REG16(GCAP);
	int stream_idx = 0;

	for (int i = 0; i < ((gcap >> 12) & 0xf); i++) {
		printk("Input Stream (idx %d)\n", stream_idx);
		dump_stream(stream_idx++);
	}
	for (int i = 0; i < ((gcap >> 8) & 0xf); i++) {
		printk("Output Stream (idx %d)\n", stream_idx);
		dump_stream(stream_idx++);
	}
	for (int i = 0; i < ((gcap >> 12) & 0xf); i++) {
		printk("Bidirectional Stream (idx %d)\n", stream_idx);
		dump_stream(stream_idx++);
	}

	for (int c = 0; c < 16; c++) {
		if ((HDA_REG16(WAKESTS) & (1 << c)) != 0) {
			enum_node(c, 0, 0);
		}
	}
}
