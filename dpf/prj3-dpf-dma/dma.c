#include "stdio.h"
#include "lib.h"

typedef unsigned int u32;
typedef unsigned char u8;
typedef char bool;

#define CMD_DMAADDH	0x54
#define CMD_DMAEND	0x00
#define CMD_DMAFLUSHP	0x35
#define CMD_DMAGO	0xa0
#define CMD_DMALD	0x04
#define CMD_DMALDP	0x25
#define CMD_DMALP	0x20
#define CMD_DMALPEND	0x28
#define CMD_DMAKILL	0x01
#define CMD_DMAMOV	0xbc
#define CMD_DMANOP	0x18
#define CMD_DMARMB	0x12
#define CMD_DMASEV	0x34
#define CMD_DMAST	0x08
#define CMD_DMASTP	0x29
#define CMD_DMASTZ	0x0c
#define CMD_DMAWFE	0x36
#define CMD_DMAWFP	0x30
#define CMD_DMAWMB	0x13

#define SZ_DMAADDH	3
#define SZ_DMAEND	1
#define SZ_DMAFLUSHP	2
#define SZ_DMALD	1
#define SZ_DMALDP	2
#define SZ_DMALP	2
#define SZ_DMALPEND	2
#define SZ_DMAKILL	1
#define SZ_DMAMOV	6
#define SZ_DMANOP	1
#define SZ_DMARMB	1
#define SZ_DMASEV	2
#define SZ_DMAST	1
#define SZ_DMASTP	2
#define SZ_DMASTZ	1
#define SZ_DMAWFE	2
#define SZ_DMAWFP	2
#define SZ_DMAWMB	1
#define SZ_DMAGO	6


enum dmamov_dst {
	SAR = 0,
	CCR,
	DAR,
};

enum pl330_dst {
	SRC = 0,
	DST,
};

enum pl330_cond {
	SINGLE,
	BURST,
	ALWAYS,
};

#define printk 	printf
//#define PL330_DEBUG_MCGEN

#ifdef PL330_DEBUG_MCGEN
static unsigned cmd_line = 0;
#define PL330_DBGCMD_DUMP(off, x...)	do { \
						printk("%x:", cmd_line); \
						printk(x); \
						cmd_line += off; \
					} while (0)
#define PL330_DBGMC_START(addr)		(cmd_line = addr)
#else
#define PL330_DBGCMD_DUMP(off, x...)	do {} while (0)
#define PL330_DBGMC_START(addr)		do {} while (0)
#endif

static inline u32 _emit_MOV(unsigned dry_run, u8 buf[],
		enum dmamov_dst dst, u32 val)
{
	if (dry_run)
		return SZ_DMAMOV;

	buf[0] = CMD_DMAMOV;
	buf[1] = dst;
	//*((u32 *)&buf[2]) = val;
	buf[2] = val & 0xff;
	buf[3] = (val>>8) & 0xff;
	buf[4] = (val>>16) & 0xff;
	buf[5] = (val>>24) & 0xff;

	PL330_DBGCMD_DUMP(SZ_DMAMOV, "\tDMAMOV %s 0x%x\n",
		dst == SAR ? "SAR" : (dst == DAR ? "DAR" : "CCR"), val);

	return SZ_DMAMOV;
}

static inline u32 _emit_LP(unsigned dry_run, u8 buf[],
		unsigned loop, u8 cnt)
{
	if (dry_run)
		return SZ_DMALP;

	buf[0] = CMD_DMALP;

	if (loop)
		buf[0] |= (1 << 1);

	cnt--; /* DMAC increments by 1 internally */
	buf[1] = cnt;

	PL330_DBGCMD_DUMP(SZ_DMALP, "\tDMALP_%c %d\n", loop ? '1' : '0', cnt);

	return SZ_DMALP;
}

static inline u32 _emit_LD(unsigned dry_run, u8 buf[],	enum pl330_cond cond)
{
	if (dry_run)
		return SZ_DMALD;

	buf[0] = CMD_DMALD;

	if (cond == SINGLE)
		buf[0] |= (0 << 1) | (1 << 0);
	else if (cond == BURST)
		buf[0] |= (1 << 1) | (1 << 0);

	PL330_DBGCMD_DUMP(SZ_DMALD, "\tDMALD%c\n",
		cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'A'));

	return SZ_DMALD;
}

static inline u32 _emit_ST(unsigned dry_run, u8 buf[], enum pl330_cond cond)
{
	if (dry_run)
		return SZ_DMAST;

	buf[0] = CMD_DMAST;

	if (cond == SINGLE)
		buf[0] |= (0 << 1) | (1 << 0);
	else if (cond == BURST)
		buf[0] |= (1 << 1) | (1 << 0);

	PL330_DBGCMD_DUMP(SZ_DMAST, "\tDMAST%c\n",
		cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'A'));

	return SZ_DMAST;
}

struct _arg_LPEND {
	enum pl330_cond cond;
	char forever;
	unsigned loop;
	u8 bjump;
};

static inline u32 _emit_LPEND(unsigned dry_run, u8 buf[],
		const struct _arg_LPEND *arg)
{
	enum pl330_cond cond = arg->cond;
	char forever = arg->forever;
	unsigned loop = arg->loop;
	u8 bjump = arg->bjump;

	if (dry_run)
		return SZ_DMALPEND;

	buf[0] = CMD_DMALPEND;

	if (loop)
		buf[0] |= (1 << 2);

	if (!forever)
		buf[0] |= (1 << 4);

	if (cond == SINGLE)
		buf[0] |= (0 << 1) | (1 << 0);
	else if (cond == BURST)
		buf[0] |= (1 << 1) | (1 << 0);

	buf[1] = bjump;

	PL330_DBGCMD_DUMP(SZ_DMALPEND, "\tDMALP%s%c_%c bjmpto_%x\n",
			forever ? "FE" : "END",
			cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'A'),
			loop ? '1' : '0',
			bjump);

	return SZ_DMALPEND;
}

static inline u32 _emit_SEV(unsigned dry_run, u8 buf[], u8 ev)
{
	if (dry_run)
		return SZ_DMASEV;

	buf[0] = CMD_DMASEV;

	ev &= 0x1f;
	ev <<= 3;
	buf[1] = ev;

	PL330_DBGCMD_DUMP(SZ_DMASEV, "\tDMASEV %u\n", ev >> 3);

	return SZ_DMASEV;
}

static inline u32 _emit_END(unsigned dry_run, u8 buf[])
{
	if (dry_run)
		return SZ_DMAEND;

	buf[0] = CMD_DMAEND;

	PL330_DBGCMD_DUMP(SZ_DMAEND, "\tDMAEND\n");

	return SZ_DMAEND;
}

static inline u32 _emit_FLUSHP(unsigned dry_run, u8 buf[], u8 peri)
{
	if (dry_run)
		return SZ_DMAFLUSHP;

	buf[0] = CMD_DMAFLUSHP;

	peri &= 0x1f;
	peri <<= 3;
	buf[1] = peri;

	PL330_DBGCMD_DUMP(SZ_DMAFLUSHP, "\tDMAFLUSHP %d\n", peri >> 3);

	return SZ_DMAFLUSHP;
}

struct _xfer_spec {
	u32 ccr;
	//struct pl330_req *r;
	//struct pl330_xfer *x;
	int size;
};

struct _arg_GO {
	u8 chan;
	u32 addr;
	unsigned ns;
};

static inline u32 _emit_GO(unsigned dry_run, u8 buf[],
		const struct _arg_GO *arg)
{
	u8 chan = arg->chan;
	u32 addr = arg->addr;
	unsigned ns = arg->ns;

	if (dry_run)
		return SZ_DMAGO;

	buf[0] = CMD_DMAGO;
	buf[0] |= (ns << 1);

	buf[1] = chan & 0x7;

	//*((u32 *)&buf[2]) = addr;
	buf[2] = addr & 0xff;
	buf[3] = (addr>>8) & 0xff;
	buf[4] = (addr>>16) & 0xff;
	buf[5] = (addr>>24) & 0xff;

	return SZ_DMAGO;
}

#define true 	1
#define false 	0

#define CC_SRCINC	(1 << 0)
#define CC_DSTINC	(1 << 14)
#define CC_SRCPRI	(1 << 8)
#define CC_DSTPRI	(1 << 22)
#define CC_SRCNS	(1 << 9)
#define CC_DSTNS	(1 << 23)
#define CC_SRCIA	(1 << 10)
#define CC_DSTIA	(1 << 24)
#define CC_SRCBRSTLEN_SHFT	4
#define CC_DSTBRSTLEN_SHFT	18
#define CC_SRCBRSTSIZE_SHFT	1
#define CC_DSTBRSTSIZE_SHFT	15
#define CC_SRCCCTRL_SHFT	11
#define CC_SRCCCTRL_MASK	0x7
#define CC_DSTCCTRL_SHFT	25
#define CC_DRCCCTRL_MASK	0x7
#define CC_SWAP_SHFT	28

static inline u32 _prepare_ccr(void)
{
	u32 ccr = 0;

	ccr |= CC_SRCINC;
//	ccr |= CC_DSTINC;

	//if (rqc->nonsecure)
		ccr |= CC_SRCNS | CC_DSTNS;		// DMA_peri must be non-secure

	ccr |= (0 << CC_SRCBRSTSIZE_SHFT);
	ccr |= (0 << CC_DSTBRSTSIZE_SHFT);
	
	ccr |= (((1 - 1) & 0xf) << CC_SRCBRSTLEN_SHFT);
	ccr |= (((1 - 1) & 0xf) << CC_DSTBRSTLEN_SHFT);

	ccr |= (1 << CC_SRCCCTRL_SHFT);
	ccr |= (1 << CC_DSTCCTRL_SHFT);

	return ccr;
}
			
#if 0
static inline u32 _prepare_ccr(const struct pl330_reqcfg *rqc)
{
	u32 ccr = 0;

	if (rqc->src_inc)
		ccr |= CC_SRCINC;

	if (rqc->dst_inc)
		ccr |= CC_DSTINC;

	/* We set same protection levels for Src and DST for now */
	if (rqc->privileged)
		ccr |= CC_SRCPRI | CC_DSTPRI;
	if (rqc->nonsecure)
		ccr |= CC_SRCNS | CC_DSTNS;
	if (rqc->insnaccess)
		ccr |= CC_SRCIA | CC_DSTIA;

	ccr |= (((rqc->brst_len - 1) & 0xf) << CC_SRCBRSTLEN_SHFT);
	ccr |= (((rqc->brst_len - 1) & 0xf) << CC_DSTBRSTLEN_SHFT);

	ccr |= (rqc->brst_size << CC_SRCBRSTSIZE_SHFT);
	ccr |= (rqc->brst_size << CC_DSTBRSTSIZE_SHFT);

	ccr |= (rqc->dcctl << CC_SRCCCTRL_SHFT);
	ccr |= (rqc->scctl << CC_DSTCCTRL_SHFT);

	ccr |= (rqc->swap << CC_SWAP_SHFT);

	return ccr;
}
#endif

#define DBGCMD		0xd04
#define DBGINST0	0xd08
#define DBGINST1	0xd0c

#define DMA_MEM		0xFA200000
#define DMA_PERI	0xE0900000

void writel(int val, int addr)
{
	*(volatile unsigned int *)addr = val;

	return;
}

//static inline void _execute_DBGINSN(struct pl330_thread *thrd,
static inline void _execute_DBGINSN(int dma_regs_base,
		u8 insn[], bool as_manager)
{
//	void __iomem *regs = thrd->dmac->pinfo->base;
	int regs = dma_regs_base;
	
	u32 val;

	val = (insn[0] << 16) | (insn[1] << 24);

	//if (!as_manager) {
	if (!1) {
		val |= (1 << 0);
		//val |= (thrd->id << 8); /* Channel Number */
		val |= (0 << 8); /* Channel Number */
	}
	writel(val, regs + DBGINST0);

//	val = *((u32 *)&insn[2]);
	val = insn[2] << 0;
	val |= insn[3] << 8;
	val |= insn[4] << 16;
	val |= insn[5] << 24;
	writel(val, regs + DBGINST1);

#if 0
	/* If timed out due to halted state-machine */
	if (_until_dmac_idle(thrd)) {
		dev_err(thrd->dmac->pinfo->dev, "DMAC halted!\n");
		return;
	}
#endif
	/* Get going */
	writel(0, regs + DBGCMD);
}

static inline u32 _emit_STP(unsigned dry_run, u8 buf[],
		enum pl330_cond cond, u8 peri)
{
	if (dry_run)
		return SZ_DMASTP;

	buf[0] = CMD_DMASTP;

	if (cond == BURST)
		buf[0] |= (1 << 1);

	peri &= 0x1f;
	peri <<= 3;
	buf[1] = peri;

	PL330_DBGCMD_DUMP(SZ_DMASTP, "\tDMASTP%c %u\n",
		cond == SINGLE ? 'S' : 'B', peri >> 3);

	return SZ_DMASTP;
}


static inline u32 _emit_WFP(unsigned dry_run, u8 buf[],
		enum pl330_cond cond, u8 peri)
{
	if (dry_run)
		return SZ_DMAWFP;

	buf[0] = CMD_DMAWFP;

	if (cond == SINGLE)
		buf[0] |= (0 << 1) | (0 << 0);
	else if (cond == BURST)
		buf[0] |= (1 << 1) | (0 << 0);
	else
		buf[0] |= (0 << 1) | (1 << 0);

	peri &= 0x1f;
	peri <<= 3;
	buf[1] = peri;

	PL330_DBGCMD_DUMP(SZ_DMAWFP, "\tDMAWFP%c %u\n",
		cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'P'), peri >> 3);

	return SZ_DMAWFP;
}

static inline int _ldst_memtomem_nobarrier(unsigned dry_run, u8 buf[],
		const struct _xfer_spec *pxs, int cyc)
{
	int off = 0;

	while (cyc--) {
		off += _emit_LD(dry_run, &buf[off], ALWAYS);
		off += _emit_ST(dry_run, &buf[off], ALWAYS);
	}

	return off;
}

static int _bursts(unsigned dry_run, u8 buf[],
		const struct _xfer_spec *pxs, int cyc)
{
	int off = 0;

#if 0
	switch (pxs->r->rqtype) {
	case MEMTODEV:
		off += _ldst_memtodev(dry_run, &buf[off], pxs, cyc);
		break;
	case DEVTOMEM:
		off += _ldst_devtomem(dry_run, &buf[off], pxs, cyc);
		break;
	case MEMTOMEM:
		off += _ldst_memtomem(dry_run, &buf[off], pxs, cyc);
		break;
	case MEMTOMEM_NOBARRIER:
		off += _ldst_memtomem_nobarrier(dry_run, &buf[off], pxs, cyc);
		break;
#endif
	off += _ldst_memtomem_nobarrier(dry_run, &buf[off], pxs, cyc);

	return off;
}


/* Returns bytes consumed and updates bursts */
static inline int _loop(unsigned dry_run, u8 buf[],
		unsigned long *bursts, 
		const struct _xfer_spec *pxs)		
{
	int cyc, cycmax, szlp, szlpend, szbrst, off;
	unsigned lcnt0, lcnt1, ljmp0, ljmp1;
	struct _arg_LPEND lpend;

	/* Max iterations possibile in DMALP is 256 */
	if (*bursts >= 256*256) {
		lcnt1 = 256;
		lcnt0 = 256;
		//cyc = *bursts / lcnt1 / lcnt0;
		cyc = *bursts / 256 / 256;
	} else if (*bursts > 256) {
		lcnt1 = 256;
		//lcnt0 = *bursts / lcnt1;
		lcnt0 = *bursts / 256;
		cyc = 1;
	} else {
		lcnt1 = *bursts;
		lcnt0 = 0;
		cyc = 1;
	}

	szlp = _emit_LP(1, buf, 0, 0);
	szbrst = _bursts(1, buf, pxs, 1);

	lpend.cond = ALWAYS;
	lpend.forever = false;
	lpend.loop = 0;
	lpend.bjump = 0;
	szlpend = _emit_LPEND(1, buf, &lpend);

	if (lcnt0) {
		szlp *= 2;
		szlpend *= 2;
	}

	/*
	 * Max bursts that we can unroll due to limit on the
	 * size of backward jump that can be encoded in DMALPEND
	 * which is 8-bits and hence 255
	 */
	//cycmax = (255 - (szlp + szlpend)) / szbrst;
	// DMALP: 2bytes    DMALPEND: 2bytes
	// szbrst = DMALD: 1byte + DMAST: 1byte
	cycmax = (255 - (2*2 + 2*2)) / 2;

	cyc = (cycmax < cyc) ? cycmax : cyc;

	off = 0;

	if (lcnt0) {
		off += _emit_LP(dry_run, &buf[off], 0, lcnt0);
		ljmp0 = off;
	}

	off += _emit_LP(dry_run, &buf[off], 1, lcnt1);
	ljmp1 = off;

	off += _bursts(dry_run, &buf[off], pxs, cyc);

	lpend.cond = ALWAYS;
	lpend.forever = false;
	lpend.loop = 1;
	lpend.bjump = off - ljmp1;
	off += _emit_LPEND(dry_run, &buf[off], &lpend);

	if (lcnt0) {
		lpend.cond = ALWAYS;
		lpend.forever = false;
		lpend.loop = 0;
		lpend.bjump = off - ljmp0;
		off += _emit_LPEND(dry_run, &buf[off], &lpend);
	}

	*bursts = lcnt1 * cyc;
	if (lcnt0)
		*bursts *= lcnt0;

	return off;
}

static inline int _setup_loops(unsigned dry_run, u8 buf[],
		const struct _xfer_spec *pxs)
{
	//struct pl330_xfer *x = pxs->x;
	//u32 ccr = pxs->ccr;
	unsigned long c, bursts = pxs->size;		// BYTE_TO_BURST(x->bytes, ccr);
	int off = 0;

	while (bursts) {
		c = bursts;
		off += _loop(dry_run, &buf[off], &c, pxs);		
		bursts -= c;
	}

	return off;
}

int dma_mem_transfer(int src, int dst, int size)
{
	u32 ccr = 0;
	int off;
	//int ljmp;
	unsigned dry_run;
	//struct _arg_LPEND lpend;
	struct _arg_GO go;
	u8 insn[6] = {0, 0, 0, 0, 0, 0};
	int ev = 0;
	//u8 buf[256];
	u8 buf[60000];
	//int cyc, i;
	//unsigned lcnt0, lcnt1, ljmp0, ljmp1;
	//unsigned lcnt1, ljmp1;	
	struct _xfer_spec xs;	

	ccr |= CC_SRCINC;
	ccr |= CC_DSTINC;

	ccr |= (0 << CC_SRCBRSTSIZE_SHFT);
	ccr |= (0 << CC_DSTBRSTSIZE_SHFT);
	
	ccr |= (((1 - 1) & 0xf) << CC_SRCBRSTLEN_SHFT);
	ccr |= (((1 - 1) & 0xf) << CC_DSTBRSTLEN_SHFT);

	ccr |= (1 << CC_SRCCCTRL_SHFT);
	ccr |= (1 << CC_DSTCCTRL_SHFT);

	off = 0;
	dry_run = 0;

	PL330_DBGMC_START(off);

	off += _emit_MOV(dry_run, &buf[off], CCR, ccr);

	off += _emit_MOV(dry_run, &buf[off], SAR, src);
	off += _emit_MOV(dry_run, &buf[off], DAR, dst);
	/* Setup Loop(s) */
	xs.size = size;
	off += _setup_loops(dry_run, &buf[off], &xs);

	off +=_emit_SEV(dry_run, &buf[off], ev);
	off += _emit_END(dry_run, &buf[off]);

	go.chan = 0;
	go.addr = (int)buf;
	go.ns = 0;	// 1 is non-secure mode, and DMA_mem could be in non-secure mode
	_emit_GO(0, insn, &go);

	_execute_DBGINSN(DMA_MEM, insn, true);

	return 0;
}

#if 0
int dma_mem_transfer(int src, int dst, int size)
{
	u32 ccr = 0;
	int off;
	//int ljmp;
	unsigned dry_run;
	struct _arg_LPEND lpend;
	struct _arg_GO go;
	u8 insn[6] = {0, 0, 0, 0, 0, 0};
	int ev = 0;
	//u8 buf[256];
	u8 buf[60000];
	int cyc, i;
	//unsigned lcnt0, lcnt1, ljmp0, ljmp1;
	unsigned lcnt1, ljmp1;

	ccr |= CC_SRCINC;
	ccr |= CC_DSTINC;

	//if (rqc->nonsecure)
	//	ccr |= CC_SRCNS | CC_DSTNS;		// DMA_peri must be non-secure

	ccr |= (0 << CC_SRCBRSTSIZE_SHFT);
	ccr |= (0 << CC_DSTBRSTSIZE_SHFT);
	
	ccr |= (((1 - 1) & 0xf) << CC_SRCBRSTLEN_SHFT);
	ccr |= (((1 - 1) & 0xf) << CC_DSTBRSTLEN_SHFT);

	ccr |= (1 << CC_SRCCCTRL_SHFT);
	ccr |= (1 << CC_DSTCCTRL_SHFT);

	off = 0;
	dry_run = 0;

	PL330_DBGMC_START(off);

	off += _emit_MOV(dry_run, &buf[off], CCR, ccr);

	off += _emit_MOV(dry_run, &buf[off], SAR, src);
	off += _emit_MOV(dry_run, &buf[off], DAR, dst);
	
#if 0
	off += _emit_LP(dry_run, &buf[off], 0,  size);
	ljmp = off;

	off += _emit_LD(dry_run, &buf[off], ALWAYS);
	off += _emit_ST(dry_run, &buf[off], ALWAYS);

	lpend.cond = SINGLE;
	lpend.forever = 0;
	lpend.loop = 0;		// lc0
	lpend.bjump = off - ljmp;
	off += _emit_LPEND(dry_run, &buf[off], &lpend);
#endif	

#if 1
//	off += _emit_LP(dry_run, &buf[off], 0,  0);
//	ljmp0 = off;
	
	lcnt1 = 256;
	cyc = 1;

//	for (i = 0; i < *bursts/256; i++) {
	for (i = 0; i < size/256; i++) {
		off += _emit_LP(dry_run, &buf[off], 1, lcnt1);
		ljmp1 = off;

	//	off += _bursts(dry_run, &buf[off], pxs, cyc);
		off += _bursts(dry_run, &buf[off], 0, cyc);

		lpend.cond = ALWAYS;
		lpend.forever = false;
		lpend.loop = 1;
		lpend.bjump = off - ljmp1;
		off += _emit_LPEND(dry_run, &buf[off], &lpend);
	}
	off +=_emit_SEV(dry_run, &buf[off], ev);

	lpend.cond = ALWAYS;
	lpend.forever = false;
	lpend.loop = 0;
//	lpend.bjump = off - ljmp0;
//	off += _emit_LPEND(dry_run, &buf[off], &lpend);
#endif

	//printbuf(buf, off);

	off +=_emit_SEV(dry_run, &buf[off], ev);
	off += _emit_END(dry_run, &buf[off]);

	go.chan = 0;
	go.addr = (int)buf;
	go.ns = 0;	// 1 is non-secure mode, and DMA_mem could be in non-secure mode
	_emit_GO(0, insn, &go);

	_execute_DBGINSN(DMA_MEM, insn, true);

	return 0;
}
#endif

int dma_peri_transfer(int src, int dst, int peri, int size)
{
	u32 ccr = 0;
	int off;
	int ljmp;
	unsigned dry_run;
	struct _arg_LPEND lpend;
	struct _arg_GO go;
	u8 insn[6] = {0, 0, 0, 0, 0, 0};
	int ev = 0;
	u8 buf[256];

	ccr |= CC_SRCINC;
//	ccr |= CC_DSTINC;

	//if (rqc->nonsecure)
		ccr |= CC_SRCNS | CC_DSTNS;		// DMA_peri must be non-secure

	ccr |= (0 << CC_SRCBRSTSIZE_SHFT);
	ccr |= (0 << CC_DSTBRSTSIZE_SHFT);
	
	ccr |= (((1 - 1) & 0xf) << CC_SRCBRSTLEN_SHFT);
	ccr |= (((1 - 1) & 0xf) << CC_DSTBRSTLEN_SHFT);

	ccr |= (1 << CC_SRCCCTRL_SHFT);
	ccr |= (1 << CC_DSTCCTRL_SHFT);

	off = 0;
	dry_run = 0;

	PL330_DBGMC_START(off);

	off += _emit_MOV(dry_run, &buf[off], CCR, ccr);
	off += _emit_FLUSHP(dry_run, &buf[off], peri);

	off += _emit_MOV(dry_run, &buf[off], SAR, src);
	off += _emit_MOV(dry_run, &buf[off], DAR, dst);

	off += _emit_LP(dry_run, &buf[off], 0,  size);
	ljmp = off;

	off += _emit_WFP(dry_run, &buf[off], SINGLE, peri);
	off += _emit_LD(dry_run, &buf[off], ALWAYS);
	off += _emit_STP(dry_run, &buf[off], SINGLE, peri);	// UART0_TX = 1
	off += _emit_FLUSHP(dry_run, &buf[off], peri);
	
	lpend.cond = SINGLE;
	lpend.forever = 0;
	lpend.loop = 0;		// lc0
	lpend.bjump = off - ljmp;
	off += _emit_LPEND(dry_run, &buf[off], &lpend);

	off +=_emit_SEV(dry_run, &buf[off], ev);
	off += _emit_END(dry_run, &buf[off]);

	go.chan = 0;
	go.addr = (int)buf;
	go.ns = 1;	// 1 is non-secure mode, and DMA_peri must be non-secure
	_emit_GO(0, insn, &go);

	_execute_DBGINSN(DMA_PERI, insn, true);

	return 0;
}

#if 0
int dma_test(void)
{
	unsigned dry_run;
	int ev;

	int off;
	unsigned ljmp;
	struct _arg_LPEND lpend;

	struct _arg_GO go;
	u8 insn[6] = {0, 0, 0, 0, 0, 0};

	dry_run = 0;
	off = 0;

	PL330_DBGMC_START(off);
	u32 ccr = _prepare_ccr();

	off += _emit_MOV(dry_run, &buf[off], CCR, ccr);
	off += _emit_FLUSHP(dry_run, &buf[off], 1);

	*(volatile char *)0xd0034000 = 'a';
	*(volatile char *)0xd0034001 = 'b';
	*(volatile char *)0xd0034002 = 'c';
	*(volatile char *)0xd0034003 = 'd';
	*(volatile char *)0xd0034004 = 'e';
	*(volatile char *)0xd0034005 = 'f';
	*(volatile char *)0xd0034006 = 'g';
	*(volatile char *)0x20000000 = 'A';
	*(volatile char *)0x20000001 = 'B';
	*(volatile char *)0x20000002 = 'C';
	*(volatile char *)0x20000003 = 'D';
	*(volatile char *)0x20000004 = 'E';
	//off += _emit_MOV(dry_run, &buf[off], SAR, 0xd0020000);
	//off += _emit_MOV(dry_run, &buf[off], SAR, 0x20000000);
	off += _emit_MOV(dry_run, &buf[off], SAR, 0xd0034000);
	//off += _emit_MOV(dry_run, &buf[off], DAR, 0xd0028000);
	off += _emit_MOV(dry_run, &buf[off], DAR, 0xe2900020);

	off += _emit_LP(dry_run, &buf[off], 0,  4);
	ljmp = off;

	printbuf(buf, off);

	off += _emit_WFP(dry_run, &buf[off], SINGLE, 1);
	off += _emit_LD(dry_run, &buf[off], ALWAYS);
	off += _emit_STP(dry_run, &buf[off], SINGLE, 1);	// UART0_TX
	off += _emit_FLUSHP(dry_run, &buf[off], 1);
	
	lpend.cond = SINGLE;
	lpend.forever = 0;
	lpend.loop = 0;		// lc0
	lpend.bjump = off - ljmp;
	off += _emit_LPEND(dry_run, &buf[off], &lpend);

	off +=_emit_SEV(dry_run, &buf[off], ev);
	
	off += _emit_END(dry_run, &buf[off]);

	printbuf(buf, off);

	go.chan = 0;
	go.addr = buf;
	go.ns = 1;	// 1 is non-secure mode, and DMA_peri must be non-secure
	_emit_GO(0, insn, &go);

	_execute_DBGINSN(0, insn, true);

	//delay();
	printf("execute ok\n");

	return 0;
} 

int dma_test_bad(void)
{
	unsigned dry_run;
	unsigned long *bursts;
	const struct _xfer_spec *pxs;
	int ev;

	int cyc, off;
	unsigned lcnt0, lcnt1, ljmp0, ljmp1, ljmpfe;
	struct _arg_LPEND lpend;

	off = 0;
	ljmpfe = off;
	int i, j;
	lcnt1 = 256;
	cyc = 1;

	struct _arg_GO go;
	unsigned ns;
	u8 insn[6] = {0, 0, 0, 0, 0, 0};

	dry_run = 0;
	PL330_DBGMC_START(off);
	// simplified by limingth
	u32 ccr = _prepare_ccr();

	/* DMAMOV CCR, ccr */
	off += _emit_MOV(dry_run, &buf[off], CCR, ccr);

	/* DMAFLUSHP 0 */
	off += _emit_FLUSHP(dry_run, &buf[off], 0);

	/* DMAMOV SAR, x->src_addr */
	off += _emit_MOV(dry_run, &buf[off], SAR, 0xd0020000);

	/* DMAMOV DAR, x->dst_addr */
	//off += _emit_MOV(dry_run, &buf[off], DAR, 0xd0028000);
	off += _emit_MOV(dry_run, &buf[off], DAR, 0xe2900020);

	//off += _emit_LP(dry_run, &buf[off], 0,  pxs->r->autoload);
	//off += _emit_LP(dry_run, &buf[off], 0,  1);	
	off += _emit_LD(dry_run, &buf[off], ALWAYS);
	off += _emit_STP(dry_run, &buf[off], SINGLE, 1);	// UART0_TX

	off += _emit_LP(dry_run, &buf[off], 0,  4);
	ljmp1 = off;

	printbuf(buf, off);

#if 1
	off += _emit_WFP(dry_run, &buf[off], SINGLE, 1);

	off += _emit_LD(dry_run, &buf[off], ALWAYS);
//	off += _emit_ST(dry_run, &buf[off], ALWAYS);
	off += _emit_STP(dry_run, &buf[off], SINGLE, 1);	// UART0_TX

	//off += _emit_LD(dry_run, &buf[off], SINGLE);
	//off += _emit_STP(dry_run, &buf[off], SINGLE, 1);	// UART0_TX
	//off += _emit_LD(dry_run, &buf[off], SINGLE);
	//off += _emit_STP(dry_run, &buf[off], SINGLE, 1);	// UART0_TX

//	off += _emit_STP(dry_run, &buf[off], SINGLE, 1);	// UART0_TX
	off += _emit_FLUSHP(dry_run, &buf[off], 1);
	
	//lpend.cond = ALWAYS;
	lpend.cond = SINGLE;
	lpend.forever = 0;
	lpend.loop = 0;
	lpend.bjump = off - ljmp1;
	off += _emit_LPEND(dry_run, &buf[off], &lpend);

	printbuf(buf, off);
#endif

	off +=_emit_SEV(dry_run, &buf[off], ev);
	
	/* DMAEND */
	off += _emit_END(dry_run, &buf[off]);

	ns = 0;
	go.chan = 0;
	go.addr = buf;
	go.ns = ns;
	_emit_GO(0, insn, &go);

	_execute_DBGINSN(0, insn, true);
	printf("execute ok\n");

	return 0;
} 
#endif

#if 0
int dma_test(void)
{
	unsigned dry_run;
	unsigned long *bursts;
	const struct _xfer_spec *pxs;
	int ev;

	int cyc, off;
	unsigned lcnt0, lcnt1, ljmp0, ljmp1, ljmpfe;
	struct _arg_LPEND lpend;
	u8 buf[256];

	off = 0;
	ljmpfe = off;
	int i, j;
	lcnt1 = 256;
	cyc = 1;

	struct _arg_GO go;
	unsigned ns;
	u8 insn[6] = {0, 0, 0, 0, 0, 0};

	PL330_DBGMC_START(off);
	printf("ccr \n");
	// simplified by limingth
	u32 ccr = _prepare_ccr();
	printbuf(buf, off);

	printf("mov CCR, ccr\n");
	/* DMAMOV CCR, ccr */
	off += _emit_MOV(dry_run, &buf[off], CCR, ccr);
	printbuf(buf, off);

#if 1
	printf("flush p\n");
	/* DMAFLUSHP 0 */
	off += _emit_FLUSHP(dry_run, &buf[off], 0);
	printbuf(buf, off);

	printf("mov sar\n");
	/* DMAMOV SAR, x->src_addr */
	//off += _emit_MOV(dry_run, &buf[off], SAR, pxs->x->src_addr);
	off += _emit_MOV(dry_run, &buf[off], SAR, 0xd0020000);
	printbuf(buf, off);

	printf("mov dar\n");
	/* DMAMOV DAR, x->dst_addr */
	//off += _emit_MOV(dry_run, &buf[off], DAR, pxs->x->dst_addr);
	off += _emit_MOV(dry_run, &buf[off], DAR, 0xd0028000);
	//off += _emit_MOV(dry_run, &buf[off], DAR, 0x24000000);

	printf("lp 32\n");
	//off += _emit_LP(dry_run, &buf[off], 0,  pxs->r->autoload);
	off += _emit_LP(dry_run, &buf[off], 0,  32);
	ljmp1 = off;

	printf("ld always\n");
	off += _emit_LD(dry_run, &buf[off], ALWAYS);
	printf("st always\n");
	off += _emit_ST(dry_run, &buf[off], ALWAYS);
	printf("ld always\n");
	off += _emit_LD(dry_run, &buf[off], ALWAYS);
	printf("st always\n");
	off += _emit_ST(dry_run, &buf[off], ALWAYS);
	printf("ld always\n");
	off += _emit_LD(dry_run, &buf[off], ALWAYS);
	printf("st always\n");
	off += _emit_ST(dry_run, &buf[off], ALWAYS);
	printf("ld always\n");
	off += _emit_LD(dry_run, &buf[off], ALWAYS);
	printf("st always\n");
	off += _emit_ST(dry_run, &buf[off], ALWAYS);

	printf("lpend begin\n");
	lpend.cond = ALWAYS;
	lpend.forever = false;
	lpend.forever = 0;
	lpend.loop = 1;
	lpend.bjump = off - ljmp1;
	off += _emit_LPEND(dry_run, &buf[off], &lpend);

	printf("dma lpend ok\n");

	off +=_emit_SEV(dry_run, &buf[off], ev);
	printf("sev ok\n");
	
	/* DMAEND */
	off += _emit_END(dry_run, &buf[off]);
	printf("end ok\n");
	printbuf(buf, off);
#endif

	ns = 0;
	go.chan = 0;
	go.addr = buf;
	go.ns = ns;
	_emit_GO(0, insn, &go);
	printf("go ok\n");
	printf("buf addr is 0x%x\n", buf);
	printbuf(insn, 6);

	_execute_DBGINSN(0, insn, true);
//	_execute_DBGINSN(0, buf, true);
	printf("execute ok\n");
#if 0
	for (i = 0; i < *bursts/256; i++) {
		off += _emit_LP(dry_run, &buf[off], 1, lcnt1);
		ljmp1 = off;

		//off += _bursts(dry_run, &buf[off], pxs, cyc);
		off += _emit_LD(dry_run, &buf[off], ALWAYS);
		off += _emit_ST(dry_run, &buf[off], ALWAYS);

		lpend.cond = ALWAYS;
		lpend.forever = false;
		lpend.forever = 0;
		lpend.loop = 1;
		lpend.bjump = off - ljmp1;
		off += _emit_LPEND(dry_run, &buf[off], &lpend);
	}
#endif
#if 0
	lpend.cond = ALWAYS;
	lpend.forever = false;
	lpend.loop = 0;
	lpend.bjump = off - ljmp0;
	off += _emit_LPEND(dry_run, &buf[off], &lpend);

	lpend.cond = ALWAYS;
	lpend.forever = true;
	lpend.loop = 1;
	lpend.bjump = off - ljmpfe;
	off +=  _emit_LPEND(dry_run, &buf[off], &lpend);
#endif
	return off;
}

#endif
