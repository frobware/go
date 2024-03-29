// From asm.c asmout.c optab.c span.c sub.c mod.c

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <link.h>
#include "../cmd/7l/7.out.h"

enum
{
	FuncAlign = 16
};

typedef	struct	Mask	Mask;
typedef	struct	Optab	Optab;
typedef	struct	Oprang	Oprang;
typedef	uchar	Opcross[32][2][32];

struct	Optab
{
	ushort	as;
	uchar	a1;
	uchar	a2;
	uchar	a3;
	char	type;
	char	size;
	char	param;
	char	flag;
};
struct	Oprang
{
	Optab*	start;
	Optab*	stop;
};
struct	Mask
{
	uchar	s;
	uchar	e;
	uchar	r;
	uvlong	v;
};

static Optab *badop;
static Oprang	oprange[ALAST];
static Opcross	opcross[8];
static uchar	repop[ALAST];
static	char	xcmp[C_NCLASS][C_NCLASS];

static void checkpool(Link*, Prog*, int);
static void flushpool(Link*, Prog*, int);
static void addpool(Link*, Prog*, Addr*);
static int32 regoff(Link*, Addr*);
static int ispcdisp(int32);
static int isaddcon(vlong);
static int isbitcon(uvlong);
static int constclass(vlong);
static vlong offsetshift(Link*, vlong, int);
static int movcon(vlong);
static int aclass(Link*, Addr*);
static Optab *oplook(Link*, Prog*);
static int cmp(int, int);
static int ocmp(const void*, const void*);
void buildop(Link*);
int chipfloat7(Link*, float64);
void asmout(Link*, Prog*, Optab*, int32 *);
static int32 oprrr(Link*, int);
static int32 opirr(Link*, int);
static int32 opbit(Link*, int);
static int32 opxrrr(Link*, int);
static int32 opimm(Link*, int);
static vlong brdist(Link*, Prog*, int, int, int);
static int32 opbra(Link*, int);
static int32 opbrr(Link*, int);
static int32 op0(Link*, int);
static int32 opload(Link*, int);
static int32 opstore(Link*, int);
static int32 olsr12u(Link*, int32, int32, int, int);
static int32 opldr12(Link*, int);
static int32 opstr12(Link*, int);
static int32 olsr9s(Link*, int32, int32, int, int);
static int32 opldr9(Link*, int);
static int32 opstr9(Link *, int);
static int32 opldrpp(Link*, int);
static int32 olsxrr(Link*, int, int, int, int);
static int32 oaddi(int32, int32, int, int);
static int32 omovlit(Link*, int, Prog*, Addr*, int);
static int32 opbfm(Link*, int, int, int, int, int);
static int32 opextr(Link*, int, int32, int, int, int);
static int movesize(int);
static void prasm(Prog*);
static Mask* findmask(uvlong v);

#define	S32	(0U<<31)
#define	S64	(1U<<31)
#define	Rm(X)	(((X)&31)<<16)
#define	Rn(X)	(((X)&31)<<5)
#define	Rd(X)	(((X)&31)<<0)
#define	Sbit	(1U<<29)

#define	OPDP2(x)	(0<<30 | 0 << 29 | 0xd6<<21 | (x)<<10)
#define	OPDP3(sf,op54,op31,o0)	((sf)<<31 | (op54)<<29 | 0x1B<<24 | (op31)<<21 | (o0)<<15)
#define	OPBcc(x)	(0x2A<<25 | 0<<24 | 0<<4 | ((x)&15))
#define	OPBLR(x)	(0x6B<<25 | 0<<23 | (x)<<21 | 0x1F<<16 | 0<<10)	/* x=0, JMP; 1, CALL; 2, RET */
#define	SYSOP(l,op0,op1,crn,crm,op2,rt)	(0x354<<22 | (l)<<21 | (op0)<<19 | (op1)<<16 | (crn)<<12 | (crm)<<8 | (op2)<<5 | (rt))
#define	SYSHINT(x)	SYSOP(0,0,3,2,0,(x),0x1F)

#define	LDSTR12U(sz,v,opc)	((sz)<<30 | 7<<27 | (v)<<26 | 1<<24 | (opc)<<22)
#define	LDSTR9S(sz,v,opc)	((sz)<<30 | 7<<27 | (v)<<26 | 0<<24 | (opc)<<22)
#define	LD2STR(o)	((o) & ~(3<<22))
#define	LDSTX(sz,o2,l,o1,o0)	((sz)<<30 | 0x8<<24 | (o2)<<23 | (l)<<22 | (o1)<<21 | (o0)<<15)

#define	FPCMP(m,s,type,op,op2)	((m)<<31 | (s)<<29 | 0x1E<<24 | (type)<<22 | 1<<21 | (op)<<14 | 8<<10 | (op2))
#define	FPCCMP(m,s,type,op)	((m)<<31 | (s)<<29 | 0x1E<<24 | (type)<<22 | 1<<21 | 1<<10 | (op)<<4)
#define	FPOP1S(m,s,type,op)	((m)<<31 | (s)<<29 | 0x1E<<24 | (type)<<22 | 1<<21 | (op)<<15 | 0x10<<10)
#define	FPOP2S(m,s,type,op)	((m)<<31 | (s)<<29 | 0x1E<<24 | (type)<<22 | 1<<21 | (op)<<12 | 2<<10)
#define	FPCVTI(sf,s,type,rmode,op)	((sf)<<31 | (s)<<29 | 0x1E<<24 | (type)<<22 | 1<<21 | (rmode)<<19 | (op)<<16 | 0<<10)
#define	FPCVTF(sf,s,type,rmode,op,scale)	((sf)<<31 | (s)<<29 | 0x1E<<24 | (type)<<22 | 0<<21 | (rmode)<<19 | (op)<<16 | (scale)<<10)
#define	ADR(p,o,rt)	((p)<<31 | ((o)&3)<<29 | (0x10<<24) | (((o>>2)&0x7FFFF)<<5) | (rt))

#define	LSL0_32	(2<<13)
#define	LSL0_64	(3<<13)

#define	OPBIT(x)	(1<<30 | 0<<29 | 0xD6<<21 | 0<<16 | (x)<<10)

static Prog zprg = {
	.as = AGOK,
	.reg = NREG,
	.from = {
		.name = D_NONE,
		.type = D_NONE,
		.reg = NREG,
	},
	.from3 = {
		.name = D_NONE,
		.type = D_NONE,
		.reg = NREG,
	},
	.to = {
		.name = D_NONE,
		.type = D_NONE,
		.reg = NREG,
	},
};

enum
{
	LFROM	= 1<<0,
	LTO	= 1<<1,
	LPOOL	= 1<<2,
};

static Optab optab[] = {
        /* struct Optab:
          OPCODE,       from, prog->reg, to,             type,size,param,flag */
	{ ATEXT,	C_LEXT,	C_NONE,	C_LCON, 	 0, 0, 0 },
	{ ATEXT,	C_LEXT,	C_REG,	C_LCON, 	 0, 0, 0 },
	{ ATEXT,	C_ADDR,	C_NONE,	C_LCON, 	 0, 0, 0 },
	{ ATEXT,	C_ADDR,	C_REG,	C_LCON, 	 0, 0, 0 },

	/* arithmetic operations */
	{ AADD,		C_REG,	C_REG,	C_REG,		 1, 4, 0 },
	{ AADD,		C_REG,	C_NONE,	C_REG,		 1, 4, 0 },
	{ AADC,		C_REG,	C_REG,	C_REG,		 1, 4, 0 },
	{ AADC,		C_REG,	C_NONE,	C_REG,		 1, 4, 0 },
	{ ANEG,		C_REG,	C_NONE,	C_REG,		25, 4, 0 },
	{ ANGC,		C_REG,	C_NONE,	C_REG,		17, 4, 0 },
	{ ACMP,		C_REG,	C_RSP,	C_NONE,		 1, 4, 0 },

	{ AADD,		C_ADDCON,	C_RSP,	C_RSP,		 2, 4, 0 },
	{ AADD,		C_ADDCON,	C_NONE,	C_RSP,		 2, 4, 0 },
	{ ACMP,		C_ADDCON,	C_RSP,	C_NONE,		 2, 4, 0 },

	{ AADD,		C_LCON,	C_REG,	C_REG,		13, 8, 0,	LFROM },
	{ AADD,		C_LCON,	C_NONE,	C_REG,		13, 8, 0,	LFROM },
	{ ACMP,		C_LCON,	C_REG,	C_NONE,		13, 8, 0,	LFROM },

	{ AADD,		C_SHIFT,C_REG,	C_REG,		 3, 4, 0 },
	{ AADD,		C_SHIFT,C_NONE,	C_REG,		 3, 4, 0 },
	{ AMVN,		C_SHIFT,C_NONE,	C_REG,		 3, 4, 0 },
	{ ACMP,		C_SHIFT,C_REG,	C_NONE,		 3, 4, 0 },
	{ ANEG,		C_SHIFT,C_NONE,	C_REG,		26, 4, 0 },

	{ AADD,		C_REG,	C_RSP,	C_RSP,		27, 4, 0 },
	{ AADD,		C_REG,	C_NONE,	C_RSP,		27, 4, 0 },
	{ AADD,		C_EXTREG,C_RSP,	C_RSP,		27, 4, 0 },
	{ AADD,		C_EXTREG,C_NONE,	C_RSP,		27, 4, 0 },
	{ AMVN,		C_EXTREG,C_NONE,	C_RSP,		27, 4, 0 },
	{ ACMP,		C_EXTREG,C_RSP,	C_NONE,		27, 4, 0 },

	{ AADD,		C_REG,	C_REG,	C_REG,		 1, 4, 0 },
	{ AADD,		C_REG,	C_NONE,	C_REG,		 1, 4, 0 },

	/* logical operations */
	{ AAND,		C_REG,	C_REG,	C_REG,		 1, 4, 0 },
	{ AAND,		C_REG,	C_NONE,	C_REG,		 1, 4, 0 },
	{ ABIC,		C_REG,	C_REG,	C_REG,		 1, 4, 0 },
	{ ABIC,		C_REG,	C_NONE,	C_REG,		 1, 4, 0 },

	{ AAND,		C_BITCON,	C_REG,	C_REG,		53, 4, 0 },
	{ AAND,		C_BITCON,	C_NONE,	C_REG,		53, 4, 0 },
	{ ABIC,		C_BITCON,	C_REG,	C_REG,		53, 4, 0 },
	{ ABIC,		C_BITCON,	C_NONE,	C_REG,		53, 4, 0 },

	{ AAND,		C_LCON,	C_REG,	C_REG,		28, 8, 0,	LFROM },
	{ AAND,		C_LCON,	C_NONE,	C_REG,		28, 8, 0,	LFROM },
	{ ABIC,		C_LCON,	C_REG,	C_REG,		28, 8, 0,	LFROM },
	{ ABIC,		C_LCON,	C_NONE,	C_REG,		28, 8, 0,	LFROM },

	{ AAND,		C_SHIFT,C_REG,	C_REG,		 3, 4, 0 },
	{ AAND,		C_SHIFT,C_NONE,	C_REG,		 3, 4, 0 },
	{ ABIC,		C_SHIFT,C_REG,	C_REG,		 3, 4, 0 },
	{ ABIC,		C_SHIFT,C_NONE,	C_REG,		 3, 4, 0 },

	{ AMOV,		C_RSP,	C_NONE,	C_RSP,		24, 4, 0 },
	{ AMVN,		C_REG,	C_NONE,	C_REG,		24, 4, 0 },
	{ AMOVB,		C_REG,	C_NONE,	C_REG,		45, 4, 0 },
	{ AMOVBU,	C_REG,	C_NONE,	C_REG,		45, 4, 0 },
	{ AMOVH,		C_REG,	C_NONE,	C_REG,		45, 4, 0 },	/* also MOVHU */
	{ AMOVW,		C_REG,	C_NONE,	C_REG,		45, 4, 0 },	/* also MOVWU */
	/* TO DO: MVN C_SHIFT */

	/* MOVs that become MOVK/MOVN/MOVZ/ADD/SUB/OR */
	{ AMOVW,		C_MOVCON,	C_NONE,	C_REG,		32, 4, 0 },
	{ AMOV,		C_MOVCON,	C_NONE,	C_REG,		32, 4, 0 },
//	{ AMOVW,		C_ADDCON,	C_NONE,	C_REG,		2, 4, 0 },
//	{ AMOV,		C_ADDCON,	C_NONE,	C_REG,		2, 4, 0 },
//	{ AMOVW,		C_BITCON,	C_NONE,	C_REG,		53, 4, 0 },
//	{ AMOV,		C_BITCON,	C_NONE,	C_REG,		53, 4, 0 },

	{ AMOVK,		C_LCON,	C_NONE,	C_REG,			33, 4, 0 },

	{ AMOV,	C_AECON,C_NONE,	C_REG,		 4, 4, REGSB },
	{ AMOV,	C_AACON,C_NONE,	C_REG,		 4, 4, REGSP },

	{ ASDIV,	C_REG,	C_NONE,	C_REG,		1, 4, 0 },
	{ ASDIV,	C_REG,	C_REG,	C_REG,		1, 4, 0 },

	{ AB,		C_NONE,	C_NONE,	C_SBRA,		 5, 4, 0 },
	{ ABL,	C_NONE,	C_NONE,	C_SBRA,		 5, 4, 0 },

	{ AB,		C_NONE,	C_NONE,	C_ZOREG,	 	6, 4, 0 },
	{ ABL,	C_NONE,	C_NONE,	C_REG,	 	6, 4, 0 },
	{ ABL,	C_NONE,	C_NONE,	C_ZOREG,	 	6, 4, 0 },
	{ ARET,	C_NONE,	C_NONE,	C_REG,		6, 4, 0 },
	{ ARET,	C_NONE, C_NONE, C_ZOREG,		6, 4, 0 },

	{ AADRP,	C_SBRA,	C_NONE,	C_REG,		60, 4, 0 },
	{ AADR,	C_SBRA,	C_NONE,	C_REG,		61, 4, 0 },

	{ ABFM,	C_LCON, C_REG, C_REG,		42, 4, 0 },
	{ ABFI,	C_LCON, C_REG, C_REG,		43, 4, 0 },

	{ AEXTR,	C_LCON, C_REG, C_REG,		44, 4, 0 },
	{ ASXTB,	C_REG,	C_NONE,	C_REG,	45, 4, 0 },
	{ ACLS,	C_REG,	C_NONE,	C_REG,	46, 4, 0 },

	{ ABEQ,	C_NONE,	C_NONE,	C_SBRA,		 7, 4, 0 },

	{ ALSL,		C_LCON,	C_REG,	C_REG,		 8, 4, 0 },
	{ ALSL,		C_LCON,	C_NONE,	C_REG,		 8, 4, 0 },

	{ ALSL,		C_REG,	C_NONE,	C_REG,		 9, 4, 0 },
	{ ALSL,		C_REG,	C_REG,	C_REG,		 9, 4, 0 },

	{ ASVC,		C_NONE,	C_NONE,	C_LCON,		10, 4, 0 },
	{ ASVC,		C_NONE,	C_NONE,	C_NONE,		10, 4, 0 },

	{ ADWORD,	C_NONE,	C_NONE,	C_VCON,		11, 8, 0 },
	{ ADWORD,	C_NONE,	C_NONE,	C_LEXT,		11, 8, 0 },
	{ ADWORD,	C_NONE,	C_NONE,	C_ADDR,		11, 8, 0 },

	{ AWORD,	C_NONE,	C_NONE,	C_LCON,		14, 4, 0 },
	{ AWORD,	C_NONE,	C_NONE,	C_LEXT,		14, 4, 0 },
	{ AWORD,	C_NONE,	C_NONE,	C_ADDR,		14, 4, 0 },

	{ AMOVW,	C_LCON,	C_NONE,	C_REG,		12, 4, 0,	LFROM },
	{ AMOV,	C_LCON,	C_NONE,	C_REG,		12, 4, 0,	LFROM },

	{ AMOVW,	C_REG,	C_NONE,	C_ADDR,		64, 8, 0,	LTO },
	{ AMOVB,	C_REG,	C_NONE,	C_ADDR,		64, 8, 0,	LTO },
	{ AMOVBU,	C_REG,	C_NONE,	C_ADDR,		64, 8, 0,	LTO },
	{ AMOV,	C_REG,	C_NONE,	C_ADDR,		64, 8, 0,	LTO },
	{ AMOVW,	C_ADDR,	C_NONE,	C_REG,		65, 8, 0,	LFROM },
	{ AMOVBU,	C_ADDR,	C_NONE,	C_REG,		65, 8, 0,	LFROM },
	{ AMOV,	C_ADDR,	C_NONE,	C_REG,		65, 8, 0,	LFROM },

	{ AMUL,		C_REG,	C_REG,	C_REG,		15, 4, 0 },
	{ AMUL,		C_REG,	C_NONE,	C_REG,		15, 4, 0 },
	{ AMADD,		C_REG,	C_REG,	C_REG,		15, 4, 0 },

	{ AREM,		C_REG,	C_REG,	C_REG,		16, 8, 0 },
	{ AREM,		C_REG,	C_NONE,	C_REG,		16, 8, 0 },

	{ ACSEL,		C_COND,	C_REG,	C_REG,		18, 4, 0 },	/* from3 optional */
	{ ACSET,		C_COND,	C_NONE,	C_REG,		18, 4, 0 },

	{ ACCMN,		C_COND,	C_REG,	C_LCON,		19, 4, 0 },	/* from3 either C_REG or C_LCON */

	/* scaled 12-bit unsigned displacement store */

	{ AMOVB,	C_REG,	C_NONE,	C_SEXT1,		20, 4, REGSB },  // 
	{ AMOVB,	C_REG,	C_NONE,	C_UAUTO4K,	20, 4, REGSP },  // 
	{ AMOVB,	C_REG,	C_NONE,	C_UOREG4K,		20, 4, 0 },  // 
	{ AMOVBU,	C_REG,	C_NONE,	C_SEXT1,		20, 4, REGSB },  // 
	{ AMOVBU,	C_REG,	C_NONE,	C_UAUTO4K,	20, 4, REGSP },  // 
	{ AMOVBU,	C_REG,	C_NONE,	C_UOREG4K,		20, 4, 0 },  // 

	{ AMOVH,	C_REG,	C_NONE,	C_SEXT2,		20, 4, REGSB },  //
	{ AMOVH,	C_REG,	C_NONE,	C_UAUTO8K,	20,	4, REGSP },	//
	{ AMOVH,	C_REG,	C_NONE,	C_ZOREG,		20, 4, 0 },  // 
	{ AMOVH,	C_REG,	C_NONE,	C_UOREG8K,	20,	4, 0 },	//

	{ AMOVW,	C_REG,	C_NONE,	C_SEXT4,		20, 4, REGSB },  //
	{ AMOVW,	C_REG,	C_NONE,	C_UAUTO16K,	20,	4, REGSP },	//
	{ AMOVW,	C_REG,	C_NONE,	C_ZOREG,		20, 4, 0 },  // 
	{ AMOVW,	C_REG,	C_NONE,	C_UOREG16K,	20,	4, 0 },	//

	/* unscaled 9-bit signed displacement store */
	{ AMOVB,	C_REG,	C_NONE,	C_NSAUTO,	20, 4, REGSP },  // 
	{ AMOVB,	C_REG,	C_NONE,	C_NSOREG,	20, 4, 0 },  // 
	{ AMOVBU,	C_REG,	C_NONE,	C_NSAUTO,	20, 4, REGSP },  // 
	{ AMOVBU,	C_REG,	C_NONE,	C_NSOREG,	20, 4, 0 },  // 

	{ AMOVH,	C_REG,	C_NONE,	C_NSAUTO,	20,	4, REGSP },	//
	{ AMOVH,	C_REG,	C_NONE,	C_NSOREG,	20,	4, 0 },	//
	{ AMOVW,	C_REG,	C_NONE,	C_NSAUTO,	20,	4, REGSP },	//
	{ AMOVW,	C_REG,	C_NONE,	C_NSOREG,	20,	4, 0 },	//

	{ AMOV,	C_REG,	C_NONE,	C_SEXT8,		20, 4, REGSB },
	{ AMOV,	C_REG,	C_NONE,	C_UAUTO32K,	20,	4, REGSP },
	{ AMOV,	C_REG,	C_NONE,	C_ZOREG,		20, 4, 0 },
	{ AMOV,	C_REG,	C_NONE,	C_UOREG32K,	20,	4, 0 },

	{ AMOV,	C_REG,	C_NONE,	C_NSOREG,	20,	4, 0 },	//
	{ AMOV,	C_REG,	C_NONE,	C_NSAUTO,	20,	4, REGSP },	//

	/* short displacement load */

	{ AMOVB,	C_SEXT1,	C_NONE,	C_REG,		21, 4, REGSB },  // 
	{ AMOVB,	C_UAUTO4K,C_NONE,	C_REG,		21, 4, REGSP },  // 
	{ AMOVB,	C_NSAUTO,C_NONE,	C_REG,	21, 4, REGSP },  // 
	{ AMOVB,	C_ZOREG,C_NONE,	C_REG,		21, 4, 0 },  // 
	{ AMOVB,	C_UOREG4K,C_NONE,	C_REG,		21, 4, REGSP },  // 
	{ AMOVB,	C_NSOREG,C_NONE,	C_REG,	21, 4, REGSP },  // 

	{ AMOVBU,	C_SEXT1,	C_NONE,	C_REG,		21, 4, REGSB },  // 
	{ AMOVBU,	C_UAUTO4K,C_NONE,	C_REG,		21, 4, REGSP },  // 
	{ AMOVBU,	C_NSAUTO,C_NONE,	C_REG,	21, 4, REGSP },  // 
	{ AMOVBU,	C_ZOREG,C_NONE,	C_REG,		21, 4, 0 },  // 
	{ AMOVBU,	C_UOREG4K,C_NONE,	C_REG,		21, 4, REGSP },  // 
	{ AMOVBU,	C_NSOREG,C_NONE,	C_REG,	21, 4, REGSP },  // 

	{ AMOVH,	C_SEXT2,	C_NONE,	C_REG,		21, 4, REGSB },  // 
	{ AMOVH,	C_UAUTO8K,C_NONE,	C_REG,		21, 4, REGSP },  // 
	{ AMOVH,	C_NSAUTO,C_NONE,	C_REG,	21, 4, REGSP },  // 
	{ AMOVH,	C_ZOREG,C_NONE,	C_REG,		21, 4, 0 },  // 
	{ AMOVH,	C_UOREG8K,C_NONE,	C_REG,		21, 4, REGSP },  // 
	{ AMOVH,	C_NSOREG,C_NONE,	C_REG,	21, 4, REGSP },  // 

	{ AMOVW,	C_SEXT4,	C_NONE,	C_REG,		21, 4, REGSB },  // 
	{ AMOVW,	C_UAUTO16K,C_NONE,	C_REG,		21, 4, REGSP },  // 
	{ AMOVW,	C_NSAUTO,C_NONE,	C_REG,	21, 4, REGSP },  // 
	{ AMOVW,	C_ZOREG,C_NONE,	C_REG,		21, 4, 0 },  // 
	{ AMOVW,	C_UOREG16K,C_NONE,	C_REG,		21, 4, REGSP },  // 
	{ AMOVW,	C_NSOREG,C_NONE,	C_REG,	21, 4, REGSP },  // 

	{ AMOV,	C_SEXT8,	C_NONE,	C_REG,		21, 4, REGSB },
	{ AMOV,	C_UAUTO32K,C_NONE,	C_REG,		21, 4, REGSP },
	{ AMOV,	C_NSAUTO,C_NONE,	C_REG,	21, 4, REGSP },
	{ AMOV,	C_ZOREG,C_NONE,	C_REG,		21, 4, 0 },
	{ AMOV,	C_UOREG32K,C_NONE,	C_REG,		21, 4, REGSP },
	{ AMOV,	C_NSOREG,C_NONE,	C_REG,	21, 4, REGSP },

	/* long displacement store */
	{ AMOVB,	C_REG,	C_NONE,	C_LEXT,		30, 8, REGSB },  // 
	{ AMOVB,	C_REG,	C_NONE,	C_LAUTO,	30, 8, REGSP },  // 
	{ AMOVB,	C_REG,	C_NONE,	C_LOREG,	30, 8, 0 },  // 
	{ AMOVH,	C_REG,	C_NONE,	C_LEXT,		30, 8, REGSB },  // 
	{ AMOVH,	C_REG,	C_NONE,	C_LAUTO,	30, 8, REGSP },  // 
	{ AMOVH,	C_REG,	C_NONE,	C_LOREG,	30, 8, 0 },  // 
	{ AMOVW,	C_REG,	C_NONE,	C_LEXT,		30, 8, REGSB },  // 
	{ AMOVW,	C_REG,	C_NONE,	C_LAUTO,	30, 8, REGSP },  // 
	{ AMOVW,	C_REG,	C_NONE,	C_LOREG,	30, 8, 0 },  // 
	{ AMOV,	C_REG,	C_NONE,	C_LEXT,		30, 8, REGSB },  // 
	{ AMOV,	C_REG,	C_NONE,	C_LAUTO,	30, 8, REGSP },  // 
	{ AMOV,	C_REG,	C_NONE,	C_LOREG,	30, 8, 0 },  // 

	/* long displacement load */
	{ AMOVB,		C_LEXT,	C_NONE,	C_REG,		31, 8, REGSB },  // 
	{ AMOVB,		C_LAUTO,C_NONE,	C_REG,		31, 8, REGSP },  // 
	{ AMOVB,		C_LOREG,C_NONE,	C_REG,		31, 8, 0 },  // 
	{ AMOVB,		C_LOREG,C_NONE,	C_REG,		31, 8, 0 },	//
	{ AMOVH,		C_LEXT,	C_NONE,	C_REG,		31, 8, REGSB },  // 
	{ AMOVH,		C_LAUTO,C_NONE,	C_REG,		31, 8, REGSP },  // 
	{ AMOVH,		C_LOREG,C_NONE,	C_REG,		31, 8, 0 },  // 
	{ AMOVH,		C_LOREG,C_NONE,	C_REG,		31, 8, 0 },	//
	{ AMOVW,		C_LEXT,	C_NONE,	C_REG,		31, 8, REGSB },  // 
	{ AMOVW,		C_LAUTO,C_NONE,	C_REG,		31, 8, REGSP },  // 
	{ AMOVW,		C_LOREG,C_NONE,	C_REG,		31, 8, 0 },  // 
	{ AMOVW,		C_LOREG,C_NONE,	C_REG,		31, 8, 0 },	//
	{ AMOV,		C_LEXT,	C_NONE,	C_REG,		31, 8, REGSB },  // 
	{ AMOV,		C_LAUTO,C_NONE,	C_REG,		31, 8, REGSP },  // 
	{ AMOV,		C_LOREG,C_NONE,	C_REG,		31, 8, 0 },  // 
	{ AMOV,		C_LOREG,C_NONE,	C_REG,		31, 8, 0 },	//

	/* load long effective stack address (load int32 offset and add) */
	{ AMOV,		C_LACON,C_NONE,	C_REG,		34, 8, REGSP,	LFROM },  //

	/* pre/post-indexed load (unscaled, signed 9-bit offset) */
	{ AMOV,		C_XPOST,	C_NONE,	C_REG,		22, 4, 0 },
	{ AMOVW, 	C_XPOST,	C_NONE,	C_REG,		22, 4, 0 },
	{ AMOVH,		C_XPOST,	C_NONE,	C_REG,		22, 4, 0 },
	{ AMOVB, 		C_XPOST,	C_NONE,	C_REG,		22, 4, 0 },
	{ AMOVBU, 	C_XPOST,	C_NONE, C_REG,		22, 4, 0 },
	{ AFMOVS, 	C_XPOST,	C_NONE,	C_FREG,	22, 4, 0 },
	{ AFMOVD, 	C_XPOST,	C_NONE,	C_FREG,	22, 4, 0 },

	{ AMOV,		C_XPRE,	C_NONE,	C_REG,		22, 4, 0 },
	{ AMOVW, 	C_XPRE,	C_NONE,	C_REG,		22, 4, 0 },
	{ AMOVH,		C_XPRE,	C_NONE,	C_REG,		22, 4, 0 },
	{ AMOVB, 		C_XPRE,	C_NONE,	C_REG,		22, 4, 0 },
	{ AMOVBU, 	C_XPRE,	C_NONE, C_REG,		22, 4, 0 },
	{ AFMOVS, 	C_XPRE,	C_NONE,	C_FREG,	22, 4, 0 },
	{ AFMOVD, 	C_XPRE,	C_NONE,	C_FREG,	22, 4, 0 },

	/* pre/post-indexed store (unscaled, signed 9-bit offset) */
	{ AMOV,		C_REG,	C_NONE,	C_XPOST,		23, 4, 0 },
	{ AMOVW,		C_REG,	C_NONE,	C_XPOST,		23, 4, 0 },
	{ AMOVH,		C_REG,	C_NONE,	C_XPOST,		23, 4, 0 },
	{ AMOVB,		C_REG,	C_NONE, C_XPOST,		23, 4, 0 },
	{ AMOVBU, 	C_REG,	C_NONE, C_XPOST,		23, 4, 0 },
	{ AFMOVS, 	C_FREG,	C_NONE,	C_XPOST,		23, 4, 0 },
	{ AFMOVD, 	C_FREG,	C_NONE,	C_XPOST,		23, 4, 0 },

	{ AMOV,		C_REG,	C_NONE,	C_XPRE,		23, 4, 0 },
	{ AMOVW,		C_REG,	C_NONE,	C_XPRE,		23, 4, 0 },
	{ AMOVH,		C_REG,	C_NONE,	C_XPRE,		23, 4, 0 },
	{ AMOVB,		C_REG,	C_NONE, C_XPRE,		23, 4, 0 },
	{ AMOVBU, 	C_REG,	C_NONE, C_XPRE,		23, 4, 0 },
	{ AFMOVS, 	C_FREG,	C_NONE,	C_XPRE,		23, 4, 0 },
	{ AFMOVD, 	C_FREG,	C_NONE,	C_XPRE,		23, 4, 0 },

	/* special */
	{ AMOV,		C_SPR,	C_NONE,	C_REG,		35, 4, 0 },
	{ AMRS,		C_SPR,	C_NONE,	C_REG,		35, 4, 0 },

	{ AMOV,		C_REG,	C_NONE,	C_SPR,		36, 4, 0 },
	{ AMSR,		C_REG,	C_NONE,	C_SPR,		36, 4, 0 },

	{ AMOV,		C_LCON,	C_NONE,	C_SPR,		37, 4, 0 },
	{ AMSR,		C_LCON,	C_NONE,	C_SPR,		37, 4, 0 },

	{ AERET,		C_NONE,	C_NONE,	C_NONE,		41, 4, 0 },

	{ AFMOVS,	C_FREG,	C_NONE,	C_SEXT4,		20, 4, REGSB },
	{ AFMOVS,	C_FREG,	C_NONE,	C_UAUTO16K,	20, 4, REGSP },
	{ AFMOVS,	C_FREG,	C_NONE,	C_NSAUTO,	20, 4, REGSP },
	{ AFMOVS,	C_FREG,	C_NONE,	C_ZOREG,		20, 4, 0 },
	{ AFMOVS,	C_FREG,	C_NONE,	C_UOREG16K,	20, 4, 0 },
	{ AFMOVS,	C_FREG,	C_NONE,	C_NSOREG,	20, 4, 0 },

	{ AFMOVD,	C_FREG,	C_NONE,	C_SEXT8,		20, 4, REGSB },
	{ AFMOVD,	C_FREG,	C_NONE,	C_UAUTO32K,	20, 4, REGSP },
	{ AFMOVD,	C_FREG,	C_NONE,	C_NSAUTO,	20, 4, REGSP },
	{ AFMOVD,	C_FREG,	C_NONE,	C_ZOREG,		20, 4, 0 },
	{ AFMOVD,	C_FREG,	C_NONE,	C_UOREG32K,	20, 4, 0 }, 
	{ AFMOVD,	C_FREG,	C_NONE,	C_NSOREG,	20, 4, 0 },

	{ AFMOVS,	C_SEXT4,	C_NONE,	C_FREG,		21, 4, REGSB },
	{ AFMOVS,	C_UAUTO16K,C_NONE,	C_FREG,		21, 4, REGSP },
	{ AFMOVS,	C_NSAUTO,C_NONE,	C_FREG,		21, 4, REGSP },
	{ AFMOVS,	C_ZOREG,C_NONE,	C_FREG,		21, 4, 0 },
	{ AFMOVS,	C_UOREG16K,C_NONE,	C_FREG,		21, 4, 0 },
	{ AFMOVS,	C_NSOREG,C_NONE,	C_FREG,		21, 4, 0 },

	{ AFMOVD,	C_SEXT8,	C_NONE,	C_FREG,		21, 4, REGSB },
	{ AFMOVD,	C_UAUTO32K,C_NONE,	C_FREG,		21, 4, REGSP },
	{ AFMOVD,	C_NSAUTO,C_NONE,	C_FREG,		21, 4, REGSP },
	{ AFMOVD,	C_ZOREG,C_NONE,	C_FREG,		21, 4, 0 },
	{ AFMOVD,	C_UOREG32K,C_NONE,	C_FREG,		21, 4, 0 },
	{ AFMOVD,	C_NSOREG,C_NONE,	C_FREG,		21, 4, 0 },

	{ AFMOVS,	C_FREG,	C_NONE,	C_LEXT,		30, 8, REGSB,	LTO },
	{ AFMOVS,	C_FREG,	C_NONE,	C_LAUTO,	30, 8, REGSP,	LTO },
	{ AFMOVS,	C_FREG,	C_NONE,	C_LOREG,	30, 8, 0,	LTO },

	{ AFMOVS,	C_LEXT,	C_NONE,	C_FREG,		31, 8, REGSB,	LFROM },
	{ AFMOVS,	C_LAUTO,C_NONE,	C_FREG,		31, 8, REGSP,	LFROM },
	{ AFMOVS,	C_LOREG,C_NONE,	C_FREG,		31, 8, 0,	LFROM },

	{ AFMOVS,	C_FREG,	C_NONE,	C_ADDR,		64, 8, 0,	LTO },
	{ AFMOVS,	C_ADDR,	C_NONE,	C_FREG,		65, 8, 0,	LFROM },
	{ AFMOVD,	C_FREG,	C_NONE,	C_ADDR,		64, 8, 0,	LTO },
	{ AFMOVD,	C_ADDR,	C_NONE,	C_FREG,		65, 8, 0,	LFROM },

	{ AFADDS,		C_FREG,	C_NONE,	C_FREG,		54, 4, 0 },
	{ AFADDS,		C_FREG,	C_REG,	C_FREG,		54, 4, 0 },
	{ AFADDS,		C_FCON,	C_NONE,	C_FREG,		54, 4, 0 },
	{ AFADDS,		C_FCON,	C_REG,	C_FREG,		54, 4, 0 },

	{ AFMOVS,	C_FCON,	C_NONE,	C_FREG,		54, 4, 0 },
	{ AFMOVS,	C_FREG, C_NONE, C_FREG,		54, 4, 0 },
	{ AFMOVD,	C_FCON,	C_NONE,	C_FREG,		54, 4, 0 },
	{ AFMOVD,	C_FREG, C_NONE, C_FREG,		54, 4, 0 },

	{ AFCVTZSD,	C_FREG,	C_NONE,	C_REG,		29, 4, 0 },
	{ ASCVTFD,	C_REG,	C_NONE,	C_FREG,		29, 4, 0 },

	{ AFCMPS,		C_FREG,	C_REG,	C_NONE,		56, 4, 0 },
	{ AFCMPS,		C_FCON,	C_REG,	C_NONE,		56, 4, 0 },

	{ AFCCMPS,	C_COND,	C_REG,	C_LCON,		57, 4, 0 },

	{ AFCSELD,	C_COND,	C_REG,	C_FREG,		18, 4, 0 },

	{ AFCVTSD,	C_FREG,	C_NONE,	C_FREG,		29, 4, 0 },

	{ ACASE,	C_REG,	C_NONE,	C_REG,		62, 4*4, 0 },
	{ ABCASE,	C_NONE, C_NONE, C_SBRA,		63, 4, 0 },

	{ ACLREX,		C_NONE,	C_NONE,	C_LCON,		38, 4, 0 },
	{ ACLREX,		C_NONE,	C_NONE,	C_NONE,		38, 4, 0 },

	{ ACBZ,		C_REG,	C_NONE,	C_SBRA,		39, 4, 0 },
	{ ATBZ,		C_LCON,	C_REG,	C_SBRA,		40, 4, 0 },

	{ ASYS,		C_LCON,	C_NONE,	C_NONE,		50, 4, 0 },
	{ ASYS,		C_LCON,	C_REG,	C_NONE,		50, 4, 0 },
	{ ASYSL,		C_LCON,	C_NONE,	C_REG,		50, 4, 0 },

	{ ADMB,		C_LCON,	C_NONE, 	C_NONE,		51, 4, 0 },
	{ AHINT,		C_LCON,	C_NONE,	C_NONE,		52, 4, 0 },

	{ ALDXR,		C_ZOREG,	C_NONE,	C_REG,		58, 4, 0 },
	{ ALDAXR,		C_ZOREG,	C_NONE,	C_REG,		58, 4, 0 },
	{ ALDXP,		C_ZOREG,	C_REG,	C_REG,		58, 4, 0 },
	{ ASTXR,		C_REG,	C_REG,	C_ZOREG,		59, 4, 0 },
	{ ASTLXR,		C_REG,	C_REG,	C_ZOREG,		59, 4, 0 },
	{ ASTXP,		C_REG, C_REG,	C_ZOREG,		59, 4, 0 },

	{ AAESD,	C_VREG,	C_NONE,	C_VREG,	29, 4, 0 },
	{ ASHA1C,	C_VREG,	C_REG,	C_VREG,	1, 4, 0 },

	{ AUNDEF,		C_NONE,	C_NONE,	C_NONE,		90, 4, 0 },

	{ AUSEFIELD,	C_ADDR,	C_NONE,	C_NONE, 	 0, 0, 0 },
	{ APCDATA,	C_LCON,	C_NONE,	C_LCON,		0, 0, 0 },
	{ AFUNCDATA,	C_LCON,	C_NONE,	C_ADDR,	0, 0, 0 },
	
	{ ADUFFZERO,	C_NONE,	C_NONE,	C_SBRA,		 5, 4, 0 },	// same as AB/ABL
	{ ADUFFCOPY,	C_NONE,	C_NONE,	C_SBRA,		 5, 4, 0 },	// same as AB/ABL

	{ AXXX,		C_NONE,	C_NONE,	C_NONE,		 0, 4, 0 },
};

/*
 * internal class codes for different constant classes:
 * they partition the constant/offset range into disjoint ranges that
 * are somehow treated specially by one or more load/store instructions.
 */
static int	autoclass[] = {C_PSAUTO, C_NSAUTO, C_NPAUTO, C_PSAUTO, C_PPAUTO, C_UAUTO4K, C_UAUTO8K, C_UAUTO16K, C_UAUTO32K, C_UAUTO64K, C_LAUTO};
static int	oregclass[] = {C_ZOREG, C_NSOREG, C_NPOREG, C_PSOREG, C_PPOREG, C_UOREG4K, C_UOREG8K, C_UOREG16K, C_UOREG32K, C_UOREG64K, C_LOREG};

/*
 * valid pstate field values, and value to use in instruction
 */
struct{
	ulong	a;
	ulong	b;
} pstatefield[] = {
D_SPSel,		(0<<16) | (4<<12) | (5<<5),
D_DAIFSet,	(3<<16) | (4<<12) | (6<<5),
D_DAIFClr,	(3<<16) | (4<<12) | (7<<5),
};

static struct {
	ulong	start;
	ulong	size;
} pool;

static void
prasm(Prog *p)
{
	print("%P\n", p);
}

void
span7(Link *ctxt, LSym *cursym)
{
	Prog *p;
	Optab *o;
	int m, bflag, i, j;
	int32 c, psz;
	int32 out[6];
	uchar *bp, *cast;
	
	p = cursym->text;
	if(p == nil || p->link == nil) // handle external functions and ELF section symbols
		return;
	ctxt->cursym = cursym;
	ctxt->autosize = (int32)(p->to.offset & 0xffffffffll) + 8;

	if(oprange[AAND].start == nil)
 		buildop(ctxt);

	bflag = 0;
	c = 0;	
	p->pc = c;
	for(p = p->link; p != nil; p = p->link) {
		ctxt->curp = p;
		if(p->as == ADWORD && ((c & 7)) != 0)
			c += 4;
		p->pc = c;
		if (p->from.type == D_CONST && p->from.offset == 0)
			p->from.reg = REGZERO;
		if (p->to.type == D_CONST && p->to.offset == 0)
			p->to.reg = REGZERO;
		o = oplook(ctxt, p);
		m = o->size;
		if(m == 0) {
			if(p->as != ANOP && p->as != AFUNCDATA && p->as != APCDATA)
				ctxt->diag("zero-width instruction\n%P", p);
			continue;
		}
		switch(o->flag & ((LFROM | LTO))) {
		case LFROM:
			addpool(ctxt, p, &p->from);
			break;
		case LTO:
			addpool(ctxt, p, &p->to);
			break;
		}
		if(p->as == AB || p->as == ARET || p->as == AERET || p->as == ARETURN) /* TO DO: other unconditional operations */
			checkpool(ctxt, p, 0);
		c += m;
		if(ctxt->blitrl)
			checkpool(ctxt, p, 1);
	}
	cursym->size = c;
	/*
	 * if any procedure is large enough to
	 * generate a large SBRA branch, then
	 * generate extra passes putting branches
	 * around jmps to fix. this is rare.
	 */
	while(bflag) {
		if(ctxt->debugvlog)
			Bprint(ctxt->bso, "%5.2f span1\n", cputime());
		bflag = 0;
		c = 0;
		for(p = cursym->text; p != nil; p = p->link) {
			if(p->as == ADWORD && ((c & 7)) != 0)
				c += 4;
			p->pc = c;
			o = oplook(ctxt, p);
			/* very large branches
						if(o->type == 6 && p->cond) {
							otxt = p->cond->pc - c;
							if(otxt < 0)
								otxt = -otxt;
							if(otxt >= (1L<<17) - 10) {
								q = ctxt->arch->prg();
								q->link = p->link;
								p->link = q;
								q->as = AB;
								q->to.type = D_BRANCH;
								q->cond = p->cond;
								p->cond = q;
								q = ctxt->arch->prg();
								q->link = p->link;
								p->link = q;
								q->as = AB;
								q->to.type = D_BRANCH;
								q->cond = q->link->link;
								bflag = 1;
							}
						}
			 */
			m = o->size;
			if(m == 0) {
				if(p->as != ANOP && p->as != AFUNCDATA && p->as != APCDATA)
					ctxt->diag("zero-width instruction\n%P", p);
				continue;
			}
			c += m;
		}
	}
	c += -c&(FuncAlign-1);
	cursym->size = c;
	/*
	 * lay out the code, emitting code and data relocations.
	 */
	if(ctxt->tlsg == nil)
		ctxt->tlsg = linklookup(ctxt, "runtime.tlsg", 0);
	symgrow(ctxt, cursym, cursym->size);
	bp = cursym->p;
	psz = 0;
	for(p = cursym->text->link; p != nil; p = p->link) {
		ctxt->pc = p->pc;
		ctxt->curp = p;
		o = oplook(ctxt, p);
		// need to align DWORDs on 8-byte boundary. The ISA doesn't
		// require it, but the various 64-bit loads we generate assume it.
		if(o->as == ADWORD && psz % 8 != 0) {
			*(int32*)bp = 0;
			bp += 4;
			psz += 4;
		}
		if(o->size > 4*nelem(out))
			sysfatal("out array in span7 is too small, need at least %d for %P", o->size/4, p);
		asmout(ctxt, p, o, out);
		for(i=0; i<o->size/4; i++) {
			cast = (uchar*)&out[i];
			for(j=0; j<4; j++, psz++)
				*bp++ = cast[inuxi4[j]];
		}
	}
}

/*
 * when the first reference to the literal pool threatens
 * to go out of range of a 1Mb PC-relative offset
 * drop the pool now, and branch round it.
 */
static void
checkpool(Link *ctxt, Prog *p, int skip)
{
	if(pool.size >= 0xffff0 || !ispcdisp(p->pc + 4 + pool.size - pool.start + 8))
		flushpool(ctxt, p, skip);
	else
		if(p->link == nil)
			flushpool(ctxt, p, 2);
}

static void
flushpool(Link *ctxt, Prog *p, int skip)
{
	Prog *q;
	if(ctxt->blitrl) {
		if(skip) {
			if(ctxt->debugvlog && skip == 1)
				print("note: flush literal pool at %#llux: len=%lud ref=%lux\n", p->pc + 4, pool.size, pool.start);
			q = ctxt->arch->prg();
			q->as = AB;
			q->to.type = D_BRANCH;
			q->pcond = p->link;
			q->link = ctxt->blitrl;
			ctxt->blitrl = q;
		} else
			if(p->pc + pool.size - pool.start < 1024 * 1024)
				return;
		ctxt->elitrl->link = p->link;
		p->link = ctxt->blitrl;
		ctxt->blitrl = 0; /* BUG: should refer back to values until out-of-range */
		ctxt->elitrl = 0;
		pool.size = 0;
		pool.start = 0;
	}
}

/*
 * TO DO: hash
 */
static void
addpool(Link *ctxt, Prog *p, Addr *a)
{
	Prog *q;
	Prog t;
	int c;
	int sz;
	c = aclass(ctxt, a);
	t = zprg;
	t.as = AWORD;
	sz = 4;
	// MOVW foo(SB), R is actually
	//	MOV addr, REGTEMP
	//	MOVW REGTEMP, R
	// where addr is the address of the DWORD containing the address of foo.
	if(p->as == AMOV || c == C_ADDR) {
		t.as = ADWORD;
		sz = 8;
	}
	switch(c) {
	default:
		t.to.offset = a->offset;
		t.to.sym = a->sym;
		t.to.type = a->type;
		t.to.name = a->name;
		break;
	case C_PSAUTO:
	case C_PPAUTO:
	case C_UAUTO4K:
	case C_UAUTO8K:
	case C_UAUTO16K:
	case C_UAUTO32K:
	case C_UAUTO64K:
	case C_NSAUTO:
	case C_NPAUTO:
	case C_LAUTO:
	case C_PPOREG:
	case C_PSOREG:
	case C_UOREG4K:
	case C_UOREG8K:
	case C_UOREG16K:
	case C_UOREG32K:
	case C_UOREG64K:
	case C_NSOREG:
	case C_NPOREG:
	case C_LOREG:
		t.to.type = D_CONST;
		t.to.offset = ctxt->instoffset;
		sz = 4;
		break;
	}
	for(q = ctxt->blitrl; q != nil; q = q->link) /* could hash on t.t0.offset */
		if(memcmp(&q->to, &t.to, sizeof ((t.to))) == 0) {
			p->pcond = q;
			return;
		}
	q = ctxt->arch->prg();
	*q = t;
	q->pc = pool.size;
	if(ctxt->blitrl == nil) {
		ctxt->blitrl = q;
		pool.start = p->pc;
	} else
		ctxt->elitrl->link = q;
	ctxt->elitrl = q;
	pool.size = -pool.size&(FuncAlign-1);
	pool.size += sz;
	p->pcond = q;
}

static int32
regoff(Link *ctxt, Addr *a)
{
	ctxt->instoffset = 0;
	aclass(ctxt, a);
	return ctxt->instoffset;
}

static int
ispcdisp(int32 v)
{
	/* pc-relative addressing will reach? */
	return v >= -0xfffff && v <= 0xfffff && ((v & 3)) == 0;
}

static int
isaddcon(vlong v)
{
	/* uimm12 or uimm24? */
	if(v < 0)
		return 0;
	if(((v & 0xFFF)) == 0)
		v >>= 12;
	return v <= 0xFFF;
}

static int
isbitcon(uvlong v)
{
	/*  fancy bimm32 or bimm64? */
	return findmask(v) != nil || ((v >> 32)) == 0 && findmask(v | ((v << 32))) != nil;
}

/*
 * return appropriate index into tables above
 */
static int
constclass(vlong l)
{
	if(l == 0)
		return 0;
	if(l < 0) {
		if(l >= -256)
			return 1;
		if(l >= -512 && ((l & 7)) == 0)
			return 2;
		return 10;
	}
	if(l <= 255)
		return 3;
	if(l <= 504 && ((l & 7)) == 0)
		return 4;
	if(l <= 4095)
		return 5;
	if(l <= 8190 && ((l & 1)) == 0)
		return 6;
	if(l <= 16380 && ((l & 3)) == 0)
		return 7;
	if(l <= 32760 && ((l & 7)) == 0)
		return 8;
	if(l <= 65520 && ((l & 0xF)) == 0)
		return 9;
	return 10;
}

/*
 * given an offset v and a class c (see above)
 * return the offset value to use in the instruction,
 * scaled if necessary
 */
static vlong
offsetshift(Link *ctxt, vlong v, int c)
{
	vlong vs;
	int s;
	static int shifts[] = {0, 1, 2, 3, 4};
	s = 0;
	if(c >= C_SEXT1 && c <= C_SEXT16)
		s = shifts[c - C_SEXT1];
	else
		if(c >= C_UAUTO4K && c <= C_UAUTO64K)
			s = shifts[c - C_UAUTO4K];
		else
			if(c >= C_UOREG4K && c <= C_UOREG64K)
				s = shifts[c - C_UOREG4K];
	vs = v >> s;
	if(vs << s != v)
		ctxt->diag("odd offset: %lld\n%P", v, ctxt->curp);
	return vs;
}

/*
 * if v contains a single 16-bit value aligned
 * on a 16-bit field, and thus suitable for movk/movn,
 * return the field index 0 to 3; otherwise return -1
 */
static int
movcon(vlong v)
{
	int s;
	for(s = 0; s < 64; s += 16)
		if(((v & ~(((uvlong)0xFFFF << s)))) == 0)
			return s / 16;
	return -1;
}

static int
aclass(Link *ctxt, Addr *a)
{
	vlong v;
	LSym *s;
	int t;
	ctxt->instoffset = 0;
	switch(a->type) {
	case D_NONE:
		return C_NONE;
	case D_REG:
		return C_REG;
	case D_VREG:
		return C_VREG;
	case D_SP:
		return C_RSP;
	case D_COND:
		return C_COND;
	case D_SHIFT:
		return C_SHIFT;
	case D_EXTREG:
		return C_EXTREG;
	case D_ROFF:
		return C_ROFF;
	case D_XPOST:
		return C_XPOST;
	case D_XPRE:
		return C_XPRE;
	case D_FREG:
		return C_FREG;
	case D_OREG:
		switch(a->name) {
		case D_EXTERN:
		case D_STATIC:
			if(a->sym == nil)
				break;
			ctxt->instoffset = a->offset;
			if(a->sym != nil) // use relocation
				return C_ADDR;
			return C_LEXT;
		case D_AUTO:
			ctxt->instoffset = ctxt->autosize + a->offset;
			return autoclass[constclass(ctxt->instoffset)];
		case D_PARAM:
			ctxt->instoffset = ctxt->autosize + a->offset + 8;
			return autoclass[constclass(ctxt->instoffset)];
		case D_NONE:
			ctxt->instoffset = a->offset;
			return oregclass[constclass(ctxt->instoffset)];
		}
		return C_GOK;
	case D_SPR:
		return C_SPR;
	case D_OCONST:
		switch(a->name) {
		case D_EXTERN:
		case D_STATIC:
			if(a->sym == nil)
				break;
			ctxt->instoffset = a->offset;
			if(a->sym != nil) // use relocation
				return C_ADDR;
			return C_LCON;
		}
		return C_GOK;
	case D_FCONST:
		return C_FCON;
	case D_CONST:
		switch(a->name) {
		case D_NONE:
			ctxt->instoffset = a->offset;
			if(a->reg != NREG && a->reg != REGZERO)
				goto aconsize;
			v = ctxt->instoffset;
			if(v == 0)
				return C_ZCON;
			if(isaddcon(v)) {
				if(isbitcon(v))
					return C_ABCON;
				if(v <= 0xFFF)
					return C_ADDCON0;
				return C_ADDCON;
			}
			t = movcon(v);
			if(t >= 0) {
				if(isbitcon(v))
					return C_MBCON;
				return C_MOVCON;
			}
			t = movcon(~v);
			if(t >= 0) {
				if(isbitcon(v))
					return C_MBCON;
				return C_MOVCON;
			}
			if(isbitcon(v))
				return C_BITCON;
			return C_LCON;
		case D_EXTERN:
		case D_STATIC:
			s = a->sym;
			if(s == nil)
				break;
			if(s->type == SCONST) {
				ctxt->instoffset = s->value + a->offset;
				goto aconsize;
			}
			ctxt->instoffset = s->value + a->offset;
			/* not sure why this barfs */
			return C_LCON;
		case D_AUTO:
			ctxt->instoffset = ctxt->autosize + a->offset;
			goto aconsize;
		case D_PARAM:
			ctxt->instoffset = ctxt->autosize + a->offset + 8;
		aconsize:
			if(isaddcon(ctxt->instoffset))
				return C_AACON;
			return C_LACON;
		}
		return C_GOK;
	case D_BRANCH:
		return C_SBRA;
	}
	return C_GOK;
}

static Optab*
oplook(Link *ctxt, Prog *p)
{
	int a1;
	int a2;
	int a3;
	int r;
	char *c1;
	char *c2;
	char *c3;
	Optab *o;
	Optab *e;
	a1 = p->optab;
	if(a1)
		return optab + ((a1 - 1));
	a1 = p->from.class;
	if(a1 == 0) {
		a1 = aclass(ctxt, &p->from) + 1;
		p->from.class = a1;
	}
	a1--;
	a3 = p->to.class;
	if(a3 == 0) {
		a3 = aclass(ctxt, &p->to) + 1;
		p->to.class = a3;
	}
	a3--;
	a2 = C_NONE;
	if(p->reg != NREG)
		a2 = C_REG;
	r = p->as;
	o = oprange[r].start;
	if(o == 0) {
		a1 = opcross[repop[r]][a1][a2][a3];
		if(a1) {
			p->optab = a1 + 1;
			return optab + a1;
		}
		o = oprange[r].stop; /* just generate an error */
	}
	if(0) {
		print("oplook %A %d %d %d\n", (int)p->as, a1, a2, a3);
		print("		%d %d\n", p->from.type, p->to.type);
	}
	e = oprange[r].stop;
	c1 = xcmp[a1];
	c2 = xcmp[a2];
	c3 = xcmp[a3];
	for(; o < e; o++)
		if(o->a2 == a2 || c2[o->a2])
			if(c1[o->a1])
				if(c3[o->a3]) {
					p->optab = ((o - optab)) + 1;
					return o;
				}
	ctxt->diag("illegal combination %P %^ %^ %^, %d %d",
                p, a1, a2, a3, p->from.type, p->to.type);
	prasm(p);
	o = badop;
	if(o == 0)
		o = optab;
	return o;
}

static int
cmp(int a, int b)
{
	if(a == b)
		return 1;
	switch(a) {
	case C_RSP:
		if(b == C_REG)
			return 1;
		break;
	case C_REG:
		if(b == C_ZCON)
			return 1;
		break;
	case C_ADDCON0:
		if(b == C_ZCON)
			return 1;
		break;
	case C_ADDCON:
		if(b == C_ZCON || b == C_ADDCON0 || b == C_ABCON)
			return 1;
		break;
	case C_BITCON:
		if(b == C_ABCON || b == C_MBCON)
			return 1;
		break;
	case C_MOVCON:
		if(b == C_MBCON || b == C_ZCON || b == C_ADDCON0)
			return 1;
		break;
	case C_LCON:
		if(b == C_ZCON || b == C_BITCON || b == C_ADDCON || b == C_ADDCON0 || b == C_ABCON || b == C_MBCON || b == C_MOVCON)
			return 1;
		break;
	case C_VCON:
		return cmp(C_LCON, b);
	case C_LACON:
		if(b == C_AACON)
			return 1;
		break;
	case C_SEXT2:
		if(b == C_SEXT1)
			return 1;
		break;
	case C_SEXT4:
		if(b == C_SEXT1 || b == C_SEXT2)
			return 1;
		break;
	case C_SEXT8:
		if(b >= C_SEXT1 && b <= C_SEXT4)
			return 1;
		break;
	case C_SEXT16:
		if(b >= C_SEXT1 && b <= C_SEXT8)
			return 1;
		break;
	case C_LEXT:
		if(b >= C_SEXT1 && b <= C_SEXT16)
			return 1;
		break;
	case C_PPAUTO:
		if(b == C_PSAUTO)
			return 1;
		break;
	case C_UAUTO4K:
		if(b == C_PSAUTO || b == C_PPAUTO)
			return 1;
		break;
	case C_UAUTO8K:
		return cmp(C_UAUTO4K, b);
	case C_UAUTO16K:
		return cmp(C_UAUTO8K, b);
	case C_UAUTO32K:
		return cmp(C_UAUTO16K, b);
	case C_UAUTO64K:
		return cmp(C_UAUTO32K, b);
	case C_NPAUTO:
		return cmp(C_NSAUTO, b);
	case C_LAUTO:
		return cmp(C_NPAUTO, b) || cmp(C_UAUTO64K, b);
	case C_PSOREG:
		if(b == C_ZOREG)
			return 1;
		break;
	case C_PPOREG:
		if(b == C_ZOREG || b == C_PSOREG)
			return 1;
		break;
	case C_UOREG4K:
		if(b == C_ZOREG || b == C_PSAUTO || b == C_PSOREG || b == C_PPAUTO || b == C_PPOREG)
			return 1;
		break;
	case C_UOREG8K:
		return cmp(C_UOREG4K, b);
	case C_UOREG16K:
		return cmp(C_UOREG8K, b);
	case C_UOREG32K:
		return cmp(C_UOREG16K, b);
	case C_UOREG64K:
		return cmp(C_UOREG32K, b);
	case C_NPOREG:
		return cmp(C_NSOREG, b);
	case C_LOREG:
		return cmp(C_NPOREG, b) || cmp(C_UOREG64K, b);
	case C_LBRA:
		if(b == C_SBRA)
			return 1;
		break;
	}
	return 0;
}

static int
ocmp(const void  *a1, const void  *a2)
{
	Optab *p1;
	Optab *p2;
	int n;
	p1 = (Optab*)a1;
	p2 = (Optab*)a2;
	n = p1->as - p2->as;
	if(n)
		return n;
	n = p1->a1 - p2->a1;
	if(n)
		return n;
	n = p1->a2 - p2->a2;
	if(n)
		return n;
	n = p1->a3 - p2->a3;
	if(n)
		return n;
	return 0;
}

void
buildop(Link *ctxt)
{
	int i;
	int n;
	int r;
	Oprang t;
	for(i = 0; i < C_GOK; i++)
		for(n = 0; n < C_GOK; n++)
			xcmp[i][n] = cmp(n, i);
	for(n = 0; optab[n].as != AXXX; n++)
		;
	badop = optab + n;
	qsort(optab, n, sizeof(optab[0]), ocmp);
	for(i = 0; i < n; i++) {
		r = optab[i].as;
		oprange[r].start = optab + i;
		while(optab[i].as == r)
			i++;
		oprange[r].stop = optab + i;
		i--;
		t = oprange[r];
		switch(r) {
		default:
			ctxt->diag("unknown op in build: %A", r);
			sysfatal("bad code");
		case AXXX:
			break;
		case AADD:
			oprange[AADDS] = t;
			oprange[ASUB] = t;
			oprange[ASUBS] = t;
			oprange[AADDW] = t;
			oprange[AADDSW] = t;
			oprange[ASUBW] = t;
			oprange[ASUBSW] = t;
			break;
		case AAND: /* logical immediate, logical shifted register */
			oprange[AANDS] = t;
			oprange[AANDSW] = t;
			oprange[AANDW] = t;
			oprange[AEOR] = t;
			oprange[AEORW] = t;
			oprange[AORR] = t;
			oprange[AORRW] = t;
			break;
		case ABIC: /* only logical shifted register */
			oprange[ABICS] = t;
			oprange[ABICSW] = t;
			oprange[ABICW] = t;
			oprange[AEON] = t;
			oprange[AEONW] = t;
			oprange[AORN] = t;
			oprange[AORNW] = t;
			break;
		case ANEG:
			oprange[ANEGS] = t;
			oprange[ANEGSW] = t;
			oprange[ANEGW] = t;
			break;
		case AADC: /* rn=Rd */
			oprange[AADCW] = t;
			oprange[AADCS] = t;
			oprange[AADCSW] = t;
			oprange[ASBC] = t;
			oprange[ASBCW] = t;
			oprange[ASBCS] = t;
			oprange[ASBCSW] = t;
			break;
		case ANGC: /* rn=REGZERO */
			oprange[ANGCW] = t;
			oprange[ANGCS] = t;
			oprange[ANGCSW] = t;
			break;
		case ACMP:
			oprange[ACMPW] = t;
			oprange[ACMN] = t;
			oprange[ACMNW] = t;
			break;
		case ATST:
			oprange[ATSTW] = t;
			break;
		/* register/register, and shifted */
		case AMVN:
			oprange[AMVNW] = t;
			break;
		case AMOVK:
			oprange[AMOVKW] = t;
			oprange[AMOVN] = t;
			oprange[AMOVNW] = t;
			oprange[AMOVZ] = t;
			oprange[AMOVZW] = t;
			break;
		case ABEQ:
			oprange[ABNE] = t;
			oprange[ABCS] = t;
			oprange[ABHS] = t;
			oprange[ABCC] = t;
			oprange[ABLO] = t;
			oprange[ABMI] = t;
			oprange[ABPL] = t;
			oprange[ABVS] = t;
			oprange[ABVC] = t;
			oprange[ABHI] = t;
			oprange[ABLS] = t;
			oprange[ABGE] = t;
			oprange[ABLT] = t;
			oprange[ABGT] = t;
			oprange[ABLE] = t;
			break;
		case ALSL:
			oprange[ALSLW] = t;
			oprange[ALSR] = t;
			oprange[ALSRW] = t;
			oprange[AASR] = t;
			oprange[AASRW] = t;
			oprange[AROR] = t;
			oprange[ARORW] = t;
			break;
		case ACLS:
			oprange[ACLSW] = t;
			oprange[ACLZ] = t;
			oprange[ACLZW] = t;
			oprange[ARBIT] = t;
			oprange[ARBITW] = t;
			oprange[AREV] = t;
			oprange[AREVW] = t;
			oprange[AREV16] = t;
			oprange[AREV16W] = t;
			oprange[AREV32] = t;
			break;
		case ASDIV:
			oprange[ASDIVW] = t;
			oprange[AUDIV] = t;
			oprange[AUDIVW] = t;
			oprange[ACRC32B] = t;
			oprange[ACRC32CB] = t;
			oprange[ACRC32CH] = t;
			oprange[ACRC32CW] = t;
			oprange[ACRC32CX] = t;
			oprange[ACRC32H] = t;
			oprange[ACRC32W] = t;
			oprange[ACRC32X] = t;
			break;
		case AMADD:
			oprange[AMADDW] = t;
			oprange[AMSUB] = t;
			oprange[AMSUBW] = t;
			oprange[ASMADDL] = t;
			oprange[ASMSUBL] = t;
			oprange[AUMADDL] = t;
			oprange[AUMSUBL] = t;
			break;
		case AREM:
			oprange[AREMW] = t;
			oprange[AUREM] = t;
			oprange[AUREMW] = t;
			break;
		case AMUL:
			oprange[AMULW] = t;
			oprange[AMNEG] = t;
			oprange[AMNEGW] = t;
			oprange[ASMNEGL] = t;
			oprange[ASMULL] = t;
			oprange[ASMULH] = t;
			oprange[AUMNEGL] = t;
			oprange[AUMULH] = t;
			oprange[AUMULL] = t;
			break;
		case AMOVH:
			oprange[AMOVHU] = t;
			break;
		case AMOVW:
			oprange[AMOVWU] = t;
			break;
		case ABFM:
			oprange[ABFMW] = t;
			oprange[ASBFM] = t;
			oprange[ASBFMW] = t;
			oprange[AUBFM] = t;
			oprange[AUBFMW] = t;
			break;
		case ABFI:
			oprange[ABFIW] = t;
			oprange[ABFXIL] = t;
			oprange[ABFXILW] = t;
			oprange[ASBFIZ] = t;
			oprange[ASBFIZW] = t;
			oprange[ASBFX] = t;
			oprange[ASBFXW] = t;
			oprange[AUBFIZ] = t;
			oprange[AUBFIZW] = t;
			oprange[AUBFX] = t;
			oprange[AUBFXW] = t;
			break;
		case AEXTR:
			oprange[AEXTRW] = t;
			break;
		case ASXTB:
			oprange[ASXTBW] = t;
			oprange[ASXTH] = t;
			oprange[ASXTHW] = t;
			oprange[ASXTW] = t;
			oprange[AUXTB] = t;
			oprange[AUXTH] = t;
			oprange[AUXTW] = t;
			oprange[AUXTBW] = t;
			oprange[AUXTHW] = t;
			break;
		case ACCMN:
			oprange[ACCMNW] = t;
			oprange[ACCMP] = t;
			oprange[ACCMPW] = t;
			break;
		case ACSEL:
			oprange[ACSELW] = t;
			oprange[ACSINC] = t;
			oprange[ACSINCW] = t;
			oprange[ACSINV] = t;
			oprange[ACSINVW] = t;
			oprange[ACSNEG] = t;
			oprange[ACSNEGW] = t;
			// aliases Rm=Rn, !cond
			oprange[ACINC] = t;
			oprange[ACINCW] = t;
			oprange[ACINV] = t;
			oprange[ACINVW] = t;
			oprange[ACNEG] = t;
			oprange[ACNEGW] = t;
			break;
		// aliases, Rm=Rn=REGZERO, !cond
		case ACSET:
			oprange[ACSETW] = t;
			oprange[ACSETM] = t;
			oprange[ACSETMW] = t;
			break;
		case AMOV:
		case AMOVB:
		case AMOVBU:
		case AB:
		case ABL:
		case AWORD:
		case ADWORD:
		case ARET:
		case ATEXT:
		case ACASE:
		case ABCASE:
			break;
		case AERET:
			oprange[ANOP] = t;
			oprange[AWFE] = t;
			oprange[AWFI] = t;
			oprange[AYIELD] = t;
			oprange[ASEV] = t;
			oprange[ASEVL] = t;
			oprange[ADRPS] = t;
			break;
		case ACBZ:
			oprange[ACBZW] = t;
			oprange[ACBNZ] = t;
			oprange[ACBNZW] = t;
			break;
		case ATBZ:
			oprange[ATBNZ] = t;
			break;
		case AADR:
		case AADRP:
			break;
		case ACLREX:
			break;
		case ASVC:
			oprange[AHLT] = t;
			oprange[AHVC] = t;
			oprange[ASMC] = t;
			oprange[ABRK] = t;
			oprange[ADCPS1] = t;
			oprange[ADCPS2] = t;
			oprange[ADCPS3] = t;
			break;
		case AFADDS:
			oprange[AFADDD] = t;
			oprange[AFSUBS] = t;
			oprange[AFSUBD] = t;
			oprange[AFMULS] = t;
			oprange[AFMULD] = t;
			oprange[AFNMULS] = t;
			oprange[AFNMULD] = t;
			oprange[AFDIVS] = t;
			oprange[AFMAXD] = t;
			oprange[AFMAXS] = t;
			oprange[AFMIND] = t;
			oprange[AFMINS] = t;
			oprange[AFMAXNMD] = t;
			oprange[AFMAXNMS] = t;
			oprange[AFMINNMD] = t;
			oprange[AFMINNMS] = t;
			oprange[AFDIVD] = t;
			break;
		case AFCVTSD:
			oprange[AFCVTDS] = t;
			oprange[AFABSD] = t;
			oprange[AFABSS] = t;
			oprange[AFNEGD] = t;
			oprange[AFNEGS] = t;
			oprange[AFSQRTD] = t;
			oprange[AFSQRTS] = t;
			oprange[AFRINTNS] = t;
			oprange[AFRINTND] = t;
			oprange[AFRINTPS] = t;
			oprange[AFRINTPD] = t;
			oprange[AFRINTMS] = t;
			oprange[AFRINTMD] = t;
			oprange[AFRINTZS] = t;
			oprange[AFRINTZD] = t;
			oprange[AFRINTAS] = t;
			oprange[AFRINTAD] = t;
			oprange[AFRINTXS] = t;
			oprange[AFRINTXD] = t;
			oprange[AFRINTIS] = t;
			oprange[AFRINTID] = t;
			oprange[AFCVTDH] = t;
			oprange[AFCVTHS] = t;
			oprange[AFCVTHD] = t;
			oprange[AFCVTSH] = t;
			break;
		case AFCMPS:
			oprange[AFCMPD] = t;
			oprange[AFCMPES] = t;
			oprange[AFCMPED] = t;
			break;
		case AFCCMPS:
			oprange[AFCCMPD] = t;
			oprange[AFCCMPES] = t;
			oprange[AFCCMPED] = t;
			break;
		case AFCSELD:
			oprange[AFCSELS] = t;
			break;
		case AFMOVS:
		case AFMOVD:
			break;
		case AFCVTZSD:
			oprange[AFCVTZSDW] = t;
			oprange[AFCVTZSS] = t;
			oprange[AFCVTZSSW] = t;
			oprange[AFCVTZUD] = t;
			oprange[AFCVTZUDW] = t;
			oprange[AFCVTZUS] = t;
			oprange[AFCVTZUSW] = t;
			break;
		case ASCVTFD:
			oprange[ASCVTFS] = t;
			oprange[ASCVTFWD] = t;
			oprange[ASCVTFWS] = t;
			oprange[AUCVTFD] = t;
			oprange[AUCVTFS] = t;
			oprange[AUCVTFWD] = t;
			oprange[AUCVTFWS] = t;
			break;
		case ASYS:
			oprange[AAT] = t;
			oprange[ADC] = t;
			oprange[AIC] = t;
			oprange[ATLBI] = t;
			break;
		case ASYSL:
		case AHINT:
			break;
		case ADMB:
			oprange[ADSB] = t;
			oprange[AISB] = t;
			break;
		case AMRS:
		case AMSR:
			break;
		case ALDXR:
			oprange[ALDXRB] = t;
			oprange[ALDXRH] = t;
			oprange[ALDXRW] = t;
			break;
		case ALDAXR:
			oprange[ALDAXRW] = t;
			break;
		case ALDXP:
			oprange[ALDXPW] = t;
			break;
		case ASTXR:
			oprange[ASTXRB] = t;
			oprange[ASTXRH] = t;
			oprange[ASTXRW] = t;
			break;
		case ASTLXR:
			oprange[ASTLXRW] = t;
			break;
		case ASTXP:
			oprange[ASTXPW] = t;
			break;
		case AAESD:
			oprange[AAESE] = t;
			oprange[AAESMC] = t;
			oprange[AAESIMC] = t;
			oprange[ASHA1H] = t;
			oprange[ASHA1SU1] = t;
			oprange[ASHA256SU0] = t;
			break;
		case ASHA1C:
			oprange[ASHA1P] = t;
			oprange[ASHA1M] = t;
			oprange[ASHA1SU0] = t;
			oprange[ASHA256H] = t;
			oprange[ASHA256H2] = t;
			oprange[ASHA256SU1] = t;
			break;
		case AUNDEF:
		case AUSEFIELD:
		case AFUNCDATA:
		case APCDATA:
		case ADUFFZERO:
		case ADUFFCOPY:
			break;
		}
	}
}

int
chipfloat7(Link *ctxt, float64 e)
{
	int n;
	ulong h1;
	int32 l, h;
	uint64 ei;

	USED(ctxt);

	memmove(&ei, &e, 8);
	l = (int32)ei;
	h = (int32)(ei>>32);

	if(l != 0 || (h&0xffff) != 0)
		goto no;
	h1 = h & 0x7fc00000;
	if(h1 != 0x40000000 && h1 != 0x3fc00000)
		goto no;
	n = 0;

	// sign bit (a)
	if(h & 0x80000000)
		n |= 1<<7;

	// exp sign bit (b)
	if(h1 == 0x3fc00000)
		n |= 1<<6;

	// rest of exp and mantissa (cd-efgh)
	n |= (h >> 16) & 0x3f;

//print("match %.8lux %.8lux %d\n", l, h, n);
	return n;

no:
	return -1;
}

void
asmout(Link *ctxt, Prog *p, Optab *o, int32 *out)
{
	int32 o1, o2, o3, o4, o5, v, hi;
	ulong u;
	vlong d;
	int r, s, rf, rt, ra, nzcv, cond, i, as;
	Mask *mask;
	Reloc *rel;
	static Prog *lastcase;

	o1 = 0;
	o2 = 0;
	o3 = 0;
	o4 = 0;
	o5 = 0;
	switch(o->type) {
	default:
		ctxt->diag("unknown asm %d", o->type);
		prasm(p);
		break;
	case 0: /* pseudo ops */
		break;
	case 1: /* op Rm,[Rn],Rd; default Rn=Rd -> op Rm<<0,[Rn,]Rd (shifted register) */
		o1 = oprrr(ctxt, p->as);
		rf = p->from.reg;
		rt = p->to.reg;
		r = p->reg;
		if(p->to.type == D_NONE)
			rt = REGZERO;
		if(r == NREG)
			r = rt;
		o1 |= ((rf << 16)) | ((r << 5)) | rt;
		break;
	case 2: /* add/sub $(uimm12|uimm24)[,R],R; cmp $(uimm12|uimm24),R */
		o1 = opirr(ctxt, p->as);
		rt = p->to.reg;
		if(p->to.type == D_NONE) {
			if(((o1 & Sbit)) == 0)
				ctxt->diag("ineffective ZR destination\n%P", p);
			rt = REGZERO;
		}
		r = p->reg;
		if(r == NREG)
			r = rt;
		v = regoff(ctxt, &p->from);
		o1 = oaddi(o1, v, r, rt);
		break;
	case 3: /* op R<<n[,R],R (shifted register) */
		o1 = oprrr(ctxt, p->as);
		o1 |= p->from.offset; /* includes reg, op, etc */
		rt = p->to.reg;
		if(p->to.type == D_NONE)
			rt = REGZERO;
		r = p->reg;
		if(p->as == AMVN || p->as == AMVNW)
			r = REGZERO;
		else
			if(r == NREG)
				r = rt;
		o1 |= ((r << 5)) | rt;
		break;
	case 4: /* mov $addcon, R; mov $recon, R; mov $racon, R */
		o1 = opirr(ctxt, p->as);
		rt = p->to.reg;
		r = o->param;
		if(r == 0)
			r = REGZERO;
		v = regoff(ctxt, &p->from);
		if(((v & 0xFFF000)) != 0) {
			v >>= 12;
			o1 |= 1 << 22; /* shift, by 12 */
		}
		o1 |= ((((v & 0xFFF)) << 10)) | ((r << 5)) | rt;
		break;
	case 5: /* b s; bl s */
		o1 = opbra(ctxt, p->as);
		if(p->to.sym == nil) {
			o1 |= brdist(ctxt, p, 0, 26, 2);
			break;
		}
		rel = addrel(ctxt->cursym);
		rel->off = ctxt->pc;
		rel->siz = 4;
		rel->sym = p->to.sym;
		rel->add = o1 | ((p->to.offset>>2) & 0x3ffffff);
		rel->type = R_CALLARM64;
		break;
	case 6: /* b ,O(R); bl ,O(R) */
		o1 = opbrr(ctxt, p->as);
		o1 |= p->to.reg << 5;
		rel = addrel(ctxt->cursym);
		rel->off = ctxt->pc;
		rel->siz = 0;
		rel->type = R_CALLIND;
		break;
	case 7: /* beq s */
		o1 = opbra(ctxt, p->as);
		o1 |= brdist(ctxt, p, 0, 19, 2) << 5;
		break;
	case 8: /* lsl $c,[R],R -> ubfm $(W-1)-c,$(-c MOD (W-1)),Rn,Rd */
		rt = p->to.reg;
		rf = p->reg;
		if(rf == NREG)
			rf = rt;
		v = p->from.offset;
		switch(p->as) {
		case AASR:
			o1 = opbfm(ctxt, ASBFM, v, 63, rf, rt);
			break;
		case AASRW:
			o1 = opbfm(ctxt, ASBFMW, v, 31, rf, rt);
			break;
		case ALSL:
			o1 = opbfm(ctxt, AUBFM, ((64 - v)) & 63, 63 - v, rf, rt);
			break;
		case ALSLW:
			o1 = opbfm(ctxt, AUBFMW, ((32 - v)) & 31, 31 - v, rf, rt);
			break;
		case ALSR:
			o1 = opbfm(ctxt, AUBFM, v, 63, rf, rt);
			break;
		case ALSRW:
			o1 = opbfm(ctxt, AUBFMW, v, 31, rf, rt);
			break;
		case AROR:
			o1 = opextr(ctxt, AEXTR, v, rf, rf, rt);
			break;
		case ARORW:
			o1 = opextr(ctxt, AEXTRW, v, rf, rf, rt);
			break;
		default:
			ctxt->diag("bad shift $con\n%P", ctxt->curp);
			break;
		}
		break;
	case 9: /* lsl Rm,[Rn],Rd -> lslv Rm, Rn, Rd */
		o1 = oprrr(ctxt, p->as);
		r = p->reg;
		if(r == NREG)
			r = p->to.reg;
		o1 |= ((p->from.reg << 16)) | ((r << 5)) | p->to.reg;
		break;
	case 10: /* brk/hvc/.../svc [$con] */
		o1 = opimm(ctxt, p->as);
		if(p->to.type != D_NONE)
			o1 |= ((p->to.offset & 0xffff)) << 5;
		break;
	case 11: /* dword */
		aclass(ctxt, &p->to);
		o1 = ctxt->instoffset;
		o2 = ctxt->instoffset >> 32;
		if(p->to.sym != nil) {
			rel = addrel(ctxt->cursym);
			rel->off = ctxt->pc;
			rel->siz = 8;
			rel->sym = p->to.sym;
			rel->add = p->to.offset;
			rel->type = R_ADDR;
			o1 = o2 = 0;
		}
		break;
	case 12: /* movT $lcon, reg */
		o1 = omovlit(ctxt, p->as, p, &p->from, p->to.reg);
		break;
	case 13: /* addop $lcon, [R], R (64 bit literal); cmp $lcon,R -> addop $lcon,R, ZR */
		o1 = omovlit(ctxt, AMOV, p, &p->from, REGTMP);
		if(!o1)
			break;
		rt = p->to.reg;
		if(p->to.type == D_NONE)
			rt = REGZERO;
		r = p->reg;
		if(r == NREG)
			r = rt;
		if(p->to.type != D_NONE && ((p->to.reg == REGSP || r == REGSP))) {
			o2 = opxrrr(ctxt, p->as);
			o2 |= REGTMP << 16;
			o2 |= LSL0_64;
		} else {
			o2 = oprrr(ctxt, p->as);
			o2 |= REGTMP << 16; /* shift is 0 */
		}
		o2 |= r << 5;
		o2 |= rt;
		break;
	case 14: /* word */
		if(aclass(ctxt, &p->to) == C_ADDR)
			ctxt->diag("address constant needs DWORD\n%P", p);
		o1 = ctxt->instoffset;
		if(p->to.sym != nil) {
			// This case happens with words generated
			// in the PC stream as part of the literal pool.
			rel = addrel(ctxt->cursym);
			rel->off = ctxt->pc;
			rel->siz = 4;
			rel->sym = p->to.sym;
			rel->add = p->to.offset;
			rel->type = R_ADDR;
			o1 = 0;
		}
		break;
	case 15: /* mul/mneg/umulh/umull r,[r,]r; madd/msub Rm,Rn,Ra,Rd */
		o1 = oprrr(ctxt, p->as);
		rf = p->from.reg;
		rt = p->to.reg;
		if(p->from3.type == D_REG) {
			r = p->from3.reg;
			ra = p->reg;
			if(ra == NREG)
				ra = REGZERO;
		} else {
			r = p->reg;
			if(r == NREG)
				r = rt;
			ra = REGZERO;
		}
		o1 |= ((rf << 16)) | ((ra << 10)) | ((r << 5)) | rt;
		break;
	case 16: /* XremY R[,R],R -> XdivY; XmsubY */
		o1 = oprrr(ctxt, p->as);
		rf = p->from.reg;
		rt = p->to.reg;
		r = p->reg;
		if(r == NREG)
			r = rt;
		o1 |= ((rf << 16)) | ((r << 5)) | REGTMP;
		o2 = oprrr(ctxt, AMSUBW);
		o2 |= o1 & ((1 << 31)); /* same size */
		o2 |= ((rf << 16)) | ((r << 10)) | ((REGTMP << 5)) | rt;
		break;
	case 17: /* op Rm,[Rn],Rd; default Rn=ZR */
		o1 = oprrr(ctxt, p->as);
		rf = p->from.reg;
		rt = p->to.reg;
		r = p->reg;
		if(p->to.type == D_NONE)
			rt = REGZERO;
		if(r == NREG)
			r = REGZERO;
		o1 |= ((rf << 16)) | ((r << 5)) | rt;
		break;
	case 18: /* csel cond,Rn,Rm,Rd; cinc/cinv/cneg cond,Rn,Rd; cset cond,Rd */
		o1 = oprrr(ctxt, p->as);
		cond = p->from.reg;
		r = p->reg;
		if(r != NREG) {
			if(p->from3.type == D_NONE) {
				/* CINC/CINV/CNEG */
				rf = r;
				cond ^= 1;
			} else
				rf = p->from3.reg; /* CSEL */
		} else {
			/* CSET */
			if(p->from3.type != D_NONE)
				ctxt->diag("invalid combination\n%P", p);
			r = rf = REGZERO;
			cond ^= 1;
		}
		rt = p->to.reg;
		o1 |= ((r << 16)) | ((cond << 12)) | ((rf << 5)) | rt;
		break;
	case 19: /* CCMN cond, (Rm|uimm5),Rn, uimm4 -> ccmn Rn,Rm,uimm4,cond */
		nzcv = p->to.offset;
		cond = p->from.reg;
		if(p->from3.type == D_REG) {
			o1 = oprrr(ctxt, p->as);
			rf = p->from3.reg; /* Rm */
		} else {
			o1 = opirr(ctxt, p->as);
			rf = p->from3.offset & 0x1F;
		}
		o1 |= ((rf << 16)) | ((cond << 12)) | ((p->reg << 5)) | nzcv;
		break;
	case 20: /* movT R,O(R) -> strT */
		v = regoff(ctxt, &p->to);
		r = p->to.reg;
		if(r == NREG)
			r = o->param;
		if(v < 0) { /* unscaled 9-bit signed */
			o1 = olsr9s(ctxt, opstr9(ctxt, p->as), v, r, p->from.reg);
		} else {
			v = offsetshift(ctxt, v, o->a3);
			o1 = olsr12u(ctxt, opstr12(ctxt, p->as), v, r, p->from.reg);
		}
		break;
	case 21: /* movT O(R),R -> ldrT */
		v = regoff(ctxt, &p->from);
		r = p->from.reg;
		if(r == NREG)
			r = o->param;
		if(v < 0) { /* unscaled 9-bit signed */
			o1 = olsr9s(ctxt, opldr9(ctxt, p->as), v, r, p->to.reg);
		} else {
			v = offsetshift(ctxt, v, o->a1);
			//print("offset=%lld v=%ld a1=%d\n", instoffset, v, o->a1);
			o1 = olsr12u(ctxt, opldr12(ctxt, p->as), v, r, p->to.reg);
		}
		break;
	case 22: /* movT (R)O!,R; movT O(R)!, R -> ldrT */
		v = p->from.offset;
		if(v < -256 || v > 255)
			ctxt->diag("offset out of range\n%P", p);
		o1 = opldrpp(ctxt, p->as);
		if(p->from.type == D_XPOST)
			o1 |= 1 << 10;
		else
			o1 |= 3 << 10;
		o1 |= ((((v & 0x1FF)) << 12)) | ((p->from.reg << 5)) | p->to.reg;
		break;
	case 23: /* movT R,(R)O!; movT O(R)!, R -> strT */
		v = p->to.offset;
		if(v < -256 || v > 255)
			ctxt->diag("offset out of range\n%P", p);
		o1 = LD2STR(opldrpp(ctxt, p->as));
		if(p->to.type == D_XPOST)
			o1 |= 1 << 10;
		else
			o1 |= 3 << 10;
		o1 |= ((((v & 0x1FF)) << 12)) | ((p->to.reg << 5)) | p->from.reg;
		break;
	case 24: /* mov/mvn Rs,Rd -> add $0,Rs,Rd or orr Rs,ZR,Rd */
		rf = p->from.reg;
		rt = p->to.reg;
		s = rf == REGSP || rt == REGSP;
		if(p->as == AMVN || p->as == AMVNW) {
			if(s)
				ctxt->diag("illegal SP reference\n%P", p);
			o1 = oprrr(ctxt, p->as);
			o1 |= ((rf << 16)) | ((REGZERO << 5)) | rt;
		} else
			if(s) {
				o1 = opirr(ctxt, p->as);
				o1 |= ((rf << 5)) | rt;
			} else {
				o1 = oprrr(ctxt, p->as);
				o1 |= ((rf << 16)) | ((REGZERO << 5)) | rt;
			}
		break;
	case 25: /* negX Rs, Rd -> subX Rs<<0, ZR, Rd */
		o1 = oprrr(ctxt, p->as);
		rf = p->from.reg;
		rt = p->to.reg;
		o1 |= ((rf << 16)) | ((REGZERO << 5)) | rt;
		break;
	case 26: /* negX Rm<<s, Rd -> subX Rm<<s, ZR, Rd */
		o1 = oprrr(ctxt, p->as);
		o1 |= p->from.offset; /* includes reg, op, etc */
		rt = p->to.reg;
		o1 |= ((REGZERO << 5)) | rt;
		break;
	case 27: /* op Rm<<n[,Rn],Rd (extended register) */
		o1 = opxrrr(ctxt, p->as);
		if(p->from.type == D_EXTREG)
			o1 |= p->from.offset; /* includes reg, op, etc */
		else
			o1 |= p->from.reg << 16;
		rt = p->to.reg;
		if(p->to.type == D_NONE)
			rt = REGZERO;
		r = p->reg;
		if(r == NREG)
			r = rt;
		o1 |= ((r << 5)) | rt;
		break;
	case 28: /* logop $lcon, [R], R (64 bit literal) */
		o1 = omovlit(ctxt, AMOV, p, &p->from, REGTMP);
		if(!o1)
			break;
		r = p->reg;
		if(r == NREG)
			r = p->to.reg;
		o2 = oprrr(ctxt, p->as);
		o2 |= REGTMP << 16; /* shift is 0 */
		o2 |= r << 5;
		o2 |= p->to.reg;
		break;
	case 29: /* op Rn, Rd */
		o1 = oprrr(ctxt, p->as);
		o1 |= p->from.reg << 5 | p->to.reg;
		break;
	case 30: /* movT R,L(R) -> strT */
		s = movesize(o->as);
		if(s < 0)
			ctxt->diag("unexpected long move, op %A tab %A\n%P", p->as, o->as, p);
		v = regoff(ctxt, &p->to);
		if(v < 0)
			ctxt->diag("negative large offset\n%P", p);
		if(((v & ((((1 << s)) - 1)))) != 0)
			ctxt->diag("misaligned offset\n%P", p);
		hi = v - ((v & ((0xFFF << s))));
		if(((hi & 0xFFF)) != 0)
			ctxt->diag("internal: miscalculated offset %ld [%d]\n%P", v, s, p);
		//fprint(2, "v=%ld (%#lux) s=%d hi=%ld (%#lux) v'=%ld (%#lux)\n", v, v, s, hi, hi, ((v-hi)>>s)&0xFFF, ((v-hi)>>s)&0xFFF);
		r = p->to.reg;
		if(r == NREG)
			r = o->param;
		o1 = oaddi(opirr(ctxt, AADD), hi, r, REGTMP);
		o2 = olsr12u(ctxt, opstr12(ctxt, p->as), ((((v - hi)) >> s)) & 0xFFF, REGTMP, p->from.reg);
		break;
	case 31: /* movT L(R), R -> ldrT */
		s = movesize(o->as);
		if(s < 0)
			ctxt->diag("unexpected long move, op %A tab %A\n%P", p->as, o->as, p);
		v = regoff(ctxt, &p->from);
		if(v < 0)
			ctxt->diag("negative large offset\n%P", p);
		if(((v & ((((1 << s)) - 1)))) != 0)
			ctxt->diag("misaligned offset\n%P", p);
		hi = v - ((v & ((0xFFF << s))));
		if(((hi & 0xFFF)) != 0)
			ctxt->diag("internal: miscalculated offset %ld [%d]\n%P", v, s, p);
		//fprint(2, "v=%ld (%#lux) s=%d hi=%ld (%#lux) v'=%ld (%#lux)\n", v, v, s, hi, hi, ((v-hi)>>s)&0xFFF, ((v-hi)>>s)&0xFFF);
		r = p->from.reg;
		if(r == NREG)
			r = o->param;
		o1 = oaddi(opirr(ctxt, AADD), hi, r, REGTMP);
		o2 = olsr12u(ctxt, opldr12(ctxt, p->as), ((((v - hi)) >> s)) & 0xFFF, REGTMP, p->to.reg);
		break;
	case 32: /* mov $con, R -> movz/movn */
		r = 32;
		if(p->as == AMOV)
			r = 64;
		d = p->from.offset;
		s = movcon(d);
		if(s < 0 || s >= r) {
			d = ~d;
			s = movcon(d);
			if(s < 0 || s >= r)
				ctxt->diag("impossible move wide: %#llux\n%P", p->from.offset, p);
			if(p->as == AMOV)
				o1 = opirr(ctxt, AMOVN);
			else
				o1 = opirr(ctxt, AMOVNW);
		} else {
			if(p->as == AMOV)
				o1 = opirr(ctxt, AMOVZ);
			else
				o1 = opirr(ctxt, AMOVZW);
		}
		rt = p->to.reg;
		o1 |= ((((((d >> ((s * 16)))) & 0xFFFF)) << 5)) | ((((s & 3)) << 21)) | rt;
		break;
	case 33: /* movk $uimm16 << pos */
		o1 = opirr(ctxt, p->as);
		d = p->from.offset;
		if(((d >> 16)) != 0)
			ctxt->diag("requires uimm16\n%P", p);
		s = 0;
		if(p->from3.type != D_NONE) {
			if(p->from3.type != D_CONST)
				ctxt->diag("missing bit position\n%P", p);
			s = p->from3.offset;
			if(((s & 0xF)) != 0 || ((s /= 16)) >= 4 || ((o1 & S64)) == 0 && s >= 2)
				ctxt->diag("illegal bit position\n%P", p);
		}
		rt = p->to.reg;
		o1 |= ((((d & 0xFFFF)) << 5)) | ((((s & 3)) << 21)) | rt;
		break;
	case 34: /* mov $lacon,R */
		o1 = omovlit(ctxt, AMOV, p, &p->from, REGTMP);
		if(!o1)
			break;
		o2 = opxrrr(ctxt, AADD);
		o2 |= REGTMP << 16;
		o2 |= LSL0_64;
		r = p->from.reg;
		if(r == NREG)
			r = o->param;
		o2 |= r << 5;
		o2 |= p->to.reg;
		break;
	case 35: /* mov SPR,R -> mrs */
		o1 = oprrr(ctxt, AMRS);
		v = p->from.offset;
		if(((o1 & ((v & ~((3 << 19)))))) != 0)
			ctxt->diag("MRS register value overlap\n%P", p);
		o1 |= v;
		o1 |= p->to.reg;
		break;
	case 36: /* mov R,SPR */
		o1 = oprrr(ctxt, AMSR);
		v = p->to.offset;
		if(((o1 & ((v & ~((3 << 19)))))) != 0)
			ctxt->diag("MSR register value overlap\n%P", p);
		o1 |= v;
		o1 |= p->from.reg;
		break;
	case 37: /* mov $con,PSTATEfield -> MSR [immediate] */
		if(((p->from.offset & ~(uvlong)0xF)) != 0)
			ctxt->diag("illegal immediate for PSTATE field\n%P", p);
		o1 = opirr(ctxt, AMSR);
		o1 |= ((p->from.offset & 0xF)) << 8; /* Crm */
		v = 0;
		for(i = 0; i < nelem(pstatefield); i++)
			if(pstatefield[i].a == p->to.offset) {
				v = pstatefield[i].b;
				break;
			}
		if(v == 0)
			ctxt->diag("illegal PSTATE field for immediate move\n%P", p);
		o1 |= v;
		break;
	case 38: /* clrex [$imm] */
		o1 = opimm(ctxt, p->as);
		if(p->to.type == D_NONE)
			o1 |= 0xF << 8;
		else
			o1 |= ((p->to.offset & 0xF)) << 8;
		break;
	case 39: /* cbz R, rel */
		o1 = opirr(ctxt, p->as);
		o1 |= p->from.reg;
		o1 |= brdist(ctxt, p, 0, 19, 2) << 5;
		break;
	case 40: /* tbz */
		o1 = opirr(ctxt, p->as);
		v = p->from.offset;
		if(v < 0 || v > 63)
			ctxt->diag("illegal bit number\n%P", p);
		o1 |= ((((v & 0x20)) << ((31 - 5)))) | ((((v & 0x1F)) << 19));
		o1 |= brdist(ctxt, p, 0, 14, 2) << 5;
		o1 |= p->reg;
		break;
	case 41: /* eret, nop, others with no operands */
		o1 = op0(ctxt, p->as);
		break;
	case 42: /* bfm R,r,s,R */
		o1 = opbfm(ctxt, p->as, p->from.offset, p->from3.offset, p->reg, p->to.reg);
		break;
	case 43: /* bfm aliases */
		r = p->from.offset;
		s = p->from3.offset;
		rf = p->reg;
		rt = p->to.reg;
		if(rf == NREG)
			rf = rt;
		switch(p->as) {
		case ABFI:
			o1 = opbfm(ctxt, ABFM, 64 - r, s - 1, rf, rt);
			break;
		case ABFIW:
			o1 = opbfm(ctxt, ABFMW, 32 - r, s - 1, rf, rt);
			break;
		case ABFXIL:
			o1 = opbfm(ctxt, ABFM, r, r + s - 1, rf, rt);
			break;
		case ABFXILW:
			o1 = opbfm(ctxt, ABFMW, r, r + s - 1, rf, rt);
			break;
		case ASBFIZ:
			o1 = opbfm(ctxt, ASBFM, 64 - r, s - 1, rf, rt);
			break;
		case ASBFIZW:
			o1 = opbfm(ctxt, ASBFMW, 32 - r, s - 1, rf, rt);
			break;
		case ASBFX:
			o1 = opbfm(ctxt, ASBFM, r, r + s - 1, rf, rt);
			break;
		case ASBFXW:
			o1 = opbfm(ctxt, ASBFMW, r, r + s - 1, rf, rt);
			break;
		case AUBFIZ:
			o1 = opbfm(ctxt, AUBFM, 64 - r, s - 1, rf, rt);
			break;
		case AUBFIZW:
			o1 = opbfm(ctxt, AUBFMW, 32 - r, s - 1, rf, rt);
			break;
		case AUBFX:
			o1 = opbfm(ctxt, AUBFM, r, r + s - 1, rf, rt);
			break;
		case AUBFXW:
			o1 = opbfm(ctxt, AUBFMW, r, r + s - 1, rf, rt);
			break;
		default:
			ctxt->diag("bad bfm alias\n%P", ctxt->curp);
			break;
		}
		break;
	case 44: /* extr $b, Rn, Rm, Rd */
		o1 = opextr(ctxt, p->as, p->from.offset, p->from3.reg, p->reg, p->to.reg);
		break;
	case 45: /* sxt/uxt[bhw] R,R; movT R,R -> sxtT R,R */
		rf = p->from.reg;
		rt = p->to.reg;
		as = p->as;
		if(rf == REGZERO)
			as = AMOVWU; /* clearer in disassembly */
		switch(as) {
		case AMOVB:
		case ASXTB:
			o1 = opbfm(ctxt, ASBFM, 0, 7, rf, rt);
			break;
		case AMOVH:
		case ASXTH:
			o1 = opbfm(ctxt, ASBFM, 0, 15, rf, rt);
			break;
		case AMOVW:
		case ASXTW:
			o1 = opbfm(ctxt, ASBFM, 0, 31, rf, rt);
			break;
		case AMOVBU:
		case AUXTB:
			o1 = opbfm(ctxt, AUBFM, 0, 7, rf, rt);
			break;
		case AMOVHU:
		case AUXTH:
			o1 = opbfm(ctxt, AUBFM, 0, 15, rf, rt);
			break;
		case AMOVWU:
			o1 = oprrr(ctxt, as) | ((rf << 16)) | ((REGZERO << 5)) | rt;
			break;
		case AUXTW:
			o1 = opbfm(ctxt, AUBFM, 0, 31, rf, rt);
			break;
		case ASXTBW:
			o1 = opbfm(ctxt, ASBFMW, 0, 7, rf, rt);
			break;
		case ASXTHW:
			o1 = opbfm(ctxt, ASBFMW, 0, 15, rf, rt);
			break;
		case AUXTBW:
			o1 = opbfm(ctxt, AUBFMW, 0, 7, rf, rt);
			break;
		case AUXTHW:
			o1 = opbfm(ctxt, AUBFMW, 0, 15, rf, rt);
			break;
		default:
			ctxt->diag("bad sxt %A", as);
			break;
		}
		break;
	case 46: /* cls */
		o1 = opbit(ctxt, p->as);
		o1 |= p->from.reg << 5;
		o1 |= p->to.reg;
		break;
	case 47: /* movT R,V(R) -> strT (huge offset) */
		o1 = omovlit(ctxt, AMOVW, p, &p->to, REGTMP);
		if(!o1)
			break;
		r = p->to.reg;
		if(r == NREG)
			r = o->param;
		o2 = olsxrr(ctxt, p->as, REGTMP, r, p->from.reg);
		break;
	case 48: /* movT V(R), R -> ldrT (huge offset) */
		o1 = omovlit(ctxt, AMOVW, p, &p->from, REGTMP);
		if(!o1)
			break;
		r = p->from.reg;
		if(r == NREG)
			r = o->param;
		o2 = olsxrr(ctxt, p->as, REGTMP, r, p->to.reg);
		break;
	case 50: /* sys/sysl */
		o1 = opirr(ctxt, p->as);
		if(((p->from.offset & ~SYSARG4(0x7, 0xF, 0xF, 0x7))) != 0)
			ctxt->diag("illegal SYS argument\n%P", p);
		o1 |= p->from.offset;
		if(p->to.type == D_REG)
			o1 |= p->to.reg;
		else
			if(p->reg != NREG)
				o1 |= p->reg;
			else
				o1 |= 0x1F;
		break;
	case 51: /* dmb */
		o1 = opirr(ctxt, p->as);
		if(p->from.type == D_CONST)
			o1 |= ((p->from.offset & 0xF)) << 8;
		break;
	case 52: /* hint */
		o1 = opirr(ctxt, p->as);
		o1 |= ((p->from.offset & 0x7F)) << 5;
		break;
	case 53: /* and/or/eor/bic/... $bimmN, Rn, Rd -> op (N,r,s), Rn, Rd */
		as = p->as;
		rt = p->to.reg;
		r = p->reg;
		if(r == NREG)
			r = rt;
		if(as == AMOV) {
			as = AORR;
			r = REGZERO;
		} else
			if(as == AMOVW) {
				as = AORRW;
				r = REGZERO;
			}
		o1 = opirr(ctxt, as);
		s = o1 & S64 ? 64 : 32;
		mask = findmask(p->from.offset);
		if(mask == nil)
			mask = findmask(p->from.offset | ((p->from.offset << 32)));
		if(mask != nil) {
			o1 |= ((((mask->r & ((s - 1)))) << 16)) | ((((((mask->s - 1)) & ((s - 1)))) << 10));
			if(s == 64) {
				if(mask->e == 64 && (((uvlong)p->from.offset >> 32)) != 0)
					o1 |= 1 << 22;
			} else {
				u = (uvlong)p->from.offset >> 32;
				if(u != 0 && u != 0xFFFFFFFF)
					ctxt->diag("mask needs 64 bits %#llux\n%P", p->from.offset, p);
			}
		} else
			ctxt->diag("invalid mask %#llux\n%P", p->from.offset, p); /* probably shouldn't happen */
		o1 |= ((r << 5)) | rt;
		break;
	case 54: /* floating point arith */
		o1 = oprrr(ctxt, p->as);
		if(p->from.type == D_FCONST) {
			rf = chipfloat7(ctxt, p->from.u.dval);
			if(rf < 0 || 1) {
				ctxt->diag("invalid floating-point immediate\n%P", p);
				rf = 0;
			}
			rf |= ((1 << 3));
		} else
			rf = p->from.reg;
		rt = p->to.reg;
		r = p->reg;
		if(((o1 & ((0x1F << 24)))) == ((0x1E << 24)) && ((o1 & ((1 << 11)))) == 0) { /* monadic */
			r = rf;
			rf = 0;
		} else
			if(r == NREG)
				r = rt;
		o1 |= ((rf << 16)) | ((r << 5)) | rt;
		break;
	case 56: /* floating point compare */
		o1 = oprrr(ctxt, p->as);
		if(p->from.type == D_FCONST) {
			o1 |= 8; /* zero */
			rf = 0;
		} else
			rf = p->from.reg;
		rt = p->reg;
		o1 |= rf << 16 | rt << 5;
		break;
	case 57: /* floating point conditional compare */
		o1 = oprrr(ctxt, p->as);
		cond = p->from.reg;
		nzcv = p->to.offset;
		if(nzcv & ~0xF)
			ctxt->diag("implausible condition\n%P", p);
		rf = p->reg;
		if(p->from3.type != D_FREG)
			ctxt->diag("illegal FCCMP\n%P", p);
		rt = p->from3.reg;
		o1 |= rf << 16 | cond << 12 | rt << 5 | nzcv;
		break;
	case 58: /* ldxr/ldaxr */
		o1 = opload(ctxt, p->as);
		o1 |= 0x1F << 16;
		o1 |= p->from.reg << 5;
		if(p->reg != NREG)
			o1 |= p->reg << 10;
		else
			o1 |= 0x1F << 10;
		o1 |= p->to.reg;
		break;
	case 59: /* stxr/stlxr */
		o1 = opstore(ctxt, p->as);
		o1 |= p->reg << 16;
		if(p->from3.type != D_NONE)
			o1 |= p->from3.reg << 10;
		else
			o1 |= 0x1F << 10;
		o1 |= p->to.reg << 5;
		o1 |= p->from.reg;
		break;
	case 60: /* adrp label,r */
		d = brdist(ctxt, p, 12, 21, 0);
		o1 = ADR(1, d, p->to.reg);
		break;
	case 61: /* adr label, r */
		d = brdist(ctxt, p, 0, 21, 0);
		o1 = ADR(0, d, p->to.reg);
		break;
	case 62: /* case Rv, Rt -> adr tab, Rt; movw Rt[R<<2], Rl; add Rt, Rl; br (Rl) */
		o1 = ADR(0, 4 * 4, p->to.reg); /* adr 4(pc), Rt */
		o2 = ((2 << 30)) | ((7 << 27)) | ((2 << 22)) | ((1 << 21)) | ((3 << 13)) | ((1 << 12)) | ((2 << 10)) | ((p->from.reg << 16)) | ((p->to.reg << 5)) | REGTMP; /* movw Rt[Rv<<2], REGTMP */
		o3 = oprrr(ctxt, AADD) | ((p->to.reg << 16)) | ((REGTMP << 5)) | REGTMP; /* add Rt, REGTMP */
		o4 = ((0x6b << 25)) | ((0x1F << 16)) | ((REGTMP << 5)); /* br (REGTMP) */
		lastcase = p;
		break;
	case 63: /* bcase */
		if(lastcase == nil) {
			ctxt->diag("missing CASE\n%P", p);
			break;
		}
		if(p->pcond != nil) {
			o1 = p->pcond->pc - ((lastcase->pc + 4 * 4));
			ctxt->diag("FIXME: some relocation needed in bcase\n%P", p);
		}
		break;
	/* reloc ops */
	case 64: /* movT R,addr */
		o1 = omovlit(ctxt, AMOV, p, &p->to, REGTMP);
		if(!o1)
			break;
		o2 = olsr12u(ctxt, opstr12(ctxt,p->as), 0, REGTMP, p->from.reg);
		break;
	case 65: /* movT addr,R */
		o1 = omovlit(ctxt, AMOV, p, &p->from, REGTMP);
		if(!o1)
			break;
		o2 = olsr12u(ctxt, opldr12(ctxt, p->as), 0, REGTMP, p->to.reg);
		break;
	case 90:
		// This is supposed to be something that stops execution.
		// It's not supposed to be reached, ever, but if it is, we'd
		// like to be able to tell how we got there.  Assemble as
		// 0xbea71700 which is guaranteed to raise undefined instruction
		// exception.
		o1 = 0xbea71700;
		break;
	}
	out[0] = o1;
	out[1] = o2;
	out[2] = o3;
	out[3] = o4;
	out[4] = o5;
	return;
}

/*
 * basic Rm op Rn -> Rd (using shifted register with 0)
 * also op Rn -> Rt
 * also Rm*Rn op Ra -> Rd
 */
static int32
oprrr(Link *ctxt, int a)
{
	switch(a) {
	case AADC:
		return S64 | 0 << 30 | 0 << 29 | 0xd0 << 21 | 0 << 10;
	case AADCW:
		return S32 | 0 << 30 | 0 << 29 | 0xd0 << 21 | 0 << 10;
	case AADCS:
		return S64 | 0 << 30 | 1 << 29 | 0xd0 << 21 | 0 << 10;
	case AADCSW:
		return S32 | 0 << 30 | 1 << 29 | 0xd0 << 21 | 0 << 10;
	case ANGC:
	case ASBC:
		return S64 | 1 << 30 | 0 << 29 | 0xd0 << 21 | 0 << 10;
	case ANGCS:
	case ASBCS:
		return S64 | 1 << 30 | 1 << 29 | 0xd0 << 21 | 0 << 10;
	case ANGCW:
	case ASBCW:
		return S32 | 1 << 30 | 0 << 29 | 0xd0 << 21 | 0 << 10;
	case ANGCSW:
	case ASBCSW:
		return S32 | 1 << 30 | 1 << 29 | 0xd0 << 21 | 0 << 10;
	case AADD:
		return S64 | 0 << 30 | 0 << 29 | 0x0b << 24 | 0 << 22 | 0 << 21 | 0 << 10;
	case AADDW:
		return S32 | 0 << 30 | 0 << 29 | 0x0b << 24 | 0 << 22 | 0 << 21 | 0 << 10;
	case ACMN:
	case AADDS:
		return S64 | 0 << 30 | 1 << 29 | 0x0b << 24 | 0 << 22 | 0 << 21 | 0 << 10;
	case ACMNW:
	case AADDSW:
		return S32 | 0 << 30 | 1 << 29 | 0x0b << 24 | 0 << 22 | 0 << 21 | 0 << 10;
	case ASUB:
		return S64 | 1 << 30 | 0 << 29 | 0x0b << 24 | 0 << 22 | 0 << 21 | 0 << 10;
	case ASUBW:
		return S32 | 1 << 30 | 0 << 29 | 0x0b << 24 | 0 << 22 | 0 << 21 | 0 << 10;
	case ACMP:
	case ASUBS:
		return S64 | 1 << 30 | 1 << 29 | 0x0b << 24 | 0 << 22 | 0 << 21 | 0 << 10;
	case ACMPW:
	case ASUBSW:
		return S32 | 1 << 30 | 1 << 29 | 0x0b << 24 | 0 << 22 | 0 << 21 | 0 << 10;
	case AAND:
		return S64 | 0 << 29 | 0xA << 24;
	case AANDW:
		return S32 | 0 << 29 | 0xA << 24;
	case AMOV:
	case AORR:
		return S64 | 1 << 29 | 0xA << 24;
	//	case AMOVW:
	case AMOVWU:
	case AORRW:
		return S32 | 1 << 29 | 0xA << 24;
	case AEOR:
		return S64 | 2 << 29 | 0xA << 24;
	case AEORW:
		return S32 | 2 << 29 | 0xA << 24;
	case AANDS:
		return S64 | 3 << 29 | 0xA << 24;
	case AANDSW:
		return S32 | 3 << 29 | 0xA << 24;
	case ABIC:
		return S64 | 0 << 29 | 0xA << 24 | 1 << 21;
	case ABICW:
		return S32 | 0 << 29 | 0xA << 24 | 1 << 21;
	case ABICS:
		return S64 | 3 << 29 | 0xA << 24 | 1 << 21;
	case ABICSW:
		return S32 | 3 << 29 | 0xA << 24 | 1 << 21;
	case AEON:
		return S64 | 2 << 29 | 0xA << 24 | 1 << 21;
	case AEONW:
		return S32 | 2 << 29 | 0xA << 24 | 1 << 21;
	case AMVN:
	case AORN:
		return S64 | 1 << 29 | 0xA << 24 | 1 << 21;
	case AMVNW:
	case AORNW:
		return S32 | 1 << 29 | 0xA << 24 | 1 << 21;
	case AASR:
		return S64 | OPDP2(10); /* also ASRV */
	case AASRW:
		return S32 | OPDP2(10);
	case ALSL:
		return S64 | OPDP2(8);
	case ALSLW:
		return S32 | OPDP2(8);
	case ALSR:
		return S64 | OPDP2(9);
	case ALSRW:
		return S32 | OPDP2(9);
	case AROR:
		return S64 | OPDP2(11);
	case ARORW:
		return S32 | OPDP2(11);
	case ACCMN:
		return S64 | 0 << 30 | 1 << 29 | 0xD2 << 21 | 0 << 11 | 0 << 10 | 0 << 4; /* cond<<12 | nzcv<<0 */
	case ACCMNW:
		return S32 | 0 << 30 | 1 << 29 | 0xD2 << 21 | 0 << 11 | 0 << 10 | 0 << 4;
	case ACCMP:
		return S64 | 1 << 30 | 1 << 29 | 0xD2 << 21 | 0 << 11 | 0 << 10 | 0 << 4; /* imm5<<16 | cond<<12 | nzcv<<0 */
	case ACCMPW:
		return S32 | 1 << 30 | 1 << 29 | 0xD2 << 21 | 0 << 11 | 0 << 10 | 0 << 4;
	case ACRC32B:
		return S32 | OPDP2(16);
	case ACRC32H:
		return S32 | OPDP2(17);
	case ACRC32W:
		return S32 | OPDP2(18);
	case ACRC32X:
		return S64 | OPDP2(19);
	case ACRC32CB:
		return S32 | OPDP2(20);
	case ACRC32CH:
		return S32 | OPDP2(21);
	case ACRC32CW:
		return S32 | OPDP2(22);
	case ACRC32CX:
		return S64 | OPDP2(23);
	case ACSEL:
		return S64 | 0 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 0 << 10;
	case ACSELW:
		return S32 | 0 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 0 << 10;
	case ACSET:
		return S64 | 0 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 1 << 10;
	case ACSETW:
		return S32 | 0 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 1 << 10;
	case ACSETM:
		return S64 | 1 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 0 << 10;
	case ACSETMW:
		return S32 | 1 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 0 << 10;
	case ACINC:
	case ACSINC:
		return S64 | 0 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 1 << 10;
	case ACINCW:
	case ACSINCW:
		return S32 | 0 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 1 << 10;
	case ACINV:
	case ACSINV:
		return S64 | 1 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 0 << 10;
	case ACINVW:
	case ACSINVW:
		return S32 | 1 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 0 << 10;
	case ACNEG:
	case ACSNEG:
		return S64 | 1 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 1 << 10;
	case ACNEGW:
	case ACSNEGW:
		return S32 | 1 << 30 | 0 << 29 | 0xD4 << 21 | 0 << 11 | 1 << 10;
	case AMUL:
	case AMADD:
		return S64 | 0 << 29 | 0x1B << 24 | 0 << 21 | 0 << 15;
	case AMULW:
	case AMADDW:
		return S32 | 0 << 29 | 0x1B << 24 | 0 << 21 | 0 << 15;
	case AMNEG:
	case AMSUB:
		return S64 | 0 << 29 | 0x1B << 24 | 0 << 21 | 1 << 15;
	case AMNEGW:
	case AMSUBW:
		return S32 | 0 << 29 | 0x1B << 24 | 0 << 21 | 1 << 15;
	case AMRS:
		return SYSOP(1, 2, 0, 0, 0, 0, 0);
	case AMSR:
		return SYSOP(0, 2, 0, 0, 0, 0, 0);
	case ANEG:
		return S64 | 1 << 30 | 0 << 29 | 0xB << 24 | 0 << 21;
	case ANEGW:
		return S32 | 1 << 30 | 0 << 29 | 0xB << 24 | 0 << 21;
	case ANEGS:
		return S64 | 1 << 30 | 1 << 29 | 0xB << 24 | 0 << 21;
	case ANEGSW:
		return S32 | 1 << 30 | 1 << 29 | 0xB << 24 | 0 << 21;
	case AREM:
	case ASDIV:
		return S64 | OPDP2(3);
	case AREMW:
	case ASDIVW:
		return S32 | OPDP2(3);
	case ASMULL:
	case ASMADDL:
		return OPDP3(1, 0, 1, 0);
	case ASMNEGL:
	case ASMSUBL:
		return OPDP3(1, 0, 1, 1);
	case ASMULH:
		return OPDP3(1, 0, 2, 0);
	case AUMULL:
	case AUMADDL:
		return OPDP3(1, 0, 5, 0);
	case AUMNEGL:
	case AUMSUBL:
		return OPDP3(1, 0, 5, 1);
	case AUMULH:
		return OPDP3(1, 0, 6, 0);
	case AUREM:
	case AUDIV:
		return S64 | OPDP2(2);
	case AUREMW:
	case AUDIVW:
		return S32 | OPDP2(2);
	case AAESE:
		return 0x4E << 24 | 2 << 20 | 8 << 16 | 4 << 12 | 2 << 10;
	case AAESD:
		return 0x4E << 24 | 2 << 20 | 8 << 16 | 5 << 12 | 2 << 10;
	case AAESMC:
		return 0x4E << 24 | 2 << 20 | 8 << 16 | 6 << 12 | 2 << 10;
	case AAESIMC:
		return 0x4E << 24 | 2 << 20 | 8 << 16 | 7 << 12 | 2 << 10;
	case ASHA1C:
		return 0x5E << 24 | 0 << 12;
	case ASHA1P:
		return 0x5E << 24 | 1 << 12;
	case ASHA1M:
		return 0x5E << 24 | 2 << 12;
	case ASHA1SU0:
		return 0x5E << 24 | 3 << 12;
	case ASHA256H:
		return 0x5E << 24 | 4 << 12;
	case ASHA256H2:
		return 0x5E << 24 | 5 << 12;
	case ASHA256SU1:
		return 0x5E << 24 | 6 << 12;
	case ASHA1H:
		return 0x5E << 24 | 2 << 20 | 8 << 16 | 0 << 12 | 2 << 10;
	case ASHA1SU1:
		return 0x5E << 24 | 2 << 20 | 8 << 16 | 1 << 12 | 2 << 10;
	case ASHA256SU0:
		return 0x5E << 24 | 2 << 20 | 8 << 16 | 2 << 12 | 2 << 10;
	case AFCVTZSD:
		return FPCVTI(1, 0, 1, 3, 0);
	case AFCVTZSDW:
		return FPCVTI(0, 0, 1, 3, 0);
	case AFCVTZSS:
		return FPCVTI(1, 0, 0, 3, 0);
	case AFCVTZSSW:
		return FPCVTI(0, 0, 0, 3, 0);
	case AFCVTZUD:
		return FPCVTI(1, 0, 1, 3, 1);
	case AFCVTZUDW:
		return FPCVTI(0, 0, 1, 3, 1);
	case AFCVTZUS:
		return FPCVTI(1, 0, 0, 3, 1);
	case AFCVTZUSW:
		return FPCVTI(0, 0, 0, 3, 1);
	case ASCVTFD:
		return FPCVTI(1, 0, 1, 0, 2);
	case ASCVTFS:
		return FPCVTI(1, 0, 0, 0, 2);
	case ASCVTFWD:
		return FPCVTI(0, 0, 1, 0, 2);
	case ASCVTFWS:
		return FPCVTI(0, 0, 0, 0, 2);
	case AUCVTFD:
		return FPCVTI(1, 0, 1, 0, 3);
	case AUCVTFS:
		return FPCVTI(1, 0, 0, 0, 3);
	case AUCVTFWD:
		return FPCVTI(0, 0, 1, 0, 3);
	case AUCVTFWS:
		return FPCVTI(0, 0, 0, 0, 3);
	case AFADDS:
		return FPOP2S(0, 0, 0, 2);
	case AFADDD:
		return FPOP2S(0, 0, 1, 2);
	case AFSUBS:
		return FPOP2S(0, 0, 0, 3);
	case AFSUBD:
		return FPOP2S(0, 0, 1, 3);
	case AFMULS:
		return FPOP2S(0, 0, 0, 0);
	case AFMULD:
		return FPOP2S(0, 0, 1, 0);
	case AFDIVS:
		return FPOP2S(0, 0, 0, 1);
	case AFDIVD:
		return FPOP2S(0, 0, 1, 1);
	case AFMAXS:
		return FPOP2S(0, 0, 0, 4);
	case AFMINS:
		return FPOP2S(0, 0, 0, 5);
	case AFMAXD:
		return FPOP2S(0, 0, 1, 4);
	case AFMIND:
		return FPOP2S(0, 0, 1, 5);
	case AFMAXNMS:
		return FPOP2S(0, 0, 0, 6);
	case AFMAXNMD:
		return FPOP2S(0, 0, 1, 6);
	case AFMINNMS:
		return FPOP2S(0, 0, 0, 7);
	case AFMINNMD:
		return FPOP2S(0, 0, 1, 7);
	case AFNMULS:
		return FPOP2S(0, 0, 0, 8);
	case AFNMULD:
		return FPOP2S(0, 0, 1, 8);
	case AFCMPS:
		return FPCMP(0, 0, 0, 0, 0);
	case AFCMPD:
		return FPCMP(0, 0, 1, 0, 0);
	case AFCMPES:
		return FPCMP(0, 0, 0, 0, 16);
	case AFCMPED:
		return FPCMP(0, 0, 1, 0, 16);
	case AFCCMPS:
		return FPCCMP(0, 0, 0, 0);
	case AFCCMPD:
		return FPCCMP(0, 0, 1, 0);
	case AFCCMPES:
		return FPCCMP(0, 0, 0, 1);
	case AFCCMPED:
		return FPCCMP(0, 0, 1, 1);
	case AFCSELS:
		return 0x1E << 24 | 0 << 22 | 1 << 21 | 3 << 10;
	case AFCSELD:
		return 0x1E << 24 | 1 << 22 | 1 << 21 | 3 << 10;
	case AFMOVS:
		return FPOP1S(0, 0, 0, 0);
	case AFABSS:
		return FPOP1S(0, 0, 0, 1);
	case AFNEGS:
		return FPOP1S(0, 0, 0, 2);
	case AFSQRTS:
		return FPOP1S(0, 0, 0, 3);
	case AFCVTSD:
		return FPOP1S(0, 0, 0, 5);
	case AFCVTSH:
		return FPOP1S(0, 0, 0, 7);
	case AFRINTNS:
		return FPOP1S(0, 0, 0, 8);
	case AFRINTPS:
		return FPOP1S(0, 0, 0, 9);
	case AFRINTMS:
		return FPOP1S(0, 0, 0, 10);
	case AFRINTZS:
		return FPOP1S(0, 0, 0, 11);
	case AFRINTAS:
		return FPOP1S(0, 0, 0, 12);
	case AFRINTXS:
		return FPOP1S(0, 0, 0, 14);
	case AFRINTIS:
		return FPOP1S(0, 0, 0, 15);
	case AFMOVD:
		return FPOP1S(0, 0, 1, 0);
	case AFABSD:
		return FPOP1S(0, 0, 1, 1);
	case AFNEGD:
		return FPOP1S(0, 0, 1, 2);
	case AFSQRTD:
		return FPOP1S(0, 0, 1, 3);
	case AFCVTDS:
		return FPOP1S(0, 0, 1, 4);
	case AFCVTDH:
		return FPOP1S(0, 0, 1, 7);
	case AFRINTND:
		return FPOP1S(0, 0, 1, 8);
	case AFRINTPD:
		return FPOP1S(0, 0, 1, 9);
	case AFRINTMD:
		return FPOP1S(0, 0, 1, 10);
	case AFRINTZD:
		return FPOP1S(0, 0, 1, 11);
	case AFRINTAD:
		return FPOP1S(0, 0, 1, 12);
	case AFRINTXD:
		return FPOP1S(0, 0, 1, 14);
	case AFRINTID:
		return FPOP1S(0, 0, 1, 15);
	case AFCVTHS:
		return FPOP1S(0, 0, 3, 4);
	case AFCVTHD:
		return FPOP1S(0, 0, 3, 5);
	}
	ctxt->diag("bad rrr %d %A", a, a);
	prasm(ctxt->curp);
	return 0;
}

/*
 * imm -> Rd
 * imm op Rn -> Rd
 */
static int32
opirr(Link *ctxt, int a)
{
	switch(a) {
	/* op $addcon, Rn, Rd */
	case AMOV:
	case AADD:
		return S64 | 0 << 30 | 0 << 29 | 0x11 << 24;
	case ACMN:
	case AADDS:
		return S64 | 0 << 30 | 1 << 29 | 0x11 << 24;
	case AMOVW:
	case AADDW:
		return S32 | 0 << 30 | 0 << 29 | 0x11 << 24;
	case ACMNW:
	case AADDSW:
		return S32 | 0 << 30 | 1 << 29 | 0x11 << 24;
	case ASUB:
		return S64 | 1 << 30 | 0 << 29 | 0x11 << 24;
	case ACMP:
	case ASUBS:
		return S64 | 1 << 30 | 1 << 29 | 0x11 << 24;
	case ASUBW:
		return S32 | 1 << 30 | 0 << 29 | 0x11 << 24;
	case ACMPW:
	case ASUBSW:
		return S32 | 1 << 30 | 1 << 29 | 0x11 << 24;
	/* op $imm(SB), Rd; op label, Rd */
	case AADR:
		return 0 << 31 | 0x10 << 24;
	case AADRP:
		return 1 << 31 | 0x10 << 24;
	/* op $bimm, Rn, Rd */
	case AAND:
		return S64 | 0 << 29 | 0x24 << 23;
	case AANDW:
		return S32 | 0 << 29 | 0x24 << 23 | 0 << 22;
	case AORR:
		return S64 | 1 << 29 | 0x24 << 23;
	case AORRW:
		return S32 | 1 << 29 | 0x24 << 23 | 0 << 22;
	case AEOR:
		return S64 | 2 << 29 | 0x24 << 23;
	case AEORW:
		return S32 | 2 << 29 | 0x24 << 23 | 0 << 22;
	case AANDS:
		return S64 | 3 << 29 | 0x24 << 23;
	case AANDSW:
		return S32 | 3 << 29 | 0x24 << 23 | 0 << 22;
	case AASR:
		return S64 | 0 << 29 | 0x26 << 23; /* alias of SBFM */
	case AASRW:
		return S32 | 0 << 29 | 0x26 << 23 | 0 << 22;
	/* op $width, $lsb, Rn, Rd */
	case ABFI:
		return S64 | 2 << 29 | 0x26 << 23 | 1 << 22; /* alias of BFM */
	case ABFIW:
		return S32 | 2 << 29 | 0x26 << 23 | 0 << 22;
	/* op $imms, $immr, Rn, Rd */
	case ABFM:
		return S64 | 1 << 29 | 0x26 << 23 | 1 << 22;
	case ABFMW:
		return S32 | 1 << 29 | 0x26 << 23 | 0 << 22;
	case ASBFM:
		return S64 | 0 << 29 | 0x26 << 23 | 1 << 22;
	case ASBFMW:
		return S32 | 0 << 29 | 0x26 << 23 | 0 << 22;
	case AUBFM:
		return S64 | 2 << 29 | 0x26 << 23 | 1 << 22;
	case AUBFMW:
		return S32 | 2 << 29 | 0x26 << 23 | 0 << 22;
	case ABFXIL:
		return S64 | 1 << 29 | 0x26 << 23 | 1 << 22; /* alias of BFM */
	case ABFXILW:
		return S32 | 1 << 29 | 0x26 << 23 | 0 << 22;
	case AEXTR:
		return S64 | 0 << 29 | 0x27 << 23 | 1 << 22 | 0 << 21;
	case AEXTRW:
		return S32 | 0 << 29 | 0x27 << 23 | 0 << 22 | 0 << 21;
	case ACBNZ:
		return S64 | 0x1A << 25 | 1 << 24;
	case ACBNZW:
		return S32 | 0x1A << 25 | 1 << 24;
	case ACBZ:
		return S64 | 0x1A << 25 | 0 << 24;
	case ACBZW:
		return S32 | 0x1A << 25 | 0 << 24;
	case ACCMN:
		return S64 | 0 << 30 | 1 << 29 | 0xD2 << 21 | 1 << 11 | 0 << 10 | 0 << 4; /* imm5<<16 | cond<<12 | nzcv<<0 */
	case ACCMNW:
		return S32 | 0 << 30 | 1 << 29 | 0xD2 << 21 | 1 << 11 | 0 << 10 | 0 << 4;
	case ACCMP:
		return S64 | 1 << 30 | 1 << 29 | 0xD2 << 21 | 1 << 11 | 0 << 10 | 0 << 4; /* imm5<<16 | cond<<12 | nzcv<<0 */
	case ACCMPW:
		return S32 | 1 << 30 | 1 << 29 | 0xD2 << 21 | 1 << 11 | 0 << 10 | 0 << 4;
	case AMOVK:
		return S64 | 3 << 29 | 0x25 << 23;
	case AMOVKW:
		return S32 | 3 << 29 | 0x25 << 23;
	case AMOVN:
		return S64 | 0 << 29 | 0x25 << 23;
	case AMOVNW:
		return S32 | 0 << 29 | 0x25 << 23;
	case AMOVZ:
		return S64 | 2 << 29 | 0x25 << 23;
	case AMOVZW:
		return S32 | 2 << 29 | 0x25 << 23;
	case AMSR:
		return SYSOP(0, 0, 0, 4, 0, 0, 0x1F); /* MSR (immediate) */
	case AAT:
	case ADC:
	case AIC:
	case ATLBI:
	case ASYS:
		return SYSOP(0, 1, 0, 0, 0, 0, 0);
	case ASYSL:
		return SYSOP(1, 1, 0, 0, 0, 0, 0);
	case ATBZ:
		return 0x36 << 24;
	case ATBNZ:
		return 0x37 << 24;
	case ADSB:
		return SYSOP(0, 0, 3, 3, 0, 4, 0x1F);
	case ADMB:
		return SYSOP(0, 0, 3, 3, 0, 5, 0x1F);
	case AISB:
		return SYSOP(0, 0, 3, 3, 0, 6, 0x1F);
	case AHINT:
		return SYSOP(0, 0, 3, 2, 0, 0, 0x1F);
	}
	ctxt->diag("bad irr %A", a);
	prasm(ctxt->curp);
	return 0;
}

static int32
opbit(Link *ctxt, int a)
{
	switch(a) {
	case ACLS:
		return S64 | OPBIT(5);
	case ACLSW:
		return S32 | OPBIT(5);
	case ACLZ:
		return S64 | OPBIT(4);
	case ACLZW:
		return S32 | OPBIT(4);
	case ARBIT:
		return S64 | OPBIT(0);
	case ARBITW:
		return S32 | OPBIT(0);
	case AREV:
		return S64 | OPBIT(3);
	case AREVW:
		return S32 | OPBIT(2);
	case AREV16:
		return S64 | OPBIT(1);
	case AREV16W:
		return S32 | OPBIT(1);
	case AREV32:
		return S64 | OPBIT(2);
	default:
		ctxt->diag("bad bit op\n%P", ctxt->curp);
		return 0;
	}
}

/*
 * add/subtract extended register
 */
static int32
opxrrr(Link *ctxt, int a)
{
	switch(a) {
	case AADD:
		return S64 | 0 << 30 | 0 << 29 | 0x0b << 24 | 0 << 22 | 1 << 21 | LSL0_64;
	case AADDW:
		return S32 | 0 << 30 | 0 << 29 | 0x0b << 24 | 0 << 22 | 1 << 21 | LSL0_32;
	case ACMN:
	case AADDS:
		return S64 | 0 << 30 | 1 << 29 | 0x0b << 24 | 0 << 22 | 1 << 21 | LSL0_64;
	case ACMNW:
	case AADDSW:
		return S32 | 0 << 30 | 1 << 29 | 0x0b << 24 | 0 << 22 | 1 << 21 | LSL0_32;
	case ASUB:
		return S64 | 1 << 30 | 0 << 29 | 0x0b << 24 | 0 << 22 | 1 << 21 | LSL0_64;
	case ASUBW:
		return S32 | 1 << 30 | 0 << 29 | 0x0b << 24 | 0 << 22 | 1 << 21 | LSL0_32;
	case ACMP:
	case ASUBS:
		return S64 | 1 << 30 | 1 << 29 | 0x0b << 24 | 0 << 22 | 1 << 21 | LSL0_64;
	case ACMPW:
	case ASUBSW:
		return S32 | 1 << 30 | 1 << 29 | 0x0b << 24 | 0 << 22 | 1 << 21 | LSL0_32;
	}
	ctxt->diag("bad opxrrr %A\n%P", a, ctxt->curp);
	return 0;
}

static int32
opimm(Link *ctxt, int a)
{
	switch(a) {
	case ASVC:
		return 0xD4 << 24 | 0 << 21 | 1; /* imm16<<5 */
	case AHVC:
		return 0xD4 << 24 | 0 << 21 | 2;
	case ASMC:
		return 0xD4 << 24 | 0 << 21 | 3;
	case ABRK:
		return 0xD4 << 24 | 1 << 21 | 0;
	case AHLT:
		return 0xD4 << 24 | 2 << 21 | 0;
	case ADCPS1:
		return 0xD4 << 24 | 5 << 21 | 1;
	case ADCPS2:
		return 0xD4 << 24 | 5 << 21 | 2;
	case ADCPS3:
		return 0xD4 << 24 | 5 << 21 | 3;
	case ACLREX:
		return SYSOP(0, 0, 3, 3, 0, 2, 0x1F);
	}
	ctxt->diag("bad imm %A", a);
	prasm(ctxt->curp);
	return 0;
}

static vlong
brdist(Link *ctxt, Prog *p, int preshift, int flen, int shift)
{
	vlong v;
	vlong t;
	v = 0;
	t = 0;
	if(p->pcond) {
		v = ((p->pcond->pc >> preshift)) - ((ctxt->pc >> preshift));
		if(((v & ((((1 << shift)) - 1)))) != 0)
			ctxt->diag("misaligned label\n%P", p);
		v >>= shift;
		t = (vlong)1 << ((flen - 1));
		if(v < -t || v >= t)
			ctxt->diag("branch too far\n%P", p);
	}
	return v & ((((t << 1)) - 1));	
}

/*
 * pc-relative branches
 */
static int32
opbra(Link *ctxt, int a)
{
	switch(a) {
	case ABEQ:
		return OPBcc(0x0);
	case ABNE:
		return OPBcc(0x1);
	case ABCS:
		return OPBcc(0x2);
	case ABHS:
		return OPBcc(0x2);
	case ABCC:
		return OPBcc(0x3);
	case ABLO:
		return OPBcc(0x3);
	case ABMI:
		return OPBcc(0x4);
	case ABPL:
		return OPBcc(0x5);
	case ABVS:
		return OPBcc(0x6);
	case ABVC:
		return OPBcc(0x7);
	case ABHI:
		return OPBcc(0x8);
	case ABLS:
		return OPBcc(0x9);
	case ABGE:
		return OPBcc(0xa);
	case ABLT:
		return OPBcc(0xb);
	case ABGT:
		return OPBcc(0xc);
	case ABLE:
		return OPBcc(0xd); /* imm19<<5 | cond */
	case ADUFFZERO:
	case AB:
		return 0 << 31 | 5 << 26; /* imm26 */
	case ABL:
		return 1 << 31 | 5 << 26;
	}
	ctxt->diag("bad bra %A", a);
	prasm(ctxt->curp);
	return 0;
}

static int32
opbrr(Link *ctxt, int a)
{
	switch(a) {
	case ABL:
		return OPBLR(1); /* BLR */
	case AB:
		return OPBLR(0); /* BR */
	case ARET:
		return OPBLR(2); /* RET */
	}
	ctxt->diag("bad brr %A", a);
	prasm(ctxt->curp);
	return 0;
}

static int32
op0(Link *ctxt, int a)
{
	switch(a) {
	case ADRPS:
		return 0x6B << 25 | 5 << 21 | 0x1F << 16 | 0x1F << 5;
	case AERET:
		return 0x6B << 25 | 4 << 21 | 0x1F << 16 | 0 << 10 | 0x1F << 5;
	case ANOP:
		return SYSHINT(0);
	case AYIELD:
		return SYSHINT(1);
	case AWFE:
		return SYSHINT(2);
	case AWFI:
		return SYSHINT(3);
	case ASEV:
		return SYSHINT(4);
	case ASEVL:
		return SYSHINT(5);
	}
	ctxt->diag("bad op0 %A", a);
	prasm(ctxt->curp);
	return 0;
}

/*
 * register offset
 */
static int32
opload(Link *ctxt, int a)
{
	switch(a) {
	case ALDAR:
		return LDSTX(3, 1, 1, 0, 1) | 0x1F << 10;
	case ALDARW:
		return LDSTX(2, 1, 1, 0, 1) | 0x1F << 10;
	case ALDARB:
		return LDSTX(0, 1, 1, 0, 1) | 0x1F << 10;
	case ALDARH:
		return LDSTX(1, 1, 1, 0, 1) | 0x1F << 10;
	case ALDAXP:
		return LDSTX(3, 0, 1, 1, 1);
	case ALDAXPW:
		return LDSTX(2, 0, 1, 1, 1);
	case ALDAXR:
		return LDSTX(3, 0, 1, 0, 1) | 0x1F << 10;
	case ALDAXRW:
		return LDSTX(2, 1, 1, 0, 1) | 0x1F << 10;
	case ALDAXRB:
		return LDSTX(0, 0, 1, 0, 1) | 0x1F << 10;
	case ALDAXRH:
		return LDSTX(1, 0, 1, 0, 1) | 0x1F << 10;
	case ALDXR:
		return LDSTX(3, 0, 1, 0, 0) | 0x1F << 10;
	case ALDXRB:
		return LDSTX(0, 0, 1, 0, 0) | 0x1F << 10;
	case ALDXRH:
		return LDSTX(1, 0, 1, 0, 0) | 0x1F << 10;
	case ALDXRW:
		return LDSTX(2, 0, 1, 0, 0) | 0x1F << 10;
	case ALDXP:
		return LDSTX(3, 0, 1, 1, 0);
	case ALDXPW:
		return LDSTX(2, 0, 1, 1, 0);
	case AMOVNP:
		return S64 | 0 << 30 | 5 << 27 | 0 << 26 | 0 << 23 | 1 << 22;
	case AMOVNPW:
		return S32 | 0 << 30 | 5 << 27 | 0 << 26 | 0 << 23 | 1 << 22;
	}
	ctxt->diag("bad opload %A\n%P", a, ctxt->curp);
	return 0;
}

static int32
opstore(Link *ctxt, int a)
{
	switch(a) {
	case ASTLR:
		return LDSTX(3, 1, 0, 0, 1) | 0x1F << 10;
	case ASTLRB:
		return LDSTX(0, 1, 0, 0, 1) | 0x1F << 10;
	case ASTLRH:
		return LDSTX(1, 1, 0, 0, 1) | 0x1F << 10;
	case ASTLP:
		return LDSTX(3, 0, 0, 1, 1);
	case ASTLPW:
		return LDSTX(2, 0, 0, 1, 1);
	case ASTLRW:
		return LDSTX(2, 1, 0, 0, 1) | 0x1F << 10;
	case ASTLXP:
		return LDSTX(2, 0, 0, 1, 1);
	case ASTLXPW:
		return LDSTX(3, 0, 0, 1, 1);
	case ASTLXR:
		return LDSTX(3, 0, 0, 0, 1) | 0x1F << 10;
	case ASTLXRB:
		return LDSTX(0, 0, 0, 0, 1) | 0x1F << 10;
	case ASTLXRH:
		return LDSTX(1, 0, 0, 0, 1) | 0x1F << 10;
	case ASTLXRW:
		return LDSTX(2, 0, 0, 0, 1) | 0x1F << 10;
	case ASTXR:
		return LDSTX(3, 0, 0, 0, 0) | 0x1F << 10;
	case ASTXRB:
		return LDSTX(0, 0, 0, 0, 0) | 0x1F << 10;
	case ASTXRH:
		return LDSTX(1, 0, 0, 0, 0) | 0x1F << 10;
	case ASTXP:
		return LDSTX(3, 0, 0, 1, 0);
	case ASTXPW:
		return LDSTX(2, 0, 0, 1, 0);
	case ASTXRW:
		return LDSTX(2, 0, 0, 0, 0) | 0x1F << 10;
	case AMOVNP:
		return S64 | 0 << 30 | 5 << 27 | 0 << 26 | 0 << 23 | 1 << 22;
	case AMOVNPW:
		return S32 | 0 << 30 | 5 << 27 | 0 << 26 | 0 << 23 | 1 << 22;
	}
	ctxt->diag("bad opstore %A\n%P", a, ctxt->curp);
	return 0;
}

/*
 * load/store register (unsigned immediate) C3.3.13
 *	these produce 64-bit values (when there's an option)
 */
static int32
olsr12u(Link *ctxt, int32 o, int32 v, int b, int r)
{
	if(v < 0 || v >= ((1 << 12)))
		ctxt->diag("offset out of range: %ld\n%P", v, ctxt->curp);
	o |= ((v & 0xFFF)) << 10;
	o |= b << 5;
	o |= r;
	return o;
}

static int32
opldr12(Link *ctxt, int a)
{
	switch(a) {
	case AMOV:
		return LDSTR12U(3, 0, 1); /* imm12<<10 | Rn<<5 | Rt */
	case AMOVW:
		return LDSTR12U(2, 0, 2);
	case AMOVWU:
		return LDSTR12U(2, 0, 1);
	case AMOVH:
		return LDSTR12U(1, 0, 2);
	case AMOVHU:
		return LDSTR12U(1, 0, 1);
	case AMOVB:
		return LDSTR12U(0, 0, 2);
	case AMOVBU:
		return LDSTR12U(0, 0, 1);
	case AFMOVS:
		return LDSTR12U(2, 1, 1);
	case AFMOVD:
		return LDSTR12U(3, 1, 1);
	}
	ctxt->diag("bad opldr12 %A\n%P", a, ctxt->curp);
	return 0;
}

static int32
opstr12(Link *ctxt, int a)
{
	return LD2STR(opldr12(ctxt, a));
}

/* 
 * load/store register (unscaled immediate) C3.3.12
 */
static int32
olsr9s(Link *ctxt, int32 o, int32 v, int b, int r)
{
	if(v < -256 || v > 255)
		ctxt->diag("offset out of range: %ld\n%P", v, ctxt->curp);
	o |= ((v & 0x1FF)) << 12;
	o |= b << 5;
	o |= r;
	return o;
}

static int32
opldr9(Link *ctxt, int a)
{
	switch(a) {
	case AMOV:
		return LDSTR9S(3, 0, 1); /* simm9<<12 | Rn<<5 | Rt */
	case AMOVW:
		return LDSTR9S(2, 0, 2);
	case AMOVWU:
		return LDSTR9S(2, 0, 1);
	case AMOVH:
		return LDSTR9S(1, 0, 2);
	case AMOVHU:
		return LDSTR9S(1, 0, 1);
	case AMOVB:
		return LDSTR9S(0, 0, 2);
	case AMOVBU:
		return LDSTR9S(0, 0, 1);
	case AFMOVS:
		return LDSTR9S(2, 1, 1);
	case AFMOVD:
		return LDSTR9S(3, 1, 1);
	}
	ctxt->diag("bad opldr9 %A\n%P", a, ctxt->curp);
	return 0;
}

static int32
opstr9(Link *ctxt, int a)
{
	return LD2STR(opldr9(ctxt, a));
}

static int32
opldrpp(Link *ctxt, int a)
{
	switch(a) {
	case AMOV:
		return 3 << 30 | 7 << 27 | 0 << 26 | 0 << 24 | 1 << 22; /* simm9<<12 | Rn<<5 | Rt */
	case AMOVW:
		return 2 << 30 | 7 << 27 | 0 << 26 | 0 << 24 | 2 << 22;
	case AMOVWU:
		return 2 << 30 | 7 << 27 | 0 << 26 | 0 << 24 | 1 << 22;
	case AMOVH:
		return 1 << 30 | 7 << 27 | 0 << 26 | 0 << 24 | 2 << 22;
	case AMOVHU:
		return 1 << 30 | 7 << 27 | 0 << 26 | 0 << 24 | 1 << 22;
	case AMOVB:
		return 0 << 30 | 7 << 27 | 0 << 26 | 0 << 24 | 2 << 22;
	case AMOVBU:
		return 0 << 30 | 7 << 27 | 0 << 26 | 0 << 24 | 1 << 22;
	}
	ctxt->diag("bad opldr %A\n%P", a, ctxt->curp);
	return 0;
}

/*
 * load/store register (extended register)
 */
static int32
olsxrr(Link *ctxt, int as, int rt, int r1, int r2)
{
	USED(as);
	USED(rt);
	USED(r1);
	USED(r2);
	ctxt->diag("need load/store extended register\n%P", ctxt->curp);
	return -1;
}

static int32
oaddi(int32 o1, int32 v, int r, int rt)
{
	if(((v & 0xFFF000)) != 0) {
		v >>= 12;
		o1 |= 1 << 22;
	}
	o1 |= ((((v & 0xFFF)) << 10)) | ((r << 5)) | rt;
	return o1;
}

/*
 * load a a literal value into dr
 */
static int32
omovlit(Link *ctxt, int as, Prog *p, Addr *a, int dr)
{
	int32 v;
	int32 o1;
	int w;
	int fp;
	if(p->pcond == nil) { /* not in literal pool */
		aclass(ctxt, a);
		fprint(2, "omovlit add %lld (%#llux)\n", ctxt->instoffset, ctxt->instoffset);
		/* TO DO: could be clever, and use general constant builder */
		o1 = opirr(ctxt, AADD);
		v = ctxt->instoffset;
		if(v != 0 && ((v & 0xFFF)) == 0) {
			v >>= 12;
			o1 |= 1 << 22; /* shift, by 12 */
		}
		o1 |= ((((v & 0xFFF)) << 10)) | ((REGZERO << 5)) | dr;
	} else {
		fp = 0;
		w = 0; /* default: 32 bit, unsigned */
		switch(as) {
		case AFMOVS:
			fp = 1;
			break;
		case AFMOVD:
			fp = 1;
			w = 1; /* 64 bit simd&fp */
			break;
		case AMOV:
			if(p->pcond->as == ADWORD)
				w = 1; /* 64 bit */
			else
				if(p->pcond->to.offset < 0)
					w = 2; /* sign extend */
			break;
		case AMOVB:
		case AMOVH:
		case AMOVW:
			w = 2; /* 32 bit, sign-extended to 64 */
			break;
		}
		v = brdist(ctxt, p, 0, 19, 2);
		o1 = ((w << 30)) | ((fp << 26)) | ((3 << 27));
		o1 |= ((v & 0x7FFFF)) << 5;
		o1 |= dr;
	}
	return o1;
}

static int32
opbfm(Link *ctxt, int a, int r, int s, int rf, int rt)
{
	int32 o;
	int32 c;
	o = opirr(ctxt, a);
	if(((o & ((1 << 31)))) == 0)
		c = 32;
	else
		c = 64;
	if(r < 0 || r >= c)
		ctxt->diag("illegal bit number\n%P", ctxt->curp);
	o |= ((r & 0x3F)) << 16;
	if(s < 0 || s >= c)
		ctxt->diag("illegal bit number\n%P", ctxt->curp);
	o |= ((s & 0x3F)) << 10;
	o |= ((rf << 5)) | rt;
	return o;
}

static int32
opextr(Link *ctxt, int a, int32 v, int rn, int rm, int rt)
{
	int32 o;
	int32 c;
	o = opirr(ctxt, a);
	c = ((o & ((1 << 31)))) != 0 ? 63 : 31;
	if(v < 0 || v > c)
		ctxt->diag("illegal bit number\n%P", ctxt->curp);
	o |= v << 10;
	o |= rn << 5;
	o |= rm << 16;
	o |= rt;
	return o;
}

/*
 * size in log2(bytes)
 */
static int
movesize(int a)
{
	switch(a) {
	case AMOV:
		return 3;
	case AMOVW:
	case AMOVWU:
		return 2;
	case AMOVH:
	case AMOVHU:
		return 1;
	case AMOVB:
	case AMOVBU:
		return 0;
	case AFMOVS:
		return 2;
	case AFMOVD:
		return 3;
	default:
		return -1;
	}
}

static Mask bitmasks[] = {
	1,	64,	0,	0x00000000000001LL,
	1,	64,	63,	0x00000000000002LL,
	2,	64,	0,	0x00000000000003LL,
	1,	64,	62,	0x00000000000004LL,
	2,	64,	63,	0x00000000000006LL,
	3,	64,	0,	0x00000000000007LL,
	1,	64,	61,	0x00000000000008LL,
	2,	64,	62,	0x0000000000000cLL,
	3,	64,	63,	0x0000000000000eLL,
	4,	64,	0,	0x0000000000000fLL,
	1,	64,	60,	0x00000000000010LL,
	2,	64,	61,	0x00000000000018LL,
	3,	64,	62,	0x0000000000001cLL,
	4,	64,	63,	0x0000000000001eLL,
	5,	64,	0,	0x0000000000001fLL,
	1,	64,	59,	0x00000000000020LL,
	2,	64,	60,	0x00000000000030LL,
	3,	64,	61,	0x00000000000038LL,
	4,	64,	62,	0x0000000000003cLL,
	5,	64,	63,	0x0000000000003eLL,
	6,	64,	0,	0x0000000000003fLL,
	1,	64,	58,	0x00000000000040LL,
	2,	64,	59,	0x00000000000060LL,
	3,	64,	60,	0x00000000000070LL,
	4,	64,	61,	0x00000000000078LL,
	5,	64,	62,	0x0000000000007cLL,
	6,	64,	63,	0x0000000000007eLL,
	7,	64,	0,	0x0000000000007fLL,
	1,	64,	57,	0x00000000000080LL,
	2,	64,	58,	0x000000000000c0LL,
	3,	64,	59,	0x000000000000e0LL,
	4,	64,	60,	0x000000000000f0LL,
	5,	64,	61,	0x000000000000f8LL,
	6,	64,	62,	0x000000000000fcLL,
	7,	64,	63,	0x000000000000feLL,
	8,	64,	0,	0x000000000000ffLL,
	1,	64,	56,	0x00000000000100LL,
	2,	64,	57,	0x00000000000180LL,
	3,	64,	58,	0x000000000001c0LL,
	4,	64,	59,	0x000000000001e0LL,
	5,	64,	60,	0x000000000001f0LL,
	6,	64,	61,	0x000000000001f8LL,
	7,	64,	62,	0x000000000001fcLL,
	8,	64,	63,	0x000000000001feLL,
	9,	64,	0,	0x000000000001ffLL,
	1,	64,	55,	0x00000000000200LL,
	2,	64,	56,	0x00000000000300LL,
	3,	64,	57,	0x00000000000380LL,
	4,	64,	58,	0x000000000003c0LL,
	5,	64,	59,	0x000000000003e0LL,
	6,	64,	60,	0x000000000003f0LL,
	7,	64,	61,	0x000000000003f8LL,
	8,	64,	62,	0x000000000003fcLL,
	9,	64,	63,	0x000000000003feLL,
	10,	64,	0,	0x000000000003ffLL,
	1,	64,	54,	0x00000000000400LL,
	2,	64,	55,	0x00000000000600LL,
	3,	64,	56,	0x00000000000700LL,
	4,	64,	57,	0x00000000000780LL,
	5,	64,	58,	0x000000000007c0LL,
	6,	64,	59,	0x000000000007e0LL,
	7,	64,	60,	0x000000000007f0LL,
	8,	64,	61,	0x000000000007f8LL,
	9,	64,	62,	0x000000000007fcLL,
	10,	64,	63,	0x000000000007feLL,
	11,	64,	0,	0x000000000007ffLL,
	1,	64,	53,	0x00000000000800LL,
	2,	64,	54,	0x00000000000c00LL,
	3,	64,	55,	0x00000000000e00LL,
	4,	64,	56,	0x00000000000f00LL,
	5,	64,	57,	0x00000000000f80LL,
	6,	64,	58,	0x00000000000fc0LL,
	7,	64,	59,	0x00000000000fe0LL,
	8,	64,	60,	0x00000000000ff0LL,
	9,	64,	61,	0x00000000000ff8LL,
	10,	64,	62,	0x00000000000ffcLL,
	11,	64,	63,	0x00000000000ffeLL,
	12,	64,	0,	0x00000000000fffLL,
	1,	64,	52,	0x00000000001000LL,
	2,	64,	53,	0x00000000001800LL,
	3,	64,	54,	0x00000000001c00LL,
	4,	64,	55,	0x00000000001e00LL,
	5,	64,	56,	0x00000000001f00LL,
	6,	64,	57,	0x00000000001f80LL,
	7,	64,	58,	0x00000000001fc0LL,
	8,	64,	59,	0x00000000001fe0LL,
	9,	64,	60,	0x00000000001ff0LL,
	10,	64,	61,	0x00000000001ff8LL,
	11,	64,	62,	0x00000000001ffcLL,
	12,	64,	63,	0x00000000001ffeLL,
	13,	64,	0,	0x00000000001fffLL,
	1,	64,	51,	0x00000000002000LL,
	2,	64,	52,	0x00000000003000LL,
	3,	64,	53,	0x00000000003800LL,
	4,	64,	54,	0x00000000003c00LL,
	5,	64,	55,	0x00000000003e00LL,
	6,	64,	56,	0x00000000003f00LL,
	7,	64,	57,	0x00000000003f80LL,
	8,	64,	58,	0x00000000003fc0LL,
	9,	64,	59,	0x00000000003fe0LL,
	10,	64,	60,	0x00000000003ff0LL,
	11,	64,	61,	0x00000000003ff8LL,
	12,	64,	62,	0x00000000003ffcLL,
	13,	64,	63,	0x00000000003ffeLL,
	14,	64,	0,	0x00000000003fffLL,
	1,	64,	50,	0x00000000004000LL,
	2,	64,	51,	0x00000000006000LL,
	3,	64,	52,	0x00000000007000LL,
	4,	64,	53,	0x00000000007800LL,
	5,	64,	54,	0x00000000007c00LL,
	6,	64,	55,	0x00000000007e00LL,
	7,	64,	56,	0x00000000007f00LL,
	8,	64,	57,	0x00000000007f80LL,
	9,	64,	58,	0x00000000007fc0LL,
	10,	64,	59,	0x00000000007fe0LL,
	11,	64,	60,	0x00000000007ff0LL,
	12,	64,	61,	0x00000000007ff8LL,
	13,	64,	62,	0x00000000007ffcLL,
	14,	64,	63,	0x00000000007ffeLL,
	15,	64,	0,	0x00000000007fffLL,
	1,	64,	49,	0x00000000008000LL,
	2,	64,	50,	0x0000000000c000LL,
	3,	64,	51,	0x0000000000e000LL,
	4,	64,	52,	0x0000000000f000LL,
	5,	64,	53,	0x0000000000f800LL,
	6,	64,	54,	0x0000000000fc00LL,
	7,	64,	55,	0x0000000000fe00LL,
	8,	64,	56,	0x0000000000ff00LL,
	9,	64,	57,	0x0000000000ff80LL,
	10,	64,	58,	0x0000000000ffc0LL,
	11,	64,	59,	0x0000000000ffe0LL,
	12,	64,	60,	0x0000000000fff0LL,
	13,	64,	61,	0x0000000000fff8LL,
	14,	64,	62,	0x0000000000fffcLL,
	15,	64,	63,	0x0000000000fffeLL,
	16,	64,	0,	0x0000000000ffffLL,
	1,	64,	48,	0x00000000010000LL,
	2,	64,	49,	0x00000000018000LL,
	3,	64,	50,	0x0000000001c000LL,
	4,	64,	51,	0x0000000001e000LL,
	5,	64,	52,	0x0000000001f000LL,
	6,	64,	53,	0x0000000001f800LL,
	7,	64,	54,	0x0000000001fc00LL,
	8,	64,	55,	0x0000000001fe00LL,
	9,	64,	56,	0x0000000001ff00LL,
	10,	64,	57,	0x0000000001ff80LL,
	11,	64,	58,	0x0000000001ffc0LL,
	12,	64,	59,	0x0000000001ffe0LL,
	13,	64,	60,	0x0000000001fff0LL,
	14,	64,	61,	0x0000000001fff8LL,
	15,	64,	62,	0x0000000001fffcLL,
	16,	64,	63,	0x0000000001fffeLL,
	17,	64,	0,	0x0000000001ffffLL,
	1,	64,	47,	0x00000000020000LL,
	2,	64,	48,	0x00000000030000LL,
	3,	64,	49,	0x00000000038000LL,
	4,	64,	50,	0x0000000003c000LL,
	5,	64,	51,	0x0000000003e000LL,
	6,	64,	52,	0x0000000003f000LL,
	7,	64,	53,	0x0000000003f800LL,
	8,	64,	54,	0x0000000003fc00LL,
	9,	64,	55,	0x0000000003fe00LL,
	10,	64,	56,	0x0000000003ff00LL,
	11,	64,	57,	0x0000000003ff80LL,
	12,	64,	58,	0x0000000003ffc0LL,
	13,	64,	59,	0x0000000003ffe0LL,
	14,	64,	60,	0x0000000003fff0LL,
	15,	64,	61,	0x0000000003fff8LL,
	16,	64,	62,	0x0000000003fffcLL,
	17,	64,	63,	0x0000000003fffeLL,
	18,	64,	0,	0x0000000003ffffLL,
	1,	64,	46,	0x00000000040000LL,
	2,	64,	47,	0x00000000060000LL,
	3,	64,	48,	0x00000000070000LL,
	4,	64,	49,	0x00000000078000LL,
	5,	64,	50,	0x0000000007c000LL,
	6,	64,	51,	0x0000000007e000LL,
	7,	64,	52,	0x0000000007f000LL,
	8,	64,	53,	0x0000000007f800LL,
	9,	64,	54,	0x0000000007fc00LL,
	10,	64,	55,	0x0000000007fe00LL,
	11,	64,	56,	0x0000000007ff00LL,
	12,	64,	57,	0x0000000007ff80LL,
	13,	64,	58,	0x0000000007ffc0LL,
	14,	64,	59,	0x0000000007ffe0LL,
	15,	64,	60,	0x0000000007fff0LL,
	16,	64,	61,	0x0000000007fff8LL,
	17,	64,	62,	0x0000000007fffcLL,
	18,	64,	63,	0x0000000007fffeLL,
	19,	64,	0,	0x0000000007ffffLL,
	1,	64,	45,	0x00000000080000LL,
	2,	64,	46,	0x000000000c0000LL,
	3,	64,	47,	0x000000000e0000LL,
	4,	64,	48,	0x000000000f0000LL,
	5,	64,	49,	0x000000000f8000LL,
	6,	64,	50,	0x000000000fc000LL,
	7,	64,	51,	0x000000000fe000LL,
	8,	64,	52,	0x000000000ff000LL,
	9,	64,	53,	0x000000000ff800LL,
	10,	64,	54,	0x000000000ffc00LL,
	11,	64,	55,	0x000000000ffe00LL,
	12,	64,	56,	0x000000000fff00LL,
	13,	64,	57,	0x000000000fff80LL,
	14,	64,	58,	0x000000000fffc0LL,
	15,	64,	59,	0x000000000fffe0LL,
	16,	64,	60,	0x000000000ffff0LL,
	17,	64,	61,	0x000000000ffff8LL,
	18,	64,	62,	0x000000000ffffcLL,
	19,	64,	63,	0x000000000ffffeLL,
	20,	64,	0,	0x000000000fffffLL,
	1,	64,	44,	0x00000000100000LL,
	2,	64,	45,	0x00000000180000LL,
	3,	64,	46,	0x000000001c0000LL,
	4,	64,	47,	0x000000001e0000LL,
	5,	64,	48,	0x000000001f0000LL,
	6,	64,	49,	0x000000001f8000LL,
	7,	64,	50,	0x000000001fc000LL,
	8,	64,	51,	0x000000001fe000LL,
	9,	64,	52,	0x000000001ff000LL,
	10,	64,	53,	0x000000001ff800LL,
	11,	64,	54,	0x000000001ffc00LL,
	12,	64,	55,	0x000000001ffe00LL,
	13,	64,	56,	0x000000001fff00LL,
	14,	64,	57,	0x000000001fff80LL,
	15,	64,	58,	0x000000001fffc0LL,
	16,	64,	59,	0x000000001fffe0LL,
	17,	64,	60,	0x000000001ffff0LL,
	18,	64,	61,	0x000000001ffff8LL,
	19,	64,	62,	0x000000001ffffcLL,
	20,	64,	63,	0x000000001ffffeLL,
	21,	64,	0,	0x000000001fffffLL,
	1,	64,	43,	0x00000000200000LL,
	2,	64,	44,	0x00000000300000LL,
	3,	64,	45,	0x00000000380000LL,
	4,	64,	46,	0x000000003c0000LL,
	5,	64,	47,	0x000000003e0000LL,
	6,	64,	48,	0x000000003f0000LL,
	7,	64,	49,	0x000000003f8000LL,
	8,	64,	50,	0x000000003fc000LL,
	9,	64,	51,	0x000000003fe000LL,
	10,	64,	52,	0x000000003ff000LL,
	11,	64,	53,	0x000000003ff800LL,
	12,	64,	54,	0x000000003ffc00LL,
	13,	64,	55,	0x000000003ffe00LL,
	14,	64,	56,	0x000000003fff00LL,
	15,	64,	57,	0x000000003fff80LL,
	16,	64,	58,	0x000000003fffc0LL,
	17,	64,	59,	0x000000003fffe0LL,
	18,	64,	60,	0x000000003ffff0LL,
	19,	64,	61,	0x000000003ffff8LL,
	20,	64,	62,	0x000000003ffffcLL,
	21,	64,	63,	0x000000003ffffeLL,
	22,	64,	0,	0x000000003fffffLL,
	1,	64,	42,	0x00000000400000LL,
	2,	64,	43,	0x00000000600000LL,
	3,	64,	44,	0x00000000700000LL,
	4,	64,	45,	0x00000000780000LL,
	5,	64,	46,	0x000000007c0000LL,
	6,	64,	47,	0x000000007e0000LL,
	7,	64,	48,	0x000000007f0000LL,
	8,	64,	49,	0x000000007f8000LL,
	9,	64,	50,	0x000000007fc000LL,
	10,	64,	51,	0x000000007fe000LL,
	11,	64,	52,	0x000000007ff000LL,
	12,	64,	53,	0x000000007ff800LL,
	13,	64,	54,	0x000000007ffc00LL,
	14,	64,	55,	0x000000007ffe00LL,
	15,	64,	56,	0x000000007fff00LL,
	16,	64,	57,	0x000000007fff80LL,
	17,	64,	58,	0x000000007fffc0LL,
	18,	64,	59,	0x000000007fffe0LL,
	19,	64,	60,	0x000000007ffff0LL,
	20,	64,	61,	0x000000007ffff8LL,
	21,	64,	62,	0x000000007ffffcLL,
	22,	64,	63,	0x000000007ffffeLL,
	23,	64,	0,	0x000000007fffffLL,
	1,	64,	41,	0x00000000800000LL,
	2,	64,	42,	0x00000000c00000LL,
	3,	64,	43,	0x00000000e00000LL,
	4,	64,	44,	0x00000000f00000LL,
	5,	64,	45,	0x00000000f80000LL,
	6,	64,	46,	0x00000000fc0000LL,
	7,	64,	47,	0x00000000fe0000LL,
	8,	64,	48,	0x00000000ff0000LL,
	9,	64,	49,	0x00000000ff8000LL,
	10,	64,	50,	0x00000000ffc000LL,
	11,	64,	51,	0x00000000ffe000LL,
	12,	64,	52,	0x00000000fff000LL,
	13,	64,	53,	0x00000000fff800LL,
	14,	64,	54,	0x00000000fffc00LL,
	15,	64,	55,	0x00000000fffe00LL,
	16,	64,	56,	0x00000000ffff00LL,
	17,	64,	57,	0x00000000ffff80LL,
	18,	64,	58,	0x00000000ffffc0LL,
	19,	64,	59,	0x00000000ffffe0LL,
	20,	64,	60,	0x00000000fffff0LL,
	21,	64,	61,	0x00000000fffff8LL,
	22,	64,	62,	0x00000000fffffcLL,
	23,	64,	63,	0x00000000fffffeLL,
	24,	64,	0,	0x00000000ffffffLL,
	1,	64,	40,	0x00000001000000LL,
	2,	64,	41,	0x00000001800000LL,
	3,	64,	42,	0x00000001c00000LL,
	4,	64,	43,	0x00000001e00000LL,
	5,	64,	44,	0x00000001f00000LL,
	6,	64,	45,	0x00000001f80000LL,
	7,	64,	46,	0x00000001fc0000LL,
	8,	64,	47,	0x00000001fe0000LL,
	9,	64,	48,	0x00000001ff0000LL,
	10,	64,	49,	0x00000001ff8000LL,
	11,	64,	50,	0x00000001ffc000LL,
	12,	64,	51,	0x00000001ffe000LL,
	13,	64,	52,	0x00000001fff000LL,
	14,	64,	53,	0x00000001fff800LL,
	15,	64,	54,	0x00000001fffc00LL,
	16,	64,	55,	0x00000001fffe00LL,
	17,	64,	56,	0x00000001ffff00LL,
	18,	64,	57,	0x00000001ffff80LL,
	19,	64,	58,	0x00000001ffffc0LL,
	20,	64,	59,	0x00000001ffffe0LL,
	21,	64,	60,	0x00000001fffff0LL,
	22,	64,	61,	0x00000001fffff8LL,
	23,	64,	62,	0x00000001fffffcLL,
	24,	64,	63,	0x00000001fffffeLL,
	25,	64,	0,	0x00000001ffffffLL,
	1,	64,	39,	0x00000002000000LL,
	2,	64,	40,	0x00000003000000LL,
	3,	64,	41,	0x00000003800000LL,
	4,	64,	42,	0x00000003c00000LL,
	5,	64,	43,	0x00000003e00000LL,
	6,	64,	44,	0x00000003f00000LL,
	7,	64,	45,	0x00000003f80000LL,
	8,	64,	46,	0x00000003fc0000LL,
	9,	64,	47,	0x00000003fe0000LL,
	10,	64,	48,	0x00000003ff0000LL,
	11,	64,	49,	0x00000003ff8000LL,
	12,	64,	50,	0x00000003ffc000LL,
	13,	64,	51,	0x00000003ffe000LL,
	14,	64,	52,	0x00000003fff000LL,
	15,	64,	53,	0x00000003fff800LL,
	16,	64,	54,	0x00000003fffc00LL,
	17,	64,	55,	0x00000003fffe00LL,
	18,	64,	56,	0x00000003ffff00LL,
	19,	64,	57,	0x00000003ffff80LL,
	20,	64,	58,	0x00000003ffffc0LL,
	21,	64,	59,	0x00000003ffffe0LL,
	22,	64,	60,	0x00000003fffff0LL,
	23,	64,	61,	0x00000003fffff8LL,
	24,	64,	62,	0x00000003fffffcLL,
	25,	64,	63,	0x00000003fffffeLL,
	26,	64,	0,	0x00000003ffffffLL,
	1,	64,	38,	0x00000004000000LL,
	2,	64,	39,	0x00000006000000LL,
	3,	64,	40,	0x00000007000000LL,
	4,	64,	41,	0x00000007800000LL,
	5,	64,	42,	0x00000007c00000LL,
	6,	64,	43,	0x00000007e00000LL,
	7,	64,	44,	0x00000007f00000LL,
	8,	64,	45,	0x00000007f80000LL,
	9,	64,	46,	0x00000007fc0000LL,
	10,	64,	47,	0x00000007fe0000LL,
	11,	64,	48,	0x00000007ff0000LL,
	12,	64,	49,	0x00000007ff8000LL,
	13,	64,	50,	0x00000007ffc000LL,
	14,	64,	51,	0x00000007ffe000LL,
	15,	64,	52,	0x00000007fff000LL,
	16,	64,	53,	0x00000007fff800LL,
	17,	64,	54,	0x00000007fffc00LL,
	18,	64,	55,	0x00000007fffe00LL,
	19,	64,	56,	0x00000007ffff00LL,
	20,	64,	57,	0x00000007ffff80LL,
	21,	64,	58,	0x00000007ffffc0LL,
	22,	64,	59,	0x00000007ffffe0LL,
	23,	64,	60,	0x00000007fffff0LL,
	24,	64,	61,	0x00000007fffff8LL,
	25,	64,	62,	0x00000007fffffcLL,
	26,	64,	63,	0x00000007fffffeLL,
	27,	64,	0,	0x00000007ffffffLL,
	1,	64,	37,	0x00000008000000LL,
	2,	64,	38,	0x0000000c000000LL,
	3,	64,	39,	0x0000000e000000LL,
	4,	64,	40,	0x0000000f000000LL,
	5,	64,	41,	0x0000000f800000LL,
	6,	64,	42,	0x0000000fc00000LL,
	7,	64,	43,	0x0000000fe00000LL,
	8,	64,	44,	0x0000000ff00000LL,
	9,	64,	45,	0x0000000ff80000LL,
	10,	64,	46,	0x0000000ffc0000LL,
	11,	64,	47,	0x0000000ffe0000LL,
	12,	64,	48,	0x0000000fff0000LL,
	13,	64,	49,	0x0000000fff8000LL,
	14,	64,	50,	0x0000000fffc000LL,
	15,	64,	51,	0x0000000fffe000LL,
	16,	64,	52,	0x0000000ffff000LL,
	17,	64,	53,	0x0000000ffff800LL,
	18,	64,	54,	0x0000000ffffc00LL,
	19,	64,	55,	0x0000000ffffe00LL,
	20,	64,	56,	0x0000000fffff00LL,
	21,	64,	57,	0x0000000fffff80LL,
	22,	64,	58,	0x0000000fffffc0LL,
	23,	64,	59,	0x0000000fffffe0LL,
	24,	64,	60,	0x0000000ffffff0LL,
	25,	64,	61,	0x0000000ffffff8LL,
	26,	64,	62,	0x0000000ffffffcLL,
	27,	64,	63,	0x0000000ffffffeLL,
	28,	64,	0,	0x0000000fffffffLL,
	1,	64,	36,	0x00000010000000LL,
	2,	64,	37,	0x00000018000000LL,
	3,	64,	38,	0x0000001c000000LL,
	4,	64,	39,	0x0000001e000000LL,
	5,	64,	40,	0x0000001f000000LL,
	6,	64,	41,	0x0000001f800000LL,
	7,	64,	42,	0x0000001fc00000LL,
	8,	64,	43,	0x0000001fe00000LL,
	9,	64,	44,	0x0000001ff00000LL,
	10,	64,	45,	0x0000001ff80000LL,
	11,	64,	46,	0x0000001ffc0000LL,
	12,	64,	47,	0x0000001ffe0000LL,
	13,	64,	48,	0x0000001fff0000LL,
	14,	64,	49,	0x0000001fff8000LL,
	15,	64,	50,	0x0000001fffc000LL,
	16,	64,	51,	0x0000001fffe000LL,
	17,	64,	52,	0x0000001ffff000LL,
	18,	64,	53,	0x0000001ffff800LL,
	19,	64,	54,	0x0000001ffffc00LL,
	20,	64,	55,	0x0000001ffffe00LL,
	21,	64,	56,	0x0000001fffff00LL,
	22,	64,	57,	0x0000001fffff80LL,
	23,	64,	58,	0x0000001fffffc0LL,
	24,	64,	59,	0x0000001fffffe0LL,
	25,	64,	60,	0x0000001ffffff0LL,
	26,	64,	61,	0x0000001ffffff8LL,
	27,	64,	62,	0x0000001ffffffcLL,
	28,	64,	63,	0x0000001ffffffeLL,
	29,	64,	0,	0x0000001fffffffLL,
	1,	64,	35,	0x00000020000000LL,
	2,	64,	36,	0x00000030000000LL,
	3,	64,	37,	0x00000038000000LL,
	4,	64,	38,	0x0000003c000000LL,
	5,	64,	39,	0x0000003e000000LL,
	6,	64,	40,	0x0000003f000000LL,
	7,	64,	41,	0x0000003f800000LL,
	8,	64,	42,	0x0000003fc00000LL,
	9,	64,	43,	0x0000003fe00000LL,
	10,	64,	44,	0x0000003ff00000LL,
	11,	64,	45,	0x0000003ff80000LL,
	12,	64,	46,	0x0000003ffc0000LL,
	13,	64,	47,	0x0000003ffe0000LL,
	14,	64,	48,	0x0000003fff0000LL,
	15,	64,	49,	0x0000003fff8000LL,
	16,	64,	50,	0x0000003fffc000LL,
	17,	64,	51,	0x0000003fffe000LL,
	18,	64,	52,	0x0000003ffff000LL,
	19,	64,	53,	0x0000003ffff800LL,
	20,	64,	54,	0x0000003ffffc00LL,
	21,	64,	55,	0x0000003ffffe00LL,
	22,	64,	56,	0x0000003fffff00LL,
	23,	64,	57,	0x0000003fffff80LL,
	24,	64,	58,	0x0000003fffffc0LL,
	25,	64,	59,	0x0000003fffffe0LL,
	26,	64,	60,	0x0000003ffffff0LL,
	27,	64,	61,	0x0000003ffffff8LL,
	28,	64,	62,	0x0000003ffffffcLL,
	29,	64,	63,	0x0000003ffffffeLL,
	30,	64,	0,	0x0000003fffffffLL,
	1,	64,	34,	0x00000040000000LL,
	2,	64,	35,	0x00000060000000LL,
	3,	64,	36,	0x00000070000000LL,
	4,	64,	37,	0x00000078000000LL,
	5,	64,	38,	0x0000007c000000LL,
	6,	64,	39,	0x0000007e000000LL,
	7,	64,	40,	0x0000007f000000LL,
	8,	64,	41,	0x0000007f800000LL,
	9,	64,	42,	0x0000007fc00000LL,
	10,	64,	43,	0x0000007fe00000LL,
	11,	64,	44,	0x0000007ff00000LL,
	12,	64,	45,	0x0000007ff80000LL,
	13,	64,	46,	0x0000007ffc0000LL,
	14,	64,	47,	0x0000007ffe0000LL,
	15,	64,	48,	0x0000007fff0000LL,
	16,	64,	49,	0x0000007fff8000LL,
	17,	64,	50,	0x0000007fffc000LL,
	18,	64,	51,	0x0000007fffe000LL,
	19,	64,	52,	0x0000007ffff000LL,
	20,	64,	53,	0x0000007ffff800LL,
	21,	64,	54,	0x0000007ffffc00LL,
	22,	64,	55,	0x0000007ffffe00LL,
	23,	64,	56,	0x0000007fffff00LL,
	24,	64,	57,	0x0000007fffff80LL,
	25,	64,	58,	0x0000007fffffc0LL,
	26,	64,	59,	0x0000007fffffe0LL,
	27,	64,	60,	0x0000007ffffff0LL,
	28,	64,	61,	0x0000007ffffff8LL,
	29,	64,	62,	0x0000007ffffffcLL,
	30,	64,	63,	0x0000007ffffffeLL,
	31,	64,	0,	0x0000007fffffffLL,
	1,	64,	33,	0x00000080000000LL,
	2,	64,	34,	0x000000c0000000LL,
	3,	64,	35,	0x000000e0000000LL,
	4,	64,	36,	0x000000f0000000LL,
	5,	64,	37,	0x000000f8000000LL,
	6,	64,	38,	0x000000fc000000LL,
	7,	64,	39,	0x000000fe000000LL,
	8,	64,	40,	0x000000ff000000LL,
	9,	64,	41,	0x000000ff800000LL,
	10,	64,	42,	0x000000ffc00000LL,
	11,	64,	43,	0x000000ffe00000LL,
	12,	64,	44,	0x000000fff00000LL,
	13,	64,	45,	0x000000fff80000LL,
	14,	64,	46,	0x000000fffc0000LL,
	15,	64,	47,	0x000000fffe0000LL,
	16,	64,	48,	0x000000ffff0000LL,
	17,	64,	49,	0x000000ffff8000LL,
	18,	64,	50,	0x000000ffffc000LL,
	19,	64,	51,	0x000000ffffe000LL,
	20,	64,	52,	0x000000fffff000LL,
	21,	64,	53,	0x000000fffff800LL,
	22,	64,	54,	0x000000fffffc00LL,
	23,	64,	55,	0x000000fffffe00LL,
	24,	64,	56,	0x000000ffffff00LL,
	25,	64,	57,	0x000000ffffff80LL,
	26,	64,	58,	0x000000ffffffc0LL,
	27,	64,	59,	0x000000ffffffe0LL,
	28,	64,	60,	0x000000fffffff0LL,
	29,	64,	61,	0x000000fffffff8LL,
	30,	64,	62,	0x000000fffffffcLL,
	31,	64,	63,	0x000000fffffffeLL,
	32,	64,	0,	0x000000ffffffffLL,
	1,	64,	32,	0x00000100000000LL,
	1,	32,	0,	0x00000100000001LL,
	2,	64,	33,	0x00000180000000LL,
	3,	64,	34,	0x000001c0000000LL,
	4,	64,	35,	0x000001e0000000LL,
	5,	64,	36,	0x000001f0000000LL,
	6,	64,	37,	0x000001f8000000LL,
	7,	64,	38,	0x000001fc000000LL,
	8,	64,	39,	0x000001fe000000LL,
	9,	64,	40,	0x000001ff000000LL,
	10,	64,	41,	0x000001ff800000LL,
	11,	64,	42,	0x000001ffc00000LL,
	12,	64,	43,	0x000001ffe00000LL,
	13,	64,	44,	0x000001fff00000LL,
	14,	64,	45,	0x000001fff80000LL,
	15,	64,	46,	0x000001fffc0000LL,
	16,	64,	47,	0x000001fffe0000LL,
	17,	64,	48,	0x000001ffff0000LL,
	18,	64,	49,	0x000001ffff8000LL,
	19,	64,	50,	0x000001ffffc000LL,
	20,	64,	51,	0x000001ffffe000LL,
	21,	64,	52,	0x000001fffff000LL,
	22,	64,	53,	0x000001fffff800LL,
	23,	64,	54,	0x000001fffffc00LL,
	24,	64,	55,	0x000001fffffe00LL,
	25,	64,	56,	0x000001ffffff00LL,
	26,	64,	57,	0x000001ffffff80LL,
	27,	64,	58,	0x000001ffffffc0LL,
	28,	64,	59,	0x000001ffffffe0LL,
	29,	64,	60,	0x000001fffffff0LL,
	30,	64,	61,	0x000001fffffff8LL,
	31,	64,	62,	0x000001fffffffcLL,
	32,	64,	63,	0x000001fffffffeLL,
	33,	64,	0,	0x000001ffffffffLL,
	1,	64,	31,	0x00000200000000LL,
	1,	32,	31,	0x00000200000002LL,
	2,	64,	32,	0x00000300000000LL,
	2,	32,	0,	0x00000300000003LL,
	3,	64,	33,	0x00000380000000LL,
	4,	64,	34,	0x000003c0000000LL,
	5,	64,	35,	0x000003e0000000LL,
	6,	64,	36,	0x000003f0000000LL,
	7,	64,	37,	0x000003f8000000LL,
	8,	64,	38,	0x000003fc000000LL,
	9,	64,	39,	0x000003fe000000LL,
	10,	64,	40,	0x000003ff000000LL,
	11,	64,	41,	0x000003ff800000LL,
	12,	64,	42,	0x000003ffc00000LL,
	13,	64,	43,	0x000003ffe00000LL,
	14,	64,	44,	0x000003fff00000LL,
	15,	64,	45,	0x000003fff80000LL,
	16,	64,	46,	0x000003fffc0000LL,
	17,	64,	47,	0x000003fffe0000LL,
	18,	64,	48,	0x000003ffff0000LL,
	19,	64,	49,	0x000003ffff8000LL,
	20,	64,	50,	0x000003ffffc000LL,
	21,	64,	51,	0x000003ffffe000LL,
	22,	64,	52,	0x000003fffff000LL,
	23,	64,	53,	0x000003fffff800LL,
	24,	64,	54,	0x000003fffffc00LL,
	25,	64,	55,	0x000003fffffe00LL,
	26,	64,	56,	0x000003ffffff00LL,
	27,	64,	57,	0x000003ffffff80LL,
	28,	64,	58,	0x000003ffffffc0LL,
	29,	64,	59,	0x000003ffffffe0LL,
	30,	64,	60,	0x000003fffffff0LL,
	31,	64,	61,	0x000003fffffff8LL,
	32,	64,	62,	0x000003fffffffcLL,
	33,	64,	63,	0x000003fffffffeLL,
	34,	64,	0,	0x000003ffffffffLL,
	1,	64,	30,	0x00000400000000LL,
	1,	32,	30,	0x00000400000004LL,
	2,	64,	31,	0x00000600000000LL,
	2,	32,	31,	0x00000600000006LL,
	3,	64,	32,	0x00000700000000LL,
	3,	32,	0,	0x00000700000007LL,
	4,	64,	33,	0x00000780000000LL,
	5,	64,	34,	0x000007c0000000LL,
	6,	64,	35,	0x000007e0000000LL,
	7,	64,	36,	0x000007f0000000LL,
	8,	64,	37,	0x000007f8000000LL,
	9,	64,	38,	0x000007fc000000LL,
	10,	64,	39,	0x000007fe000000LL,
	11,	64,	40,	0x000007ff000000LL,
	12,	64,	41,	0x000007ff800000LL,
	13,	64,	42,	0x000007ffc00000LL,
	14,	64,	43,	0x000007ffe00000LL,
	15,	64,	44,	0x000007fff00000LL,
	16,	64,	45,	0x000007fff80000LL,
	17,	64,	46,	0x000007fffc0000LL,
	18,	64,	47,	0x000007fffe0000LL,
	19,	64,	48,	0x000007ffff0000LL,
	20,	64,	49,	0x000007ffff8000LL,
	21,	64,	50,	0x000007ffffc000LL,
	22,	64,	51,	0x000007ffffe000LL,
	23,	64,	52,	0x000007fffff000LL,
	24,	64,	53,	0x000007fffff800LL,
	25,	64,	54,	0x000007fffffc00LL,
	26,	64,	55,	0x000007fffffe00LL,
	27,	64,	56,	0x000007ffffff00LL,
	28,	64,	57,	0x000007ffffff80LL,
	29,	64,	58,	0x000007ffffffc0LL,
	30,	64,	59,	0x000007ffffffe0LL,
	31,	64,	60,	0x000007fffffff0LL,
	32,	64,	61,	0x000007fffffff8LL,
	33,	64,	62,	0x000007fffffffcLL,
	34,	64,	63,	0x000007fffffffeLL,
	35,	64,	0,	0x000007ffffffffLL,
	1,	64,	29,	0x00000800000000LL,
	1,	32,	29,	0x00000800000008LL,
	2,	64,	30,	0x00000c00000000LL,
	2,	32,	30,	0x00000c0000000cLL,
	3,	64,	31,	0x00000e00000000LL,
	3,	32,	31,	0x00000e0000000eLL,
	4,	64,	32,	0x00000f00000000LL,
	4,	32,	0,	0x00000f0000000fLL,
	5,	64,	33,	0x00000f80000000LL,
	6,	64,	34,	0x00000fc0000000LL,
	7,	64,	35,	0x00000fe0000000LL,
	8,	64,	36,	0x00000ff0000000LL,
	9,	64,	37,	0x00000ff8000000LL,
	10,	64,	38,	0x00000ffc000000LL,
	11,	64,	39,	0x00000ffe000000LL,
	12,	64,	40,	0x00000fff000000LL,
	13,	64,	41,	0x00000fff800000LL,
	14,	64,	42,	0x00000fffc00000LL,
	15,	64,	43,	0x00000fffe00000LL,
	16,	64,	44,	0x00000ffff00000LL,
	17,	64,	45,	0x00000ffff80000LL,
	18,	64,	46,	0x00000ffffc0000LL,
	19,	64,	47,	0x00000ffffe0000LL,
	20,	64,	48,	0x00000fffff0000LL,
	21,	64,	49,	0x00000fffff8000LL,
	22,	64,	50,	0x00000fffffc000LL,
	23,	64,	51,	0x00000fffffe000LL,
	24,	64,	52,	0x00000ffffff000LL,
	25,	64,	53,	0x00000ffffff800LL,
	26,	64,	54,	0x00000ffffffc00LL,
	27,	64,	55,	0x00000ffffffe00LL,
	28,	64,	56,	0x00000fffffff00LL,
	29,	64,	57,	0x00000fffffff80LL,
	30,	64,	58,	0x00000fffffffc0LL,
	31,	64,	59,	0x00000fffffffe0LL,
	32,	64,	60,	0x00000ffffffff0LL,
	33,	64,	61,	0x00000ffffffff8LL,
	34,	64,	62,	0x00000ffffffffcLL,
	35,	64,	63,	0x00000ffffffffeLL,
	36,	64,	0,	0x00000fffffffffLL,
	1,	64,	28,	0x00001000000000LL,
	1,	32,	28,	0x00001000000010LL,
	2,	64,	29,	0x00001800000000LL,
	2,	32,	29,	0x00001800000018LL,
	3,	64,	30,	0x00001c00000000LL,
	3,	32,	30,	0x00001c0000001cLL,
	4,	64,	31,	0x00001e00000000LL,
	4,	32,	31,	0x00001e0000001eLL,
	5,	64,	32,	0x00001f00000000LL,
	5,	32,	0,	0x00001f0000001fLL,
	6,	64,	33,	0x00001f80000000LL,
	7,	64,	34,	0x00001fc0000000LL,
	8,	64,	35,	0x00001fe0000000LL,
	9,	64,	36,	0x00001ff0000000LL,
	10,	64,	37,	0x00001ff8000000LL,
	11,	64,	38,	0x00001ffc000000LL,
	12,	64,	39,	0x00001ffe000000LL,
	13,	64,	40,	0x00001fff000000LL,
	14,	64,	41,	0x00001fff800000LL,
	15,	64,	42,	0x00001fffc00000LL,
	16,	64,	43,	0x00001fffe00000LL,
	17,	64,	44,	0x00001ffff00000LL,
	18,	64,	45,	0x00001ffff80000LL,
	19,	64,	46,	0x00001ffffc0000LL,
	20,	64,	47,	0x00001ffffe0000LL,
	21,	64,	48,	0x00001fffff0000LL,
	22,	64,	49,	0x00001fffff8000LL,
	23,	64,	50,	0x00001fffffc000LL,
	24,	64,	51,	0x00001fffffe000LL,
	25,	64,	52,	0x00001ffffff000LL,
	26,	64,	53,	0x00001ffffff800LL,
	27,	64,	54,	0x00001ffffffc00LL,
	28,	64,	55,	0x00001ffffffe00LL,
	29,	64,	56,	0x00001fffffff00LL,
	30,	64,	57,	0x00001fffffff80LL,
	31,	64,	58,	0x00001fffffffc0LL,
	32,	64,	59,	0x00001fffffffe0LL,
	33,	64,	60,	0x00001ffffffff0LL,
	34,	64,	61,	0x00001ffffffff8LL,
	35,	64,	62,	0x00001ffffffffcLL,
	36,	64,	63,	0x00001ffffffffeLL,
	37,	64,	0,	0x00001fffffffffLL,
	1,	64,	27,	0x00002000000000LL,
	1,	32,	27,	0x00002000000020LL,
	2,	64,	28,	0x00003000000000LL,
	2,	32,	28,	0x00003000000030LL,
	3,	64,	29,	0x00003800000000LL,
	3,	32,	29,	0x00003800000038LL,
	4,	64,	30,	0x00003c00000000LL,
	4,	32,	30,	0x00003c0000003cLL,
	5,	64,	31,	0x00003e00000000LL,
	5,	32,	31,	0x00003e0000003eLL,
	6,	64,	32,	0x00003f00000000LL,
	6,	32,	0,	0x00003f0000003fLL,
	7,	64,	33,	0x00003f80000000LL,
	8,	64,	34,	0x00003fc0000000LL,
	9,	64,	35,	0x00003fe0000000LL,
	10,	64,	36,	0x00003ff0000000LL,
	11,	64,	37,	0x00003ff8000000LL,
	12,	64,	38,	0x00003ffc000000LL,
	13,	64,	39,	0x00003ffe000000LL,
	14,	64,	40,	0x00003fff000000LL,
	15,	64,	41,	0x00003fff800000LL,
	16,	64,	42,	0x00003fffc00000LL,
	17,	64,	43,	0x00003fffe00000LL,
	18,	64,	44,	0x00003ffff00000LL,
	19,	64,	45,	0x00003ffff80000LL,
	20,	64,	46,	0x00003ffffc0000LL,
	21,	64,	47,	0x00003ffffe0000LL,
	22,	64,	48,	0x00003fffff0000LL,
	23,	64,	49,	0x00003fffff8000LL,
	24,	64,	50,	0x00003fffffc000LL,
	25,	64,	51,	0x00003fffffe000LL,
	26,	64,	52,	0x00003ffffff000LL,
	27,	64,	53,	0x00003ffffff800LL,
	28,	64,	54,	0x00003ffffffc00LL,
	29,	64,	55,	0x00003ffffffe00LL,
	30,	64,	56,	0x00003fffffff00LL,
	31,	64,	57,	0x00003fffffff80LL,
	32,	64,	58,	0x00003fffffffc0LL,
	33,	64,	59,	0x00003fffffffe0LL,
	34,	64,	60,	0x00003ffffffff0LL,
	35,	64,	61,	0x00003ffffffff8LL,
	36,	64,	62,	0x00003ffffffffcLL,
	37,	64,	63,	0x00003ffffffffeLL,
	38,	64,	0,	0x00003fffffffffLL,
	1,	64,	26,	0x00004000000000LL,
	1,	32,	26,	0x00004000000040LL,
	2,	64,	27,	0x00006000000000LL,
	2,	32,	27,	0x00006000000060LL,
	3,	64,	28,	0x00007000000000LL,
	3,	32,	28,	0x00007000000070LL,
	4,	64,	29,	0x00007800000000LL,
	4,	32,	29,	0x00007800000078LL,
	5,	64,	30,	0x00007c00000000LL,
	5,	32,	30,	0x00007c0000007cLL,
	6,	64,	31,	0x00007e00000000LL,
	6,	32,	31,	0x00007e0000007eLL,
	7,	64,	32,	0x00007f00000000LL,
	7,	32,	0,	0x00007f0000007fLL,
	8,	64,	33,	0x00007f80000000LL,
	9,	64,	34,	0x00007fc0000000LL,
	10,	64,	35,	0x00007fe0000000LL,
	11,	64,	36,	0x00007ff0000000LL,
	12,	64,	37,	0x00007ff8000000LL,
	13,	64,	38,	0x00007ffc000000LL,
	14,	64,	39,	0x00007ffe000000LL,
	15,	64,	40,	0x00007fff000000LL,
	16,	64,	41,	0x00007fff800000LL,
	17,	64,	42,	0x00007fffc00000LL,
	18,	64,	43,	0x00007fffe00000LL,
	19,	64,	44,	0x00007ffff00000LL,
	20,	64,	45,	0x00007ffff80000LL,
	21,	64,	46,	0x00007ffffc0000LL,
	22,	64,	47,	0x00007ffffe0000LL,
	23,	64,	48,	0x00007fffff0000LL,
	24,	64,	49,	0x00007fffff8000LL,
	25,	64,	50,	0x00007fffffc000LL,
	26,	64,	51,	0x00007fffffe000LL,
	27,	64,	52,	0x00007ffffff000LL,
	28,	64,	53,	0x00007ffffff800LL,
	29,	64,	54,	0x00007ffffffc00LL,
	30,	64,	55,	0x00007ffffffe00LL,
	31,	64,	56,	0x00007fffffff00LL,
	32,	64,	57,	0x00007fffffff80LL,
	33,	64,	58,	0x00007fffffffc0LL,
	34,	64,	59,	0x00007fffffffe0LL,
	35,	64,	60,	0x00007ffffffff0LL,
	36,	64,	61,	0x00007ffffffff8LL,
	37,	64,	62,	0x00007ffffffffcLL,
	38,	64,	63,	0x00007ffffffffeLL,
	39,	64,	0,	0x00007fffffffffLL,
	1,	64,	25,	0x00008000000000LL,
	1,	32,	25,	0x00008000000080LL,
	2,	64,	26,	0x0000c000000000LL,
	2,	32,	26,	0x0000c0000000c0LL,
	3,	64,	27,	0x0000e000000000LL,
	3,	32,	27,	0x0000e0000000e0LL,
	4,	64,	28,	0x0000f000000000LL,
	4,	32,	28,	0x0000f0000000f0LL,
	5,	64,	29,	0x0000f800000000LL,
	5,	32,	29,	0x0000f8000000f8LL,
	6,	64,	30,	0x0000fc00000000LL,
	6,	32,	30,	0x0000fc000000fcLL,
	7,	64,	31,	0x0000fe00000000LL,
	7,	32,	31,	0x0000fe000000feLL,
	8,	64,	32,	0x0000ff00000000LL,
	8,	32,	0,	0x0000ff000000ffLL,
	9,	64,	33,	0x0000ff80000000LL,
	10,	64,	34,	0x0000ffc0000000LL,
	11,	64,	35,	0x0000ffe0000000LL,
	12,	64,	36,	0x0000fff0000000LL,
	13,	64,	37,	0x0000fff8000000LL,
	14,	64,	38,	0x0000fffc000000LL,
	15,	64,	39,	0x0000fffe000000LL,
	16,	64,	40,	0x0000ffff000000LL,
	17,	64,	41,	0x0000ffff800000LL,
	18,	64,	42,	0x0000ffffc00000LL,
	19,	64,	43,	0x0000ffffe00000LL,
	20,	64,	44,	0x0000fffff00000LL,
	21,	64,	45,	0x0000fffff80000LL,
	22,	64,	46,	0x0000fffffc0000LL,
	23,	64,	47,	0x0000fffffe0000LL,
	24,	64,	48,	0x0000ffffff0000LL,
	25,	64,	49,	0x0000ffffff8000LL,
	26,	64,	50,	0x0000ffffffc000LL,
	27,	64,	51,	0x0000ffffffe000LL,
	28,	64,	52,	0x0000fffffff000LL,
	29,	64,	53,	0x0000fffffff800LL,
	30,	64,	54,	0x0000fffffffc00LL,
	31,	64,	55,	0x0000fffffffe00LL,
	32,	64,	56,	0x0000ffffffff00LL,
	33,	64,	57,	0x0000ffffffff80LL,
	34,	64,	58,	0x0000ffffffffc0LL,
	35,	64,	59,	0x0000ffffffffe0LL,
	36,	64,	60,	0x0000fffffffff0LL,
	37,	64,	61,	0x0000fffffffff8LL,
	38,	64,	62,	0x0000fffffffffcLL,
	39,	64,	63,	0x0000fffffffffeLL,
	40,	64,	0,	0x0000ffffffffffLL,
	1,	64,	24,	0x00010000000000LL,
	1,	32,	24,	0x00010000000100LL,
	2,	64,	25,	0x00018000000000LL,
	2,	32,	25,	0x00018000000180LL,
	3,	64,	26,	0x0001c000000000LL,
	3,	32,	26,	0x0001c0000001c0LL,
	4,	64,	27,	0x0001e000000000LL,
	4,	32,	27,	0x0001e0000001e0LL,
	5,	64,	28,	0x0001f000000000LL,
	5,	32,	28,	0x0001f0000001f0LL,
	6,	64,	29,	0x0001f800000000LL,
	6,	32,	29,	0x0001f8000001f8LL,
	7,	64,	30,	0x0001fc00000000LL,
	7,	32,	30,	0x0001fc000001fcLL,
	8,	64,	31,	0x0001fe00000000LL,
	8,	32,	31,	0x0001fe000001feLL,
	9,	64,	32,	0x0001ff00000000LL,
	9,	32,	0,	0x0001ff000001ffLL,
	10,	64,	33,	0x0001ff80000000LL,
	11,	64,	34,	0x0001ffc0000000LL,
	12,	64,	35,	0x0001ffe0000000LL,
	13,	64,	36,	0x0001fff0000000LL,
	14,	64,	37,	0x0001fff8000000LL,
	15,	64,	38,	0x0001fffc000000LL,
	16,	64,	39,	0x0001fffe000000LL,
	17,	64,	40,	0x0001ffff000000LL,
	18,	64,	41,	0x0001ffff800000LL,
	19,	64,	42,	0x0001ffffc00000LL,
	20,	64,	43,	0x0001ffffe00000LL,
	21,	64,	44,	0x0001fffff00000LL,
	22,	64,	45,	0x0001fffff80000LL,
	23,	64,	46,	0x0001fffffc0000LL,
	24,	64,	47,	0x0001fffffe0000LL,
	25,	64,	48,	0x0001ffffff0000LL,
	26,	64,	49,	0x0001ffffff8000LL,
	27,	64,	50,	0x0001ffffffc000LL,
	28,	64,	51,	0x0001ffffffe000LL,
	29,	64,	52,	0x0001fffffff000LL,
	30,	64,	53,	0x0001fffffff800LL,
	31,	64,	54,	0x0001fffffffc00LL,
	32,	64,	55,	0x0001fffffffe00LL,
	33,	64,	56,	0x0001ffffffff00LL,
	34,	64,	57,	0x0001ffffffff80LL,
	35,	64,	58,	0x0001ffffffffc0LL,
	36,	64,	59,	0x0001ffffffffe0LL,
	37,	64,	60,	0x0001fffffffff0LL,
	38,	64,	61,	0x0001fffffffff8LL,
	39,	64,	62,	0x0001fffffffffcLL,
	40,	64,	63,	0x0001fffffffffeLL,
	41,	64,	0,	0x0001ffffffffffLL,
	1,	64,	23,	0x00020000000000LL,
	1,	32,	23,	0x00020000000200LL,
	2,	64,	24,	0x00030000000000LL,
	2,	32,	24,	0x00030000000300LL,
	3,	64,	25,	0x00038000000000LL,
	3,	32,	25,	0x00038000000380LL,
	4,	64,	26,	0x0003c000000000LL,
	4,	32,	26,	0x0003c0000003c0LL,
	5,	64,	27,	0x0003e000000000LL,
	5,	32,	27,	0x0003e0000003e0LL,
	6,	64,	28,	0x0003f000000000LL,
	6,	32,	28,	0x0003f0000003f0LL,
	7,	64,	29,	0x0003f800000000LL,
	7,	32,	29,	0x0003f8000003f8LL,
	8,	64,	30,	0x0003fc00000000LL,
	8,	32,	30,	0x0003fc000003fcLL,
	9,	64,	31,	0x0003fe00000000LL,
	9,	32,	31,	0x0003fe000003feLL,
	10,	64,	32,	0x0003ff00000000LL,
	10,	32,	0,	0x0003ff000003ffLL,
	11,	64,	33,	0x0003ff80000000LL,
	12,	64,	34,	0x0003ffc0000000LL,
	13,	64,	35,	0x0003ffe0000000LL,
	14,	64,	36,	0x0003fff0000000LL,
	15,	64,	37,	0x0003fff8000000LL,
	16,	64,	38,	0x0003fffc000000LL,
	17,	64,	39,	0x0003fffe000000LL,
	18,	64,	40,	0x0003ffff000000LL,
	19,	64,	41,	0x0003ffff800000LL,
	20,	64,	42,	0x0003ffffc00000LL,
	21,	64,	43,	0x0003ffffe00000LL,
	22,	64,	44,	0x0003fffff00000LL,
	23,	64,	45,	0x0003fffff80000LL,
	24,	64,	46,	0x0003fffffc0000LL,
	25,	64,	47,	0x0003fffffe0000LL,
	26,	64,	48,	0x0003ffffff0000LL,
	27,	64,	49,	0x0003ffffff8000LL,
	28,	64,	50,	0x0003ffffffc000LL,
	29,	64,	51,	0x0003ffffffe000LL,
	30,	64,	52,	0x0003fffffff000LL,
	31,	64,	53,	0x0003fffffff800LL,
	32,	64,	54,	0x0003fffffffc00LL,
	33,	64,	55,	0x0003fffffffe00LL,
	34,	64,	56,	0x0003ffffffff00LL,
	35,	64,	57,	0x0003ffffffff80LL,
	36,	64,	58,	0x0003ffffffffc0LL,
	37,	64,	59,	0x0003ffffffffe0LL,
	38,	64,	60,	0x0003fffffffff0LL,
	39,	64,	61,	0x0003fffffffff8LL,
	40,	64,	62,	0x0003fffffffffcLL,
	41,	64,	63,	0x0003fffffffffeLL,
	42,	64,	0,	0x0003ffffffffffLL,
	1,	64,	22,	0x00040000000000LL,
	1,	32,	22,	0x00040000000400LL,
	2,	64,	23,	0x00060000000000LL,
	2,	32,	23,	0x00060000000600LL,
	3,	64,	24,	0x00070000000000LL,
	3,	32,	24,	0x00070000000700LL,
	4,	64,	25,	0x00078000000000LL,
	4,	32,	25,	0x00078000000780LL,
	5,	64,	26,	0x0007c000000000LL,
	5,	32,	26,	0x0007c0000007c0LL,
	6,	64,	27,	0x0007e000000000LL,
	6,	32,	27,	0x0007e0000007e0LL,
	7,	64,	28,	0x0007f000000000LL,
	7,	32,	28,	0x0007f0000007f0LL,
	8,	64,	29,	0x0007f800000000LL,
	8,	32,	29,	0x0007f8000007f8LL,
	9,	64,	30,	0x0007fc00000000LL,
	9,	32,	30,	0x0007fc000007fcLL,
	10,	64,	31,	0x0007fe00000000LL,
	10,	32,	31,	0x0007fe000007feLL,
	11,	64,	32,	0x0007ff00000000LL,
	11,	32,	0,	0x0007ff000007ffLL,
	12,	64,	33,	0x0007ff80000000LL,
	13,	64,	34,	0x0007ffc0000000LL,
	14,	64,	35,	0x0007ffe0000000LL,
	15,	64,	36,	0x0007fff0000000LL,
	16,	64,	37,	0x0007fff8000000LL,
	17,	64,	38,	0x0007fffc000000LL,
	18,	64,	39,	0x0007fffe000000LL,
	19,	64,	40,	0x0007ffff000000LL,
	20,	64,	41,	0x0007ffff800000LL,
	21,	64,	42,	0x0007ffffc00000LL,
	22,	64,	43,	0x0007ffffe00000LL,
	23,	64,	44,	0x0007fffff00000LL,
	24,	64,	45,	0x0007fffff80000LL,
	25,	64,	46,	0x0007fffffc0000LL,
	26,	64,	47,	0x0007fffffe0000LL,
	27,	64,	48,	0x0007ffffff0000LL,
	28,	64,	49,	0x0007ffffff8000LL,
	29,	64,	50,	0x0007ffffffc000LL,
	30,	64,	51,	0x0007ffffffe000LL,
	31,	64,	52,	0x0007fffffff000LL,
	32,	64,	53,	0x0007fffffff800LL,
	33,	64,	54,	0x0007fffffffc00LL,
	34,	64,	55,	0x0007fffffffe00LL,
	35,	64,	56,	0x0007ffffffff00LL,
	36,	64,	57,	0x0007ffffffff80LL,
	37,	64,	58,	0x0007ffffffffc0LL,
	38,	64,	59,	0x0007ffffffffe0LL,
	39,	64,	60,	0x0007fffffffff0LL,
	40,	64,	61,	0x0007fffffffff8LL,
	41,	64,	62,	0x0007fffffffffcLL,
	42,	64,	63,	0x0007fffffffffeLL,
	43,	64,	0,	0x0007ffffffffffLL,
	1,	64,	21,	0x00080000000000LL,
	1,	32,	21,	0x00080000000800LL,
	2,	64,	22,	0x000c0000000000LL,
	2,	32,	22,	0x000c0000000c00LL,
	3,	64,	23,	0x000e0000000000LL,
	3,	32,	23,	0x000e0000000e00LL,
	4,	64,	24,	0x000f0000000000LL,
	4,	32,	24,	0x000f0000000f00LL,
	5,	64,	25,	0x000f8000000000LL,
	5,	32,	25,	0x000f8000000f80LL,
	6,	64,	26,	0x000fc000000000LL,
	6,	32,	26,	0x000fc000000fc0LL,
	7,	64,	27,	0x000fe000000000LL,
	7,	32,	27,	0x000fe000000fe0LL,
	8,	64,	28,	0x000ff000000000LL,
	8,	32,	28,	0x000ff000000ff0LL,
	9,	64,	29,	0x000ff800000000LL,
	9,	32,	29,	0x000ff800000ff8LL,
	10,	64,	30,	0x000ffc00000000LL,
	10,	32,	30,	0x000ffc00000ffcLL,
	11,	64,	31,	0x000ffe00000000LL,
	11,	32,	31,	0x000ffe00000ffeLL,
	12,	64,	32,	0x000fff00000000LL,
	12,	32,	0,	0x000fff00000fffLL,
	13,	64,	33,	0x000fff80000000LL,
	14,	64,	34,	0x000fffc0000000LL,
	15,	64,	35,	0x000fffe0000000LL,
	16,	64,	36,	0x000ffff0000000LL,
	17,	64,	37,	0x000ffff8000000LL,
	18,	64,	38,	0x000ffffc000000LL,
	19,	64,	39,	0x000ffffe000000LL,
	20,	64,	40,	0x000fffff000000LL,
	21,	64,	41,	0x000fffff800000LL,
	22,	64,	42,	0x000fffffc00000LL,
	23,	64,	43,	0x000fffffe00000LL,
	24,	64,	44,	0x000ffffff00000LL,
	25,	64,	45,	0x000ffffff80000LL,
	26,	64,	46,	0x000ffffffc0000LL,
	27,	64,	47,	0x000ffffffe0000LL,
	28,	64,	48,	0x000fffffff0000LL,
	29,	64,	49,	0x000fffffff8000LL,
	30,	64,	50,	0x000fffffffc000LL,
	31,	64,	51,	0x000fffffffe000LL,
	32,	64,	52,	0x000ffffffff000LL,
	33,	64,	53,	0x000ffffffff800LL,
	34,	64,	54,	0x000ffffffffc00LL,
	35,	64,	55,	0x000ffffffffe00LL,
	36,	64,	56,	0x000fffffffff00LL,
	37,	64,	57,	0x000fffffffff80LL,
	38,	64,	58,	0x000fffffffffc0LL,
	39,	64,	59,	0x000fffffffffe0LL,
	40,	64,	60,	0x000ffffffffff0LL,
	41,	64,	61,	0x000ffffffffff8LL,
	42,	64,	62,	0x000ffffffffffcLL,
	43,	64,	63,	0x000ffffffffffeLL,
	44,	64,	0,	0x000fffffffffffLL,
	1,	64,	20,	0x00100000000000LL,
	1,	32,	20,	0x00100000001000LL,
	2,	64,	21,	0x00180000000000LL,
	2,	32,	21,	0x00180000001800LL,
	3,	64,	22,	0x001c0000000000LL,
	3,	32,	22,	0x001c0000001c00LL,
	4,	64,	23,	0x001e0000000000LL,
	4,	32,	23,	0x001e0000001e00LL,
	5,	64,	24,	0x001f0000000000LL,
	5,	32,	24,	0x001f0000001f00LL,
	6,	64,	25,	0x001f8000000000LL,
	6,	32,	25,	0x001f8000001f80LL,
	7,	64,	26,	0x001fc000000000LL,
	7,	32,	26,	0x001fc000001fc0LL,
	8,	64,	27,	0x001fe000000000LL,
	8,	32,	27,	0x001fe000001fe0LL,
	9,	64,	28,	0x001ff000000000LL,
	9,	32,	28,	0x001ff000001ff0LL,
	10,	64,	29,	0x001ff800000000LL,
	10,	32,	29,	0x001ff800001ff8LL,
	11,	64,	30,	0x001ffc00000000LL,
	11,	32,	30,	0x001ffc00001ffcLL,
	12,	64,	31,	0x001ffe00000000LL,
	12,	32,	31,	0x001ffe00001ffeLL,
	13,	64,	32,	0x001fff00000000LL,
	13,	32,	0,	0x001fff00001fffLL,
	14,	64,	33,	0x001fff80000000LL,
	15,	64,	34,	0x001fffc0000000LL,
	16,	64,	35,	0x001fffe0000000LL,
	17,	64,	36,	0x001ffff0000000LL,
	18,	64,	37,	0x001ffff8000000LL,
	19,	64,	38,	0x001ffffc000000LL,
	20,	64,	39,	0x001ffffe000000LL,
	21,	64,	40,	0x001fffff000000LL,
	22,	64,	41,	0x001fffff800000LL,
	23,	64,	42,	0x001fffffc00000LL,
	24,	64,	43,	0x001fffffe00000LL,
	25,	64,	44,	0x001ffffff00000LL,
	26,	64,	45,	0x001ffffff80000LL,
	27,	64,	46,	0x001ffffffc0000LL,
	28,	64,	47,	0x001ffffffe0000LL,
	29,	64,	48,	0x001fffffff0000LL,
	30,	64,	49,	0x001fffffff8000LL,
	31,	64,	50,	0x001fffffffc000LL,
	32,	64,	51,	0x001fffffffe000LL,
	33,	64,	52,	0x001ffffffff000LL,
	34,	64,	53,	0x001ffffffff800LL,
	35,	64,	54,	0x001ffffffffc00LL,
	36,	64,	55,	0x001ffffffffe00LL,
	37,	64,	56,	0x001fffffffff00LL,
	38,	64,	57,	0x001fffffffff80LL,
	39,	64,	58,	0x001fffffffffc0LL,
	40,	64,	59,	0x001fffffffffe0LL,
	41,	64,	60,	0x001ffffffffff0LL,
	42,	64,	61,	0x001ffffffffff8LL,
	43,	64,	62,	0x001ffffffffffcLL,
	44,	64,	63,	0x001ffffffffffeLL,
	45,	64,	0,	0x001fffffffffffLL,
	1,	64,	19,	0x00200000000000LL,
	1,	32,	19,	0x00200000002000LL,
	2,	64,	20,	0x00300000000000LL,
	2,	32,	20,	0x00300000003000LL,
	3,	64,	21,	0x00380000000000LL,
	3,	32,	21,	0x00380000003800LL,
	4,	64,	22,	0x003c0000000000LL,
	4,	32,	22,	0x003c0000003c00LL,
	5,	64,	23,	0x003e0000000000LL,
	5,	32,	23,	0x003e0000003e00LL,
	6,	64,	24,	0x003f0000000000LL,
	6,	32,	24,	0x003f0000003f00LL,
	7,	64,	25,	0x003f8000000000LL,
	7,	32,	25,	0x003f8000003f80LL,
	8,	64,	26,	0x003fc000000000LL,
	8,	32,	26,	0x003fc000003fc0LL,
	9,	64,	27,	0x003fe000000000LL,
	9,	32,	27,	0x003fe000003fe0LL,
	10,	64,	28,	0x003ff000000000LL,
	10,	32,	28,	0x003ff000003ff0LL,
	11,	64,	29,	0x003ff800000000LL,
	11,	32,	29,	0x003ff800003ff8LL,
	12,	64,	30,	0x003ffc00000000LL,
	12,	32,	30,	0x003ffc00003ffcLL,
	13,	64,	31,	0x003ffe00000000LL,
	13,	32,	31,	0x003ffe00003ffeLL,
	14,	64,	32,	0x003fff00000000LL,
	14,	32,	0,	0x003fff00003fffLL,
	15,	64,	33,	0x003fff80000000LL,
	16,	64,	34,	0x003fffc0000000LL,
	17,	64,	35,	0x003fffe0000000LL,
	18,	64,	36,	0x003ffff0000000LL,
	19,	64,	37,	0x003ffff8000000LL,
	20,	64,	38,	0x003ffffc000000LL,
	21,	64,	39,	0x003ffffe000000LL,
	22,	64,	40,	0x003fffff000000LL,
	23,	64,	41,	0x003fffff800000LL,
	24,	64,	42,	0x003fffffc00000LL,
	25,	64,	43,	0x003fffffe00000LL,
	26,	64,	44,	0x003ffffff00000LL,
	27,	64,	45,	0x003ffffff80000LL,
	28,	64,	46,	0x003ffffffc0000LL,
	29,	64,	47,	0x003ffffffe0000LL,
	30,	64,	48,	0x003fffffff0000LL,
	31,	64,	49,	0x003fffffff8000LL,
	32,	64,	50,	0x003fffffffc000LL,
	33,	64,	51,	0x003fffffffe000LL,
	34,	64,	52,	0x003ffffffff000LL,
	35,	64,	53,	0x003ffffffff800LL,
	36,	64,	54,	0x003ffffffffc00LL,
	37,	64,	55,	0x003ffffffffe00LL,
	38,	64,	56,	0x003fffffffff00LL,
	39,	64,	57,	0x003fffffffff80LL,
	40,	64,	58,	0x003fffffffffc0LL,
	41,	64,	59,	0x003fffffffffe0LL,
	42,	64,	60,	0x003ffffffffff0LL,
	43,	64,	61,	0x003ffffffffff8LL,
	44,	64,	62,	0x003ffffffffffcLL,
	45,	64,	63,	0x003ffffffffffeLL,
	46,	64,	0,	0x003fffffffffffLL,
	1,	64,	18,	0x00400000000000LL,
	1,	32,	18,	0x00400000004000LL,
	2,	64,	19,	0x00600000000000LL,
	2,	32,	19,	0x00600000006000LL,
	3,	64,	20,	0x00700000000000LL,
	3,	32,	20,	0x00700000007000LL,
	4,	64,	21,	0x00780000000000LL,
	4,	32,	21,	0x00780000007800LL,
	5,	64,	22,	0x007c0000000000LL,
	5,	32,	22,	0x007c0000007c00LL,
	6,	64,	23,	0x007e0000000000LL,
	6,	32,	23,	0x007e0000007e00LL,
	7,	64,	24,	0x007f0000000000LL,
	7,	32,	24,	0x007f0000007f00LL,
	8,	64,	25,	0x007f8000000000LL,
	8,	32,	25,	0x007f8000007f80LL,
	9,	64,	26,	0x007fc000000000LL,
	9,	32,	26,	0x007fc000007fc0LL,
	10,	64,	27,	0x007fe000000000LL,
	10,	32,	27,	0x007fe000007fe0LL,
	11,	64,	28,	0x007ff000000000LL,
	11,	32,	28,	0x007ff000007ff0LL,
	12,	64,	29,	0x007ff800000000LL,
	12,	32,	29,	0x007ff800007ff8LL,
	13,	64,	30,	0x007ffc00000000LL,
	13,	32,	30,	0x007ffc00007ffcLL,
	14,	64,	31,	0x007ffe00000000LL,
	14,	32,	31,	0x007ffe00007ffeLL,
	15,	64,	32,	0x007fff00000000LL,
	15,	32,	0,	0x007fff00007fffLL,
	16,	64,	33,	0x007fff80000000LL,
	17,	64,	34,	0x007fffc0000000LL,
	18,	64,	35,	0x007fffe0000000LL,
	19,	64,	36,	0x007ffff0000000LL,
	20,	64,	37,	0x007ffff8000000LL,
	21,	64,	38,	0x007ffffc000000LL,
	22,	64,	39,	0x007ffffe000000LL,
	23,	64,	40,	0x007fffff000000LL,
	24,	64,	41,	0x007fffff800000LL,
	25,	64,	42,	0x007fffffc00000LL,
	26,	64,	43,	0x007fffffe00000LL,
	27,	64,	44,	0x007ffffff00000LL,
	28,	64,	45,	0x007ffffff80000LL,
	29,	64,	46,	0x007ffffffc0000LL,
	30,	64,	47,	0x007ffffffe0000LL,
	31,	64,	48,	0x007fffffff0000LL,
	32,	64,	49,	0x007fffffff8000LL,
	33,	64,	50,	0x007fffffffc000LL,
	34,	64,	51,	0x007fffffffe000LL,
	35,	64,	52,	0x007ffffffff000LL,
	36,	64,	53,	0x007ffffffff800LL,
	37,	64,	54,	0x007ffffffffc00LL,
	38,	64,	55,	0x007ffffffffe00LL,
	39,	64,	56,	0x007fffffffff00LL,
	40,	64,	57,	0x007fffffffff80LL,
	41,	64,	58,	0x007fffffffffc0LL,
	42,	64,	59,	0x007fffffffffe0LL,
	43,	64,	60,	0x007ffffffffff0LL,
	44,	64,	61,	0x007ffffffffff8LL,
	45,	64,	62,	0x007ffffffffffcLL,
	46,	64,	63,	0x007ffffffffffeLL,
	47,	64,	0,	0x007fffffffffffLL,
	1,	64,	17,	0x00800000000000LL,
	1,	32,	17,	0x00800000008000LL,
	2,	64,	18,	0x00c00000000000LL,
	2,	32,	18,	0x00c0000000c000LL,
	3,	64,	19,	0x00e00000000000LL,
	3,	32,	19,	0x00e0000000e000LL,
	4,	64,	20,	0x00f00000000000LL,
	4,	32,	20,	0x00f0000000f000LL,
	5,	64,	21,	0x00f80000000000LL,
	5,	32,	21,	0x00f8000000f800LL,
	6,	64,	22,	0x00fc0000000000LL,
	6,	32,	22,	0x00fc000000fc00LL,
	7,	64,	23,	0x00fe0000000000LL,
	7,	32,	23,	0x00fe000000fe00LL,
	8,	64,	24,	0x00ff0000000000LL,
	8,	32,	24,	0x00ff000000ff00LL,
	9,	64,	25,	0x00ff8000000000LL,
	9,	32,	25,	0x00ff800000ff80LL,
	10,	64,	26,	0x00ffc000000000LL,
	10,	32,	26,	0x00ffc00000ffc0LL,
	11,	64,	27,	0x00ffe000000000LL,
	11,	32,	27,	0x00ffe00000ffe0LL,
	12,	64,	28,	0x00fff000000000LL,
	12,	32,	28,	0x00fff00000fff0LL,
	13,	64,	29,	0x00fff800000000LL,
	13,	32,	29,	0x00fff80000fff8LL,
	14,	64,	30,	0x00fffc00000000LL,
	14,	32,	30,	0x00fffc0000fffcLL,
	15,	64,	31,	0x00fffe00000000LL,
	15,	32,	31,	0x00fffe0000fffeLL,
	16,	64,	32,	0x00ffff00000000LL,
	16,	32,	0,	0x00ffff0000ffffLL,
	17,	64,	33,	0x00ffff80000000LL,
	18,	64,	34,	0x00ffffc0000000LL,
	19,	64,	35,	0x00ffffe0000000LL,
	20,	64,	36,	0x00fffff0000000LL,
	21,	64,	37,	0x00fffff8000000LL,
	22,	64,	38,	0x00fffffc000000LL,
	23,	64,	39,	0x00fffffe000000LL,
	24,	64,	40,	0x00ffffff000000LL,
	25,	64,	41,	0x00ffffff800000LL,
	26,	64,	42,	0x00ffffffc00000LL,
	27,	64,	43,	0x00ffffffe00000LL,
	28,	64,	44,	0x00fffffff00000LL,
	29,	64,	45,	0x00fffffff80000LL,
	30,	64,	46,	0x00fffffffc0000LL,
	31,	64,	47,	0x00fffffffe0000LL,
	32,	64,	48,	0x00ffffffff0000LL,
	33,	64,	49,	0x00ffffffff8000LL,
	34,	64,	50,	0x00ffffffffc000LL,
	35,	64,	51,	0x00ffffffffe000LL,
	36,	64,	52,	0x00fffffffff000LL,
	37,	64,	53,	0x00fffffffff800LL,
	38,	64,	54,	0x00fffffffffc00LL,
	39,	64,	55,	0x00fffffffffe00LL,
	40,	64,	56,	0x00ffffffffff00LL,
	41,	64,	57,	0x00ffffffffff80LL,
	42,	64,	58,	0x00ffffffffffc0LL,
	43,	64,	59,	0x00ffffffffffe0LL,
	44,	64,	60,	0x00fffffffffff0LL,
	45,	64,	61,	0x00fffffffffff8LL,
	46,	64,	62,	0x00fffffffffffcLL,
	47,	64,	63,	0x00fffffffffffeLL,
	48,	64,	0,	0x00ffffffffffffLL,
	1,	64,	16,	0x01000000000000LL,
	1,	32,	16,	0x01000000010000LL,
	1,	16,	0,	0x01000100010001LL,
	2,	64,	17,	0x01800000000000LL,
	2,	32,	17,	0x01800000018000LL,
	3,	64,	18,	0x01c00000000000LL,
	3,	32,	18,	0x01c0000001c000LL,
	4,	64,	19,	0x01e00000000000LL,
	4,	32,	19,	0x01e0000001e000LL,
	5,	64,	20,	0x01f00000000000LL,
	5,	32,	20,	0x01f0000001f000LL,
	6,	64,	21,	0x01f80000000000LL,
	6,	32,	21,	0x01f8000001f800LL,
	7,	64,	22,	0x01fc0000000000LL,
	7,	32,	22,	0x01fc000001fc00LL,
	8,	64,	23,	0x01fe0000000000LL,
	8,	32,	23,	0x01fe000001fe00LL,
	9,	64,	24,	0x01ff0000000000LL,
	9,	32,	24,	0x01ff000001ff00LL,
	10,	64,	25,	0x01ff8000000000LL,
	10,	32,	25,	0x01ff800001ff80LL,
	11,	64,	26,	0x01ffc000000000LL,
	11,	32,	26,	0x01ffc00001ffc0LL,
	12,	64,	27,	0x01ffe000000000LL,
	12,	32,	27,	0x01ffe00001ffe0LL,
	13,	64,	28,	0x01fff000000000LL,
	13,	32,	28,	0x01fff00001fff0LL,
	14,	64,	29,	0x01fff800000000LL,
	14,	32,	29,	0x01fff80001fff8LL,
	15,	64,	30,	0x01fffc00000000LL,
	15,	32,	30,	0x01fffc0001fffcLL,
	16,	64,	31,	0x01fffe00000000LL,
	16,	32,	31,	0x01fffe0001fffeLL,
	17,	64,	32,	0x01ffff00000000LL,
	17,	32,	0,	0x01ffff0001ffffLL,
	18,	64,	33,	0x01ffff80000000LL,
	19,	64,	34,	0x01ffffc0000000LL,
	20,	64,	35,	0x01ffffe0000000LL,
	21,	64,	36,	0x01fffff0000000LL,
	22,	64,	37,	0x01fffff8000000LL,
	23,	64,	38,	0x01fffffc000000LL,
	24,	64,	39,	0x01fffffe000000LL,
	25,	64,	40,	0x01ffffff000000LL,
	26,	64,	41,	0x01ffffff800000LL,
	27,	64,	42,	0x01ffffffc00000LL,
	28,	64,	43,	0x01ffffffe00000LL,
	29,	64,	44,	0x01fffffff00000LL,
	30,	64,	45,	0x01fffffff80000LL,
	31,	64,	46,	0x01fffffffc0000LL,
	32,	64,	47,	0x01fffffffe0000LL,
	33,	64,	48,	0x01ffffffff0000LL,
	34,	64,	49,	0x01ffffffff8000LL,
	35,	64,	50,	0x01ffffffffc000LL,
	36,	64,	51,	0x01ffffffffe000LL,
	37,	64,	52,	0x01fffffffff000LL,
	38,	64,	53,	0x01fffffffff800LL,
	39,	64,	54,	0x01fffffffffc00LL,
	40,	64,	55,	0x01fffffffffe00LL,
	41,	64,	56,	0x01ffffffffff00LL,
	42,	64,	57,	0x01ffffffffff80LL,
	43,	64,	58,	0x01ffffffffffc0LL,
	44,	64,	59,	0x01ffffffffffe0LL,
	45,	64,	60,	0x01fffffffffff0LL,
	46,	64,	61,	0x01fffffffffff8LL,
	47,	64,	62,	0x01fffffffffffcLL,
	48,	64,	63,	0x01fffffffffffeLL,
	49,	64,	0,	0x01ffffffffffffLL,
	1,	64,	15,	0x02000000000000LL,
	1,	32,	15,	0x02000000020000LL,
	1,	16,	15,	0x02000200020002LL,
	2,	64,	16,	0x03000000000000LL,
	2,	32,	16,	0x03000000030000LL,
	2,	16,	0,	0x03000300030003LL,
	3,	64,	17,	0x03800000000000LL,
	3,	32,	17,	0x03800000038000LL,
	4,	64,	18,	0x03c00000000000LL,
	4,	32,	18,	0x03c0000003c000LL,
	5,	64,	19,	0x03e00000000000LL,
	5,	32,	19,	0x03e0000003e000LL,
	6,	64,	20,	0x03f00000000000LL,
	6,	32,	20,	0x03f0000003f000LL,
	7,	64,	21,	0x03f80000000000LL,
	7,	32,	21,	0x03f8000003f800LL,
	8,	64,	22,	0x03fc0000000000LL,
	8,	32,	22,	0x03fc000003fc00LL,
	9,	64,	23,	0x03fe0000000000LL,
	9,	32,	23,	0x03fe000003fe00LL,
	10,	64,	24,	0x03ff0000000000LL,
	10,	32,	24,	0x03ff000003ff00LL,
	11,	64,	25,	0x03ff8000000000LL,
	11,	32,	25,	0x03ff800003ff80LL,
	12,	64,	26,	0x03ffc000000000LL,
	12,	32,	26,	0x03ffc00003ffc0LL,
	13,	64,	27,	0x03ffe000000000LL,
	13,	32,	27,	0x03ffe00003ffe0LL,
	14,	64,	28,	0x03fff000000000LL,
	14,	32,	28,	0x03fff00003fff0LL,
	15,	64,	29,	0x03fff800000000LL,
	15,	32,	29,	0x03fff80003fff8LL,
	16,	64,	30,	0x03fffc00000000LL,
	16,	32,	30,	0x03fffc0003fffcLL,
	17,	64,	31,	0x03fffe00000000LL,
	17,	32,	31,	0x03fffe0003fffeLL,
	18,	64,	32,	0x03ffff00000000LL,
	18,	32,	0,	0x03ffff0003ffffLL,
	19,	64,	33,	0x03ffff80000000LL,
	20,	64,	34,	0x03ffffc0000000LL,
	21,	64,	35,	0x03ffffe0000000LL,
	22,	64,	36,	0x03fffff0000000LL,
	23,	64,	37,	0x03fffff8000000LL,
	24,	64,	38,	0x03fffffc000000LL,
	25,	64,	39,	0x03fffffe000000LL,
	26,	64,	40,	0x03ffffff000000LL,
	27,	64,	41,	0x03ffffff800000LL,
	28,	64,	42,	0x03ffffffc00000LL,
	29,	64,	43,	0x03ffffffe00000LL,
	30,	64,	44,	0x03fffffff00000LL,
	31,	64,	45,	0x03fffffff80000LL,
	32,	64,	46,	0x03fffffffc0000LL,
	33,	64,	47,	0x03fffffffe0000LL,
	34,	64,	48,	0x03ffffffff0000LL,
	35,	64,	49,	0x03ffffffff8000LL,
	36,	64,	50,	0x03ffffffffc000LL,
	37,	64,	51,	0x03ffffffffe000LL,
	38,	64,	52,	0x03fffffffff000LL,
	39,	64,	53,	0x03fffffffff800LL,
	40,	64,	54,	0x03fffffffffc00LL,
	41,	64,	55,	0x03fffffffffe00LL,
	42,	64,	56,	0x03ffffffffff00LL,
	43,	64,	57,	0x03ffffffffff80LL,
	44,	64,	58,	0x03ffffffffffc0LL,
	45,	64,	59,	0x03ffffffffffe0LL,
	46,	64,	60,	0x03fffffffffff0LL,
	47,	64,	61,	0x03fffffffffff8LL,
	48,	64,	62,	0x03fffffffffffcLL,
	49,	64,	63,	0x03fffffffffffeLL,
	50,	64,	0,	0x03ffffffffffffLL,
	1,	64,	14,	0x04000000000000LL,
	1,	32,	14,	0x04000000040000LL,
	1,	16,	14,	0x04000400040004LL,
	2,	64,	15,	0x06000000000000LL,
	2,	32,	15,	0x06000000060000LL,
	2,	16,	15,	0x06000600060006LL,
	3,	64,	16,	0x07000000000000LL,
	3,	32,	16,	0x07000000070000LL,
	3,	16,	0,	0x07000700070007LL,
	4,	64,	17,	0x07800000000000LL,
	4,	32,	17,	0x07800000078000LL,
	5,	64,	18,	0x07c00000000000LL,
	5,	32,	18,	0x07c0000007c000LL,
	6,	64,	19,	0x07e00000000000LL,
	6,	32,	19,	0x07e0000007e000LL,
	7,	64,	20,	0x07f00000000000LL,
	7,	32,	20,	0x07f0000007f000LL,
	8,	64,	21,	0x07f80000000000LL,
	8,	32,	21,	0x07f8000007f800LL,
	9,	64,	22,	0x07fc0000000000LL,
	9,	32,	22,	0x07fc000007fc00LL,
	10,	64,	23,	0x07fe0000000000LL,
	10,	32,	23,	0x07fe000007fe00LL,
	11,	64,	24,	0x07ff0000000000LL,
	11,	32,	24,	0x07ff000007ff00LL,
	12,	64,	25,	0x07ff8000000000LL,
	12,	32,	25,	0x07ff800007ff80LL,
	13,	64,	26,	0x07ffc000000000LL,
	13,	32,	26,	0x07ffc00007ffc0LL,
	14,	64,	27,	0x07ffe000000000LL,
	14,	32,	27,	0x07ffe00007ffe0LL,
	15,	64,	28,	0x07fff000000000LL,
	15,	32,	28,	0x07fff00007fff0LL,
	16,	64,	29,	0x07fff800000000LL,
	16,	32,	29,	0x07fff80007fff8LL,
	17,	64,	30,	0x07fffc00000000LL,
	17,	32,	30,	0x07fffc0007fffcLL,
	18,	64,	31,	0x07fffe00000000LL,
	18,	32,	31,	0x07fffe0007fffeLL,
	19,	64,	32,	0x07ffff00000000LL,
	19,	32,	0,	0x07ffff0007ffffLL,
	20,	64,	33,	0x07ffff80000000LL,
	21,	64,	34,	0x07ffffc0000000LL,
	22,	64,	35,	0x07ffffe0000000LL,
	23,	64,	36,	0x07fffff0000000LL,
	24,	64,	37,	0x07fffff8000000LL,
	25,	64,	38,	0x07fffffc000000LL,
	26,	64,	39,	0x07fffffe000000LL,
	27,	64,	40,	0x07ffffff000000LL,
	28,	64,	41,	0x07ffffff800000LL,
	29,	64,	42,	0x07ffffffc00000LL,
	30,	64,	43,	0x07ffffffe00000LL,
	31,	64,	44,	0x07fffffff00000LL,
	32,	64,	45,	0x07fffffff80000LL,
	33,	64,	46,	0x07fffffffc0000LL,
	34,	64,	47,	0x07fffffffe0000LL,
	35,	64,	48,	0x07ffffffff0000LL,
	36,	64,	49,	0x07ffffffff8000LL,
	37,	64,	50,	0x07ffffffffc000LL,
	38,	64,	51,	0x07ffffffffe000LL,
	39,	64,	52,	0x07fffffffff000LL,
	40,	64,	53,	0x07fffffffff800LL,
	41,	64,	54,	0x07fffffffffc00LL,
	42,	64,	55,	0x07fffffffffe00LL,
	43,	64,	56,	0x07ffffffffff00LL,
	44,	64,	57,	0x07ffffffffff80LL,
	45,	64,	58,	0x07ffffffffffc0LL,
	46,	64,	59,	0x07ffffffffffe0LL,
	47,	64,	60,	0x07fffffffffff0LL,
	48,	64,	61,	0x07fffffffffff8LL,
	49,	64,	62,	0x07fffffffffffcLL,
	50,	64,	63,	0x07fffffffffffeLL,
	51,	64,	0,	0x07ffffffffffffLL,
	1,	64,	13,	0x08000000000000LL,
	1,	32,	13,	0x08000000080000LL,
	1,	16,	13,	0x08000800080008LL,
	2,	64,	14,	0x0c000000000000LL,
	2,	32,	14,	0x0c0000000c0000LL,
	2,	16,	14,	0x0c000c000c000cLL,
	3,	64,	15,	0x0e000000000000LL,
	3,	32,	15,	0x0e0000000e0000LL,
	3,	16,	15,	0x0e000e000e000eLL,
	4,	64,	16,	0x0f000000000000LL,
	4,	32,	16,	0x0f0000000f0000LL,
	4,	16,	0,	0x0f000f000f000fLL,
	5,	64,	17,	0x0f800000000000LL,
	5,	32,	17,	0x0f8000000f8000LL,
	6,	64,	18,	0x0fc00000000000LL,
	6,	32,	18,	0x0fc000000fc000LL,
	7,	64,	19,	0x0fe00000000000LL,
	7,	32,	19,	0x0fe000000fe000LL,
	8,	64,	20,	0x0ff00000000000LL,
	8,	32,	20,	0x0ff000000ff000LL,
	9,	64,	21,	0x0ff80000000000LL,
	9,	32,	21,	0x0ff800000ff800LL,
	10,	64,	22,	0x0ffc0000000000LL,
	10,	32,	22,	0x0ffc00000ffc00LL,
	11,	64,	23,	0x0ffe0000000000LL,
	11,	32,	23,	0x0ffe00000ffe00LL,
	12,	64,	24,	0x0fff0000000000LL,
	12,	32,	24,	0x0fff00000fff00LL,
	13,	64,	25,	0x0fff8000000000LL,
	13,	32,	25,	0x0fff80000fff80LL,
	14,	64,	26,	0x0fffc000000000LL,
	14,	32,	26,	0x0fffc0000fffc0LL,
	15,	64,	27,	0x0fffe000000000LL,
	15,	32,	27,	0x0fffe0000fffe0LL,
	16,	64,	28,	0x0ffff000000000LL,
	16,	32,	28,	0x0ffff0000ffff0LL,
	17,	64,	29,	0x0ffff800000000LL,
	17,	32,	29,	0x0ffff8000ffff8LL,
	18,	64,	30,	0x0ffffc00000000LL,
	18,	32,	30,	0x0ffffc000ffffcLL,
	19,	64,	31,	0x0ffffe00000000LL,
	19,	32,	31,	0x0ffffe000ffffeLL,
	20,	64,	32,	0x0fffff00000000LL,
	20,	32,	0,	0x0fffff000fffffLL,
	21,	64,	33,	0x0fffff80000000LL,
	22,	64,	34,	0x0fffffc0000000LL,
	23,	64,	35,	0x0fffffe0000000LL,
	24,	64,	36,	0x0ffffff0000000LL,
	25,	64,	37,	0x0ffffff8000000LL,
	26,	64,	38,	0x0ffffffc000000LL,
	27,	64,	39,	0x0ffffffe000000LL,
	28,	64,	40,	0x0fffffff000000LL,
	29,	64,	41,	0x0fffffff800000LL,
	30,	64,	42,	0x0fffffffc00000LL,
	31,	64,	43,	0x0fffffffe00000LL,
	32,	64,	44,	0x0ffffffff00000LL,
	33,	64,	45,	0x0ffffffff80000LL,
	34,	64,	46,	0x0ffffffffc0000LL,
	35,	64,	47,	0x0ffffffffe0000LL,
	36,	64,	48,	0x0fffffffff0000LL,
	37,	64,	49,	0x0fffffffff8000LL,
	38,	64,	50,	0x0fffffffffc000LL,
	39,	64,	51,	0x0fffffffffe000LL,
	40,	64,	52,	0x0ffffffffff000LL,
	41,	64,	53,	0x0ffffffffff800LL,
	42,	64,	54,	0x0ffffffffffc00LL,
	43,	64,	55,	0x0ffffffffffe00LL,
	44,	64,	56,	0x0fffffffffff00LL,
	45,	64,	57,	0x0fffffffffff80LL,
	46,	64,	58,	0x0fffffffffffc0LL,
	47,	64,	59,	0x0fffffffffffe0LL,
	48,	64,	60,	0x0ffffffffffff0LL,
	49,	64,	61,	0x0ffffffffffff8LL,
	50,	64,	62,	0x0ffffffffffffcLL,
	51,	64,	63,	0x0ffffffffffffeLL,
	52,	64,	0,	0x0fffffffffffffLL,
	1,	64,	12,	0x10000000000000LL,
	1,	32,	12,	0x10000000100000LL,
	1,	16,	12,	0x10001000100010LL,
	2,	64,	13,	0x18000000000000LL,
	2,	32,	13,	0x18000000180000LL,
	2,	16,	13,	0x18001800180018LL,
	3,	64,	14,	0x1c000000000000LL,
	3,	32,	14,	0x1c0000001c0000LL,
	3,	16,	14,	0x1c001c001c001cLL,
	4,	64,	15,	0x1e000000000000LL,
	4,	32,	15,	0x1e0000001e0000LL,
	4,	16,	15,	0x1e001e001e001eLL,
	5,	64,	16,	0x1f000000000000LL,
	5,	32,	16,	0x1f0000001f0000LL,
	5,	16,	0,	0x1f001f001f001fLL,
	6,	64,	17,	0x1f800000000000LL,
	6,	32,	17,	0x1f8000001f8000LL,
	7,	64,	18,	0x1fc00000000000LL,
	7,	32,	18,	0x1fc000001fc000LL,
	8,	64,	19,	0x1fe00000000000LL,
	8,	32,	19,	0x1fe000001fe000LL,
	9,	64,	20,	0x1ff00000000000LL,
	9,	32,	20,	0x1ff000001ff000LL,
	10,	64,	21,	0x1ff80000000000LL,
	10,	32,	21,	0x1ff800001ff800LL,
	11,	64,	22,	0x1ffc0000000000LL,
	11,	32,	22,	0x1ffc00001ffc00LL,
	12,	64,	23,	0x1ffe0000000000LL,
	12,	32,	23,	0x1ffe00001ffe00LL,
	13,	64,	24,	0x1fff0000000000LL,
	13,	32,	24,	0x1fff00001fff00LL,
	14,	64,	25,	0x1fff8000000000LL,
	14,	32,	25,	0x1fff80001fff80LL,
	15,	64,	26,	0x1fffc000000000LL,
	15,	32,	26,	0x1fffc0001fffc0LL,
	16,	64,	27,	0x1fffe000000000LL,
	16,	32,	27,	0x1fffe0001fffe0LL,
	17,	64,	28,	0x1ffff000000000LL,
	17,	32,	28,	0x1ffff0001ffff0LL,
	18,	64,	29,	0x1ffff800000000LL,
	18,	32,	29,	0x1ffff8001ffff8LL,
	19,	64,	30,	0x1ffffc00000000LL,
	19,	32,	30,	0x1ffffc001ffffcLL,
	20,	64,	31,	0x1ffffe00000000LL,
	20,	32,	31,	0x1ffffe001ffffeLL,
	21,	64,	32,	0x1fffff00000000LL,
	21,	32,	0,	0x1fffff001fffffLL,
	22,	64,	33,	0x1fffff80000000LL,
	23,	64,	34,	0x1fffffc0000000LL,
	24,	64,	35,	0x1fffffe0000000LL,
	25,	64,	36,	0x1ffffff0000000LL,
	26,	64,	37,	0x1ffffff8000000LL,
	27,	64,	38,	0x1ffffffc000000LL,
	28,	64,	39,	0x1ffffffe000000LL,
	29,	64,	40,	0x1fffffff000000LL,
	30,	64,	41,	0x1fffffff800000LL,
	31,	64,	42,	0x1fffffffc00000LL,
	32,	64,	43,	0x1fffffffe00000LL,
	33,	64,	44,	0x1ffffffff00000LL,
	34,	64,	45,	0x1ffffffff80000LL,
	35,	64,	46,	0x1ffffffffc0000LL,
	36,	64,	47,	0x1ffffffffe0000LL,
	37,	64,	48,	0x1fffffffff0000LL,
	38,	64,	49,	0x1fffffffff8000LL,
	39,	64,	50,	0x1fffffffffc000LL,
	40,	64,	51,	0x1fffffffffe000LL,
	41,	64,	52,	0x1ffffffffff000LL,
	42,	64,	53,	0x1ffffffffff800LL,
	43,	64,	54,	0x1ffffffffffc00LL,
	44,	64,	55,	0x1ffffffffffe00LL,
	45,	64,	56,	0x1fffffffffff00LL,
	46,	64,	57,	0x1fffffffffff80LL,
	47,	64,	58,	0x1fffffffffffc0LL,
	48,	64,	59,	0x1fffffffffffe0LL,
	49,	64,	60,	0x1ffffffffffff0LL,
	50,	64,	61,	0x1ffffffffffff8LL,
	51,	64,	62,	0x1ffffffffffffcLL,
	52,	64,	63,	0x1ffffffffffffeLL,
	53,	64,	0,	0x1fffffffffffffLL,
	1,	64,	11,	0x20000000000000LL,
	1,	32,	11,	0x20000000200000LL,
	1,	16,	11,	0x20002000200020LL,
	2,	64,	12,	0x30000000000000LL,
	2,	32,	12,	0x30000000300000LL,
	2,	16,	12,	0x30003000300030LL,
	3,	64,	13,	0x38000000000000LL,
	3,	32,	13,	0x38000000380000LL,
	3,	16,	13,	0x38003800380038LL,
	4,	64,	14,	0x3c000000000000LL,
	4,	32,	14,	0x3c0000003c0000LL,
	4,	16,	14,	0x3c003c003c003cLL,
	5,	64,	15,	0x3e000000000000LL,
	5,	32,	15,	0x3e0000003e0000LL,
	5,	16,	15,	0x3e003e003e003eLL,
	6,	64,	16,	0x3f000000000000LL,
	6,	32,	16,	0x3f0000003f0000LL,
	6,	16,	0,	0x3f003f003f003fLL,
	7,	64,	17,	0x3f800000000000LL,
	7,	32,	17,	0x3f8000003f8000LL,
	8,	64,	18,	0x3fc00000000000LL,
	8,	32,	18,	0x3fc000003fc000LL,
	9,	64,	19,	0x3fe00000000000LL,
	9,	32,	19,	0x3fe000003fe000LL,
	10,	64,	20,	0x3ff00000000000LL,
	10,	32,	20,	0x3ff000003ff000LL,
	11,	64,	21,	0x3ff80000000000LL,
	11,	32,	21,	0x3ff800003ff800LL,
	12,	64,	22,	0x3ffc0000000000LL,
	12,	32,	22,	0x3ffc00003ffc00LL,
	13,	64,	23,	0x3ffe0000000000LL,
	13,	32,	23,	0x3ffe00003ffe00LL,
	14,	64,	24,	0x3fff0000000000LL,
	14,	32,	24,	0x3fff00003fff00LL,
	15,	64,	25,	0x3fff8000000000LL,
	15,	32,	25,	0x3fff80003fff80LL,
	16,	64,	26,	0x3fffc000000000LL,
	16,	32,	26,	0x3fffc0003fffc0LL,
	17,	64,	27,	0x3fffe000000000LL,
	17,	32,	27,	0x3fffe0003fffe0LL,
	18,	64,	28,	0x3ffff000000000LL,
	18,	32,	28,	0x3ffff0003ffff0LL,
	19,	64,	29,	0x3ffff800000000LL,
	19,	32,	29,	0x3ffff8003ffff8LL,
	20,	64,	30,	0x3ffffc00000000LL,
	20,	32,	30,	0x3ffffc003ffffcLL,
	21,	64,	31,	0x3ffffe00000000LL,
	21,	32,	31,	0x3ffffe003ffffeLL,
	22,	64,	32,	0x3fffff00000000LL,
	22,	32,	0,	0x3fffff003fffffLL,
	23,	64,	33,	0x3fffff80000000LL,
	24,	64,	34,	0x3fffffc0000000LL,
	25,	64,	35,	0x3fffffe0000000LL,
	26,	64,	36,	0x3ffffff0000000LL,
	27,	64,	37,	0x3ffffff8000000LL,
	28,	64,	38,	0x3ffffffc000000LL,
	29,	64,	39,	0x3ffffffe000000LL,
	30,	64,	40,	0x3fffffff000000LL,
	31,	64,	41,	0x3fffffff800000LL,
	32,	64,	42,	0x3fffffffc00000LL,
	33,	64,	43,	0x3fffffffe00000LL,
	34,	64,	44,	0x3ffffffff00000LL,
	35,	64,	45,	0x3ffffffff80000LL,
	36,	64,	46,	0x3ffffffffc0000LL,
	37,	64,	47,	0x3ffffffffe0000LL,
	38,	64,	48,	0x3fffffffff0000LL,
	39,	64,	49,	0x3fffffffff8000LL,
	40,	64,	50,	0x3fffffffffc000LL,
	41,	64,	51,	0x3fffffffffe000LL,
	42,	64,	52,	0x3ffffffffff000LL,
	43,	64,	53,	0x3ffffffffff800LL,
	44,	64,	54,	0x3ffffffffffc00LL,
	45,	64,	55,	0x3ffffffffffe00LL,
	46,	64,	56,	0x3fffffffffff00LL,
	47,	64,	57,	0x3fffffffffff80LL,
	48,	64,	58,	0x3fffffffffffc0LL,
	49,	64,	59,	0x3fffffffffffe0LL,
	50,	64,	60,	0x3ffffffffffff0LL,
	51,	64,	61,	0x3ffffffffffff8LL,
	52,	64,	62,	0x3ffffffffffffcLL,
	53,	64,	63,	0x3ffffffffffffeLL,
	54,	64,	0,	0x3fffffffffffffLL,
	1,	64,	10,	0x40000000000000LL,
	1,	32,	10,	0x40000000400000LL,
	1,	16,	10,	0x40004000400040LL,
	2,	64,	11,	0x60000000000000LL,
	2,	32,	11,	0x60000000600000LL,
	2,	16,	11,	0x60006000600060LL,
	3,	64,	12,	0x70000000000000LL,
	3,	32,	12,	0x70000000700000LL,
	3,	16,	12,	0x70007000700070LL,
	4,	64,	13,	0x78000000000000LL,
	4,	32,	13,	0x78000000780000LL,
	4,	16,	13,	0x78007800780078LL,
	5,	64,	14,	0x7c000000000000LL,
	5,	32,	14,	0x7c0000007c0000LL,
	5,	16,	14,	0x7c007c007c007cLL,
	6,	64,	15,	0x7e000000000000LL,
	6,	32,	15,	0x7e0000007e0000LL,
	6,	16,	15,	0x7e007e007e007eLL,
	7,	64,	16,	0x7f000000000000LL,
	7,	32,	16,	0x7f0000007f0000LL,
	7,	16,	0,	0x7f007f007f007fLL,
	8,	64,	17,	0x7f800000000000LL,
	8,	32,	17,	0x7f8000007f8000LL,
	9,	64,	18,	0x7fc00000000000LL,
	9,	32,	18,	0x7fc000007fc000LL,
	10,	64,	19,	0x7fe00000000000LL,
	10,	32,	19,	0x7fe000007fe000LL,
	11,	64,	20,	0x7ff00000000000LL,
	11,	32,	20,	0x7ff000007ff000LL,
	12,	64,	21,	0x7ff80000000000LL,
	12,	32,	21,	0x7ff800007ff800LL,
	13,	64,	22,	0x7ffc0000000000LL,
	13,	32,	22,	0x7ffc00007ffc00LL,
	14,	64,	23,	0x7ffe0000000000LL,
	14,	32,	23,	0x7ffe00007ffe00LL,
	15,	64,	24,	0x7fff0000000000LL,
	15,	32,	24,	0x7fff00007fff00LL,
	16,	64,	25,	0x7fff8000000000LL,
	16,	32,	25,	0x7fff80007fff80LL,
	17,	64,	26,	0x7fffc000000000LL,
	17,	32,	26,	0x7fffc0007fffc0LL,
	18,	64,	27,	0x7fffe000000000LL,
	18,	32,	27,	0x7fffe0007fffe0LL,
	19,	64,	28,	0x7ffff000000000LL,
	19,	32,	28,	0x7ffff0007ffff0LL,
	20,	64,	29,	0x7ffff800000000LL,
	20,	32,	29,	0x7ffff8007ffff8LL,
	21,	64,	30,	0x7ffffc00000000LL,
	21,	32,	30,	0x7ffffc007ffffcLL,
	22,	64,	31,	0x7ffffe00000000LL,
	22,	32,	31,	0x7ffffe007ffffeLL,
	23,	64,	32,	0x7fffff00000000LL,
	23,	32,	0,	0x7fffff007fffffLL,
	24,	64,	33,	0x7fffff80000000LL,
	25,	64,	34,	0x7fffffc0000000LL,
	26,	64,	35,	0x7fffffe0000000LL,
	27,	64,	36,	0x7ffffff0000000LL,
	28,	64,	37,	0x7ffffff8000000LL,
	29,	64,	38,	0x7ffffffc000000LL,
	30,	64,	39,	0x7ffffffe000000LL,
	31,	64,	40,	0x7fffffff000000LL,
	32,	64,	41,	0x7fffffff800000LL,
	33,	64,	42,	0x7fffffffc00000LL,
	34,	64,	43,	0x7fffffffe00000LL,
	35,	64,	44,	0x7ffffffff00000LL,
	36,	64,	45,	0x7ffffffff80000LL,
	37,	64,	46,	0x7ffffffffc0000LL,
	38,	64,	47,	0x7ffffffffe0000LL,
	39,	64,	48,	0x7fffffffff0000LL,
	40,	64,	49,	0x7fffffffff8000LL,
	41,	64,	50,	0x7fffffffffc000LL,
	42,	64,	51,	0x7fffffffffe000LL,
	43,	64,	52,	0x7ffffffffff000LL,
	44,	64,	53,	0x7ffffffffff800LL,
	45,	64,	54,	0x7ffffffffffc00LL,
	46,	64,	55,	0x7ffffffffffe00LL,
	47,	64,	56,	0x7fffffffffff00LL,
	48,	64,	57,	0x7fffffffffff80LL,
	49,	64,	58,	0x7fffffffffffc0LL,
	50,	64,	59,	0x7fffffffffffe0LL,
	51,	64,	60,	0x7ffffffffffff0LL,
	52,	64,	61,	0x7ffffffffffff8LL,
	53,	64,	62,	0x7ffffffffffffcLL,
	54,	64,	63,	0x7ffffffffffffeLL,
	55,	64,	0,	0x7fffffffffffffLL,
	1,	64,	9,	0x80000000000000LL,
	1,	32,	9,	0x80000000800000LL,
	1,	16,	9,	0x80008000800080LL,
	2,	64,	10,	0xc0000000000000LL,
	2,	32,	10,	0xc0000000c00000LL,
	2,	16,	10,	0xc000c000c000c0LL,
	3,	64,	11,	0xe0000000000000LL,
	3,	32,	11,	0xe0000000e00000LL,
	3,	16,	11,	0xe000e000e000e0LL,
	4,	64,	12,	0xf0000000000000LL,
	4,	32,	12,	0xf0000000f00000LL,
	4,	16,	12,	0xf000f000f000f0LL,
	5,	64,	13,	0xf8000000000000LL,
	5,	32,	13,	0xf8000000f80000LL,
	5,	16,	13,	0xf800f800f800f8LL,
	6,	64,	14,	0xfc000000000000LL,
	6,	32,	14,	0xfc000000fc0000LL,
	6,	16,	14,	0xfc00fc00fc00fcLL,
	7,	64,	15,	0xfe000000000000LL,
	7,	32,	15,	0xfe000000fe0000LL,
	7,	16,	15,	0xfe00fe00fe00feLL,
	8,	64,	16,	0xff000000000000LL,
	8,	32,	16,	0xff000000ff0000LL,
	8,	16,	0,	0xff00ff00ff00ffLL,
	9,	64,	17,	0xff800000000000LL,
	9,	32,	17,	0xff800000ff8000LL,
	10,	64,	18,	0xffc00000000000LL,
	10,	32,	18,	0xffc00000ffc000LL,
	11,	64,	19,	0xffe00000000000LL,
	11,	32,	19,	0xffe00000ffe000LL,
	12,	64,	20,	0xfff00000000000LL,
	12,	32,	20,	0xfff00000fff000LL,
	13,	64,	21,	0xfff80000000000LL,
	13,	32,	21,	0xfff80000fff800LL,
	14,	64,	22,	0xfffc0000000000LL,
	14,	32,	22,	0xfffc0000fffc00LL,
	15,	64,	23,	0xfffe0000000000LL,
	15,	32,	23,	0xfffe0000fffe00LL,
	16,	64,	24,	0xffff0000000000LL,
	16,	32,	24,	0xffff0000ffff00LL,
	17,	64,	25,	0xffff8000000000LL,
	17,	32,	25,	0xffff8000ffff80LL,
	18,	64,	26,	0xffffc000000000LL,
	18,	32,	26,	0xffffc000ffffc0LL,
	19,	64,	27,	0xffffe000000000LL,
	19,	32,	27,	0xffffe000ffffe0LL,
	20,	64,	28,	0xfffff000000000LL,
	20,	32,	28,	0xfffff000fffff0LL,
	21,	64,	29,	0xfffff800000000LL,
	21,	32,	29,	0xfffff800fffff8LL,
	22,	64,	30,	0xfffffc00000000LL,
	22,	32,	30,	0xfffffc00fffffcLL,
	23,	64,	31,	0xfffffe00000000LL,
	23,	32,	31,	0xfffffe00fffffeLL,
	24,	64,	32,	0xffffff00000000LL,
	24,	32,	0,	0xffffff00ffffffLL,
	25,	64,	33,	0xffffff80000000LL,
	26,	64,	34,	0xffffffc0000000LL,
	27,	64,	35,	0xffffffe0000000LL,
	28,	64,	36,	0xfffffff0000000LL,
	29,	64,	37,	0xfffffff8000000LL,
	30,	64,	38,	0xfffffffc000000LL,
	31,	64,	39,	0xfffffffe000000LL,
	32,	64,	40,	0xffffffff000000LL,
	33,	64,	41,	0xffffffff800000LL,
	34,	64,	42,	0xffffffffc00000LL,
	35,	64,	43,	0xffffffffe00000LL,
	36,	64,	44,	0xfffffffff00000LL,
	37,	64,	45,	0xfffffffff80000LL,
	38,	64,	46,	0xfffffffffc0000LL,
	39,	64,	47,	0xfffffffffe0000LL,
	40,	64,	48,	0xffffffffff0000LL,
	41,	64,	49,	0xffffffffff8000LL,
	42,	64,	50,	0xffffffffffc000LL,
	43,	64,	51,	0xffffffffffe000LL,
	44,	64,	52,	0xfffffffffff000LL,
	45,	64,	53,	0xfffffffffff800LL,
	46,	64,	54,	0xfffffffffffc00LL,
	47,	64,	55,	0xfffffffffffe00LL,
	48,	64,	56,	0xffffffffffff00LL,
	49,	64,	57,	0xffffffffffff80LL,
	50,	64,	58,	0xffffffffffffc0LL,
	51,	64,	59,	0xffffffffffffe0LL,
	52,	64,	60,	0xfffffffffffff0LL,
	53,	64,	61,	0xfffffffffffff8LL,
	54,	64,	62,	0xfffffffffffffcLL,
	55,	64,	63,	0xfffffffffffffeLL,
	56,	64,	0,	0xffffffffffffffLL,
	1,	64,	8,	0x100000000000000LL,
	1,	32,	8,	0x100000001000000LL,
	1,	16,	8,	0x100010001000100LL,
	1,	8,	0,	0x101010101010101LL,
	2,	64,	9,	0x180000000000000LL,
	2,	32,	9,	0x180000001800000LL,
	2,	16,	9,	0x180018001800180LL,
	3,	64,	10,	0x1c0000000000000LL,
	3,	32,	10,	0x1c0000001c00000LL,
	3,	16,	10,	0x1c001c001c001c0LL,
	4,	64,	11,	0x1e0000000000000LL,
	4,	32,	11,	0x1e0000001e00000LL,
	4,	16,	11,	0x1e001e001e001e0LL,
	5,	64,	12,	0x1f0000000000000LL,
	5,	32,	12,	0x1f0000001f00000LL,
	5,	16,	12,	0x1f001f001f001f0LL,
	6,	64,	13,	0x1f8000000000000LL,
	6,	32,	13,	0x1f8000001f80000LL,
	6,	16,	13,	0x1f801f801f801f8LL,
	7,	64,	14,	0x1fc000000000000LL,
	7,	32,	14,	0x1fc000001fc0000LL,
	7,	16,	14,	0x1fc01fc01fc01fcLL,
	8,	64,	15,	0x1fe000000000000LL,
	8,	32,	15,	0x1fe000001fe0000LL,
	8,	16,	15,	0x1fe01fe01fe01feLL,
	9,	64,	16,	0x1ff000000000000LL,
	9,	32,	16,	0x1ff000001ff0000LL,
	9,	16,	0,	0x1ff01ff01ff01ffLL,
	10,	64,	17,	0x1ff800000000000LL,
	10,	32,	17,	0x1ff800001ff8000LL,
	11,	64,	18,	0x1ffc00000000000LL,
	11,	32,	18,	0x1ffc00001ffc000LL,
	12,	64,	19,	0x1ffe00000000000LL,
	12,	32,	19,	0x1ffe00001ffe000LL,
	13,	64,	20,	0x1fff00000000000LL,
	13,	32,	20,	0x1fff00001fff000LL,
	14,	64,	21,	0x1fff80000000000LL,
	14,	32,	21,	0x1fff80001fff800LL,
	15,	64,	22,	0x1fffc0000000000LL,
	15,	32,	22,	0x1fffc0001fffc00LL,
	16,	64,	23,	0x1fffe0000000000LL,
	16,	32,	23,	0x1fffe0001fffe00LL,
	17,	64,	24,	0x1ffff0000000000LL,
	17,	32,	24,	0x1ffff0001ffff00LL,
	18,	64,	25,	0x1ffff8000000000LL,
	18,	32,	25,	0x1ffff8001ffff80LL,
	19,	64,	26,	0x1ffffc000000000LL,
	19,	32,	26,	0x1ffffc001ffffc0LL,
	20,	64,	27,	0x1ffffe000000000LL,
	20,	32,	27,	0x1ffffe001ffffe0LL,
	21,	64,	28,	0x1fffff000000000LL,
	21,	32,	28,	0x1fffff001fffff0LL,
	22,	64,	29,	0x1fffff800000000LL,
	22,	32,	29,	0x1fffff801fffff8LL,
	23,	64,	30,	0x1fffffc00000000LL,
	23,	32,	30,	0x1fffffc01fffffcLL,
	24,	64,	31,	0x1fffffe00000000LL,
	24,	32,	31,	0x1fffffe01fffffeLL,
	25,	64,	32,	0x1ffffff00000000LL,
	25,	32,	0,	0x1ffffff01ffffffLL,
	26,	64,	33,	0x1ffffff80000000LL,
	27,	64,	34,	0x1ffffffc0000000LL,
	28,	64,	35,	0x1ffffffe0000000LL,
	29,	64,	36,	0x1fffffff0000000LL,
	30,	64,	37,	0x1fffffff8000000LL,
	31,	64,	38,	0x1fffffffc000000LL,
	32,	64,	39,	0x1fffffffe000000LL,
	33,	64,	40,	0x1ffffffff000000LL,
	34,	64,	41,	0x1ffffffff800000LL,
	35,	64,	42,	0x1ffffffffc00000LL,
	36,	64,	43,	0x1ffffffffe00000LL,
	37,	64,	44,	0x1fffffffff00000LL,
	38,	64,	45,	0x1fffffffff80000LL,
	39,	64,	46,	0x1fffffffffc0000LL,
	40,	64,	47,	0x1fffffffffe0000LL,
	41,	64,	48,	0x1ffffffffff0000LL,
	42,	64,	49,	0x1ffffffffff8000LL,
	43,	64,	50,	0x1ffffffffffc000LL,
	44,	64,	51,	0x1ffffffffffe000LL,
	45,	64,	52,	0x1fffffffffff000LL,
	46,	64,	53,	0x1fffffffffff800LL,
	47,	64,	54,	0x1fffffffffffc00LL,
	48,	64,	55,	0x1fffffffffffe00LL,
	49,	64,	56,	0x1ffffffffffff00LL,
	50,	64,	57,	0x1ffffffffffff80LL,
	51,	64,	58,	0x1ffffffffffffc0LL,
	52,	64,	59,	0x1ffffffffffffe0LL,
	53,	64,	60,	0x1fffffffffffff0LL,
	54,	64,	61,	0x1fffffffffffff8LL,
	55,	64,	62,	0x1fffffffffffffcLL,
	56,	64,	63,	0x1fffffffffffffeLL,
	57,	64,	0,	0x1ffffffffffffffLL,
	1,	64,	7,	0x200000000000000LL,
	1,	32,	7,	0x200000002000000LL,
	1,	16,	7,	0x200020002000200LL,
	1,	8,	7,	0x202020202020202LL,
	2,	64,	8,	0x300000000000000LL,
	2,	32,	8,	0x300000003000000LL,
	2,	16,	8,	0x300030003000300LL,
	2,	8,	0,	0x303030303030303LL,
	3,	64,	9,	0x380000000000000LL,
	3,	32,	9,	0x380000003800000LL,
	3,	16,	9,	0x380038003800380LL,
	4,	64,	10,	0x3c0000000000000LL,
	4,	32,	10,	0x3c0000003c00000LL,
	4,	16,	10,	0x3c003c003c003c0LL,
	5,	64,	11,	0x3e0000000000000LL,
	5,	32,	11,	0x3e0000003e00000LL,
	5,	16,	11,	0x3e003e003e003e0LL,
	6,	64,	12,	0x3f0000000000000LL,
	6,	32,	12,	0x3f0000003f00000LL,
	6,	16,	12,	0x3f003f003f003f0LL,
	7,	64,	13,	0x3f8000000000000LL,
	7,	32,	13,	0x3f8000003f80000LL,
	7,	16,	13,	0x3f803f803f803f8LL,
	8,	64,	14,	0x3fc000000000000LL,
	8,	32,	14,	0x3fc000003fc0000LL,
	8,	16,	14,	0x3fc03fc03fc03fcLL,
	9,	64,	15,	0x3fe000000000000LL,
	9,	32,	15,	0x3fe000003fe0000LL,
	9,	16,	15,	0x3fe03fe03fe03feLL,
	10,	64,	16,	0x3ff000000000000LL,
	10,	32,	16,	0x3ff000003ff0000LL,
	10,	16,	0,	0x3ff03ff03ff03ffLL,
	11,	64,	17,	0x3ff800000000000LL,
	11,	32,	17,	0x3ff800003ff8000LL,
	12,	64,	18,	0x3ffc00000000000LL,
	12,	32,	18,	0x3ffc00003ffc000LL,
	13,	64,	19,	0x3ffe00000000000LL,
	13,	32,	19,	0x3ffe00003ffe000LL,
	14,	64,	20,	0x3fff00000000000LL,
	14,	32,	20,	0x3fff00003fff000LL,
	15,	64,	21,	0x3fff80000000000LL,
	15,	32,	21,	0x3fff80003fff800LL,
	16,	64,	22,	0x3fffc0000000000LL,
	16,	32,	22,	0x3fffc0003fffc00LL,
	17,	64,	23,	0x3fffe0000000000LL,
	17,	32,	23,	0x3fffe0003fffe00LL,
	18,	64,	24,	0x3ffff0000000000LL,
	18,	32,	24,	0x3ffff0003ffff00LL,
	19,	64,	25,	0x3ffff8000000000LL,
	19,	32,	25,	0x3ffff8003ffff80LL,
	20,	64,	26,	0x3ffffc000000000LL,
	20,	32,	26,	0x3ffffc003ffffc0LL,
	21,	64,	27,	0x3ffffe000000000LL,
	21,	32,	27,	0x3ffffe003ffffe0LL,
	22,	64,	28,	0x3fffff000000000LL,
	22,	32,	28,	0x3fffff003fffff0LL,
	23,	64,	29,	0x3fffff800000000LL,
	23,	32,	29,	0x3fffff803fffff8LL,
	24,	64,	30,	0x3fffffc00000000LL,
	24,	32,	30,	0x3fffffc03fffffcLL,
	25,	64,	31,	0x3fffffe00000000LL,
	25,	32,	31,	0x3fffffe03fffffeLL,
	26,	64,	32,	0x3ffffff00000000LL,
	26,	32,	0,	0x3ffffff03ffffffLL,
	27,	64,	33,	0x3ffffff80000000LL,
	28,	64,	34,	0x3ffffffc0000000LL,
	29,	64,	35,	0x3ffffffe0000000LL,
	30,	64,	36,	0x3fffffff0000000LL,
	31,	64,	37,	0x3fffffff8000000LL,
	32,	64,	38,	0x3fffffffc000000LL,
	33,	64,	39,	0x3fffffffe000000LL,
	34,	64,	40,	0x3ffffffff000000LL,
	35,	64,	41,	0x3ffffffff800000LL,
	36,	64,	42,	0x3ffffffffc00000LL,
	37,	64,	43,	0x3ffffffffe00000LL,
	38,	64,	44,	0x3fffffffff00000LL,
	39,	64,	45,	0x3fffffffff80000LL,
	40,	64,	46,	0x3fffffffffc0000LL,
	41,	64,	47,	0x3fffffffffe0000LL,
	42,	64,	48,	0x3ffffffffff0000LL,
	43,	64,	49,	0x3ffffffffff8000LL,
	44,	64,	50,	0x3ffffffffffc000LL,
	45,	64,	51,	0x3ffffffffffe000LL,
	46,	64,	52,	0x3fffffffffff000LL,
	47,	64,	53,	0x3fffffffffff800LL,
	48,	64,	54,	0x3fffffffffffc00LL,
	49,	64,	55,	0x3fffffffffffe00LL,
	50,	64,	56,	0x3ffffffffffff00LL,
	51,	64,	57,	0x3ffffffffffff80LL,
	52,	64,	58,	0x3ffffffffffffc0LL,
	53,	64,	59,	0x3ffffffffffffe0LL,
	54,	64,	60,	0x3fffffffffffff0LL,
	55,	64,	61,	0x3fffffffffffff8LL,
	56,	64,	62,	0x3fffffffffffffcLL,
	57,	64,	63,	0x3fffffffffffffeLL,
	58,	64,	0,	0x3ffffffffffffffLL,
	1,	64,	6,	0x400000000000000LL,
	1,	32,	6,	0x400000004000000LL,
	1,	16,	6,	0x400040004000400LL,
	1,	8,	6,	0x404040404040404LL,
	2,	64,	7,	0x600000000000000LL,
	2,	32,	7,	0x600000006000000LL,
	2,	16,	7,	0x600060006000600LL,
	2,	8,	7,	0x606060606060606LL,
	3,	64,	8,	0x700000000000000LL,
	3,	32,	8,	0x700000007000000LL,
	3,	16,	8,	0x700070007000700LL,
	3,	8,	0,	0x707070707070707LL,
	4,	64,	9,	0x780000000000000LL,
	4,	32,	9,	0x780000007800000LL,
	4,	16,	9,	0x780078007800780LL,
	5,	64,	10,	0x7c0000000000000LL,
	5,	32,	10,	0x7c0000007c00000LL,
	5,	16,	10,	0x7c007c007c007c0LL,
	6,	64,	11,	0x7e0000000000000LL,
	6,	32,	11,	0x7e0000007e00000LL,
	6,	16,	11,	0x7e007e007e007e0LL,
	7,	64,	12,	0x7f0000000000000LL,
	7,	32,	12,	0x7f0000007f00000LL,
	7,	16,	12,	0x7f007f007f007f0LL,
	8,	64,	13,	0x7f8000000000000LL,
	8,	32,	13,	0x7f8000007f80000LL,
	8,	16,	13,	0x7f807f807f807f8LL,
	9,	64,	14,	0x7fc000000000000LL,
	9,	32,	14,	0x7fc000007fc0000LL,
	9,	16,	14,	0x7fc07fc07fc07fcLL,
	10,	64,	15,	0x7fe000000000000LL,
	10,	32,	15,	0x7fe000007fe0000LL,
	10,	16,	15,	0x7fe07fe07fe07feLL,
	11,	64,	16,	0x7ff000000000000LL,
	11,	32,	16,	0x7ff000007ff0000LL,
	11,	16,	0,	0x7ff07ff07ff07ffLL,
	12,	64,	17,	0x7ff800000000000LL,
	12,	32,	17,	0x7ff800007ff8000LL,
	13,	64,	18,	0x7ffc00000000000LL,
	13,	32,	18,	0x7ffc00007ffc000LL,
	14,	64,	19,	0x7ffe00000000000LL,
	14,	32,	19,	0x7ffe00007ffe000LL,
	15,	64,	20,	0x7fff00000000000LL,
	15,	32,	20,	0x7fff00007fff000LL,
	16,	64,	21,	0x7fff80000000000LL,
	16,	32,	21,	0x7fff80007fff800LL,
	17,	64,	22,	0x7fffc0000000000LL,
	17,	32,	22,	0x7fffc0007fffc00LL,
	18,	64,	23,	0x7fffe0000000000LL,
	18,	32,	23,	0x7fffe0007fffe00LL,
	19,	64,	24,	0x7ffff0000000000LL,
	19,	32,	24,	0x7ffff0007ffff00LL,
	20,	64,	25,	0x7ffff8000000000LL,
	20,	32,	25,	0x7ffff8007ffff80LL,
	21,	64,	26,	0x7ffffc000000000LL,
	21,	32,	26,	0x7ffffc007ffffc0LL,
	22,	64,	27,	0x7ffffe000000000LL,
	22,	32,	27,	0x7ffffe007ffffe0LL,
	23,	64,	28,	0x7fffff000000000LL,
	23,	32,	28,	0x7fffff007fffff0LL,
	24,	64,	29,	0x7fffff800000000LL,
	24,	32,	29,	0x7fffff807fffff8LL,
	25,	64,	30,	0x7fffffc00000000LL,
	25,	32,	30,	0x7fffffc07fffffcLL,
	26,	64,	31,	0x7fffffe00000000LL,
	26,	32,	31,	0x7fffffe07fffffeLL,
	27,	64,	32,	0x7ffffff00000000LL,
	27,	32,	0,	0x7ffffff07ffffffLL,
	28,	64,	33,	0x7ffffff80000000LL,
	29,	64,	34,	0x7ffffffc0000000LL,
	30,	64,	35,	0x7ffffffe0000000LL,
	31,	64,	36,	0x7fffffff0000000LL,
	32,	64,	37,	0x7fffffff8000000LL,
	33,	64,	38,	0x7fffffffc000000LL,
	34,	64,	39,	0x7fffffffe000000LL,
	35,	64,	40,	0x7ffffffff000000LL,
	36,	64,	41,	0x7ffffffff800000LL,
	37,	64,	42,	0x7ffffffffc00000LL,
	38,	64,	43,	0x7ffffffffe00000LL,
	39,	64,	44,	0x7fffffffff00000LL,
	40,	64,	45,	0x7fffffffff80000LL,
	41,	64,	46,	0x7fffffffffc0000LL,
	42,	64,	47,	0x7fffffffffe0000LL,
	43,	64,	48,	0x7ffffffffff0000LL,
	44,	64,	49,	0x7ffffffffff8000LL,
	45,	64,	50,	0x7ffffffffffc000LL,
	46,	64,	51,	0x7ffffffffffe000LL,
	47,	64,	52,	0x7fffffffffff000LL,
	48,	64,	53,	0x7fffffffffff800LL,
	49,	64,	54,	0x7fffffffffffc00LL,
	50,	64,	55,	0x7fffffffffffe00LL,
	51,	64,	56,	0x7ffffffffffff00LL,
	52,	64,	57,	0x7ffffffffffff80LL,
	53,	64,	58,	0x7ffffffffffffc0LL,
	54,	64,	59,	0x7ffffffffffffe0LL,
	55,	64,	60,	0x7fffffffffffff0LL,
	56,	64,	61,	0x7fffffffffffff8LL,
	57,	64,	62,	0x7fffffffffffffcLL,
	58,	64,	63,	0x7fffffffffffffeLL,
	59,	64,	0,	0x7ffffffffffffffLL,
	1,	64,	5,	0x800000000000000LL,
	1,	32,	5,	0x800000008000000LL,
	1,	16,	5,	0x800080008000800LL,
	1,	8,	5,	0x808080808080808LL,
	2,	64,	6,	0xc00000000000000LL,
	2,	32,	6,	0xc0000000c000000LL,
	2,	16,	6,	0xc000c000c000c00LL,
	2,	8,	6,	0xc0c0c0c0c0c0c0cLL,
	3,	64,	7,	0xe00000000000000LL,
	3,	32,	7,	0xe0000000e000000LL,
	3,	16,	7,	0xe000e000e000e00LL,
	3,	8,	7,	0xe0e0e0e0e0e0e0eLL,
	4,	64,	8,	0xf00000000000000LL,
	4,	32,	8,	0xf0000000f000000LL,
	4,	16,	8,	0xf000f000f000f00LL,
	4,	8,	0,	0xf0f0f0f0f0f0f0fLL,
	5,	64,	9,	0xf80000000000000LL,
	5,	32,	9,	0xf8000000f800000LL,
	5,	16,	9,	0xf800f800f800f80LL,
	6,	64,	10,	0xfc0000000000000LL,
	6,	32,	10,	0xfc000000fc00000LL,
	6,	16,	10,	0xfc00fc00fc00fc0LL,
	7,	64,	11,	0xfe0000000000000LL,
	7,	32,	11,	0xfe000000fe00000LL,
	7,	16,	11,	0xfe00fe00fe00fe0LL,
	8,	64,	12,	0xff0000000000000LL,
	8,	32,	12,	0xff000000ff00000LL,
	8,	16,	12,	0xff00ff00ff00ff0LL,
	9,	64,	13,	0xff8000000000000LL,
	9,	32,	13,	0xff800000ff80000LL,
	9,	16,	13,	0xff80ff80ff80ff8LL,
	10,	64,	14,	0xffc000000000000LL,
	10,	32,	14,	0xffc00000ffc0000LL,
	10,	16,	14,	0xffc0ffc0ffc0ffcLL,
	11,	64,	15,	0xffe000000000000LL,
	11,	32,	15,	0xffe00000ffe0000LL,
	11,	16,	15,	0xffe0ffe0ffe0ffeLL,
	12,	64,	16,	0xfff000000000000LL,
	12,	32,	16,	0xfff00000fff0000LL,
	12,	16,	0,	0xfff0fff0fff0fffLL,
	13,	64,	17,	0xfff800000000000LL,
	13,	32,	17,	0xfff80000fff8000LL,
	14,	64,	18,	0xfffc00000000000LL,
	14,	32,	18,	0xfffc0000fffc000LL,
	15,	64,	19,	0xfffe00000000000LL,
	15,	32,	19,	0xfffe0000fffe000LL,
	16,	64,	20,	0xffff00000000000LL,
	16,	32,	20,	0xffff0000ffff000LL,
	17,	64,	21,	0xffff80000000000LL,
	17,	32,	21,	0xffff8000ffff800LL,
	18,	64,	22,	0xffffc0000000000LL,
	18,	32,	22,	0xffffc000ffffc00LL,
	19,	64,	23,	0xffffe0000000000LL,
	19,	32,	23,	0xffffe000ffffe00LL,
	20,	64,	24,	0xfffff0000000000LL,
	20,	32,	24,	0xfffff000fffff00LL,
	21,	64,	25,	0xfffff8000000000LL,
	21,	32,	25,	0xfffff800fffff80LL,
	22,	64,	26,	0xfffffc000000000LL,
	22,	32,	26,	0xfffffc00fffffc0LL,
	23,	64,	27,	0xfffffe000000000LL,
	23,	32,	27,	0xfffffe00fffffe0LL,
	24,	64,	28,	0xffffff000000000LL,
	24,	32,	28,	0xffffff00ffffff0LL,
	25,	64,	29,	0xffffff800000000LL,
	25,	32,	29,	0xffffff80ffffff8LL,
	26,	64,	30,	0xffffffc00000000LL,
	26,	32,	30,	0xffffffc0ffffffcLL,
	27,	64,	31,	0xffffffe00000000LL,
	27,	32,	31,	0xffffffe0ffffffeLL,
	28,	64,	32,	0xfffffff00000000LL,
	28,	32,	0,	0xfffffff0fffffffLL,
	29,	64,	33,	0xfffffff80000000LL,
	30,	64,	34,	0xfffffffc0000000LL,
	31,	64,	35,	0xfffffffe0000000LL,
	32,	64,	36,	0xffffffff0000000LL,
	33,	64,	37,	0xffffffff8000000LL,
	34,	64,	38,	0xffffffffc000000LL,
	35,	64,	39,	0xffffffffe000000LL,
	36,	64,	40,	0xfffffffff000000LL,
	37,	64,	41,	0xfffffffff800000LL,
	38,	64,	42,	0xfffffffffc00000LL,
	39,	64,	43,	0xfffffffffe00000LL,
	40,	64,	44,	0xffffffffff00000LL,
	41,	64,	45,	0xffffffffff80000LL,
	42,	64,	46,	0xffffffffffc0000LL,
	43,	64,	47,	0xffffffffffe0000LL,
	44,	64,	48,	0xfffffffffff0000LL,
	45,	64,	49,	0xfffffffffff8000LL,
	46,	64,	50,	0xfffffffffffc000LL,
	47,	64,	51,	0xfffffffffffe000LL,
	48,	64,	52,	0xffffffffffff000LL,
	49,	64,	53,	0xffffffffffff800LL,
	50,	64,	54,	0xffffffffffffc00LL,
	51,	64,	55,	0xffffffffffffe00LL,
	52,	64,	56,	0xfffffffffffff00LL,
	53,	64,	57,	0xfffffffffffff80LL,
	54,	64,	58,	0xfffffffffffffc0LL,
	55,	64,	59,	0xfffffffffffffe0LL,
	56,	64,	60,	0xffffffffffffff0LL,
	57,	64,	61,	0xffffffffffffff8LL,
	58,	64,	62,	0xffffffffffffffcLL,
	59,	64,	63,	0xffffffffffffffeLL,
	60,	64,	0,	0xfffffffffffffffLL,
	1,	64,	4,	0x1000000000000000LL,
	1,	32,	4,	0x1000000010000000LL,
	1,	16,	4,	0x1000100010001000LL,
	1,	8,	4,	0x1010101010101010LL,
	1,	4,	0,	0x1111111111111111LL,
	2,	64,	5,	0x1800000000000000LL,
	2,	32,	5,	0x1800000018000000LL,
	2,	16,	5,	0x1800180018001800LL,
	2,	8,	5,	0x1818181818181818LL,
	3,	64,	6,	0x1c00000000000000LL,
	3,	32,	6,	0x1c0000001c000000LL,
	3,	16,	6,	0x1c001c001c001c00LL,
	3,	8,	6,	0x1c1c1c1c1c1c1c1cLL,
	4,	64,	7,	0x1e00000000000000LL,
	4,	32,	7,	0x1e0000001e000000LL,
	4,	16,	7,	0x1e001e001e001e00LL,
	4,	8,	7,	0x1e1e1e1e1e1e1e1eLL,
	5,	64,	8,	0x1f00000000000000LL,
	5,	32,	8,	0x1f0000001f000000LL,
	5,	16,	8,	0x1f001f001f001f00LL,
	5,	8,	0,	0x1f1f1f1f1f1f1f1fLL,
	6,	64,	9,	0x1f80000000000000LL,
	6,	32,	9,	0x1f8000001f800000LL,
	6,	16,	9,	0x1f801f801f801f80LL,
	7,	64,	10,	0x1fc0000000000000LL,
	7,	32,	10,	0x1fc000001fc00000LL,
	7,	16,	10,	0x1fc01fc01fc01fc0LL,
	8,	64,	11,	0x1fe0000000000000LL,
	8,	32,	11,	0x1fe000001fe00000LL,
	8,	16,	11,	0x1fe01fe01fe01fe0LL,
	9,	64,	12,	0x1ff0000000000000LL,
	9,	32,	12,	0x1ff000001ff00000LL,
	9,	16,	12,	0x1ff01ff01ff01ff0LL,
	10,	64,	13,	0x1ff8000000000000LL,
	10,	32,	13,	0x1ff800001ff80000LL,
	10,	16,	13,	0x1ff81ff81ff81ff8LL,
	11,	64,	14,	0x1ffc000000000000LL,
	11,	32,	14,	0x1ffc00001ffc0000LL,
	11,	16,	14,	0x1ffc1ffc1ffc1ffcLL,
	12,	64,	15,	0x1ffe000000000000LL,
	12,	32,	15,	0x1ffe00001ffe0000LL,
	12,	16,	15,	0x1ffe1ffe1ffe1ffeLL,
	13,	64,	16,	0x1fff000000000000LL,
	13,	32,	16,	0x1fff00001fff0000LL,
	13,	16,	0,	0x1fff1fff1fff1fffLL,
	14,	64,	17,	0x1fff800000000000LL,
	14,	32,	17,	0x1fff80001fff8000LL,
	15,	64,	18,	0x1fffc00000000000LL,
	15,	32,	18,	0x1fffc0001fffc000LL,
	16,	64,	19,	0x1fffe00000000000LL,
	16,	32,	19,	0x1fffe0001fffe000LL,
	17,	64,	20,	0x1ffff00000000000LL,
	17,	32,	20,	0x1ffff0001ffff000LL,
	18,	64,	21,	0x1ffff80000000000LL,
	18,	32,	21,	0x1ffff8001ffff800LL,
	19,	64,	22,	0x1ffffc0000000000LL,
	19,	32,	22,	0x1ffffc001ffffc00LL,
	20,	64,	23,	0x1ffffe0000000000LL,
	20,	32,	23,	0x1ffffe001ffffe00LL,
	21,	64,	24,	0x1fffff0000000000LL,
	21,	32,	24,	0x1fffff001fffff00LL,
	22,	64,	25,	0x1fffff8000000000LL,
	22,	32,	25,	0x1fffff801fffff80LL,
	23,	64,	26,	0x1fffffc000000000LL,
	23,	32,	26,	0x1fffffc01fffffc0LL,
	24,	64,	27,	0x1fffffe000000000LL,
	24,	32,	27,	0x1fffffe01fffffe0LL,
	25,	64,	28,	0x1ffffff000000000LL,
	25,	32,	28,	0x1ffffff01ffffff0LL,
	26,	64,	29,	0x1ffffff800000000LL,
	26,	32,	29,	0x1ffffff81ffffff8LL,
	27,	64,	30,	0x1ffffffc00000000LL,
	27,	32,	30,	0x1ffffffc1ffffffcLL,
	28,	64,	31,	0x1ffffffe00000000LL,
	28,	32,	31,	0x1ffffffe1ffffffeLL,
	29,	64,	32,	0x1fffffff00000000LL,
	29,	32,	0,	0x1fffffff1fffffffLL,
	30,	64,	33,	0x1fffffff80000000LL,
	31,	64,	34,	0x1fffffffc0000000LL,
	32,	64,	35,	0x1fffffffe0000000LL,
	33,	64,	36,	0x1ffffffff0000000LL,
	34,	64,	37,	0x1ffffffff8000000LL,
	35,	64,	38,	0x1ffffffffc000000LL,
	36,	64,	39,	0x1ffffffffe000000LL,
	37,	64,	40,	0x1fffffffff000000LL,
	38,	64,	41,	0x1fffffffff800000LL,
	39,	64,	42,	0x1fffffffffc00000LL,
	40,	64,	43,	0x1fffffffffe00000LL,
	41,	64,	44,	0x1ffffffffff00000LL,
	42,	64,	45,	0x1ffffffffff80000LL,
	43,	64,	46,	0x1ffffffffffc0000LL,
	44,	64,	47,	0x1ffffffffffe0000LL,
	45,	64,	48,	0x1fffffffffff0000LL,
	46,	64,	49,	0x1fffffffffff8000LL,
	47,	64,	50,	0x1fffffffffffc000LL,
	48,	64,	51,	0x1fffffffffffe000LL,
	49,	64,	52,	0x1ffffffffffff000LL,
	50,	64,	53,	0x1ffffffffffff800LL,
	51,	64,	54,	0x1ffffffffffffc00LL,
	52,	64,	55,	0x1ffffffffffffe00LL,
	53,	64,	56,	0x1fffffffffffff00LL,
	54,	64,	57,	0x1fffffffffffff80LL,
	55,	64,	58,	0x1fffffffffffffc0LL,
	56,	64,	59,	0x1fffffffffffffe0LL,
	57,	64,	60,	0x1ffffffffffffff0LL,
	58,	64,	61,	0x1ffffffffffffff8LL,
	59,	64,	62,	0x1ffffffffffffffcLL,
	60,	64,	63,	0x1ffffffffffffffeLL,
	61,	64,	0,	0x1fffffffffffffffLL,
	1,	64,	3,	0x2000000000000000LL,
	1,	32,	3,	0x2000000020000000LL,
	1,	16,	3,	0x2000200020002000LL,
	1,	8,	3,	0x2020202020202020LL,
	1,	4,	3,	0x2222222222222222LL,
	2,	64,	4,	0x3000000000000000LL,
	2,	32,	4,	0x3000000030000000LL,
	2,	16,	4,	0x3000300030003000LL,
	2,	8,	4,	0x3030303030303030LL,
	2,	4,	0,	0x3333333333333333LL,
	3,	64,	5,	0x3800000000000000LL,
	3,	32,	5,	0x3800000038000000LL,
	3,	16,	5,	0x3800380038003800LL,
	3,	8,	5,	0x3838383838383838LL,
	4,	64,	6,	0x3c00000000000000LL,
	4,	32,	6,	0x3c0000003c000000LL,
	4,	16,	6,	0x3c003c003c003c00LL,
	4,	8,	6,	0x3c3c3c3c3c3c3c3cLL,
	5,	64,	7,	0x3e00000000000000LL,
	5,	32,	7,	0x3e0000003e000000LL,
	5,	16,	7,	0x3e003e003e003e00LL,
	5,	8,	7,	0x3e3e3e3e3e3e3e3eLL,
	6,	64,	8,	0x3f00000000000000LL,
	6,	32,	8,	0x3f0000003f000000LL,
	6,	16,	8,	0x3f003f003f003f00LL,
	6,	8,	0,	0x3f3f3f3f3f3f3f3fLL,
	7,	64,	9,	0x3f80000000000000LL,
	7,	32,	9,	0x3f8000003f800000LL,
	7,	16,	9,	0x3f803f803f803f80LL,
	8,	64,	10,	0x3fc0000000000000LL,
	8,	32,	10,	0x3fc000003fc00000LL,
	8,	16,	10,	0x3fc03fc03fc03fc0LL,
	9,	64,	11,	0x3fe0000000000000LL,
	9,	32,	11,	0x3fe000003fe00000LL,
	9,	16,	11,	0x3fe03fe03fe03fe0LL,
	10,	64,	12,	0x3ff0000000000000LL,
	10,	32,	12,	0x3ff000003ff00000LL,
	10,	16,	12,	0x3ff03ff03ff03ff0LL,
	11,	64,	13,	0x3ff8000000000000LL,
	11,	32,	13,	0x3ff800003ff80000LL,
	11,	16,	13,	0x3ff83ff83ff83ff8LL,
	12,	64,	14,	0x3ffc000000000000LL,
	12,	32,	14,	0x3ffc00003ffc0000LL,
	12,	16,	14,	0x3ffc3ffc3ffc3ffcLL,
	13,	64,	15,	0x3ffe000000000000LL,
	13,	32,	15,	0x3ffe00003ffe0000LL,
	13,	16,	15,	0x3ffe3ffe3ffe3ffeLL,
	14,	64,	16,	0x3fff000000000000LL,
	14,	32,	16,	0x3fff00003fff0000LL,
	14,	16,	0,	0x3fff3fff3fff3fffLL,
	15,	64,	17,	0x3fff800000000000LL,
	15,	32,	17,	0x3fff80003fff8000LL,
	16,	64,	18,	0x3fffc00000000000LL,
	16,	32,	18,	0x3fffc0003fffc000LL,
	17,	64,	19,	0x3fffe00000000000LL,
	17,	32,	19,	0x3fffe0003fffe000LL,
	18,	64,	20,	0x3ffff00000000000LL,
	18,	32,	20,	0x3ffff0003ffff000LL,
	19,	64,	21,	0x3ffff80000000000LL,
	19,	32,	21,	0x3ffff8003ffff800LL,
	20,	64,	22,	0x3ffffc0000000000LL,
	20,	32,	22,	0x3ffffc003ffffc00LL,
	21,	64,	23,	0x3ffffe0000000000LL,
	21,	32,	23,	0x3ffffe003ffffe00LL,
	22,	64,	24,	0x3fffff0000000000LL,
	22,	32,	24,	0x3fffff003fffff00LL,
	23,	64,	25,	0x3fffff8000000000LL,
	23,	32,	25,	0x3fffff803fffff80LL,
	24,	64,	26,	0x3fffffc000000000LL,
	24,	32,	26,	0x3fffffc03fffffc0LL,
	25,	64,	27,	0x3fffffe000000000LL,
	25,	32,	27,	0x3fffffe03fffffe0LL,
	26,	64,	28,	0x3ffffff000000000LL,
	26,	32,	28,	0x3ffffff03ffffff0LL,
	27,	64,	29,	0x3ffffff800000000LL,
	27,	32,	29,	0x3ffffff83ffffff8LL,
	28,	64,	30,	0x3ffffffc00000000LL,
	28,	32,	30,	0x3ffffffc3ffffffcLL,
	29,	64,	31,	0x3ffffffe00000000LL,
	29,	32,	31,	0x3ffffffe3ffffffeLL,
	30,	64,	32,	0x3fffffff00000000LL,
	30,	32,	0,	0x3fffffff3fffffffLL,
	31,	64,	33,	0x3fffffff80000000LL,
	32,	64,	34,	0x3fffffffc0000000LL,
	33,	64,	35,	0x3fffffffe0000000LL,
	34,	64,	36,	0x3ffffffff0000000LL,
	35,	64,	37,	0x3ffffffff8000000LL,
	36,	64,	38,	0x3ffffffffc000000LL,
	37,	64,	39,	0x3ffffffffe000000LL,
	38,	64,	40,	0x3fffffffff000000LL,
	39,	64,	41,	0x3fffffffff800000LL,
	40,	64,	42,	0x3fffffffffc00000LL,
	41,	64,	43,	0x3fffffffffe00000LL,
	42,	64,	44,	0x3ffffffffff00000LL,
	43,	64,	45,	0x3ffffffffff80000LL,
	44,	64,	46,	0x3ffffffffffc0000LL,
	45,	64,	47,	0x3ffffffffffe0000LL,
	46,	64,	48,	0x3fffffffffff0000LL,
	47,	64,	49,	0x3fffffffffff8000LL,
	48,	64,	50,	0x3fffffffffffc000LL,
	49,	64,	51,	0x3fffffffffffe000LL,
	50,	64,	52,	0x3ffffffffffff000LL,
	51,	64,	53,	0x3ffffffffffff800LL,
	52,	64,	54,	0x3ffffffffffffc00LL,
	53,	64,	55,	0x3ffffffffffffe00LL,
	54,	64,	56,	0x3fffffffffffff00LL,
	55,	64,	57,	0x3fffffffffffff80LL,
	56,	64,	58,	0x3fffffffffffffc0LL,
	57,	64,	59,	0x3fffffffffffffe0LL,
	58,	64,	60,	0x3ffffffffffffff0LL,
	59,	64,	61,	0x3ffffffffffffff8LL,
	60,	64,	62,	0x3ffffffffffffffcLL,
	61,	64,	63,	0x3ffffffffffffffeLL,
	62,	64,	0,	0x3fffffffffffffffLL,
	1,	64,	2,	0x4000000000000000LL,
	1,	32,	2,	0x4000000040000000LL,
	1,	16,	2,	0x4000400040004000LL,
	1,	8,	2,	0x4040404040404040LL,
	1,	4,	2,	0x4444444444444444LL,
	1,	2,	0,	0x5555555555555555LL,
	2,	64,	3,	0x6000000000000000LL,
	2,	32,	3,	0x6000000060000000LL,
	2,	16,	3,	0x6000600060006000LL,
	2,	8,	3,	0x6060606060606060LL,
	2,	4,	3,	0x6666666666666666LL,
	3,	64,	4,	0x7000000000000000LL,
	3,	32,	4,	0x7000000070000000LL,
	3,	16,	4,	0x7000700070007000LL,
	3,	8,	4,	0x7070707070707070LL,
	3,	4,	0,	0x7777777777777777LL,
	4,	64,	5,	0x7800000000000000LL,
	4,	32,	5,	0x7800000078000000LL,
	4,	16,	5,	0x7800780078007800LL,
	4,	8,	5,	0x7878787878787878LL,
	5,	64,	6,	0x7c00000000000000LL,
	5,	32,	6,	0x7c0000007c000000LL,
	5,	16,	6,	0x7c007c007c007c00LL,
	5,	8,	6,	0x7c7c7c7c7c7c7c7cLL,
	6,	64,	7,	0x7e00000000000000LL,
	6,	32,	7,	0x7e0000007e000000LL,
	6,	16,	7,	0x7e007e007e007e00LL,
	6,	8,	7,	0x7e7e7e7e7e7e7e7eLL,
	7,	64,	8,	0x7f00000000000000LL,
	7,	32,	8,	0x7f0000007f000000LL,
	7,	16,	8,	0x7f007f007f007f00LL,
	7,	8,	0,	0x7f7f7f7f7f7f7f7fLL,
	8,	64,	9,	0x7f80000000000000LL,
	8,	32,	9,	0x7f8000007f800000LL,
	8,	16,	9,	0x7f807f807f807f80LL,
	9,	64,	10,	0x7fc0000000000000LL,
	9,	32,	10,	0x7fc000007fc00000LL,
	9,	16,	10,	0x7fc07fc07fc07fc0LL,
	10,	64,	11,	0x7fe0000000000000LL,
	10,	32,	11,	0x7fe000007fe00000LL,
	10,	16,	11,	0x7fe07fe07fe07fe0LL,
	11,	64,	12,	0x7ff0000000000000LL,
	11,	32,	12,	0x7ff000007ff00000LL,
	11,	16,	12,	0x7ff07ff07ff07ff0LL,
	12,	64,	13,	0x7ff8000000000000LL,
	12,	32,	13,	0x7ff800007ff80000LL,
	12,	16,	13,	0x7ff87ff87ff87ff8LL,
	13,	64,	14,	0x7ffc000000000000LL,
	13,	32,	14,	0x7ffc00007ffc0000LL,
	13,	16,	14,	0x7ffc7ffc7ffc7ffcLL,
	14,	64,	15,	0x7ffe000000000000LL,
	14,	32,	15,	0x7ffe00007ffe0000LL,
	14,	16,	15,	0x7ffe7ffe7ffe7ffeLL,
	15,	64,	16,	0x7fff000000000000LL,
	15,	32,	16,	0x7fff00007fff0000LL,
	15,	16,	0,	0x7fff7fff7fff7fffLL,
	16,	64,	17,	0x7fff800000000000LL,
	16,	32,	17,	0x7fff80007fff8000LL,
	17,	64,	18,	0x7fffc00000000000LL,
	17,	32,	18,	0x7fffc0007fffc000LL,
	18,	64,	19,	0x7fffe00000000000LL,
	18,	32,	19,	0x7fffe0007fffe000LL,
	19,	64,	20,	0x7ffff00000000000LL,
	19,	32,	20,	0x7ffff0007ffff000LL,
	20,	64,	21,	0x7ffff80000000000LL,
	20,	32,	21,	0x7ffff8007ffff800LL,
	21,	64,	22,	0x7ffffc0000000000LL,
	21,	32,	22,	0x7ffffc007ffffc00LL,
	22,	64,	23,	0x7ffffe0000000000LL,
	22,	32,	23,	0x7ffffe007ffffe00LL,
	23,	64,	24,	0x7fffff0000000000LL,
	23,	32,	24,	0x7fffff007fffff00LL,
	24,	64,	25,	0x7fffff8000000000LL,
	24,	32,	25,	0x7fffff807fffff80LL,
	25,	64,	26,	0x7fffffc000000000LL,
	25,	32,	26,	0x7fffffc07fffffc0LL,
	26,	64,	27,	0x7fffffe000000000LL,
	26,	32,	27,	0x7fffffe07fffffe0LL,
	27,	64,	28,	0x7ffffff000000000LL,
	27,	32,	28,	0x7ffffff07ffffff0LL,
	28,	64,	29,	0x7ffffff800000000LL,
	28,	32,	29,	0x7ffffff87ffffff8LL,
	29,	64,	30,	0x7ffffffc00000000LL,
	29,	32,	30,	0x7ffffffc7ffffffcLL,
	30,	64,	31,	0x7ffffffe00000000LL,
	30,	32,	31,	0x7ffffffe7ffffffeLL,
	31,	64,	32,	0x7fffffff00000000LL,
	31,	32,	0,	0x7fffffff7fffffffLL,
	32,	64,	33,	0x7fffffff80000000LL,
	33,	64,	34,	0x7fffffffc0000000LL,
	34,	64,	35,	0x7fffffffe0000000LL,
	35,	64,	36,	0x7ffffffff0000000LL,
	36,	64,	37,	0x7ffffffff8000000LL,
	37,	64,	38,	0x7ffffffffc000000LL,
	38,	64,	39,	0x7ffffffffe000000LL,
	39,	64,	40,	0x7fffffffff000000LL,
	40,	64,	41,	0x7fffffffff800000LL,
	41,	64,	42,	0x7fffffffffc00000LL,
	42,	64,	43,	0x7fffffffffe00000LL,
	43,	64,	44,	0x7ffffffffff00000LL,
	44,	64,	45,	0x7ffffffffff80000LL,
	45,	64,	46,	0x7ffffffffffc0000LL,
	46,	64,	47,	0x7ffffffffffe0000LL,
	47,	64,	48,	0x7fffffffffff0000LL,
	48,	64,	49,	0x7fffffffffff8000LL,
	49,	64,	50,	0x7fffffffffffc000LL,
	50,	64,	51,	0x7fffffffffffe000LL,
	51,	64,	52,	0x7ffffffffffff000LL,
	52,	64,	53,	0x7ffffffffffff800LL,
	53,	64,	54,	0x7ffffffffffffc00LL,
	54,	64,	55,	0x7ffffffffffffe00LL,
	55,	64,	56,	0x7fffffffffffff00LL,
	56,	64,	57,	0x7fffffffffffff80LL,
	57,	64,	58,	0x7fffffffffffffc0LL,
	58,	64,	59,	0x7fffffffffffffe0LL,
	59,	64,	60,	0x7ffffffffffffff0LL,
	60,	64,	61,	0x7ffffffffffffff8LL,
	61,	64,	62,	0x7ffffffffffffffcLL,
	62,	64,	63,	0x7ffffffffffffffeLL,
	63,	64,	0,	0x7fffffffffffffffLL,
	1,	64,	1,	0x8000000000000000LL,
	2,	64,	1,	0x8000000000000001LL,
	3,	64,	1,	0x8000000000000003LL,
	4,	64,	1,	0x8000000000000007LL,
	5,	64,	1,	0x800000000000000fLL,
	6,	64,	1,	0x800000000000001fLL,
	7,	64,	1,	0x800000000000003fLL,
	8,	64,	1,	0x800000000000007fLL,
	9,	64,	1,	0x80000000000000ffLL,
	10,	64,	1,	0x80000000000001ffLL,
	11,	64,	1,	0x80000000000003ffLL,
	12,	64,	1,	0x80000000000007ffLL,
	13,	64,	1,	0x8000000000000fffLL,
	14,	64,	1,	0x8000000000001fffLL,
	15,	64,	1,	0x8000000000003fffLL,
	16,	64,	1,	0x8000000000007fffLL,
	17,	64,	1,	0x800000000000ffffLL,
	18,	64,	1,	0x800000000001ffffLL,
	19,	64,	1,	0x800000000003ffffLL,
	20,	64,	1,	0x800000000007ffffLL,
	21,	64,	1,	0x80000000000fffffLL,
	22,	64,	1,	0x80000000001fffffLL,
	23,	64,	1,	0x80000000003fffffLL,
	24,	64,	1,	0x80000000007fffffLL,
	25,	64,	1,	0x8000000000ffffffLL,
	26,	64,	1,	0x8000000001ffffffLL,
	27,	64,	1,	0x8000000003ffffffLL,
	28,	64,	1,	0x8000000007ffffffLL,
	29,	64,	1,	0x800000000fffffffLL,
	30,	64,	1,	0x800000001fffffffLL,
	31,	64,	1,	0x800000003fffffffLL,
	32,	64,	1,	0x800000007fffffffLL,
	1,	32,	1,	0x8000000080000000LL,
	33,	64,	1,	0x80000000ffffffffLL,
	2,	32,	1,	0x8000000180000001LL,
	34,	64,	1,	0x80000001ffffffffLL,
	3,	32,	1,	0x8000000380000003LL,
	35,	64,	1,	0x80000003ffffffffLL,
	4,	32,	1,	0x8000000780000007LL,
	36,	64,	1,	0x80000007ffffffffLL,
	5,	32,	1,	0x8000000f8000000fLL,
	37,	64,	1,	0x8000000fffffffffLL,
	6,	32,	1,	0x8000001f8000001fLL,
	38,	64,	1,	0x8000001fffffffffLL,
	7,	32,	1,	0x8000003f8000003fLL,
	39,	64,	1,	0x8000003fffffffffLL,
	8,	32,	1,	0x8000007f8000007fLL,
	40,	64,	1,	0x8000007fffffffffLL,
	9,	32,	1,	0x800000ff800000ffLL,
	41,	64,	1,	0x800000ffffffffffLL,
	10,	32,	1,	0x800001ff800001ffLL,
	42,	64,	1,	0x800001ffffffffffLL,
	11,	32,	1,	0x800003ff800003ffLL,
	43,	64,	1,	0x800003ffffffffffLL,
	12,	32,	1,	0x800007ff800007ffLL,
	44,	64,	1,	0x800007ffffffffffLL,
	13,	32,	1,	0x80000fff80000fffLL,
	45,	64,	1,	0x80000fffffffffffLL,
	14,	32,	1,	0x80001fff80001fffLL,
	46,	64,	1,	0x80001fffffffffffLL,
	15,	32,	1,	0x80003fff80003fffLL,
	47,	64,	1,	0x80003fffffffffffLL,
	16,	32,	1,	0x80007fff80007fffLL,
	48,	64,	1,	0x80007fffffffffffLL,
	1,	16,	1,	0x8000800080008000LL,
	17,	32,	1,	0x8000ffff8000ffffLL,
	49,	64,	1,	0x8000ffffffffffffLL,
	2,	16,	1,	0x8001800180018001LL,
	18,	32,	1,	0x8001ffff8001ffffLL,
	50,	64,	1,	0x8001ffffffffffffLL,
	3,	16,	1,	0x8003800380038003LL,
	19,	32,	1,	0x8003ffff8003ffffLL,
	51,	64,	1,	0x8003ffffffffffffLL,
	4,	16,	1,	0x8007800780078007LL,
	20,	32,	1,	0x8007ffff8007ffffLL,
	52,	64,	1,	0x8007ffffffffffffLL,
	5,	16,	1,	0x800f800f800f800fLL,
	21,	32,	1,	0x800fffff800fffffLL,
	53,	64,	1,	0x800fffffffffffffLL,
	6,	16,	1,	0x801f801f801f801fLL,
	22,	32,	1,	0x801fffff801fffffLL,
	54,	64,	1,	0x801fffffffffffffLL,
	7,	16,	1,	0x803f803f803f803fLL,
	23,	32,	1,	0x803fffff803fffffLL,
	55,	64,	1,	0x803fffffffffffffLL,
	8,	16,	1,	0x807f807f807f807fLL,
	24,	32,	1,	0x807fffff807fffffLL,
	56,	64,	1,	0x807fffffffffffffLL,
	1,	8,	1,	0x8080808080808080LL,
	9,	16,	1,	0x80ff80ff80ff80ffLL,
	25,	32,	1,	0x80ffffff80ffffffLL,
	57,	64,	1,	0x80ffffffffffffffLL,
	2,	8,	1,	0x8181818181818181LL,
	10,	16,	1,	0x81ff81ff81ff81ffLL,
	26,	32,	1,	0x81ffffff81ffffffLL,
	58,	64,	1,	0x81ffffffffffffffLL,
	3,	8,	1,	0x8383838383838383LL,
	11,	16,	1,	0x83ff83ff83ff83ffLL,
	27,	32,	1,	0x83ffffff83ffffffLL,
	59,	64,	1,	0x83ffffffffffffffLL,
	4,	8,	1,	0x8787878787878787LL,
	12,	16,	1,	0x87ff87ff87ff87ffLL,
	28,	32,	1,	0x87ffffff87ffffffLL,
	60,	64,	1,	0x87ffffffffffffffLL,
	1,	4,	1,	0x8888888888888888LL,
	5,	8,	1,	0x8f8f8f8f8f8f8f8fLL,
	13,	16,	1,	0x8fff8fff8fff8fffLL,
	29,	32,	1,	0x8fffffff8fffffffLL,
	61,	64,	1,	0x8fffffffffffffffLL,
	2,	4,	1,	0x9999999999999999LL,
	6,	8,	1,	0x9f9f9f9f9f9f9f9fLL,
	14,	16,	1,	0x9fff9fff9fff9fffLL,
	30,	32,	1,	0x9fffffff9fffffffLL,
	62,	64,	1,	0x9fffffffffffffffLL,
	1,	2,	1,	0xaaaaaaaaaaaaaaaaLL,
	3,	4,	1,	0xbbbbbbbbbbbbbbbbLL,
	7,	8,	1,	0xbfbfbfbfbfbfbfbfLL,
	15,	16,	1,	0xbfffbfffbfffbfffLL,
	31,	32,	1,	0xbfffffffbfffffffLL,
	63,	64,	1,	0xbfffffffffffffffLL,
	2,	64,	2,	0xc000000000000000LL,
	3,	64,	2,	0xc000000000000001LL,
	4,	64,	2,	0xc000000000000003LL,
	5,	64,	2,	0xc000000000000007LL,
	6,	64,	2,	0xc00000000000000fLL,
	7,	64,	2,	0xc00000000000001fLL,
	8,	64,	2,	0xc00000000000003fLL,
	9,	64,	2,	0xc00000000000007fLL,
	10,	64,	2,	0xc0000000000000ffLL,
	11,	64,	2,	0xc0000000000001ffLL,
	12,	64,	2,	0xc0000000000003ffLL,
	13,	64,	2,	0xc0000000000007ffLL,
	14,	64,	2,	0xc000000000000fffLL,
	15,	64,	2,	0xc000000000001fffLL,
	16,	64,	2,	0xc000000000003fffLL,
	17,	64,	2,	0xc000000000007fffLL,
	18,	64,	2,	0xc00000000000ffffLL,
	19,	64,	2,	0xc00000000001ffffLL,
	20,	64,	2,	0xc00000000003ffffLL,
	21,	64,	2,	0xc00000000007ffffLL,
	22,	64,	2,	0xc0000000000fffffLL,
	23,	64,	2,	0xc0000000001fffffLL,
	24,	64,	2,	0xc0000000003fffffLL,
	25,	64,	2,	0xc0000000007fffffLL,
	26,	64,	2,	0xc000000000ffffffLL,
	27,	64,	2,	0xc000000001ffffffLL,
	28,	64,	2,	0xc000000003ffffffLL,
	29,	64,	2,	0xc000000007ffffffLL,
	30,	64,	2,	0xc00000000fffffffLL,
	31,	64,	2,	0xc00000001fffffffLL,
	32,	64,	2,	0xc00000003fffffffLL,
	33,	64,	2,	0xc00000007fffffffLL,
	2,	32,	2,	0xc0000000c0000000LL,
	34,	64,	2,	0xc0000000ffffffffLL,
	3,	32,	2,	0xc0000001c0000001LL,
	35,	64,	2,	0xc0000001ffffffffLL,
	4,	32,	2,	0xc0000003c0000003LL,
	36,	64,	2,	0xc0000003ffffffffLL,
	5,	32,	2,	0xc0000007c0000007LL,
	37,	64,	2,	0xc0000007ffffffffLL,
	6,	32,	2,	0xc000000fc000000fLL,
	38,	64,	2,	0xc000000fffffffffLL,
	7,	32,	2,	0xc000001fc000001fLL,
	39,	64,	2,	0xc000001fffffffffLL,
	8,	32,	2,	0xc000003fc000003fLL,
	40,	64,	2,	0xc000003fffffffffLL,
	9,	32,	2,	0xc000007fc000007fLL,
	41,	64,	2,	0xc000007fffffffffLL,
	10,	32,	2,	0xc00000ffc00000ffLL,
	42,	64,	2,	0xc00000ffffffffffLL,
	11,	32,	2,	0xc00001ffc00001ffLL,
	43,	64,	2,	0xc00001ffffffffffLL,
	12,	32,	2,	0xc00003ffc00003ffLL,
	44,	64,	2,	0xc00003ffffffffffLL,
	13,	32,	2,	0xc00007ffc00007ffLL,
	45,	64,	2,	0xc00007ffffffffffLL,
	14,	32,	2,	0xc0000fffc0000fffLL,
	46,	64,	2,	0xc0000fffffffffffLL,
	15,	32,	2,	0xc0001fffc0001fffLL,
	47,	64,	2,	0xc0001fffffffffffLL,
	16,	32,	2,	0xc0003fffc0003fffLL,
	48,	64,	2,	0xc0003fffffffffffLL,
	17,	32,	2,	0xc0007fffc0007fffLL,
	49,	64,	2,	0xc0007fffffffffffLL,
	2,	16,	2,	0xc000c000c000c000LL,
	18,	32,	2,	0xc000ffffc000ffffLL,
	50,	64,	2,	0xc000ffffffffffffLL,
	3,	16,	2,	0xc001c001c001c001LL,
	19,	32,	2,	0xc001ffffc001ffffLL,
	51,	64,	2,	0xc001ffffffffffffLL,
	4,	16,	2,	0xc003c003c003c003LL,
	20,	32,	2,	0xc003ffffc003ffffLL,
	52,	64,	2,	0xc003ffffffffffffLL,
	5,	16,	2,	0xc007c007c007c007LL,
	21,	32,	2,	0xc007ffffc007ffffLL,
	53,	64,	2,	0xc007ffffffffffffLL,
	6,	16,	2,	0xc00fc00fc00fc00fLL,
	22,	32,	2,	0xc00fffffc00fffffLL,
	54,	64,	2,	0xc00fffffffffffffLL,
	7,	16,	2,	0xc01fc01fc01fc01fLL,
	23,	32,	2,	0xc01fffffc01fffffLL,
	55,	64,	2,	0xc01fffffffffffffLL,
	8,	16,	2,	0xc03fc03fc03fc03fLL,
	24,	32,	2,	0xc03fffffc03fffffLL,
	56,	64,	2,	0xc03fffffffffffffLL,
	9,	16,	2,	0xc07fc07fc07fc07fLL,
	25,	32,	2,	0xc07fffffc07fffffLL,
	57,	64,	2,	0xc07fffffffffffffLL,
	2,	8,	2,	0xc0c0c0c0c0c0c0c0LL,
	10,	16,	2,	0xc0ffc0ffc0ffc0ffLL,
	26,	32,	2,	0xc0ffffffc0ffffffLL,
	58,	64,	2,	0xc0ffffffffffffffLL,
	3,	8,	2,	0xc1c1c1c1c1c1c1c1LL,
	11,	16,	2,	0xc1ffc1ffc1ffc1ffLL,
	27,	32,	2,	0xc1ffffffc1ffffffLL,
	59,	64,	2,	0xc1ffffffffffffffLL,
	4,	8,	2,	0xc3c3c3c3c3c3c3c3LL,
	12,	16,	2,	0xc3ffc3ffc3ffc3ffLL,
	28,	32,	2,	0xc3ffffffc3ffffffLL,
	60,	64,	2,	0xc3ffffffffffffffLL,
	5,	8,	2,	0xc7c7c7c7c7c7c7c7LL,
	13,	16,	2,	0xc7ffc7ffc7ffc7ffLL,
	29,	32,	2,	0xc7ffffffc7ffffffLL,
	61,	64,	2,	0xc7ffffffffffffffLL,
	2,	4,	2,	0xccccccccccccccccLL,
	6,	8,	2,	0xcfcfcfcfcfcfcfcfLL,
	14,	16,	2,	0xcfffcfffcfffcfffLL,
	30,	32,	2,	0xcfffffffcfffffffLL,
	62,	64,	2,	0xcfffffffffffffffLL,
	3,	4,	2,	0xddddddddddddddddLL,
	7,	8,	2,	0xdfdfdfdfdfdfdfdfLL,
	15,	16,	2,	0xdfffdfffdfffdfffLL,
	31,	32,	2,	0xdfffffffdfffffffLL,
	63,	64,	2,	0xdfffffffffffffffLL,
	3,	64,	3,	0xe000000000000000LL,
	4,	64,	3,	0xe000000000000001LL,
	5,	64,	3,	0xe000000000000003LL,
	6,	64,	3,	0xe000000000000007LL,
	7,	64,	3,	0xe00000000000000fLL,
	8,	64,	3,	0xe00000000000001fLL,
	9,	64,	3,	0xe00000000000003fLL,
	10,	64,	3,	0xe00000000000007fLL,
	11,	64,	3,	0xe0000000000000ffLL,
	12,	64,	3,	0xe0000000000001ffLL,
	13,	64,	3,	0xe0000000000003ffLL,
	14,	64,	3,	0xe0000000000007ffLL,
	15,	64,	3,	0xe000000000000fffLL,
	16,	64,	3,	0xe000000000001fffLL,
	17,	64,	3,	0xe000000000003fffLL,
	18,	64,	3,	0xe000000000007fffLL,
	19,	64,	3,	0xe00000000000ffffLL,
	20,	64,	3,	0xe00000000001ffffLL,
	21,	64,	3,	0xe00000000003ffffLL,
	22,	64,	3,	0xe00000000007ffffLL,
	23,	64,	3,	0xe0000000000fffffLL,
	24,	64,	3,	0xe0000000001fffffLL,
	25,	64,	3,	0xe0000000003fffffLL,
	26,	64,	3,	0xe0000000007fffffLL,
	27,	64,	3,	0xe000000000ffffffLL,
	28,	64,	3,	0xe000000001ffffffLL,
	29,	64,	3,	0xe000000003ffffffLL,
	30,	64,	3,	0xe000000007ffffffLL,
	31,	64,	3,	0xe00000000fffffffLL,
	32,	64,	3,	0xe00000001fffffffLL,
	33,	64,	3,	0xe00000003fffffffLL,
	34,	64,	3,	0xe00000007fffffffLL,
	3,	32,	3,	0xe0000000e0000000LL,
	35,	64,	3,	0xe0000000ffffffffLL,
	4,	32,	3,	0xe0000001e0000001LL,
	36,	64,	3,	0xe0000001ffffffffLL,
	5,	32,	3,	0xe0000003e0000003LL,
	37,	64,	3,	0xe0000003ffffffffLL,
	6,	32,	3,	0xe0000007e0000007LL,
	38,	64,	3,	0xe0000007ffffffffLL,
	7,	32,	3,	0xe000000fe000000fLL,
	39,	64,	3,	0xe000000fffffffffLL,
	8,	32,	3,	0xe000001fe000001fLL,
	40,	64,	3,	0xe000001fffffffffLL,
	9,	32,	3,	0xe000003fe000003fLL,
	41,	64,	3,	0xe000003fffffffffLL,
	10,	32,	3,	0xe000007fe000007fLL,
	42,	64,	3,	0xe000007fffffffffLL,
	11,	32,	3,	0xe00000ffe00000ffLL,
	43,	64,	3,	0xe00000ffffffffffLL,
	12,	32,	3,	0xe00001ffe00001ffLL,
	44,	64,	3,	0xe00001ffffffffffLL,
	13,	32,	3,	0xe00003ffe00003ffLL,
	45,	64,	3,	0xe00003ffffffffffLL,
	14,	32,	3,	0xe00007ffe00007ffLL,
	46,	64,	3,	0xe00007ffffffffffLL,
	15,	32,	3,	0xe0000fffe0000fffLL,
	47,	64,	3,	0xe0000fffffffffffLL,
	16,	32,	3,	0xe0001fffe0001fffLL,
	48,	64,	3,	0xe0001fffffffffffLL,
	17,	32,	3,	0xe0003fffe0003fffLL,
	49,	64,	3,	0xe0003fffffffffffLL,
	18,	32,	3,	0xe0007fffe0007fffLL,
	50,	64,	3,	0xe0007fffffffffffLL,
	3,	16,	3,	0xe000e000e000e000LL,
	19,	32,	3,	0xe000ffffe000ffffLL,
	51,	64,	3,	0xe000ffffffffffffLL,
	4,	16,	3,	0xe001e001e001e001LL,
	20,	32,	3,	0xe001ffffe001ffffLL,
	52,	64,	3,	0xe001ffffffffffffLL,
	5,	16,	3,	0xe003e003e003e003LL,
	21,	32,	3,	0xe003ffffe003ffffLL,
	53,	64,	3,	0xe003ffffffffffffLL,
	6,	16,	3,	0xe007e007e007e007LL,
	22,	32,	3,	0xe007ffffe007ffffLL,
	54,	64,	3,	0xe007ffffffffffffLL,
	7,	16,	3,	0xe00fe00fe00fe00fLL,
	23,	32,	3,	0xe00fffffe00fffffLL,
	55,	64,	3,	0xe00fffffffffffffLL,
	8,	16,	3,	0xe01fe01fe01fe01fLL,
	24,	32,	3,	0xe01fffffe01fffffLL,
	56,	64,	3,	0xe01fffffffffffffLL,
	9,	16,	3,	0xe03fe03fe03fe03fLL,
	25,	32,	3,	0xe03fffffe03fffffLL,
	57,	64,	3,	0xe03fffffffffffffLL,
	10,	16,	3,	0xe07fe07fe07fe07fLL,
	26,	32,	3,	0xe07fffffe07fffffLL,
	58,	64,	3,	0xe07fffffffffffffLL,
	3,	8,	3,	0xe0e0e0e0e0e0e0e0LL,
	11,	16,	3,	0xe0ffe0ffe0ffe0ffLL,
	27,	32,	3,	0xe0ffffffe0ffffffLL,
	59,	64,	3,	0xe0ffffffffffffffLL,
	4,	8,	3,	0xe1e1e1e1e1e1e1e1LL,
	12,	16,	3,	0xe1ffe1ffe1ffe1ffLL,
	28,	32,	3,	0xe1ffffffe1ffffffLL,
	60,	64,	3,	0xe1ffffffffffffffLL,
	5,	8,	3,	0xe3e3e3e3e3e3e3e3LL,
	13,	16,	3,	0xe3ffe3ffe3ffe3ffLL,
	29,	32,	3,	0xe3ffffffe3ffffffLL,
	61,	64,	3,	0xe3ffffffffffffffLL,
	6,	8,	3,	0xe7e7e7e7e7e7e7e7LL,
	14,	16,	3,	0xe7ffe7ffe7ffe7ffLL,
	30,	32,	3,	0xe7ffffffe7ffffffLL,
	62,	64,	3,	0xe7ffffffffffffffLL,
	3,	4,	3,	0xeeeeeeeeeeeeeeeeLL,
	7,	8,	3,	0xefefefefefefefefLL,
	15,	16,	3,	0xefffefffefffefffLL,
	31,	32,	3,	0xefffffffefffffffLL,
	63,	64,	3,	0xefffffffffffffffLL,
	4,	64,	4,	0xf000000000000000LL,
	5,	64,	4,	0xf000000000000001LL,
	6,	64,	4,	0xf000000000000003LL,
	7,	64,	4,	0xf000000000000007LL,
	8,	64,	4,	0xf00000000000000fLL,
	9,	64,	4,	0xf00000000000001fLL,
	10,	64,	4,	0xf00000000000003fLL,
	11,	64,	4,	0xf00000000000007fLL,
	12,	64,	4,	0xf0000000000000ffLL,
	13,	64,	4,	0xf0000000000001ffLL,
	14,	64,	4,	0xf0000000000003ffLL,
	15,	64,	4,	0xf0000000000007ffLL,
	16,	64,	4,	0xf000000000000fffLL,
	17,	64,	4,	0xf000000000001fffLL,
	18,	64,	4,	0xf000000000003fffLL,
	19,	64,	4,	0xf000000000007fffLL,
	20,	64,	4,	0xf00000000000ffffLL,
	21,	64,	4,	0xf00000000001ffffLL,
	22,	64,	4,	0xf00000000003ffffLL,
	23,	64,	4,	0xf00000000007ffffLL,
	24,	64,	4,	0xf0000000000fffffLL,
	25,	64,	4,	0xf0000000001fffffLL,
	26,	64,	4,	0xf0000000003fffffLL,
	27,	64,	4,	0xf0000000007fffffLL,
	28,	64,	4,	0xf000000000ffffffLL,
	29,	64,	4,	0xf000000001ffffffLL,
	30,	64,	4,	0xf000000003ffffffLL,
	31,	64,	4,	0xf000000007ffffffLL,
	32,	64,	4,	0xf00000000fffffffLL,
	33,	64,	4,	0xf00000001fffffffLL,
	34,	64,	4,	0xf00000003fffffffLL,
	35,	64,	4,	0xf00000007fffffffLL,
	4,	32,	4,	0xf0000000f0000000LL,
	36,	64,	4,	0xf0000000ffffffffLL,
	5,	32,	4,	0xf0000001f0000001LL,
	37,	64,	4,	0xf0000001ffffffffLL,
	6,	32,	4,	0xf0000003f0000003LL,
	38,	64,	4,	0xf0000003ffffffffLL,
	7,	32,	4,	0xf0000007f0000007LL,
	39,	64,	4,	0xf0000007ffffffffLL,
	8,	32,	4,	0xf000000ff000000fLL,
	40,	64,	4,	0xf000000fffffffffLL,
	9,	32,	4,	0xf000001ff000001fLL,
	41,	64,	4,	0xf000001fffffffffLL,
	10,	32,	4,	0xf000003ff000003fLL,
	42,	64,	4,	0xf000003fffffffffLL,
	11,	32,	4,	0xf000007ff000007fLL,
	43,	64,	4,	0xf000007fffffffffLL,
	12,	32,	4,	0xf00000fff00000ffLL,
	44,	64,	4,	0xf00000ffffffffffLL,
	13,	32,	4,	0xf00001fff00001ffLL,
	45,	64,	4,	0xf00001ffffffffffLL,
	14,	32,	4,	0xf00003fff00003ffLL,
	46,	64,	4,	0xf00003ffffffffffLL,
	15,	32,	4,	0xf00007fff00007ffLL,
	47,	64,	4,	0xf00007ffffffffffLL,
	16,	32,	4,	0xf0000ffff0000fffLL,
	48,	64,	4,	0xf0000fffffffffffLL,
	17,	32,	4,	0xf0001ffff0001fffLL,
	49,	64,	4,	0xf0001fffffffffffLL,
	18,	32,	4,	0xf0003ffff0003fffLL,
	50,	64,	4,	0xf0003fffffffffffLL,
	19,	32,	4,	0xf0007ffff0007fffLL,
	51,	64,	4,	0xf0007fffffffffffLL,
	4,	16,	4,	0xf000f000f000f000LL,
	20,	32,	4,	0xf000fffff000ffffLL,
	52,	64,	4,	0xf000ffffffffffffLL,
	5,	16,	4,	0xf001f001f001f001LL,
	21,	32,	4,	0xf001fffff001ffffLL,
	53,	64,	4,	0xf001ffffffffffffLL,
	6,	16,	4,	0xf003f003f003f003LL,
	22,	32,	4,	0xf003fffff003ffffLL,
	54,	64,	4,	0xf003ffffffffffffLL,
	7,	16,	4,	0xf007f007f007f007LL,
	23,	32,	4,	0xf007fffff007ffffLL,
	55,	64,	4,	0xf007ffffffffffffLL,
	8,	16,	4,	0xf00ff00ff00ff00fLL,
	24,	32,	4,	0xf00ffffff00fffffLL,
	56,	64,	4,	0xf00fffffffffffffLL,
	9,	16,	4,	0xf01ff01ff01ff01fLL,
	25,	32,	4,	0xf01ffffff01fffffLL,
	57,	64,	4,	0xf01fffffffffffffLL,
	10,	16,	4,	0xf03ff03ff03ff03fLL,
	26,	32,	4,	0xf03ffffff03fffffLL,
	58,	64,	4,	0xf03fffffffffffffLL,
	11,	16,	4,	0xf07ff07ff07ff07fLL,
	27,	32,	4,	0xf07ffffff07fffffLL,
	59,	64,	4,	0xf07fffffffffffffLL,
	4,	8,	4,	0xf0f0f0f0f0f0f0f0LL,
	12,	16,	4,	0xf0fff0fff0fff0ffLL,
	28,	32,	4,	0xf0fffffff0ffffffLL,
	60,	64,	4,	0xf0ffffffffffffffLL,
	5,	8,	4,	0xf1f1f1f1f1f1f1f1LL,
	13,	16,	4,	0xf1fff1fff1fff1ffLL,
	29,	32,	4,	0xf1fffffff1ffffffLL,
	61,	64,	4,	0xf1ffffffffffffffLL,
	6,	8,	4,	0xf3f3f3f3f3f3f3f3LL,
	14,	16,	4,	0xf3fff3fff3fff3ffLL,
	30,	32,	4,	0xf3fffffff3ffffffLL,
	62,	64,	4,	0xf3ffffffffffffffLL,
	7,	8,	4,	0xf7f7f7f7f7f7f7f7LL,
	15,	16,	4,	0xf7fff7fff7fff7ffLL,
	31,	32,	4,	0xf7fffffff7ffffffLL,
	63,	64,	4,	0xf7ffffffffffffffLL,
	5,	64,	5,	0xf800000000000000LL,
	6,	64,	5,	0xf800000000000001LL,
	7,	64,	5,	0xf800000000000003LL,
	8,	64,	5,	0xf800000000000007LL,
	9,	64,	5,	0xf80000000000000fLL,
	10,	64,	5,	0xf80000000000001fLL,
	11,	64,	5,	0xf80000000000003fLL,
	12,	64,	5,	0xf80000000000007fLL,
	13,	64,	5,	0xf8000000000000ffLL,
	14,	64,	5,	0xf8000000000001ffLL,
	15,	64,	5,	0xf8000000000003ffLL,
	16,	64,	5,	0xf8000000000007ffLL,
	17,	64,	5,	0xf800000000000fffLL,
	18,	64,	5,	0xf800000000001fffLL,
	19,	64,	5,	0xf800000000003fffLL,
	20,	64,	5,	0xf800000000007fffLL,
	21,	64,	5,	0xf80000000000ffffLL,
	22,	64,	5,	0xf80000000001ffffLL,
	23,	64,	5,	0xf80000000003ffffLL,
	24,	64,	5,	0xf80000000007ffffLL,
	25,	64,	5,	0xf8000000000fffffLL,
	26,	64,	5,	0xf8000000001fffffLL,
	27,	64,	5,	0xf8000000003fffffLL,
	28,	64,	5,	0xf8000000007fffffLL,
	29,	64,	5,	0xf800000000ffffffLL,
	30,	64,	5,	0xf800000001ffffffLL,
	31,	64,	5,	0xf800000003ffffffLL,
	32,	64,	5,	0xf800000007ffffffLL,
	33,	64,	5,	0xf80000000fffffffLL,
	34,	64,	5,	0xf80000001fffffffLL,
	35,	64,	5,	0xf80000003fffffffLL,
	36,	64,	5,	0xf80000007fffffffLL,
	5,	32,	5,	0xf8000000f8000000LL,
	37,	64,	5,	0xf8000000ffffffffLL,
	6,	32,	5,	0xf8000001f8000001LL,
	38,	64,	5,	0xf8000001ffffffffLL,
	7,	32,	5,	0xf8000003f8000003LL,
	39,	64,	5,	0xf8000003ffffffffLL,
	8,	32,	5,	0xf8000007f8000007LL,
	40,	64,	5,	0xf8000007ffffffffLL,
	9,	32,	5,	0xf800000ff800000fLL,
	41,	64,	5,	0xf800000fffffffffLL,
	10,	32,	5,	0xf800001ff800001fLL,
	42,	64,	5,	0xf800001fffffffffLL,
	11,	32,	5,	0xf800003ff800003fLL,
	43,	64,	5,	0xf800003fffffffffLL,
	12,	32,	5,	0xf800007ff800007fLL,
	44,	64,	5,	0xf800007fffffffffLL,
	13,	32,	5,	0xf80000fff80000ffLL,
	45,	64,	5,	0xf80000ffffffffffLL,
	14,	32,	5,	0xf80001fff80001ffLL,
	46,	64,	5,	0xf80001ffffffffffLL,
	15,	32,	5,	0xf80003fff80003ffLL,
	47,	64,	5,	0xf80003ffffffffffLL,
	16,	32,	5,	0xf80007fff80007ffLL,
	48,	64,	5,	0xf80007ffffffffffLL,
	17,	32,	5,	0xf8000ffff8000fffLL,
	49,	64,	5,	0xf8000fffffffffffLL,
	18,	32,	5,	0xf8001ffff8001fffLL,
	50,	64,	5,	0xf8001fffffffffffLL,
	19,	32,	5,	0xf8003ffff8003fffLL,
	51,	64,	5,	0xf8003fffffffffffLL,
	20,	32,	5,	0xf8007ffff8007fffLL,
	52,	64,	5,	0xf8007fffffffffffLL,
	5,	16,	5,	0xf800f800f800f800LL,
	21,	32,	5,	0xf800fffff800ffffLL,
	53,	64,	5,	0xf800ffffffffffffLL,
	6,	16,	5,	0xf801f801f801f801LL,
	22,	32,	5,	0xf801fffff801ffffLL,
	54,	64,	5,	0xf801ffffffffffffLL,
	7,	16,	5,	0xf803f803f803f803LL,
	23,	32,	5,	0xf803fffff803ffffLL,
	55,	64,	5,	0xf803ffffffffffffLL,
	8,	16,	5,	0xf807f807f807f807LL,
	24,	32,	5,	0xf807fffff807ffffLL,
	56,	64,	5,	0xf807ffffffffffffLL,
	9,	16,	5,	0xf80ff80ff80ff80fLL,
	25,	32,	5,	0xf80ffffff80fffffLL,
	57,	64,	5,	0xf80fffffffffffffLL,
	10,	16,	5,	0xf81ff81ff81ff81fLL,
	26,	32,	5,	0xf81ffffff81fffffLL,
	58,	64,	5,	0xf81fffffffffffffLL,
	11,	16,	5,	0xf83ff83ff83ff83fLL,
	27,	32,	5,	0xf83ffffff83fffffLL,
	59,	64,	5,	0xf83fffffffffffffLL,
	12,	16,	5,	0xf87ff87ff87ff87fLL,
	28,	32,	5,	0xf87ffffff87fffffLL,
	60,	64,	5,	0xf87fffffffffffffLL,
	5,	8,	5,	0xf8f8f8f8f8f8f8f8LL,
	13,	16,	5,	0xf8fff8fff8fff8ffLL,
	29,	32,	5,	0xf8fffffff8ffffffLL,
	61,	64,	5,	0xf8ffffffffffffffLL,
	6,	8,	5,	0xf9f9f9f9f9f9f9f9LL,
	14,	16,	5,	0xf9fff9fff9fff9ffLL,
	30,	32,	5,	0xf9fffffff9ffffffLL,
	62,	64,	5,	0xf9ffffffffffffffLL,
	7,	8,	5,	0xfbfbfbfbfbfbfbfbLL,
	15,	16,	5,	0xfbfffbfffbfffbffLL,
	31,	32,	5,	0xfbfffffffbffffffLL,
	63,	64,	5,	0xfbffffffffffffffLL,
	6,	64,	6,	0xfc00000000000000LL,
	7,	64,	6,	0xfc00000000000001LL,
	8,	64,	6,	0xfc00000000000003LL,
	9,	64,	6,	0xfc00000000000007LL,
	10,	64,	6,	0xfc0000000000000fLL,
	11,	64,	6,	0xfc0000000000001fLL,
	12,	64,	6,	0xfc0000000000003fLL,
	13,	64,	6,	0xfc0000000000007fLL,
	14,	64,	6,	0xfc000000000000ffLL,
	15,	64,	6,	0xfc000000000001ffLL,
	16,	64,	6,	0xfc000000000003ffLL,
	17,	64,	6,	0xfc000000000007ffLL,
	18,	64,	6,	0xfc00000000000fffLL,
	19,	64,	6,	0xfc00000000001fffLL,
	20,	64,	6,	0xfc00000000003fffLL,
	21,	64,	6,	0xfc00000000007fffLL,
	22,	64,	6,	0xfc0000000000ffffLL,
	23,	64,	6,	0xfc0000000001ffffLL,
	24,	64,	6,	0xfc0000000003ffffLL,
	25,	64,	6,	0xfc0000000007ffffLL,
	26,	64,	6,	0xfc000000000fffffLL,
	27,	64,	6,	0xfc000000001fffffLL,
	28,	64,	6,	0xfc000000003fffffLL,
	29,	64,	6,	0xfc000000007fffffLL,
	30,	64,	6,	0xfc00000000ffffffLL,
	31,	64,	6,	0xfc00000001ffffffLL,
	32,	64,	6,	0xfc00000003ffffffLL,
	33,	64,	6,	0xfc00000007ffffffLL,
	34,	64,	6,	0xfc0000000fffffffLL,
	35,	64,	6,	0xfc0000001fffffffLL,
	36,	64,	6,	0xfc0000003fffffffLL,
	37,	64,	6,	0xfc0000007fffffffLL,
	6,	32,	6,	0xfc000000fc000000LL,
	38,	64,	6,	0xfc000000ffffffffLL,
	7,	32,	6,	0xfc000001fc000001LL,
	39,	64,	6,	0xfc000001ffffffffLL,
	8,	32,	6,	0xfc000003fc000003LL,
	40,	64,	6,	0xfc000003ffffffffLL,
	9,	32,	6,	0xfc000007fc000007LL,
	41,	64,	6,	0xfc000007ffffffffLL,
	10,	32,	6,	0xfc00000ffc00000fLL,
	42,	64,	6,	0xfc00000fffffffffLL,
	11,	32,	6,	0xfc00001ffc00001fLL,
	43,	64,	6,	0xfc00001fffffffffLL,
	12,	32,	6,	0xfc00003ffc00003fLL,
	44,	64,	6,	0xfc00003fffffffffLL,
	13,	32,	6,	0xfc00007ffc00007fLL,
	45,	64,	6,	0xfc00007fffffffffLL,
	14,	32,	6,	0xfc0000fffc0000ffLL,
	46,	64,	6,	0xfc0000ffffffffffLL,
	15,	32,	6,	0xfc0001fffc0001ffLL,
	47,	64,	6,	0xfc0001ffffffffffLL,
	16,	32,	6,	0xfc0003fffc0003ffLL,
	48,	64,	6,	0xfc0003ffffffffffLL,
	17,	32,	6,	0xfc0007fffc0007ffLL,
	49,	64,	6,	0xfc0007ffffffffffLL,
	18,	32,	6,	0xfc000ffffc000fffLL,
	50,	64,	6,	0xfc000fffffffffffLL,
	19,	32,	6,	0xfc001ffffc001fffLL,
	51,	64,	6,	0xfc001fffffffffffLL,
	20,	32,	6,	0xfc003ffffc003fffLL,
	52,	64,	6,	0xfc003fffffffffffLL,
	21,	32,	6,	0xfc007ffffc007fffLL,
	53,	64,	6,	0xfc007fffffffffffLL,
	6,	16,	6,	0xfc00fc00fc00fc00LL,
	22,	32,	6,	0xfc00fffffc00ffffLL,
	54,	64,	6,	0xfc00ffffffffffffLL,
	7,	16,	6,	0xfc01fc01fc01fc01LL,
	23,	32,	6,	0xfc01fffffc01ffffLL,
	55,	64,	6,	0xfc01ffffffffffffLL,
	8,	16,	6,	0xfc03fc03fc03fc03LL,
	24,	32,	6,	0xfc03fffffc03ffffLL,
	56,	64,	6,	0xfc03ffffffffffffLL,
	9,	16,	6,	0xfc07fc07fc07fc07LL,
	25,	32,	6,	0xfc07fffffc07ffffLL,
	57,	64,	6,	0xfc07ffffffffffffLL,
	10,	16,	6,	0xfc0ffc0ffc0ffc0fLL,
	26,	32,	6,	0xfc0ffffffc0fffffLL,
	58,	64,	6,	0xfc0fffffffffffffLL,
	11,	16,	6,	0xfc1ffc1ffc1ffc1fLL,
	27,	32,	6,	0xfc1ffffffc1fffffLL,
	59,	64,	6,	0xfc1fffffffffffffLL,
	12,	16,	6,	0xfc3ffc3ffc3ffc3fLL,
	28,	32,	6,	0xfc3ffffffc3fffffLL,
	60,	64,	6,	0xfc3fffffffffffffLL,
	13,	16,	6,	0xfc7ffc7ffc7ffc7fLL,
	29,	32,	6,	0xfc7ffffffc7fffffLL,
	61,	64,	6,	0xfc7fffffffffffffLL,
	6,	8,	6,	0xfcfcfcfcfcfcfcfcLL,
	14,	16,	6,	0xfcfffcfffcfffcffLL,
	30,	32,	6,	0xfcfffffffcffffffLL,
	62,	64,	6,	0xfcffffffffffffffLL,
	7,	8,	6,	0xfdfdfdfdfdfdfdfdLL,
	15,	16,	6,	0xfdfffdfffdfffdffLL,
	31,	32,	6,	0xfdfffffffdffffffLL,
	63,	64,	6,	0xfdffffffffffffffLL,
	7,	64,	7,	0xfe00000000000000LL,
	8,	64,	7,	0xfe00000000000001LL,
	9,	64,	7,	0xfe00000000000003LL,
	10,	64,	7,	0xfe00000000000007LL,
	11,	64,	7,	0xfe0000000000000fLL,
	12,	64,	7,	0xfe0000000000001fLL,
	13,	64,	7,	0xfe0000000000003fLL,
	14,	64,	7,	0xfe0000000000007fLL,
	15,	64,	7,	0xfe000000000000ffLL,
	16,	64,	7,	0xfe000000000001ffLL,
	17,	64,	7,	0xfe000000000003ffLL,
	18,	64,	7,	0xfe000000000007ffLL,
	19,	64,	7,	0xfe00000000000fffLL,
	20,	64,	7,	0xfe00000000001fffLL,
	21,	64,	7,	0xfe00000000003fffLL,
	22,	64,	7,	0xfe00000000007fffLL,
	23,	64,	7,	0xfe0000000000ffffLL,
	24,	64,	7,	0xfe0000000001ffffLL,
	25,	64,	7,	0xfe0000000003ffffLL,
	26,	64,	7,	0xfe0000000007ffffLL,
	27,	64,	7,	0xfe000000000fffffLL,
	28,	64,	7,	0xfe000000001fffffLL,
	29,	64,	7,	0xfe000000003fffffLL,
	30,	64,	7,	0xfe000000007fffffLL,
	31,	64,	7,	0xfe00000000ffffffLL,
	32,	64,	7,	0xfe00000001ffffffLL,
	33,	64,	7,	0xfe00000003ffffffLL,
	34,	64,	7,	0xfe00000007ffffffLL,
	35,	64,	7,	0xfe0000000fffffffLL,
	36,	64,	7,	0xfe0000001fffffffLL,
	37,	64,	7,	0xfe0000003fffffffLL,
	38,	64,	7,	0xfe0000007fffffffLL,
	7,	32,	7,	0xfe000000fe000000LL,
	39,	64,	7,	0xfe000000ffffffffLL,
	8,	32,	7,	0xfe000001fe000001LL,
	40,	64,	7,	0xfe000001ffffffffLL,
	9,	32,	7,	0xfe000003fe000003LL,
	41,	64,	7,	0xfe000003ffffffffLL,
	10,	32,	7,	0xfe000007fe000007LL,
	42,	64,	7,	0xfe000007ffffffffLL,
	11,	32,	7,	0xfe00000ffe00000fLL,
	43,	64,	7,	0xfe00000fffffffffLL,
	12,	32,	7,	0xfe00001ffe00001fLL,
	44,	64,	7,	0xfe00001fffffffffLL,
	13,	32,	7,	0xfe00003ffe00003fLL,
	45,	64,	7,	0xfe00003fffffffffLL,
	14,	32,	7,	0xfe00007ffe00007fLL,
	46,	64,	7,	0xfe00007fffffffffLL,
	15,	32,	7,	0xfe0000fffe0000ffLL,
	47,	64,	7,	0xfe0000ffffffffffLL,
	16,	32,	7,	0xfe0001fffe0001ffLL,
	48,	64,	7,	0xfe0001ffffffffffLL,
	17,	32,	7,	0xfe0003fffe0003ffLL,
	49,	64,	7,	0xfe0003ffffffffffLL,
	18,	32,	7,	0xfe0007fffe0007ffLL,
	50,	64,	7,	0xfe0007ffffffffffLL,
	19,	32,	7,	0xfe000ffffe000fffLL,
	51,	64,	7,	0xfe000fffffffffffLL,
	20,	32,	7,	0xfe001ffffe001fffLL,
	52,	64,	7,	0xfe001fffffffffffLL,
	21,	32,	7,	0xfe003ffffe003fffLL,
	53,	64,	7,	0xfe003fffffffffffLL,
	22,	32,	7,	0xfe007ffffe007fffLL,
	54,	64,	7,	0xfe007fffffffffffLL,
	7,	16,	7,	0xfe00fe00fe00fe00LL,
	23,	32,	7,	0xfe00fffffe00ffffLL,
	55,	64,	7,	0xfe00ffffffffffffLL,
	8,	16,	7,	0xfe01fe01fe01fe01LL,
	24,	32,	7,	0xfe01fffffe01ffffLL,
	56,	64,	7,	0xfe01ffffffffffffLL,
	9,	16,	7,	0xfe03fe03fe03fe03LL,
	25,	32,	7,	0xfe03fffffe03ffffLL,
	57,	64,	7,	0xfe03ffffffffffffLL,
	10,	16,	7,	0xfe07fe07fe07fe07LL,
	26,	32,	7,	0xfe07fffffe07ffffLL,
	58,	64,	7,	0xfe07ffffffffffffLL,
	11,	16,	7,	0xfe0ffe0ffe0ffe0fLL,
	27,	32,	7,	0xfe0ffffffe0fffffLL,
	59,	64,	7,	0xfe0fffffffffffffLL,
	12,	16,	7,	0xfe1ffe1ffe1ffe1fLL,
	28,	32,	7,	0xfe1ffffffe1fffffLL,
	60,	64,	7,	0xfe1fffffffffffffLL,
	13,	16,	7,	0xfe3ffe3ffe3ffe3fLL,
	29,	32,	7,	0xfe3ffffffe3fffffLL,
	61,	64,	7,	0xfe3fffffffffffffLL,
	14,	16,	7,	0xfe7ffe7ffe7ffe7fLL,
	30,	32,	7,	0xfe7ffffffe7fffffLL,
	62,	64,	7,	0xfe7fffffffffffffLL,
	7,	8,	7,	0xfefefefefefefefeLL,
	15,	16,	7,	0xfefffefffefffeffLL,
	31,	32,	7,	0xfefffffffeffffffLL,
	63,	64,	7,	0xfeffffffffffffffLL,
	8,	64,	8,	0xff00000000000000LL,
	9,	64,	8,	0xff00000000000001LL,
	10,	64,	8,	0xff00000000000003LL,
	11,	64,	8,	0xff00000000000007LL,
	12,	64,	8,	0xff0000000000000fLL,
	13,	64,	8,	0xff0000000000001fLL,
	14,	64,	8,	0xff0000000000003fLL,
	15,	64,	8,	0xff0000000000007fLL,
	16,	64,	8,	0xff000000000000ffLL,
	17,	64,	8,	0xff000000000001ffLL,
	18,	64,	8,	0xff000000000003ffLL,
	19,	64,	8,	0xff000000000007ffLL,
	20,	64,	8,	0xff00000000000fffLL,
	21,	64,	8,	0xff00000000001fffLL,
	22,	64,	8,	0xff00000000003fffLL,
	23,	64,	8,	0xff00000000007fffLL,
	24,	64,	8,	0xff0000000000ffffLL,
	25,	64,	8,	0xff0000000001ffffLL,
	26,	64,	8,	0xff0000000003ffffLL,
	27,	64,	8,	0xff0000000007ffffLL,
	28,	64,	8,	0xff000000000fffffLL,
	29,	64,	8,	0xff000000001fffffLL,
	30,	64,	8,	0xff000000003fffffLL,
	31,	64,	8,	0xff000000007fffffLL,
	32,	64,	8,	0xff00000000ffffffLL,
	33,	64,	8,	0xff00000001ffffffLL,
	34,	64,	8,	0xff00000003ffffffLL,
	35,	64,	8,	0xff00000007ffffffLL,
	36,	64,	8,	0xff0000000fffffffLL,
	37,	64,	8,	0xff0000001fffffffLL,
	38,	64,	8,	0xff0000003fffffffLL,
	39,	64,	8,	0xff0000007fffffffLL,
	8,	32,	8,	0xff000000ff000000LL,
	40,	64,	8,	0xff000000ffffffffLL,
	9,	32,	8,	0xff000001ff000001LL,
	41,	64,	8,	0xff000001ffffffffLL,
	10,	32,	8,	0xff000003ff000003LL,
	42,	64,	8,	0xff000003ffffffffLL,
	11,	32,	8,	0xff000007ff000007LL,
	43,	64,	8,	0xff000007ffffffffLL,
	12,	32,	8,	0xff00000fff00000fLL,
	44,	64,	8,	0xff00000fffffffffLL,
	13,	32,	8,	0xff00001fff00001fLL,
	45,	64,	8,	0xff00001fffffffffLL,
	14,	32,	8,	0xff00003fff00003fLL,
	46,	64,	8,	0xff00003fffffffffLL,
	15,	32,	8,	0xff00007fff00007fLL,
	47,	64,	8,	0xff00007fffffffffLL,
	16,	32,	8,	0xff0000ffff0000ffLL,
	48,	64,	8,	0xff0000ffffffffffLL,
	17,	32,	8,	0xff0001ffff0001ffLL,
	49,	64,	8,	0xff0001ffffffffffLL,
	18,	32,	8,	0xff0003ffff0003ffLL,
	50,	64,	8,	0xff0003ffffffffffLL,
	19,	32,	8,	0xff0007ffff0007ffLL,
	51,	64,	8,	0xff0007ffffffffffLL,
	20,	32,	8,	0xff000fffff000fffLL,
	52,	64,	8,	0xff000fffffffffffLL,
	21,	32,	8,	0xff001fffff001fffLL,
	53,	64,	8,	0xff001fffffffffffLL,
	22,	32,	8,	0xff003fffff003fffLL,
	54,	64,	8,	0xff003fffffffffffLL,
	23,	32,	8,	0xff007fffff007fffLL,
	55,	64,	8,	0xff007fffffffffffLL,
	8,	16,	8,	0xff00ff00ff00ff00LL,
	24,	32,	8,	0xff00ffffff00ffffLL,
	56,	64,	8,	0xff00ffffffffffffLL,
	9,	16,	8,	0xff01ff01ff01ff01LL,
	25,	32,	8,	0xff01ffffff01ffffLL,
	57,	64,	8,	0xff01ffffffffffffLL,
	10,	16,	8,	0xff03ff03ff03ff03LL,
	26,	32,	8,	0xff03ffffff03ffffLL,
	58,	64,	8,	0xff03ffffffffffffLL,
	11,	16,	8,	0xff07ff07ff07ff07LL,
	27,	32,	8,	0xff07ffffff07ffffLL,
	59,	64,	8,	0xff07ffffffffffffLL,
	12,	16,	8,	0xff0fff0fff0fff0fLL,
	28,	32,	8,	0xff0fffffff0fffffLL,
	60,	64,	8,	0xff0fffffffffffffLL,
	13,	16,	8,	0xff1fff1fff1fff1fLL,
	29,	32,	8,	0xff1fffffff1fffffLL,
	61,	64,	8,	0xff1fffffffffffffLL,
	14,	16,	8,	0xff3fff3fff3fff3fLL,
	30,	32,	8,	0xff3fffffff3fffffLL,
	62,	64,	8,	0xff3fffffffffffffLL,
	15,	16,	8,	0xff7fff7fff7fff7fLL,
	31,	32,	8,	0xff7fffffff7fffffLL,
	63,	64,	8,	0xff7fffffffffffffLL,
	9,	64,	9,	0xff80000000000000LL,
	10,	64,	9,	0xff80000000000001LL,
	11,	64,	9,	0xff80000000000003LL,
	12,	64,	9,	0xff80000000000007LL,
	13,	64,	9,	0xff8000000000000fLL,
	14,	64,	9,	0xff8000000000001fLL,
	15,	64,	9,	0xff8000000000003fLL,
	16,	64,	9,	0xff8000000000007fLL,
	17,	64,	9,	0xff800000000000ffLL,
	18,	64,	9,	0xff800000000001ffLL,
	19,	64,	9,	0xff800000000003ffLL,
	20,	64,	9,	0xff800000000007ffLL,
	21,	64,	9,	0xff80000000000fffLL,
	22,	64,	9,	0xff80000000001fffLL,
	23,	64,	9,	0xff80000000003fffLL,
	24,	64,	9,	0xff80000000007fffLL,
	25,	64,	9,	0xff8000000000ffffLL,
	26,	64,	9,	0xff8000000001ffffLL,
	27,	64,	9,	0xff8000000003ffffLL,
	28,	64,	9,	0xff8000000007ffffLL,
	29,	64,	9,	0xff800000000fffffLL,
	30,	64,	9,	0xff800000001fffffLL,
	31,	64,	9,	0xff800000003fffffLL,
	32,	64,	9,	0xff800000007fffffLL,
	33,	64,	9,	0xff80000000ffffffLL,
	34,	64,	9,	0xff80000001ffffffLL,
	35,	64,	9,	0xff80000003ffffffLL,
	36,	64,	9,	0xff80000007ffffffLL,
	37,	64,	9,	0xff8000000fffffffLL,
	38,	64,	9,	0xff8000001fffffffLL,
	39,	64,	9,	0xff8000003fffffffLL,
	40,	64,	9,	0xff8000007fffffffLL,
	9,	32,	9,	0xff800000ff800000LL,
	41,	64,	9,	0xff800000ffffffffLL,
	10,	32,	9,	0xff800001ff800001LL,
	42,	64,	9,	0xff800001ffffffffLL,
	11,	32,	9,	0xff800003ff800003LL,
	43,	64,	9,	0xff800003ffffffffLL,
	12,	32,	9,	0xff800007ff800007LL,
	44,	64,	9,	0xff800007ffffffffLL,
	13,	32,	9,	0xff80000fff80000fLL,
	45,	64,	9,	0xff80000fffffffffLL,
	14,	32,	9,	0xff80001fff80001fLL,
	46,	64,	9,	0xff80001fffffffffLL,
	15,	32,	9,	0xff80003fff80003fLL,
	47,	64,	9,	0xff80003fffffffffLL,
	16,	32,	9,	0xff80007fff80007fLL,
	48,	64,	9,	0xff80007fffffffffLL,
	17,	32,	9,	0xff8000ffff8000ffLL,
	49,	64,	9,	0xff8000ffffffffffLL,
	18,	32,	9,	0xff8001ffff8001ffLL,
	50,	64,	9,	0xff8001ffffffffffLL,
	19,	32,	9,	0xff8003ffff8003ffLL,
	51,	64,	9,	0xff8003ffffffffffLL,
	20,	32,	9,	0xff8007ffff8007ffLL,
	52,	64,	9,	0xff8007ffffffffffLL,
	21,	32,	9,	0xff800fffff800fffLL,
	53,	64,	9,	0xff800fffffffffffLL,
	22,	32,	9,	0xff801fffff801fffLL,
	54,	64,	9,	0xff801fffffffffffLL,
	23,	32,	9,	0xff803fffff803fffLL,
	55,	64,	9,	0xff803fffffffffffLL,
	24,	32,	9,	0xff807fffff807fffLL,
	56,	64,	9,	0xff807fffffffffffLL,
	9,	16,	9,	0xff80ff80ff80ff80LL,
	25,	32,	9,	0xff80ffffff80ffffLL,
	57,	64,	9,	0xff80ffffffffffffLL,
	10,	16,	9,	0xff81ff81ff81ff81LL,
	26,	32,	9,	0xff81ffffff81ffffLL,
	58,	64,	9,	0xff81ffffffffffffLL,
	11,	16,	9,	0xff83ff83ff83ff83LL,
	27,	32,	9,	0xff83ffffff83ffffLL,
	59,	64,	9,	0xff83ffffffffffffLL,
	12,	16,	9,	0xff87ff87ff87ff87LL,
	28,	32,	9,	0xff87ffffff87ffffLL,
	60,	64,	9,	0xff87ffffffffffffLL,
	13,	16,	9,	0xff8fff8fff8fff8fLL,
	29,	32,	9,	0xff8fffffff8fffffLL,
	61,	64,	9,	0xff8fffffffffffffLL,
	14,	16,	9,	0xff9fff9fff9fff9fLL,
	30,	32,	9,	0xff9fffffff9fffffLL,
	62,	64,	9,	0xff9fffffffffffffLL,
	15,	16,	9,	0xffbfffbfffbfffbfLL,
	31,	32,	9,	0xffbfffffffbfffffLL,
	63,	64,	9,	0xffbfffffffffffffLL,
	10,	64,	10,	0xffc0000000000000LL,
	11,	64,	10,	0xffc0000000000001LL,
	12,	64,	10,	0xffc0000000000003LL,
	13,	64,	10,	0xffc0000000000007LL,
	14,	64,	10,	0xffc000000000000fLL,
	15,	64,	10,	0xffc000000000001fLL,
	16,	64,	10,	0xffc000000000003fLL,
	17,	64,	10,	0xffc000000000007fLL,
	18,	64,	10,	0xffc00000000000ffLL,
	19,	64,	10,	0xffc00000000001ffLL,
	20,	64,	10,	0xffc00000000003ffLL,
	21,	64,	10,	0xffc00000000007ffLL,
	22,	64,	10,	0xffc0000000000fffLL,
	23,	64,	10,	0xffc0000000001fffLL,
	24,	64,	10,	0xffc0000000003fffLL,
	25,	64,	10,	0xffc0000000007fffLL,
	26,	64,	10,	0xffc000000000ffffLL,
	27,	64,	10,	0xffc000000001ffffLL,
	28,	64,	10,	0xffc000000003ffffLL,
	29,	64,	10,	0xffc000000007ffffLL,
	30,	64,	10,	0xffc00000000fffffLL,
	31,	64,	10,	0xffc00000001fffffLL,
	32,	64,	10,	0xffc00000003fffffLL,
	33,	64,	10,	0xffc00000007fffffLL,
	34,	64,	10,	0xffc0000000ffffffLL,
	35,	64,	10,	0xffc0000001ffffffLL,
	36,	64,	10,	0xffc0000003ffffffLL,
	37,	64,	10,	0xffc0000007ffffffLL,
	38,	64,	10,	0xffc000000fffffffLL,
	39,	64,	10,	0xffc000001fffffffLL,
	40,	64,	10,	0xffc000003fffffffLL,
	41,	64,	10,	0xffc000007fffffffLL,
	10,	32,	10,	0xffc00000ffc00000LL,
	42,	64,	10,	0xffc00000ffffffffLL,
	11,	32,	10,	0xffc00001ffc00001LL,
	43,	64,	10,	0xffc00001ffffffffLL,
	12,	32,	10,	0xffc00003ffc00003LL,
	44,	64,	10,	0xffc00003ffffffffLL,
	13,	32,	10,	0xffc00007ffc00007LL,
	45,	64,	10,	0xffc00007ffffffffLL,
	14,	32,	10,	0xffc0000fffc0000fLL,
	46,	64,	10,	0xffc0000fffffffffLL,
	15,	32,	10,	0xffc0001fffc0001fLL,
	47,	64,	10,	0xffc0001fffffffffLL,
	16,	32,	10,	0xffc0003fffc0003fLL,
	48,	64,	10,	0xffc0003fffffffffLL,
	17,	32,	10,	0xffc0007fffc0007fLL,
	49,	64,	10,	0xffc0007fffffffffLL,
	18,	32,	10,	0xffc000ffffc000ffLL,
	50,	64,	10,	0xffc000ffffffffffLL,
	19,	32,	10,	0xffc001ffffc001ffLL,
	51,	64,	10,	0xffc001ffffffffffLL,
	20,	32,	10,	0xffc003ffffc003ffLL,
	52,	64,	10,	0xffc003ffffffffffLL,
	21,	32,	10,	0xffc007ffffc007ffLL,
	53,	64,	10,	0xffc007ffffffffffLL,
	22,	32,	10,	0xffc00fffffc00fffLL,
	54,	64,	10,	0xffc00fffffffffffLL,
	23,	32,	10,	0xffc01fffffc01fffLL,
	55,	64,	10,	0xffc01fffffffffffLL,
	24,	32,	10,	0xffc03fffffc03fffLL,
	56,	64,	10,	0xffc03fffffffffffLL,
	25,	32,	10,	0xffc07fffffc07fffLL,
	57,	64,	10,	0xffc07fffffffffffLL,
	10,	16,	10,	0xffc0ffc0ffc0ffc0LL,
	26,	32,	10,	0xffc0ffffffc0ffffLL,
	58,	64,	10,	0xffc0ffffffffffffLL,
	11,	16,	10,	0xffc1ffc1ffc1ffc1LL,
	27,	32,	10,	0xffc1ffffffc1ffffLL,
	59,	64,	10,	0xffc1ffffffffffffLL,
	12,	16,	10,	0xffc3ffc3ffc3ffc3LL,
	28,	32,	10,	0xffc3ffffffc3ffffLL,
	60,	64,	10,	0xffc3ffffffffffffLL,
	13,	16,	10,	0xffc7ffc7ffc7ffc7LL,
	29,	32,	10,	0xffc7ffffffc7ffffLL,
	61,	64,	10,	0xffc7ffffffffffffLL,
	14,	16,	10,	0xffcfffcfffcfffcfLL,
	30,	32,	10,	0xffcfffffffcfffffLL,
	62,	64,	10,	0xffcfffffffffffffLL,
	15,	16,	10,	0xffdfffdfffdfffdfLL,
	31,	32,	10,	0xffdfffffffdfffffLL,
	63,	64,	10,	0xffdfffffffffffffLL,
	11,	64,	11,	0xffe0000000000000LL,
	12,	64,	11,	0xffe0000000000001LL,
	13,	64,	11,	0xffe0000000000003LL,
	14,	64,	11,	0xffe0000000000007LL,
	15,	64,	11,	0xffe000000000000fLL,
	16,	64,	11,	0xffe000000000001fLL,
	17,	64,	11,	0xffe000000000003fLL,
	18,	64,	11,	0xffe000000000007fLL,
	19,	64,	11,	0xffe00000000000ffLL,
	20,	64,	11,	0xffe00000000001ffLL,
	21,	64,	11,	0xffe00000000003ffLL,
	22,	64,	11,	0xffe00000000007ffLL,
	23,	64,	11,	0xffe0000000000fffLL,
	24,	64,	11,	0xffe0000000001fffLL,
	25,	64,	11,	0xffe0000000003fffLL,
	26,	64,	11,	0xffe0000000007fffLL,
	27,	64,	11,	0xffe000000000ffffLL,
	28,	64,	11,	0xffe000000001ffffLL,
	29,	64,	11,	0xffe000000003ffffLL,
	30,	64,	11,	0xffe000000007ffffLL,
	31,	64,	11,	0xffe00000000fffffLL,
	32,	64,	11,	0xffe00000001fffffLL,
	33,	64,	11,	0xffe00000003fffffLL,
	34,	64,	11,	0xffe00000007fffffLL,
	35,	64,	11,	0xffe0000000ffffffLL,
	36,	64,	11,	0xffe0000001ffffffLL,
	37,	64,	11,	0xffe0000003ffffffLL,
	38,	64,	11,	0xffe0000007ffffffLL,
	39,	64,	11,	0xffe000000fffffffLL,
	40,	64,	11,	0xffe000001fffffffLL,
	41,	64,	11,	0xffe000003fffffffLL,
	42,	64,	11,	0xffe000007fffffffLL,
	11,	32,	11,	0xffe00000ffe00000LL,
	43,	64,	11,	0xffe00000ffffffffLL,
	12,	32,	11,	0xffe00001ffe00001LL,
	44,	64,	11,	0xffe00001ffffffffLL,
	13,	32,	11,	0xffe00003ffe00003LL,
	45,	64,	11,	0xffe00003ffffffffLL,
	14,	32,	11,	0xffe00007ffe00007LL,
	46,	64,	11,	0xffe00007ffffffffLL,
	15,	32,	11,	0xffe0000fffe0000fLL,
	47,	64,	11,	0xffe0000fffffffffLL,
	16,	32,	11,	0xffe0001fffe0001fLL,
	48,	64,	11,	0xffe0001fffffffffLL,
	17,	32,	11,	0xffe0003fffe0003fLL,
	49,	64,	11,	0xffe0003fffffffffLL,
	18,	32,	11,	0xffe0007fffe0007fLL,
	50,	64,	11,	0xffe0007fffffffffLL,
	19,	32,	11,	0xffe000ffffe000ffLL,
	51,	64,	11,	0xffe000ffffffffffLL,
	20,	32,	11,	0xffe001ffffe001ffLL,
	52,	64,	11,	0xffe001ffffffffffLL,
	21,	32,	11,	0xffe003ffffe003ffLL,
	53,	64,	11,	0xffe003ffffffffffLL,
	22,	32,	11,	0xffe007ffffe007ffLL,
	54,	64,	11,	0xffe007ffffffffffLL,
	23,	32,	11,	0xffe00fffffe00fffLL,
	55,	64,	11,	0xffe00fffffffffffLL,
	24,	32,	11,	0xffe01fffffe01fffLL,
	56,	64,	11,	0xffe01fffffffffffLL,
	25,	32,	11,	0xffe03fffffe03fffLL,
	57,	64,	11,	0xffe03fffffffffffLL,
	26,	32,	11,	0xffe07fffffe07fffLL,
	58,	64,	11,	0xffe07fffffffffffLL,
	11,	16,	11,	0xffe0ffe0ffe0ffe0LL,
	27,	32,	11,	0xffe0ffffffe0ffffLL,
	59,	64,	11,	0xffe0ffffffffffffLL,
	12,	16,	11,	0xffe1ffe1ffe1ffe1LL,
	28,	32,	11,	0xffe1ffffffe1ffffLL,
	60,	64,	11,	0xffe1ffffffffffffLL,
	13,	16,	11,	0xffe3ffe3ffe3ffe3LL,
	29,	32,	11,	0xffe3ffffffe3ffffLL,
	61,	64,	11,	0xffe3ffffffffffffLL,
	14,	16,	11,	0xffe7ffe7ffe7ffe7LL,
	30,	32,	11,	0xffe7ffffffe7ffffLL,
	62,	64,	11,	0xffe7ffffffffffffLL,
	15,	16,	11,	0xffefffefffefffefLL,
	31,	32,	11,	0xffefffffffefffffLL,
	63,	64,	11,	0xffefffffffffffffLL,
	12,	64,	12,	0xfff0000000000000LL,
	13,	64,	12,	0xfff0000000000001LL,
	14,	64,	12,	0xfff0000000000003LL,
	15,	64,	12,	0xfff0000000000007LL,
	16,	64,	12,	0xfff000000000000fLL,
	17,	64,	12,	0xfff000000000001fLL,
	18,	64,	12,	0xfff000000000003fLL,
	19,	64,	12,	0xfff000000000007fLL,
	20,	64,	12,	0xfff00000000000ffLL,
	21,	64,	12,	0xfff00000000001ffLL,
	22,	64,	12,	0xfff00000000003ffLL,
	23,	64,	12,	0xfff00000000007ffLL,
	24,	64,	12,	0xfff0000000000fffLL,
	25,	64,	12,	0xfff0000000001fffLL,
	26,	64,	12,	0xfff0000000003fffLL,
	27,	64,	12,	0xfff0000000007fffLL,
	28,	64,	12,	0xfff000000000ffffLL,
	29,	64,	12,	0xfff000000001ffffLL,
	30,	64,	12,	0xfff000000003ffffLL,
	31,	64,	12,	0xfff000000007ffffLL,
	32,	64,	12,	0xfff00000000fffffLL,
	33,	64,	12,	0xfff00000001fffffLL,
	34,	64,	12,	0xfff00000003fffffLL,
	35,	64,	12,	0xfff00000007fffffLL,
	36,	64,	12,	0xfff0000000ffffffLL,
	37,	64,	12,	0xfff0000001ffffffLL,
	38,	64,	12,	0xfff0000003ffffffLL,
	39,	64,	12,	0xfff0000007ffffffLL,
	40,	64,	12,	0xfff000000fffffffLL,
	41,	64,	12,	0xfff000001fffffffLL,
	42,	64,	12,	0xfff000003fffffffLL,
	43,	64,	12,	0xfff000007fffffffLL,
	12,	32,	12,	0xfff00000fff00000LL,
	44,	64,	12,	0xfff00000ffffffffLL,
	13,	32,	12,	0xfff00001fff00001LL,
	45,	64,	12,	0xfff00001ffffffffLL,
	14,	32,	12,	0xfff00003fff00003LL,
	46,	64,	12,	0xfff00003ffffffffLL,
	15,	32,	12,	0xfff00007fff00007LL,
	47,	64,	12,	0xfff00007ffffffffLL,
	16,	32,	12,	0xfff0000ffff0000fLL,
	48,	64,	12,	0xfff0000fffffffffLL,
	17,	32,	12,	0xfff0001ffff0001fLL,
	49,	64,	12,	0xfff0001fffffffffLL,
	18,	32,	12,	0xfff0003ffff0003fLL,
	50,	64,	12,	0xfff0003fffffffffLL,
	19,	32,	12,	0xfff0007ffff0007fLL,
	51,	64,	12,	0xfff0007fffffffffLL,
	20,	32,	12,	0xfff000fffff000ffLL,
	52,	64,	12,	0xfff000ffffffffffLL,
	21,	32,	12,	0xfff001fffff001ffLL,
	53,	64,	12,	0xfff001ffffffffffLL,
	22,	32,	12,	0xfff003fffff003ffLL,
	54,	64,	12,	0xfff003ffffffffffLL,
	23,	32,	12,	0xfff007fffff007ffLL,
	55,	64,	12,	0xfff007ffffffffffLL,
	24,	32,	12,	0xfff00ffffff00fffLL,
	56,	64,	12,	0xfff00fffffffffffLL,
	25,	32,	12,	0xfff01ffffff01fffLL,
	57,	64,	12,	0xfff01fffffffffffLL,
	26,	32,	12,	0xfff03ffffff03fffLL,
	58,	64,	12,	0xfff03fffffffffffLL,
	27,	32,	12,	0xfff07ffffff07fffLL,
	59,	64,	12,	0xfff07fffffffffffLL,
	12,	16,	12,	0xfff0fff0fff0fff0LL,
	28,	32,	12,	0xfff0fffffff0ffffLL,
	60,	64,	12,	0xfff0ffffffffffffLL,
	13,	16,	12,	0xfff1fff1fff1fff1LL,
	29,	32,	12,	0xfff1fffffff1ffffLL,
	61,	64,	12,	0xfff1ffffffffffffLL,
	14,	16,	12,	0xfff3fff3fff3fff3LL,
	30,	32,	12,	0xfff3fffffff3ffffLL,
	62,	64,	12,	0xfff3ffffffffffffLL,
	15,	16,	12,	0xfff7fff7fff7fff7LL,
	31,	32,	12,	0xfff7fffffff7ffffLL,
	63,	64,	12,	0xfff7ffffffffffffLL,
	13,	64,	13,	0xfff8000000000000LL,
	14,	64,	13,	0xfff8000000000001LL,
	15,	64,	13,	0xfff8000000000003LL,
	16,	64,	13,	0xfff8000000000007LL,
	17,	64,	13,	0xfff800000000000fLL,
	18,	64,	13,	0xfff800000000001fLL,
	19,	64,	13,	0xfff800000000003fLL,
	20,	64,	13,	0xfff800000000007fLL,
	21,	64,	13,	0xfff80000000000ffLL,
	22,	64,	13,	0xfff80000000001ffLL,
	23,	64,	13,	0xfff80000000003ffLL,
	24,	64,	13,	0xfff80000000007ffLL,
	25,	64,	13,	0xfff8000000000fffLL,
	26,	64,	13,	0xfff8000000001fffLL,
	27,	64,	13,	0xfff8000000003fffLL,
	28,	64,	13,	0xfff8000000007fffLL,
	29,	64,	13,	0xfff800000000ffffLL,
	30,	64,	13,	0xfff800000001ffffLL,
	31,	64,	13,	0xfff800000003ffffLL,
	32,	64,	13,	0xfff800000007ffffLL,
	33,	64,	13,	0xfff80000000fffffLL,
	34,	64,	13,	0xfff80000001fffffLL,
	35,	64,	13,	0xfff80000003fffffLL,
	36,	64,	13,	0xfff80000007fffffLL,
	37,	64,	13,	0xfff8000000ffffffLL,
	38,	64,	13,	0xfff8000001ffffffLL,
	39,	64,	13,	0xfff8000003ffffffLL,
	40,	64,	13,	0xfff8000007ffffffLL,
	41,	64,	13,	0xfff800000fffffffLL,
	42,	64,	13,	0xfff800001fffffffLL,
	43,	64,	13,	0xfff800003fffffffLL,
	44,	64,	13,	0xfff800007fffffffLL,
	13,	32,	13,	0xfff80000fff80000LL,
	45,	64,	13,	0xfff80000ffffffffLL,
	14,	32,	13,	0xfff80001fff80001LL,
	46,	64,	13,	0xfff80001ffffffffLL,
	15,	32,	13,	0xfff80003fff80003LL,
	47,	64,	13,	0xfff80003ffffffffLL,
	16,	32,	13,	0xfff80007fff80007LL,
	48,	64,	13,	0xfff80007ffffffffLL,
	17,	32,	13,	0xfff8000ffff8000fLL,
	49,	64,	13,	0xfff8000fffffffffLL,
	18,	32,	13,	0xfff8001ffff8001fLL,
	50,	64,	13,	0xfff8001fffffffffLL,
	19,	32,	13,	0xfff8003ffff8003fLL,
	51,	64,	13,	0xfff8003fffffffffLL,
	20,	32,	13,	0xfff8007ffff8007fLL,
	52,	64,	13,	0xfff8007fffffffffLL,
	21,	32,	13,	0xfff800fffff800ffLL,
	53,	64,	13,	0xfff800ffffffffffLL,
	22,	32,	13,	0xfff801fffff801ffLL,
	54,	64,	13,	0xfff801ffffffffffLL,
	23,	32,	13,	0xfff803fffff803ffLL,
	55,	64,	13,	0xfff803ffffffffffLL,
	24,	32,	13,	0xfff807fffff807ffLL,
	56,	64,	13,	0xfff807ffffffffffLL,
	25,	32,	13,	0xfff80ffffff80fffLL,
	57,	64,	13,	0xfff80fffffffffffLL,
	26,	32,	13,	0xfff81ffffff81fffLL,
	58,	64,	13,	0xfff81fffffffffffLL,
	27,	32,	13,	0xfff83ffffff83fffLL,
	59,	64,	13,	0xfff83fffffffffffLL,
	28,	32,	13,	0xfff87ffffff87fffLL,
	60,	64,	13,	0xfff87fffffffffffLL,
	13,	16,	13,	0xfff8fff8fff8fff8LL,
	29,	32,	13,	0xfff8fffffff8ffffLL,
	61,	64,	13,	0xfff8ffffffffffffLL,
	14,	16,	13,	0xfff9fff9fff9fff9LL,
	30,	32,	13,	0xfff9fffffff9ffffLL,
	62,	64,	13,	0xfff9ffffffffffffLL,
	15,	16,	13,	0xfffbfffbfffbfffbLL,
	31,	32,	13,	0xfffbfffffffbffffLL,
	63,	64,	13,	0xfffbffffffffffffLL,
	14,	64,	14,	0xfffc000000000000LL,
	15,	64,	14,	0xfffc000000000001LL,
	16,	64,	14,	0xfffc000000000003LL,
	17,	64,	14,	0xfffc000000000007LL,
	18,	64,	14,	0xfffc00000000000fLL,
	19,	64,	14,	0xfffc00000000001fLL,
	20,	64,	14,	0xfffc00000000003fLL,
	21,	64,	14,	0xfffc00000000007fLL,
	22,	64,	14,	0xfffc0000000000ffLL,
	23,	64,	14,	0xfffc0000000001ffLL,
	24,	64,	14,	0xfffc0000000003ffLL,
	25,	64,	14,	0xfffc0000000007ffLL,
	26,	64,	14,	0xfffc000000000fffLL,
	27,	64,	14,	0xfffc000000001fffLL,
	28,	64,	14,	0xfffc000000003fffLL,
	29,	64,	14,	0xfffc000000007fffLL,
	30,	64,	14,	0xfffc00000000ffffLL,
	31,	64,	14,	0xfffc00000001ffffLL,
	32,	64,	14,	0xfffc00000003ffffLL,
	33,	64,	14,	0xfffc00000007ffffLL,
	34,	64,	14,	0xfffc0000000fffffLL,
	35,	64,	14,	0xfffc0000001fffffLL,
	36,	64,	14,	0xfffc0000003fffffLL,
	37,	64,	14,	0xfffc0000007fffffLL,
	38,	64,	14,	0xfffc000000ffffffLL,
	39,	64,	14,	0xfffc000001ffffffLL,
	40,	64,	14,	0xfffc000003ffffffLL,
	41,	64,	14,	0xfffc000007ffffffLL,
	42,	64,	14,	0xfffc00000fffffffLL,
	43,	64,	14,	0xfffc00001fffffffLL,
	44,	64,	14,	0xfffc00003fffffffLL,
	45,	64,	14,	0xfffc00007fffffffLL,
	14,	32,	14,	0xfffc0000fffc0000LL,
	46,	64,	14,	0xfffc0000ffffffffLL,
	15,	32,	14,	0xfffc0001fffc0001LL,
	47,	64,	14,	0xfffc0001ffffffffLL,
	16,	32,	14,	0xfffc0003fffc0003LL,
	48,	64,	14,	0xfffc0003ffffffffLL,
	17,	32,	14,	0xfffc0007fffc0007LL,
	49,	64,	14,	0xfffc0007ffffffffLL,
	18,	32,	14,	0xfffc000ffffc000fLL,
	50,	64,	14,	0xfffc000fffffffffLL,
	19,	32,	14,	0xfffc001ffffc001fLL,
	51,	64,	14,	0xfffc001fffffffffLL,
	20,	32,	14,	0xfffc003ffffc003fLL,
	52,	64,	14,	0xfffc003fffffffffLL,
	21,	32,	14,	0xfffc007ffffc007fLL,
	53,	64,	14,	0xfffc007fffffffffLL,
	22,	32,	14,	0xfffc00fffffc00ffLL,
	54,	64,	14,	0xfffc00ffffffffffLL,
	23,	32,	14,	0xfffc01fffffc01ffLL,
	55,	64,	14,	0xfffc01ffffffffffLL,
	24,	32,	14,	0xfffc03fffffc03ffLL,
	56,	64,	14,	0xfffc03ffffffffffLL,
	25,	32,	14,	0xfffc07fffffc07ffLL,
	57,	64,	14,	0xfffc07ffffffffffLL,
	26,	32,	14,	0xfffc0ffffffc0fffLL,
	58,	64,	14,	0xfffc0fffffffffffLL,
	27,	32,	14,	0xfffc1ffffffc1fffLL,
	59,	64,	14,	0xfffc1fffffffffffLL,
	28,	32,	14,	0xfffc3ffffffc3fffLL,
	60,	64,	14,	0xfffc3fffffffffffLL,
	29,	32,	14,	0xfffc7ffffffc7fffLL,
	61,	64,	14,	0xfffc7fffffffffffLL,
	14,	16,	14,	0xfffcfffcfffcfffcLL,
	30,	32,	14,	0xfffcfffffffcffffLL,
	62,	64,	14,	0xfffcffffffffffffLL,
	15,	16,	14,	0xfffdfffdfffdfffdLL,
	31,	32,	14,	0xfffdfffffffdffffLL,
	63,	64,	14,	0xfffdffffffffffffLL,
	15,	64,	15,	0xfffe000000000000LL,
	16,	64,	15,	0xfffe000000000001LL,
	17,	64,	15,	0xfffe000000000003LL,
	18,	64,	15,	0xfffe000000000007LL,
	19,	64,	15,	0xfffe00000000000fLL,
	20,	64,	15,	0xfffe00000000001fLL,
	21,	64,	15,	0xfffe00000000003fLL,
	22,	64,	15,	0xfffe00000000007fLL,
	23,	64,	15,	0xfffe0000000000ffLL,
	24,	64,	15,	0xfffe0000000001ffLL,
	25,	64,	15,	0xfffe0000000003ffLL,
	26,	64,	15,	0xfffe0000000007ffLL,
	27,	64,	15,	0xfffe000000000fffLL,
	28,	64,	15,	0xfffe000000001fffLL,
	29,	64,	15,	0xfffe000000003fffLL,
	30,	64,	15,	0xfffe000000007fffLL,
	31,	64,	15,	0xfffe00000000ffffLL,
	32,	64,	15,	0xfffe00000001ffffLL,
	33,	64,	15,	0xfffe00000003ffffLL,
	34,	64,	15,	0xfffe00000007ffffLL,
	35,	64,	15,	0xfffe0000000fffffLL,
	36,	64,	15,	0xfffe0000001fffffLL,
	37,	64,	15,	0xfffe0000003fffffLL,
	38,	64,	15,	0xfffe0000007fffffLL,
	39,	64,	15,	0xfffe000000ffffffLL,
	40,	64,	15,	0xfffe000001ffffffLL,
	41,	64,	15,	0xfffe000003ffffffLL,
	42,	64,	15,	0xfffe000007ffffffLL,
	43,	64,	15,	0xfffe00000fffffffLL,
	44,	64,	15,	0xfffe00001fffffffLL,
	45,	64,	15,	0xfffe00003fffffffLL,
	46,	64,	15,	0xfffe00007fffffffLL,
	15,	32,	15,	0xfffe0000fffe0000LL,
	47,	64,	15,	0xfffe0000ffffffffLL,
	16,	32,	15,	0xfffe0001fffe0001LL,
	48,	64,	15,	0xfffe0001ffffffffLL,
	17,	32,	15,	0xfffe0003fffe0003LL,
	49,	64,	15,	0xfffe0003ffffffffLL,
	18,	32,	15,	0xfffe0007fffe0007LL,
	50,	64,	15,	0xfffe0007ffffffffLL,
	19,	32,	15,	0xfffe000ffffe000fLL,
	51,	64,	15,	0xfffe000fffffffffLL,
	20,	32,	15,	0xfffe001ffffe001fLL,
	52,	64,	15,	0xfffe001fffffffffLL,
	21,	32,	15,	0xfffe003ffffe003fLL,
	53,	64,	15,	0xfffe003fffffffffLL,
	22,	32,	15,	0xfffe007ffffe007fLL,
	54,	64,	15,	0xfffe007fffffffffLL,
	23,	32,	15,	0xfffe00fffffe00ffLL,
	55,	64,	15,	0xfffe00ffffffffffLL,
	24,	32,	15,	0xfffe01fffffe01ffLL,
	56,	64,	15,	0xfffe01ffffffffffLL,
	25,	32,	15,	0xfffe03fffffe03ffLL,
	57,	64,	15,	0xfffe03ffffffffffLL,
	26,	32,	15,	0xfffe07fffffe07ffLL,
	58,	64,	15,	0xfffe07ffffffffffLL,
	27,	32,	15,	0xfffe0ffffffe0fffLL,
	59,	64,	15,	0xfffe0fffffffffffLL,
	28,	32,	15,	0xfffe1ffffffe1fffLL,
	60,	64,	15,	0xfffe1fffffffffffLL,
	29,	32,	15,	0xfffe3ffffffe3fffLL,
	61,	64,	15,	0xfffe3fffffffffffLL,
	30,	32,	15,	0xfffe7ffffffe7fffLL,
	62,	64,	15,	0xfffe7fffffffffffLL,
	15,	16,	15,	0xfffefffefffefffeLL,
	31,	32,	15,	0xfffefffffffeffffLL,
	63,	64,	15,	0xfffeffffffffffffLL,
	16,	64,	16,	0xffff000000000000LL,
	17,	64,	16,	0xffff000000000001LL,
	18,	64,	16,	0xffff000000000003LL,
	19,	64,	16,	0xffff000000000007LL,
	20,	64,	16,	0xffff00000000000fLL,
	21,	64,	16,	0xffff00000000001fLL,
	22,	64,	16,	0xffff00000000003fLL,
	23,	64,	16,	0xffff00000000007fLL,
	24,	64,	16,	0xffff0000000000ffLL,
	25,	64,	16,	0xffff0000000001ffLL,
	26,	64,	16,	0xffff0000000003ffLL,
	27,	64,	16,	0xffff0000000007ffLL,
	28,	64,	16,	0xffff000000000fffLL,
	29,	64,	16,	0xffff000000001fffLL,
	30,	64,	16,	0xffff000000003fffLL,
	31,	64,	16,	0xffff000000007fffLL,
	32,	64,	16,	0xffff00000000ffffLL,
	33,	64,	16,	0xffff00000001ffffLL,
	34,	64,	16,	0xffff00000003ffffLL,
	35,	64,	16,	0xffff00000007ffffLL,
	36,	64,	16,	0xffff0000000fffffLL,
	37,	64,	16,	0xffff0000001fffffLL,
	38,	64,	16,	0xffff0000003fffffLL,
	39,	64,	16,	0xffff0000007fffffLL,
	40,	64,	16,	0xffff000000ffffffLL,
	41,	64,	16,	0xffff000001ffffffLL,
	42,	64,	16,	0xffff000003ffffffLL,
	43,	64,	16,	0xffff000007ffffffLL,
	44,	64,	16,	0xffff00000fffffffLL,
	45,	64,	16,	0xffff00001fffffffLL,
	46,	64,	16,	0xffff00003fffffffLL,
	47,	64,	16,	0xffff00007fffffffLL,
	16,	32,	16,	0xffff0000ffff0000LL,
	48,	64,	16,	0xffff0000ffffffffLL,
	17,	32,	16,	0xffff0001ffff0001LL,
	49,	64,	16,	0xffff0001ffffffffLL,
	18,	32,	16,	0xffff0003ffff0003LL,
	50,	64,	16,	0xffff0003ffffffffLL,
	19,	32,	16,	0xffff0007ffff0007LL,
	51,	64,	16,	0xffff0007ffffffffLL,
	20,	32,	16,	0xffff000fffff000fLL,
	52,	64,	16,	0xffff000fffffffffLL,
	21,	32,	16,	0xffff001fffff001fLL,
	53,	64,	16,	0xffff001fffffffffLL,
	22,	32,	16,	0xffff003fffff003fLL,
	54,	64,	16,	0xffff003fffffffffLL,
	23,	32,	16,	0xffff007fffff007fLL,
	55,	64,	16,	0xffff007fffffffffLL,
	24,	32,	16,	0xffff00ffffff00ffLL,
	56,	64,	16,	0xffff00ffffffffffLL,
	25,	32,	16,	0xffff01ffffff01ffLL,
	57,	64,	16,	0xffff01ffffffffffLL,
	26,	32,	16,	0xffff03ffffff03ffLL,
	58,	64,	16,	0xffff03ffffffffffLL,
	27,	32,	16,	0xffff07ffffff07ffLL,
	59,	64,	16,	0xffff07ffffffffffLL,
	28,	32,	16,	0xffff0fffffff0fffLL,
	60,	64,	16,	0xffff0fffffffffffLL,
	29,	32,	16,	0xffff1fffffff1fffLL,
	61,	64,	16,	0xffff1fffffffffffLL,
	30,	32,	16,	0xffff3fffffff3fffLL,
	62,	64,	16,	0xffff3fffffffffffLL,
	31,	32,	16,	0xffff7fffffff7fffLL,
	63,	64,	16,	0xffff7fffffffffffLL,
	17,	64,	17,	0xffff800000000000LL,
	18,	64,	17,	0xffff800000000001LL,
	19,	64,	17,	0xffff800000000003LL,
	20,	64,	17,	0xffff800000000007LL,
	21,	64,	17,	0xffff80000000000fLL,
	22,	64,	17,	0xffff80000000001fLL,
	23,	64,	17,	0xffff80000000003fLL,
	24,	64,	17,	0xffff80000000007fLL,
	25,	64,	17,	0xffff8000000000ffLL,
	26,	64,	17,	0xffff8000000001ffLL,
	27,	64,	17,	0xffff8000000003ffLL,
	28,	64,	17,	0xffff8000000007ffLL,
	29,	64,	17,	0xffff800000000fffLL,
	30,	64,	17,	0xffff800000001fffLL,
	31,	64,	17,	0xffff800000003fffLL,
	32,	64,	17,	0xffff800000007fffLL,
	33,	64,	17,	0xffff80000000ffffLL,
	34,	64,	17,	0xffff80000001ffffLL,
	35,	64,	17,	0xffff80000003ffffLL,
	36,	64,	17,	0xffff80000007ffffLL,
	37,	64,	17,	0xffff8000000fffffLL,
	38,	64,	17,	0xffff8000001fffffLL,
	39,	64,	17,	0xffff8000003fffffLL,
	40,	64,	17,	0xffff8000007fffffLL,
	41,	64,	17,	0xffff800000ffffffLL,
	42,	64,	17,	0xffff800001ffffffLL,
	43,	64,	17,	0xffff800003ffffffLL,
	44,	64,	17,	0xffff800007ffffffLL,
	45,	64,	17,	0xffff80000fffffffLL,
	46,	64,	17,	0xffff80001fffffffLL,
	47,	64,	17,	0xffff80003fffffffLL,
	48,	64,	17,	0xffff80007fffffffLL,
	17,	32,	17,	0xffff8000ffff8000LL,
	49,	64,	17,	0xffff8000ffffffffLL,
	18,	32,	17,	0xffff8001ffff8001LL,
	50,	64,	17,	0xffff8001ffffffffLL,
	19,	32,	17,	0xffff8003ffff8003LL,
	51,	64,	17,	0xffff8003ffffffffLL,
	20,	32,	17,	0xffff8007ffff8007LL,
	52,	64,	17,	0xffff8007ffffffffLL,
	21,	32,	17,	0xffff800fffff800fLL,
	53,	64,	17,	0xffff800fffffffffLL,
	22,	32,	17,	0xffff801fffff801fLL,
	54,	64,	17,	0xffff801fffffffffLL,
	23,	32,	17,	0xffff803fffff803fLL,
	55,	64,	17,	0xffff803fffffffffLL,
	24,	32,	17,	0xffff807fffff807fLL,
	56,	64,	17,	0xffff807fffffffffLL,
	25,	32,	17,	0xffff80ffffff80ffLL,
	57,	64,	17,	0xffff80ffffffffffLL,
	26,	32,	17,	0xffff81ffffff81ffLL,
	58,	64,	17,	0xffff81ffffffffffLL,
	27,	32,	17,	0xffff83ffffff83ffLL,
	59,	64,	17,	0xffff83ffffffffffLL,
	28,	32,	17,	0xffff87ffffff87ffLL,
	60,	64,	17,	0xffff87ffffffffffLL,
	29,	32,	17,	0xffff8fffffff8fffLL,
	61,	64,	17,	0xffff8fffffffffffLL,
	30,	32,	17,	0xffff9fffffff9fffLL,
	62,	64,	17,	0xffff9fffffffffffLL,
	31,	32,	17,	0xffffbfffffffbfffLL,
	63,	64,	17,	0xffffbfffffffffffLL,
	18,	64,	18,	0xffffc00000000000LL,
	19,	64,	18,	0xffffc00000000001LL,
	20,	64,	18,	0xffffc00000000003LL,
	21,	64,	18,	0xffffc00000000007LL,
	22,	64,	18,	0xffffc0000000000fLL,
	23,	64,	18,	0xffffc0000000001fLL,
	24,	64,	18,	0xffffc0000000003fLL,
	25,	64,	18,	0xffffc0000000007fLL,
	26,	64,	18,	0xffffc000000000ffLL,
	27,	64,	18,	0xffffc000000001ffLL,
	28,	64,	18,	0xffffc000000003ffLL,
	29,	64,	18,	0xffffc000000007ffLL,
	30,	64,	18,	0xffffc00000000fffLL,
	31,	64,	18,	0xffffc00000001fffLL,
	32,	64,	18,	0xffffc00000003fffLL,
	33,	64,	18,	0xffffc00000007fffLL,
	34,	64,	18,	0xffffc0000000ffffLL,
	35,	64,	18,	0xffffc0000001ffffLL,
	36,	64,	18,	0xffffc0000003ffffLL,
	37,	64,	18,	0xffffc0000007ffffLL,
	38,	64,	18,	0xffffc000000fffffLL,
	39,	64,	18,	0xffffc000001fffffLL,
	40,	64,	18,	0xffffc000003fffffLL,
	41,	64,	18,	0xffffc000007fffffLL,
	42,	64,	18,	0xffffc00000ffffffLL,
	43,	64,	18,	0xffffc00001ffffffLL,
	44,	64,	18,	0xffffc00003ffffffLL,
	45,	64,	18,	0xffffc00007ffffffLL,
	46,	64,	18,	0xffffc0000fffffffLL,
	47,	64,	18,	0xffffc0001fffffffLL,
	48,	64,	18,	0xffffc0003fffffffLL,
	49,	64,	18,	0xffffc0007fffffffLL,
	18,	32,	18,	0xffffc000ffffc000LL,
	50,	64,	18,	0xffffc000ffffffffLL,
	19,	32,	18,	0xffffc001ffffc001LL,
	51,	64,	18,	0xffffc001ffffffffLL,
	20,	32,	18,	0xffffc003ffffc003LL,
	52,	64,	18,	0xffffc003ffffffffLL,
	21,	32,	18,	0xffffc007ffffc007LL,
	53,	64,	18,	0xffffc007ffffffffLL,
	22,	32,	18,	0xffffc00fffffc00fLL,
	54,	64,	18,	0xffffc00fffffffffLL,
	23,	32,	18,	0xffffc01fffffc01fLL,
	55,	64,	18,	0xffffc01fffffffffLL,
	24,	32,	18,	0xffffc03fffffc03fLL,
	56,	64,	18,	0xffffc03fffffffffLL,
	25,	32,	18,	0xffffc07fffffc07fLL,
	57,	64,	18,	0xffffc07fffffffffLL,
	26,	32,	18,	0xffffc0ffffffc0ffLL,
	58,	64,	18,	0xffffc0ffffffffffLL,
	27,	32,	18,	0xffffc1ffffffc1ffLL,
	59,	64,	18,	0xffffc1ffffffffffLL,
	28,	32,	18,	0xffffc3ffffffc3ffLL,
	60,	64,	18,	0xffffc3ffffffffffLL,
	29,	32,	18,	0xffffc7ffffffc7ffLL,
	61,	64,	18,	0xffffc7ffffffffffLL,
	30,	32,	18,	0xffffcfffffffcfffLL,
	62,	64,	18,	0xffffcfffffffffffLL,
	31,	32,	18,	0xffffdfffffffdfffLL,
	63,	64,	18,	0xffffdfffffffffffLL,
	19,	64,	19,	0xffffe00000000000LL,
	20,	64,	19,	0xffffe00000000001LL,
	21,	64,	19,	0xffffe00000000003LL,
	22,	64,	19,	0xffffe00000000007LL,
	23,	64,	19,	0xffffe0000000000fLL,
	24,	64,	19,	0xffffe0000000001fLL,
	25,	64,	19,	0xffffe0000000003fLL,
	26,	64,	19,	0xffffe0000000007fLL,
	27,	64,	19,	0xffffe000000000ffLL,
	28,	64,	19,	0xffffe000000001ffLL,
	29,	64,	19,	0xffffe000000003ffLL,
	30,	64,	19,	0xffffe000000007ffLL,
	31,	64,	19,	0xffffe00000000fffLL,
	32,	64,	19,	0xffffe00000001fffLL,
	33,	64,	19,	0xffffe00000003fffLL,
	34,	64,	19,	0xffffe00000007fffLL,
	35,	64,	19,	0xffffe0000000ffffLL,
	36,	64,	19,	0xffffe0000001ffffLL,
	37,	64,	19,	0xffffe0000003ffffLL,
	38,	64,	19,	0xffffe0000007ffffLL,
	39,	64,	19,	0xffffe000000fffffLL,
	40,	64,	19,	0xffffe000001fffffLL,
	41,	64,	19,	0xffffe000003fffffLL,
	42,	64,	19,	0xffffe000007fffffLL,
	43,	64,	19,	0xffffe00000ffffffLL,
	44,	64,	19,	0xffffe00001ffffffLL,
	45,	64,	19,	0xffffe00003ffffffLL,
	46,	64,	19,	0xffffe00007ffffffLL,
	47,	64,	19,	0xffffe0000fffffffLL,
	48,	64,	19,	0xffffe0001fffffffLL,
	49,	64,	19,	0xffffe0003fffffffLL,
	50,	64,	19,	0xffffe0007fffffffLL,
	19,	32,	19,	0xffffe000ffffe000LL,
	51,	64,	19,	0xffffe000ffffffffLL,
	20,	32,	19,	0xffffe001ffffe001LL,
	52,	64,	19,	0xffffe001ffffffffLL,
	21,	32,	19,	0xffffe003ffffe003LL,
	53,	64,	19,	0xffffe003ffffffffLL,
	22,	32,	19,	0xffffe007ffffe007LL,
	54,	64,	19,	0xffffe007ffffffffLL,
	23,	32,	19,	0xffffe00fffffe00fLL,
	55,	64,	19,	0xffffe00fffffffffLL,
	24,	32,	19,	0xffffe01fffffe01fLL,
	56,	64,	19,	0xffffe01fffffffffLL,
	25,	32,	19,	0xffffe03fffffe03fLL,
	57,	64,	19,	0xffffe03fffffffffLL,
	26,	32,	19,	0xffffe07fffffe07fLL,
	58,	64,	19,	0xffffe07fffffffffLL,
	27,	32,	19,	0xffffe0ffffffe0ffLL,
	59,	64,	19,	0xffffe0ffffffffffLL,
	28,	32,	19,	0xffffe1ffffffe1ffLL,
	60,	64,	19,	0xffffe1ffffffffffLL,
	29,	32,	19,	0xffffe3ffffffe3ffLL,
	61,	64,	19,	0xffffe3ffffffffffLL,
	30,	32,	19,	0xffffe7ffffffe7ffLL,
	62,	64,	19,	0xffffe7ffffffffffLL,
	31,	32,	19,	0xffffefffffffefffLL,
	63,	64,	19,	0xffffefffffffffffLL,
	20,	64,	20,	0xfffff00000000000LL,
	21,	64,	20,	0xfffff00000000001LL,
	22,	64,	20,	0xfffff00000000003LL,
	23,	64,	20,	0xfffff00000000007LL,
	24,	64,	20,	0xfffff0000000000fLL,
	25,	64,	20,	0xfffff0000000001fLL,
	26,	64,	20,	0xfffff0000000003fLL,
	27,	64,	20,	0xfffff0000000007fLL,
	28,	64,	20,	0xfffff000000000ffLL,
	29,	64,	20,	0xfffff000000001ffLL,
	30,	64,	20,	0xfffff000000003ffLL,
	31,	64,	20,	0xfffff000000007ffLL,
	32,	64,	20,	0xfffff00000000fffLL,
	33,	64,	20,	0xfffff00000001fffLL,
	34,	64,	20,	0xfffff00000003fffLL,
	35,	64,	20,	0xfffff00000007fffLL,
	36,	64,	20,	0xfffff0000000ffffLL,
	37,	64,	20,	0xfffff0000001ffffLL,
	38,	64,	20,	0xfffff0000003ffffLL,
	39,	64,	20,	0xfffff0000007ffffLL,
	40,	64,	20,	0xfffff000000fffffLL,
	41,	64,	20,	0xfffff000001fffffLL,
	42,	64,	20,	0xfffff000003fffffLL,
	43,	64,	20,	0xfffff000007fffffLL,
	44,	64,	20,	0xfffff00000ffffffLL,
	45,	64,	20,	0xfffff00001ffffffLL,
	46,	64,	20,	0xfffff00003ffffffLL,
	47,	64,	20,	0xfffff00007ffffffLL,
	48,	64,	20,	0xfffff0000fffffffLL,
	49,	64,	20,	0xfffff0001fffffffLL,
	50,	64,	20,	0xfffff0003fffffffLL,
	51,	64,	20,	0xfffff0007fffffffLL,
	20,	32,	20,	0xfffff000fffff000LL,
	52,	64,	20,	0xfffff000ffffffffLL,
	21,	32,	20,	0xfffff001fffff001LL,
	53,	64,	20,	0xfffff001ffffffffLL,
	22,	32,	20,	0xfffff003fffff003LL,
	54,	64,	20,	0xfffff003ffffffffLL,
	23,	32,	20,	0xfffff007fffff007LL,
	55,	64,	20,	0xfffff007ffffffffLL,
	24,	32,	20,	0xfffff00ffffff00fLL,
	56,	64,	20,	0xfffff00fffffffffLL,
	25,	32,	20,	0xfffff01ffffff01fLL,
	57,	64,	20,	0xfffff01fffffffffLL,
	26,	32,	20,	0xfffff03ffffff03fLL,
	58,	64,	20,	0xfffff03fffffffffLL,
	27,	32,	20,	0xfffff07ffffff07fLL,
	59,	64,	20,	0xfffff07fffffffffLL,
	28,	32,	20,	0xfffff0fffffff0ffLL,
	60,	64,	20,	0xfffff0ffffffffffLL,
	29,	32,	20,	0xfffff1fffffff1ffLL,
	61,	64,	20,	0xfffff1ffffffffffLL,
	30,	32,	20,	0xfffff3fffffff3ffLL,
	62,	64,	20,	0xfffff3ffffffffffLL,
	31,	32,	20,	0xfffff7fffffff7ffLL,
	63,	64,	20,	0xfffff7ffffffffffLL,
	21,	64,	21,	0xfffff80000000000LL,
	22,	64,	21,	0xfffff80000000001LL,
	23,	64,	21,	0xfffff80000000003LL,
	24,	64,	21,	0xfffff80000000007LL,
	25,	64,	21,	0xfffff8000000000fLL,
	26,	64,	21,	0xfffff8000000001fLL,
	27,	64,	21,	0xfffff8000000003fLL,
	28,	64,	21,	0xfffff8000000007fLL,
	29,	64,	21,	0xfffff800000000ffLL,
	30,	64,	21,	0xfffff800000001ffLL,
	31,	64,	21,	0xfffff800000003ffLL,
	32,	64,	21,	0xfffff800000007ffLL,
	33,	64,	21,	0xfffff80000000fffLL,
	34,	64,	21,	0xfffff80000001fffLL,
	35,	64,	21,	0xfffff80000003fffLL,
	36,	64,	21,	0xfffff80000007fffLL,
	37,	64,	21,	0xfffff8000000ffffLL,
	38,	64,	21,	0xfffff8000001ffffLL,
	39,	64,	21,	0xfffff8000003ffffLL,
	40,	64,	21,	0xfffff8000007ffffLL,
	41,	64,	21,	0xfffff800000fffffLL,
	42,	64,	21,	0xfffff800001fffffLL,
	43,	64,	21,	0xfffff800003fffffLL,
	44,	64,	21,	0xfffff800007fffffLL,
	45,	64,	21,	0xfffff80000ffffffLL,
	46,	64,	21,	0xfffff80001ffffffLL,
	47,	64,	21,	0xfffff80003ffffffLL,
	48,	64,	21,	0xfffff80007ffffffLL,
	49,	64,	21,	0xfffff8000fffffffLL,
	50,	64,	21,	0xfffff8001fffffffLL,
	51,	64,	21,	0xfffff8003fffffffLL,
	52,	64,	21,	0xfffff8007fffffffLL,
	21,	32,	21,	0xfffff800fffff800LL,
	53,	64,	21,	0xfffff800ffffffffLL,
	22,	32,	21,	0xfffff801fffff801LL,
	54,	64,	21,	0xfffff801ffffffffLL,
	23,	32,	21,	0xfffff803fffff803LL,
	55,	64,	21,	0xfffff803ffffffffLL,
	24,	32,	21,	0xfffff807fffff807LL,
	56,	64,	21,	0xfffff807ffffffffLL,
	25,	32,	21,	0xfffff80ffffff80fLL,
	57,	64,	21,	0xfffff80fffffffffLL,
	26,	32,	21,	0xfffff81ffffff81fLL,
	58,	64,	21,	0xfffff81fffffffffLL,
	27,	32,	21,	0xfffff83ffffff83fLL,
	59,	64,	21,	0xfffff83fffffffffLL,
	28,	32,	21,	0xfffff87ffffff87fLL,
	60,	64,	21,	0xfffff87fffffffffLL,
	29,	32,	21,	0xfffff8fffffff8ffLL,
	61,	64,	21,	0xfffff8ffffffffffLL,
	30,	32,	21,	0xfffff9fffffff9ffLL,
	62,	64,	21,	0xfffff9ffffffffffLL,
	31,	32,	21,	0xfffffbfffffffbffLL,
	63,	64,	21,	0xfffffbffffffffffLL,
	22,	64,	22,	0xfffffc0000000000LL,
	23,	64,	22,	0xfffffc0000000001LL,
	24,	64,	22,	0xfffffc0000000003LL,
	25,	64,	22,	0xfffffc0000000007LL,
	26,	64,	22,	0xfffffc000000000fLL,
	27,	64,	22,	0xfffffc000000001fLL,
	28,	64,	22,	0xfffffc000000003fLL,
	29,	64,	22,	0xfffffc000000007fLL,
	30,	64,	22,	0xfffffc00000000ffLL,
	31,	64,	22,	0xfffffc00000001ffLL,
	32,	64,	22,	0xfffffc00000003ffLL,
	33,	64,	22,	0xfffffc00000007ffLL,
	34,	64,	22,	0xfffffc0000000fffLL,
	35,	64,	22,	0xfffffc0000001fffLL,
	36,	64,	22,	0xfffffc0000003fffLL,
	37,	64,	22,	0xfffffc0000007fffLL,
	38,	64,	22,	0xfffffc000000ffffLL,
	39,	64,	22,	0xfffffc000001ffffLL,
	40,	64,	22,	0xfffffc000003ffffLL,
	41,	64,	22,	0xfffffc000007ffffLL,
	42,	64,	22,	0xfffffc00000fffffLL,
	43,	64,	22,	0xfffffc00001fffffLL,
	44,	64,	22,	0xfffffc00003fffffLL,
	45,	64,	22,	0xfffffc00007fffffLL,
	46,	64,	22,	0xfffffc0000ffffffLL,
	47,	64,	22,	0xfffffc0001ffffffLL,
	48,	64,	22,	0xfffffc0003ffffffLL,
	49,	64,	22,	0xfffffc0007ffffffLL,
	50,	64,	22,	0xfffffc000fffffffLL,
	51,	64,	22,	0xfffffc001fffffffLL,
	52,	64,	22,	0xfffffc003fffffffLL,
	53,	64,	22,	0xfffffc007fffffffLL,
	22,	32,	22,	0xfffffc00fffffc00LL,
	54,	64,	22,	0xfffffc00ffffffffLL,
	23,	32,	22,	0xfffffc01fffffc01LL,
	55,	64,	22,	0xfffffc01ffffffffLL,
	24,	32,	22,	0xfffffc03fffffc03LL,
	56,	64,	22,	0xfffffc03ffffffffLL,
	25,	32,	22,	0xfffffc07fffffc07LL,
	57,	64,	22,	0xfffffc07ffffffffLL,
	26,	32,	22,	0xfffffc0ffffffc0fLL,
	58,	64,	22,	0xfffffc0fffffffffLL,
	27,	32,	22,	0xfffffc1ffffffc1fLL,
	59,	64,	22,	0xfffffc1fffffffffLL,
	28,	32,	22,	0xfffffc3ffffffc3fLL,
	60,	64,	22,	0xfffffc3fffffffffLL,
	29,	32,	22,	0xfffffc7ffffffc7fLL,
	61,	64,	22,	0xfffffc7fffffffffLL,
	30,	32,	22,	0xfffffcfffffffcffLL,
	62,	64,	22,	0xfffffcffffffffffLL,
	31,	32,	22,	0xfffffdfffffffdffLL,
	63,	64,	22,	0xfffffdffffffffffLL,
	23,	64,	23,	0xfffffe0000000000LL,
	24,	64,	23,	0xfffffe0000000001LL,
	25,	64,	23,	0xfffffe0000000003LL,
	26,	64,	23,	0xfffffe0000000007LL,
	27,	64,	23,	0xfffffe000000000fLL,
	28,	64,	23,	0xfffffe000000001fLL,
	29,	64,	23,	0xfffffe000000003fLL,
	30,	64,	23,	0xfffffe000000007fLL,
	31,	64,	23,	0xfffffe00000000ffLL,
	32,	64,	23,	0xfffffe00000001ffLL,
	33,	64,	23,	0xfffffe00000003ffLL,
	34,	64,	23,	0xfffffe00000007ffLL,
	35,	64,	23,	0xfffffe0000000fffLL,
	36,	64,	23,	0xfffffe0000001fffLL,
	37,	64,	23,	0xfffffe0000003fffLL,
	38,	64,	23,	0xfffffe0000007fffLL,
	39,	64,	23,	0xfffffe000000ffffLL,
	40,	64,	23,	0xfffffe000001ffffLL,
	41,	64,	23,	0xfffffe000003ffffLL,
	42,	64,	23,	0xfffffe000007ffffLL,
	43,	64,	23,	0xfffffe00000fffffLL,
	44,	64,	23,	0xfffffe00001fffffLL,
	45,	64,	23,	0xfffffe00003fffffLL,
	46,	64,	23,	0xfffffe00007fffffLL,
	47,	64,	23,	0xfffffe0000ffffffLL,
	48,	64,	23,	0xfffffe0001ffffffLL,
	49,	64,	23,	0xfffffe0003ffffffLL,
	50,	64,	23,	0xfffffe0007ffffffLL,
	51,	64,	23,	0xfffffe000fffffffLL,
	52,	64,	23,	0xfffffe001fffffffLL,
	53,	64,	23,	0xfffffe003fffffffLL,
	54,	64,	23,	0xfffffe007fffffffLL,
	23,	32,	23,	0xfffffe00fffffe00LL,
	55,	64,	23,	0xfffffe00ffffffffLL,
	24,	32,	23,	0xfffffe01fffffe01LL,
	56,	64,	23,	0xfffffe01ffffffffLL,
	25,	32,	23,	0xfffffe03fffffe03LL,
	57,	64,	23,	0xfffffe03ffffffffLL,
	26,	32,	23,	0xfffffe07fffffe07LL,
	58,	64,	23,	0xfffffe07ffffffffLL,
	27,	32,	23,	0xfffffe0ffffffe0fLL,
	59,	64,	23,	0xfffffe0fffffffffLL,
	28,	32,	23,	0xfffffe1ffffffe1fLL,
	60,	64,	23,	0xfffffe1fffffffffLL,
	29,	32,	23,	0xfffffe3ffffffe3fLL,
	61,	64,	23,	0xfffffe3fffffffffLL,
	30,	32,	23,	0xfffffe7ffffffe7fLL,
	62,	64,	23,	0xfffffe7fffffffffLL,
	31,	32,	23,	0xfffffefffffffeffLL,
	63,	64,	23,	0xfffffeffffffffffLL,
	24,	64,	24,	0xffffff0000000000LL,
	25,	64,	24,	0xffffff0000000001LL,
	26,	64,	24,	0xffffff0000000003LL,
	27,	64,	24,	0xffffff0000000007LL,
	28,	64,	24,	0xffffff000000000fLL,
	29,	64,	24,	0xffffff000000001fLL,
	30,	64,	24,	0xffffff000000003fLL,
	31,	64,	24,	0xffffff000000007fLL,
	32,	64,	24,	0xffffff00000000ffLL,
	33,	64,	24,	0xffffff00000001ffLL,
	34,	64,	24,	0xffffff00000003ffLL,
	35,	64,	24,	0xffffff00000007ffLL,
	36,	64,	24,	0xffffff0000000fffLL,
	37,	64,	24,	0xffffff0000001fffLL,
	38,	64,	24,	0xffffff0000003fffLL,
	39,	64,	24,	0xffffff0000007fffLL,
	40,	64,	24,	0xffffff000000ffffLL,
	41,	64,	24,	0xffffff000001ffffLL,
	42,	64,	24,	0xffffff000003ffffLL,
	43,	64,	24,	0xffffff000007ffffLL,
	44,	64,	24,	0xffffff00000fffffLL,
	45,	64,	24,	0xffffff00001fffffLL,
	46,	64,	24,	0xffffff00003fffffLL,
	47,	64,	24,	0xffffff00007fffffLL,
	48,	64,	24,	0xffffff0000ffffffLL,
	49,	64,	24,	0xffffff0001ffffffLL,
	50,	64,	24,	0xffffff0003ffffffLL,
	51,	64,	24,	0xffffff0007ffffffLL,
	52,	64,	24,	0xffffff000fffffffLL,
	53,	64,	24,	0xffffff001fffffffLL,
	54,	64,	24,	0xffffff003fffffffLL,
	55,	64,	24,	0xffffff007fffffffLL,
	24,	32,	24,	0xffffff00ffffff00LL,
	56,	64,	24,	0xffffff00ffffffffLL,
	25,	32,	24,	0xffffff01ffffff01LL,
	57,	64,	24,	0xffffff01ffffffffLL,
	26,	32,	24,	0xffffff03ffffff03LL,
	58,	64,	24,	0xffffff03ffffffffLL,
	27,	32,	24,	0xffffff07ffffff07LL,
	59,	64,	24,	0xffffff07ffffffffLL,
	28,	32,	24,	0xffffff0fffffff0fLL,
	60,	64,	24,	0xffffff0fffffffffLL,
	29,	32,	24,	0xffffff1fffffff1fLL,
	61,	64,	24,	0xffffff1fffffffffLL,
	30,	32,	24,	0xffffff3fffffff3fLL,
	62,	64,	24,	0xffffff3fffffffffLL,
	31,	32,	24,	0xffffff7fffffff7fLL,
	63,	64,	24,	0xffffff7fffffffffLL,
	25,	64,	25,	0xffffff8000000000LL,
	26,	64,	25,	0xffffff8000000001LL,
	27,	64,	25,	0xffffff8000000003LL,
	28,	64,	25,	0xffffff8000000007LL,
	29,	64,	25,	0xffffff800000000fLL,
	30,	64,	25,	0xffffff800000001fLL,
	31,	64,	25,	0xffffff800000003fLL,
	32,	64,	25,	0xffffff800000007fLL,
	33,	64,	25,	0xffffff80000000ffLL,
	34,	64,	25,	0xffffff80000001ffLL,
	35,	64,	25,	0xffffff80000003ffLL,
	36,	64,	25,	0xffffff80000007ffLL,
	37,	64,	25,	0xffffff8000000fffLL,
	38,	64,	25,	0xffffff8000001fffLL,
	39,	64,	25,	0xffffff8000003fffLL,
	40,	64,	25,	0xffffff8000007fffLL,
	41,	64,	25,	0xffffff800000ffffLL,
	42,	64,	25,	0xffffff800001ffffLL,
	43,	64,	25,	0xffffff800003ffffLL,
	44,	64,	25,	0xffffff800007ffffLL,
	45,	64,	25,	0xffffff80000fffffLL,
	46,	64,	25,	0xffffff80001fffffLL,
	47,	64,	25,	0xffffff80003fffffLL,
	48,	64,	25,	0xffffff80007fffffLL,
	49,	64,	25,	0xffffff8000ffffffLL,
	50,	64,	25,	0xffffff8001ffffffLL,
	51,	64,	25,	0xffffff8003ffffffLL,
	52,	64,	25,	0xffffff8007ffffffLL,
	53,	64,	25,	0xffffff800fffffffLL,
	54,	64,	25,	0xffffff801fffffffLL,
	55,	64,	25,	0xffffff803fffffffLL,
	56,	64,	25,	0xffffff807fffffffLL,
	25,	32,	25,	0xffffff80ffffff80LL,
	57,	64,	25,	0xffffff80ffffffffLL,
	26,	32,	25,	0xffffff81ffffff81LL,
	58,	64,	25,	0xffffff81ffffffffLL,
	27,	32,	25,	0xffffff83ffffff83LL,
	59,	64,	25,	0xffffff83ffffffffLL,
	28,	32,	25,	0xffffff87ffffff87LL,
	60,	64,	25,	0xffffff87ffffffffLL,
	29,	32,	25,	0xffffff8fffffff8fLL,
	61,	64,	25,	0xffffff8fffffffffLL,
	30,	32,	25,	0xffffff9fffffff9fLL,
	62,	64,	25,	0xffffff9fffffffffLL,
	31,	32,	25,	0xffffffbfffffffbfLL,
	63,	64,	25,	0xffffffbfffffffffLL,
	26,	64,	26,	0xffffffc000000000LL,
	27,	64,	26,	0xffffffc000000001LL,
	28,	64,	26,	0xffffffc000000003LL,
	29,	64,	26,	0xffffffc000000007LL,
	30,	64,	26,	0xffffffc00000000fLL,
	31,	64,	26,	0xffffffc00000001fLL,
	32,	64,	26,	0xffffffc00000003fLL,
	33,	64,	26,	0xffffffc00000007fLL,
	34,	64,	26,	0xffffffc0000000ffLL,
	35,	64,	26,	0xffffffc0000001ffLL,
	36,	64,	26,	0xffffffc0000003ffLL,
	37,	64,	26,	0xffffffc0000007ffLL,
	38,	64,	26,	0xffffffc000000fffLL,
	39,	64,	26,	0xffffffc000001fffLL,
	40,	64,	26,	0xffffffc000003fffLL,
	41,	64,	26,	0xffffffc000007fffLL,
	42,	64,	26,	0xffffffc00000ffffLL,
	43,	64,	26,	0xffffffc00001ffffLL,
	44,	64,	26,	0xffffffc00003ffffLL,
	45,	64,	26,	0xffffffc00007ffffLL,
	46,	64,	26,	0xffffffc0000fffffLL,
	47,	64,	26,	0xffffffc0001fffffLL,
	48,	64,	26,	0xffffffc0003fffffLL,
	49,	64,	26,	0xffffffc0007fffffLL,
	50,	64,	26,	0xffffffc000ffffffLL,
	51,	64,	26,	0xffffffc001ffffffLL,
	52,	64,	26,	0xffffffc003ffffffLL,
	53,	64,	26,	0xffffffc007ffffffLL,
	54,	64,	26,	0xffffffc00fffffffLL,
	55,	64,	26,	0xffffffc01fffffffLL,
	56,	64,	26,	0xffffffc03fffffffLL,
	57,	64,	26,	0xffffffc07fffffffLL,
	26,	32,	26,	0xffffffc0ffffffc0LL,
	58,	64,	26,	0xffffffc0ffffffffLL,
	27,	32,	26,	0xffffffc1ffffffc1LL,
	59,	64,	26,	0xffffffc1ffffffffLL,
	28,	32,	26,	0xffffffc3ffffffc3LL,
	60,	64,	26,	0xffffffc3ffffffffLL,
	29,	32,	26,	0xffffffc7ffffffc7LL,
	61,	64,	26,	0xffffffc7ffffffffLL,
	30,	32,	26,	0xffffffcfffffffcfLL,
	62,	64,	26,	0xffffffcfffffffffLL,
	31,	32,	26,	0xffffffdfffffffdfLL,
	63,	64,	26,	0xffffffdfffffffffLL,
	27,	64,	27,	0xffffffe000000000LL,
	28,	64,	27,	0xffffffe000000001LL,
	29,	64,	27,	0xffffffe000000003LL,
	30,	64,	27,	0xffffffe000000007LL,
	31,	64,	27,	0xffffffe00000000fLL,
	32,	64,	27,	0xffffffe00000001fLL,
	33,	64,	27,	0xffffffe00000003fLL,
	34,	64,	27,	0xffffffe00000007fLL,
	35,	64,	27,	0xffffffe0000000ffLL,
	36,	64,	27,	0xffffffe0000001ffLL,
	37,	64,	27,	0xffffffe0000003ffLL,
	38,	64,	27,	0xffffffe0000007ffLL,
	39,	64,	27,	0xffffffe000000fffLL,
	40,	64,	27,	0xffffffe000001fffLL,
	41,	64,	27,	0xffffffe000003fffLL,
	42,	64,	27,	0xffffffe000007fffLL,
	43,	64,	27,	0xffffffe00000ffffLL,
	44,	64,	27,	0xffffffe00001ffffLL,
	45,	64,	27,	0xffffffe00003ffffLL,
	46,	64,	27,	0xffffffe00007ffffLL,
	47,	64,	27,	0xffffffe0000fffffLL,
	48,	64,	27,	0xffffffe0001fffffLL,
	49,	64,	27,	0xffffffe0003fffffLL,
	50,	64,	27,	0xffffffe0007fffffLL,
	51,	64,	27,	0xffffffe000ffffffLL,
	52,	64,	27,	0xffffffe001ffffffLL,
	53,	64,	27,	0xffffffe003ffffffLL,
	54,	64,	27,	0xffffffe007ffffffLL,
	55,	64,	27,	0xffffffe00fffffffLL,
	56,	64,	27,	0xffffffe01fffffffLL,
	57,	64,	27,	0xffffffe03fffffffLL,
	58,	64,	27,	0xffffffe07fffffffLL,
	27,	32,	27,	0xffffffe0ffffffe0LL,
	59,	64,	27,	0xffffffe0ffffffffLL,
	28,	32,	27,	0xffffffe1ffffffe1LL,
	60,	64,	27,	0xffffffe1ffffffffLL,
	29,	32,	27,	0xffffffe3ffffffe3LL,
	61,	64,	27,	0xffffffe3ffffffffLL,
	30,	32,	27,	0xffffffe7ffffffe7LL,
	62,	64,	27,	0xffffffe7ffffffffLL,
	31,	32,	27,	0xffffffefffffffefLL,
	63,	64,	27,	0xffffffefffffffffLL,
	28,	64,	28,	0xfffffff000000000LL,
	29,	64,	28,	0xfffffff000000001LL,
	30,	64,	28,	0xfffffff000000003LL,
	31,	64,	28,	0xfffffff000000007LL,
	32,	64,	28,	0xfffffff00000000fLL,
	33,	64,	28,	0xfffffff00000001fLL,
	34,	64,	28,	0xfffffff00000003fLL,
	35,	64,	28,	0xfffffff00000007fLL,
	36,	64,	28,	0xfffffff0000000ffLL,
	37,	64,	28,	0xfffffff0000001ffLL,
	38,	64,	28,	0xfffffff0000003ffLL,
	39,	64,	28,	0xfffffff0000007ffLL,
	40,	64,	28,	0xfffffff000000fffLL,
	41,	64,	28,	0xfffffff000001fffLL,
	42,	64,	28,	0xfffffff000003fffLL,
	43,	64,	28,	0xfffffff000007fffLL,
	44,	64,	28,	0xfffffff00000ffffLL,
	45,	64,	28,	0xfffffff00001ffffLL,
	46,	64,	28,	0xfffffff00003ffffLL,
	47,	64,	28,	0xfffffff00007ffffLL,
	48,	64,	28,	0xfffffff0000fffffLL,
	49,	64,	28,	0xfffffff0001fffffLL,
	50,	64,	28,	0xfffffff0003fffffLL,
	51,	64,	28,	0xfffffff0007fffffLL,
	52,	64,	28,	0xfffffff000ffffffLL,
	53,	64,	28,	0xfffffff001ffffffLL,
	54,	64,	28,	0xfffffff003ffffffLL,
	55,	64,	28,	0xfffffff007ffffffLL,
	56,	64,	28,	0xfffffff00fffffffLL,
	57,	64,	28,	0xfffffff01fffffffLL,
	58,	64,	28,	0xfffffff03fffffffLL,
	59,	64,	28,	0xfffffff07fffffffLL,
	28,	32,	28,	0xfffffff0fffffff0LL,
	60,	64,	28,	0xfffffff0ffffffffLL,
	29,	32,	28,	0xfffffff1fffffff1LL,
	61,	64,	28,	0xfffffff1ffffffffLL,
	30,	32,	28,	0xfffffff3fffffff3LL,
	62,	64,	28,	0xfffffff3ffffffffLL,
	31,	32,	28,	0xfffffff7fffffff7LL,
	63,	64,	28,	0xfffffff7ffffffffLL,
	29,	64,	29,	0xfffffff800000000LL,
	30,	64,	29,	0xfffffff800000001LL,
	31,	64,	29,	0xfffffff800000003LL,
	32,	64,	29,	0xfffffff800000007LL,
	33,	64,	29,	0xfffffff80000000fLL,
	34,	64,	29,	0xfffffff80000001fLL,
	35,	64,	29,	0xfffffff80000003fLL,
	36,	64,	29,	0xfffffff80000007fLL,
	37,	64,	29,	0xfffffff8000000ffLL,
	38,	64,	29,	0xfffffff8000001ffLL,
	39,	64,	29,	0xfffffff8000003ffLL,
	40,	64,	29,	0xfffffff8000007ffLL,
	41,	64,	29,	0xfffffff800000fffLL,
	42,	64,	29,	0xfffffff800001fffLL,
	43,	64,	29,	0xfffffff800003fffLL,
	44,	64,	29,	0xfffffff800007fffLL,
	45,	64,	29,	0xfffffff80000ffffLL,
	46,	64,	29,	0xfffffff80001ffffLL,
	47,	64,	29,	0xfffffff80003ffffLL,
	48,	64,	29,	0xfffffff80007ffffLL,
	49,	64,	29,	0xfffffff8000fffffLL,
	50,	64,	29,	0xfffffff8001fffffLL,
	51,	64,	29,	0xfffffff8003fffffLL,
	52,	64,	29,	0xfffffff8007fffffLL,
	53,	64,	29,	0xfffffff800ffffffLL,
	54,	64,	29,	0xfffffff801ffffffLL,
	55,	64,	29,	0xfffffff803ffffffLL,
	56,	64,	29,	0xfffffff807ffffffLL,
	57,	64,	29,	0xfffffff80fffffffLL,
	58,	64,	29,	0xfffffff81fffffffLL,
	59,	64,	29,	0xfffffff83fffffffLL,
	60,	64,	29,	0xfffffff87fffffffLL,
	29,	32,	29,	0xfffffff8fffffff8LL,
	61,	64,	29,	0xfffffff8ffffffffLL,
	30,	32,	29,	0xfffffff9fffffff9LL,
	62,	64,	29,	0xfffffff9ffffffffLL,
	31,	32,	29,	0xfffffffbfffffffbLL,
	63,	64,	29,	0xfffffffbffffffffLL,
	30,	64,	30,	0xfffffffc00000000LL,
	31,	64,	30,	0xfffffffc00000001LL,
	32,	64,	30,	0xfffffffc00000003LL,
	33,	64,	30,	0xfffffffc00000007LL,
	34,	64,	30,	0xfffffffc0000000fLL,
	35,	64,	30,	0xfffffffc0000001fLL,
	36,	64,	30,	0xfffffffc0000003fLL,
	37,	64,	30,	0xfffffffc0000007fLL,
	38,	64,	30,	0xfffffffc000000ffLL,
	39,	64,	30,	0xfffffffc000001ffLL,
	40,	64,	30,	0xfffffffc000003ffLL,
	41,	64,	30,	0xfffffffc000007ffLL,
	42,	64,	30,	0xfffffffc00000fffLL,
	43,	64,	30,	0xfffffffc00001fffLL,
	44,	64,	30,	0xfffffffc00003fffLL,
	45,	64,	30,	0xfffffffc00007fffLL,
	46,	64,	30,	0xfffffffc0000ffffLL,
	47,	64,	30,	0xfffffffc0001ffffLL,
	48,	64,	30,	0xfffffffc0003ffffLL,
	49,	64,	30,	0xfffffffc0007ffffLL,
	50,	64,	30,	0xfffffffc000fffffLL,
	51,	64,	30,	0xfffffffc001fffffLL,
	52,	64,	30,	0xfffffffc003fffffLL,
	53,	64,	30,	0xfffffffc007fffffLL,
	54,	64,	30,	0xfffffffc00ffffffLL,
	55,	64,	30,	0xfffffffc01ffffffLL,
	56,	64,	30,	0xfffffffc03ffffffLL,
	57,	64,	30,	0xfffffffc07ffffffLL,
	58,	64,	30,	0xfffffffc0fffffffLL,
	59,	64,	30,	0xfffffffc1fffffffLL,
	60,	64,	30,	0xfffffffc3fffffffLL,
	61,	64,	30,	0xfffffffc7fffffffLL,
	30,	32,	30,	0xfffffffcfffffffcLL,
	62,	64,	30,	0xfffffffcffffffffLL,
	31,	32,	30,	0xfffffffdfffffffdLL,
	63,	64,	30,	0xfffffffdffffffffLL,
	31,	64,	31,	0xfffffffe00000000LL,
	32,	64,	31,	0xfffffffe00000001LL,
	33,	64,	31,	0xfffffffe00000003LL,
	34,	64,	31,	0xfffffffe00000007LL,
	35,	64,	31,	0xfffffffe0000000fLL,
	36,	64,	31,	0xfffffffe0000001fLL,
	37,	64,	31,	0xfffffffe0000003fLL,
	38,	64,	31,	0xfffffffe0000007fLL,
	39,	64,	31,	0xfffffffe000000ffLL,
	40,	64,	31,	0xfffffffe000001ffLL,
	41,	64,	31,	0xfffffffe000003ffLL,
	42,	64,	31,	0xfffffffe000007ffLL,
	43,	64,	31,	0xfffffffe00000fffLL,
	44,	64,	31,	0xfffffffe00001fffLL,
	45,	64,	31,	0xfffffffe00003fffLL,
	46,	64,	31,	0xfffffffe00007fffLL,
	47,	64,	31,	0xfffffffe0000ffffLL,
	48,	64,	31,	0xfffffffe0001ffffLL,
	49,	64,	31,	0xfffffffe0003ffffLL,
	50,	64,	31,	0xfffffffe0007ffffLL,
	51,	64,	31,	0xfffffffe000fffffLL,
	52,	64,	31,	0xfffffffe001fffffLL,
	53,	64,	31,	0xfffffffe003fffffLL,
	54,	64,	31,	0xfffffffe007fffffLL,
	55,	64,	31,	0xfffffffe00ffffffLL,
	56,	64,	31,	0xfffffffe01ffffffLL,
	57,	64,	31,	0xfffffffe03ffffffLL,
	58,	64,	31,	0xfffffffe07ffffffLL,
	59,	64,	31,	0xfffffffe0fffffffLL,
	60,	64,	31,	0xfffffffe1fffffffLL,
	61,	64,	31,	0xfffffffe3fffffffLL,
	62,	64,	31,	0xfffffffe7fffffffLL,
	31,	32,	31,	0xfffffffefffffffeLL,
	63,	64,	31,	0xfffffffeffffffffLL,
	32,	64,	32,	0xffffffff00000000LL,
	33,	64,	32,	0xffffffff00000001LL,
	34,	64,	32,	0xffffffff00000003LL,
	35,	64,	32,	0xffffffff00000007LL,
	36,	64,	32,	0xffffffff0000000fLL,
	37,	64,	32,	0xffffffff0000001fLL,
	38,	64,	32,	0xffffffff0000003fLL,
	39,	64,	32,	0xffffffff0000007fLL,
	40,	64,	32,	0xffffffff000000ffLL,
	41,	64,	32,	0xffffffff000001ffLL,
	42,	64,	32,	0xffffffff000003ffLL,
	43,	64,	32,	0xffffffff000007ffLL,
	44,	64,	32,	0xffffffff00000fffLL,
	45,	64,	32,	0xffffffff00001fffLL,
	46,	64,	32,	0xffffffff00003fffLL,
	47,	64,	32,	0xffffffff00007fffLL,
	48,	64,	32,	0xffffffff0000ffffLL,
	49,	64,	32,	0xffffffff0001ffffLL,
	50,	64,	32,	0xffffffff0003ffffLL,
	51,	64,	32,	0xffffffff0007ffffLL,
	52,	64,	32,	0xffffffff000fffffLL,
	53,	64,	32,	0xffffffff001fffffLL,
	54,	64,	32,	0xffffffff003fffffLL,
	55,	64,	32,	0xffffffff007fffffLL,
	56,	64,	32,	0xffffffff00ffffffLL,
	57,	64,	32,	0xffffffff01ffffffLL,
	58,	64,	32,	0xffffffff03ffffffLL,
	59,	64,	32,	0xffffffff07ffffffLL,
	60,	64,	32,	0xffffffff0fffffffLL,
	61,	64,	32,	0xffffffff1fffffffLL,
	62,	64,	32,	0xffffffff3fffffffLL,
	63,	64,	32,	0xffffffff7fffffffLL,
	33,	64,	33,	0xffffffff80000000LL,
	34,	64,	33,	0xffffffff80000001LL,
	35,	64,	33,	0xffffffff80000003LL,
	36,	64,	33,	0xffffffff80000007LL,
	37,	64,	33,	0xffffffff8000000fLL,
	38,	64,	33,	0xffffffff8000001fLL,
	39,	64,	33,	0xffffffff8000003fLL,
	40,	64,	33,	0xffffffff8000007fLL,
	41,	64,	33,	0xffffffff800000ffLL,
	42,	64,	33,	0xffffffff800001ffLL,
	43,	64,	33,	0xffffffff800003ffLL,
	44,	64,	33,	0xffffffff800007ffLL,
	45,	64,	33,	0xffffffff80000fffLL,
	46,	64,	33,	0xffffffff80001fffLL,
	47,	64,	33,	0xffffffff80003fffLL,
	48,	64,	33,	0xffffffff80007fffLL,
	49,	64,	33,	0xffffffff8000ffffLL,
	50,	64,	33,	0xffffffff8001ffffLL,
	51,	64,	33,	0xffffffff8003ffffLL,
	52,	64,	33,	0xffffffff8007ffffLL,
	53,	64,	33,	0xffffffff800fffffLL,
	54,	64,	33,	0xffffffff801fffffLL,
	55,	64,	33,	0xffffffff803fffffLL,
	56,	64,	33,	0xffffffff807fffffLL,
	57,	64,	33,	0xffffffff80ffffffLL,
	58,	64,	33,	0xffffffff81ffffffLL,
	59,	64,	33,	0xffffffff83ffffffLL,
	60,	64,	33,	0xffffffff87ffffffLL,
	61,	64,	33,	0xffffffff8fffffffLL,
	62,	64,	33,	0xffffffff9fffffffLL,
	63,	64,	33,	0xffffffffbfffffffLL,
	34,	64,	34,	0xffffffffc0000000LL,
	35,	64,	34,	0xffffffffc0000001LL,
	36,	64,	34,	0xffffffffc0000003LL,
	37,	64,	34,	0xffffffffc0000007LL,
	38,	64,	34,	0xffffffffc000000fLL,
	39,	64,	34,	0xffffffffc000001fLL,
	40,	64,	34,	0xffffffffc000003fLL,
	41,	64,	34,	0xffffffffc000007fLL,
	42,	64,	34,	0xffffffffc00000ffLL,
	43,	64,	34,	0xffffffffc00001ffLL,
	44,	64,	34,	0xffffffffc00003ffLL,
	45,	64,	34,	0xffffffffc00007ffLL,
	46,	64,	34,	0xffffffffc0000fffLL,
	47,	64,	34,	0xffffffffc0001fffLL,
	48,	64,	34,	0xffffffffc0003fffLL,
	49,	64,	34,	0xffffffffc0007fffLL,
	50,	64,	34,	0xffffffffc000ffffLL,
	51,	64,	34,	0xffffffffc001ffffLL,
	52,	64,	34,	0xffffffffc003ffffLL,
	53,	64,	34,	0xffffffffc007ffffLL,
	54,	64,	34,	0xffffffffc00fffffLL,
	55,	64,	34,	0xffffffffc01fffffLL,
	56,	64,	34,	0xffffffffc03fffffLL,
	57,	64,	34,	0xffffffffc07fffffLL,
	58,	64,	34,	0xffffffffc0ffffffLL,
	59,	64,	34,	0xffffffffc1ffffffLL,
	60,	64,	34,	0xffffffffc3ffffffLL,
	61,	64,	34,	0xffffffffc7ffffffLL,
	62,	64,	34,	0xffffffffcfffffffLL,
	63,	64,	34,	0xffffffffdfffffffLL,
	35,	64,	35,	0xffffffffe0000000LL,
	36,	64,	35,	0xffffffffe0000001LL,
	37,	64,	35,	0xffffffffe0000003LL,
	38,	64,	35,	0xffffffffe0000007LL,
	39,	64,	35,	0xffffffffe000000fLL,
	40,	64,	35,	0xffffffffe000001fLL,
	41,	64,	35,	0xffffffffe000003fLL,
	42,	64,	35,	0xffffffffe000007fLL,
	43,	64,	35,	0xffffffffe00000ffLL,
	44,	64,	35,	0xffffffffe00001ffLL,
	45,	64,	35,	0xffffffffe00003ffLL,
	46,	64,	35,	0xffffffffe00007ffLL,
	47,	64,	35,	0xffffffffe0000fffLL,
	48,	64,	35,	0xffffffffe0001fffLL,
	49,	64,	35,	0xffffffffe0003fffLL,
	50,	64,	35,	0xffffffffe0007fffLL,
	51,	64,	35,	0xffffffffe000ffffLL,
	52,	64,	35,	0xffffffffe001ffffLL,
	53,	64,	35,	0xffffffffe003ffffLL,
	54,	64,	35,	0xffffffffe007ffffLL,
	55,	64,	35,	0xffffffffe00fffffLL,
	56,	64,	35,	0xffffffffe01fffffLL,
	57,	64,	35,	0xffffffffe03fffffLL,
	58,	64,	35,	0xffffffffe07fffffLL,
	59,	64,	35,	0xffffffffe0ffffffLL,
	60,	64,	35,	0xffffffffe1ffffffLL,
	61,	64,	35,	0xffffffffe3ffffffLL,
	62,	64,	35,	0xffffffffe7ffffffLL,
	63,	64,	35,	0xffffffffefffffffLL,
	36,	64,	36,	0xfffffffff0000000LL,
	37,	64,	36,	0xfffffffff0000001LL,
	38,	64,	36,	0xfffffffff0000003LL,
	39,	64,	36,	0xfffffffff0000007LL,
	40,	64,	36,	0xfffffffff000000fLL,
	41,	64,	36,	0xfffffffff000001fLL,
	42,	64,	36,	0xfffffffff000003fLL,
	43,	64,	36,	0xfffffffff000007fLL,
	44,	64,	36,	0xfffffffff00000ffLL,
	45,	64,	36,	0xfffffffff00001ffLL,
	46,	64,	36,	0xfffffffff00003ffLL,
	47,	64,	36,	0xfffffffff00007ffLL,
	48,	64,	36,	0xfffffffff0000fffLL,
	49,	64,	36,	0xfffffffff0001fffLL,
	50,	64,	36,	0xfffffffff0003fffLL,
	51,	64,	36,	0xfffffffff0007fffLL,
	52,	64,	36,	0xfffffffff000ffffLL,
	53,	64,	36,	0xfffffffff001ffffLL,
	54,	64,	36,	0xfffffffff003ffffLL,
	55,	64,	36,	0xfffffffff007ffffLL,
	56,	64,	36,	0xfffffffff00fffffLL,
	57,	64,	36,	0xfffffffff01fffffLL,
	58,	64,	36,	0xfffffffff03fffffLL,
	59,	64,	36,	0xfffffffff07fffffLL,
	60,	64,	36,	0xfffffffff0ffffffLL,
	61,	64,	36,	0xfffffffff1ffffffLL,
	62,	64,	36,	0xfffffffff3ffffffLL,
	63,	64,	36,	0xfffffffff7ffffffLL,
	37,	64,	37,	0xfffffffff8000000LL,
	38,	64,	37,	0xfffffffff8000001LL,
	39,	64,	37,	0xfffffffff8000003LL,
	40,	64,	37,	0xfffffffff8000007LL,
	41,	64,	37,	0xfffffffff800000fLL,
	42,	64,	37,	0xfffffffff800001fLL,
	43,	64,	37,	0xfffffffff800003fLL,
	44,	64,	37,	0xfffffffff800007fLL,
	45,	64,	37,	0xfffffffff80000ffLL,
	46,	64,	37,	0xfffffffff80001ffLL,
	47,	64,	37,	0xfffffffff80003ffLL,
	48,	64,	37,	0xfffffffff80007ffLL,
	49,	64,	37,	0xfffffffff8000fffLL,
	50,	64,	37,	0xfffffffff8001fffLL,
	51,	64,	37,	0xfffffffff8003fffLL,
	52,	64,	37,	0xfffffffff8007fffLL,
	53,	64,	37,	0xfffffffff800ffffLL,
	54,	64,	37,	0xfffffffff801ffffLL,
	55,	64,	37,	0xfffffffff803ffffLL,
	56,	64,	37,	0xfffffffff807ffffLL,
	57,	64,	37,	0xfffffffff80fffffLL,
	58,	64,	37,	0xfffffffff81fffffLL,
	59,	64,	37,	0xfffffffff83fffffLL,
	60,	64,	37,	0xfffffffff87fffffLL,
	61,	64,	37,	0xfffffffff8ffffffLL,
	62,	64,	37,	0xfffffffff9ffffffLL,
	63,	64,	37,	0xfffffffffbffffffLL,
	38,	64,	38,	0xfffffffffc000000LL,
	39,	64,	38,	0xfffffffffc000001LL,
	40,	64,	38,	0xfffffffffc000003LL,
	41,	64,	38,	0xfffffffffc000007LL,
	42,	64,	38,	0xfffffffffc00000fLL,
	43,	64,	38,	0xfffffffffc00001fLL,
	44,	64,	38,	0xfffffffffc00003fLL,
	45,	64,	38,	0xfffffffffc00007fLL,
	46,	64,	38,	0xfffffffffc0000ffLL,
	47,	64,	38,	0xfffffffffc0001ffLL,
	48,	64,	38,	0xfffffffffc0003ffLL,
	49,	64,	38,	0xfffffffffc0007ffLL,
	50,	64,	38,	0xfffffffffc000fffLL,
	51,	64,	38,	0xfffffffffc001fffLL,
	52,	64,	38,	0xfffffffffc003fffLL,
	53,	64,	38,	0xfffffffffc007fffLL,
	54,	64,	38,	0xfffffffffc00ffffLL,
	55,	64,	38,	0xfffffffffc01ffffLL,
	56,	64,	38,	0xfffffffffc03ffffLL,
	57,	64,	38,	0xfffffffffc07ffffLL,
	58,	64,	38,	0xfffffffffc0fffffLL,
	59,	64,	38,	0xfffffffffc1fffffLL,
	60,	64,	38,	0xfffffffffc3fffffLL,
	61,	64,	38,	0xfffffffffc7fffffLL,
	62,	64,	38,	0xfffffffffcffffffLL,
	63,	64,	38,	0xfffffffffdffffffLL,
	39,	64,	39,	0xfffffffffe000000LL,
	40,	64,	39,	0xfffffffffe000001LL,
	41,	64,	39,	0xfffffffffe000003LL,
	42,	64,	39,	0xfffffffffe000007LL,
	43,	64,	39,	0xfffffffffe00000fLL,
	44,	64,	39,	0xfffffffffe00001fLL,
	45,	64,	39,	0xfffffffffe00003fLL,
	46,	64,	39,	0xfffffffffe00007fLL,
	47,	64,	39,	0xfffffffffe0000ffLL,
	48,	64,	39,	0xfffffffffe0001ffLL,
	49,	64,	39,	0xfffffffffe0003ffLL,
	50,	64,	39,	0xfffffffffe0007ffLL,
	51,	64,	39,	0xfffffffffe000fffLL,
	52,	64,	39,	0xfffffffffe001fffLL,
	53,	64,	39,	0xfffffffffe003fffLL,
	54,	64,	39,	0xfffffffffe007fffLL,
	55,	64,	39,	0xfffffffffe00ffffLL,
	56,	64,	39,	0xfffffffffe01ffffLL,
	57,	64,	39,	0xfffffffffe03ffffLL,
	58,	64,	39,	0xfffffffffe07ffffLL,
	59,	64,	39,	0xfffffffffe0fffffLL,
	60,	64,	39,	0xfffffffffe1fffffLL,
	61,	64,	39,	0xfffffffffe3fffffLL,
	62,	64,	39,	0xfffffffffe7fffffLL,
	63,	64,	39,	0xfffffffffeffffffLL,
	40,	64,	40,	0xffffffffff000000LL,
	41,	64,	40,	0xffffffffff000001LL,
	42,	64,	40,	0xffffffffff000003LL,
	43,	64,	40,	0xffffffffff000007LL,
	44,	64,	40,	0xffffffffff00000fLL,
	45,	64,	40,	0xffffffffff00001fLL,
	46,	64,	40,	0xffffffffff00003fLL,
	47,	64,	40,	0xffffffffff00007fLL,
	48,	64,	40,	0xffffffffff0000ffLL,
	49,	64,	40,	0xffffffffff0001ffLL,
	50,	64,	40,	0xffffffffff0003ffLL,
	51,	64,	40,	0xffffffffff0007ffLL,
	52,	64,	40,	0xffffffffff000fffLL,
	53,	64,	40,	0xffffffffff001fffLL,
	54,	64,	40,	0xffffffffff003fffLL,
	55,	64,	40,	0xffffffffff007fffLL,
	56,	64,	40,	0xffffffffff00ffffLL,
	57,	64,	40,	0xffffffffff01ffffLL,
	58,	64,	40,	0xffffffffff03ffffLL,
	59,	64,	40,	0xffffffffff07ffffLL,
	60,	64,	40,	0xffffffffff0fffffLL,
	61,	64,	40,	0xffffffffff1fffffLL,
	62,	64,	40,	0xffffffffff3fffffLL,
	63,	64,	40,	0xffffffffff7fffffLL,
	41,	64,	41,	0xffffffffff800000LL,
	42,	64,	41,	0xffffffffff800001LL,
	43,	64,	41,	0xffffffffff800003LL,
	44,	64,	41,	0xffffffffff800007LL,
	45,	64,	41,	0xffffffffff80000fLL,
	46,	64,	41,	0xffffffffff80001fLL,
	47,	64,	41,	0xffffffffff80003fLL,
	48,	64,	41,	0xffffffffff80007fLL,
	49,	64,	41,	0xffffffffff8000ffLL,
	50,	64,	41,	0xffffffffff8001ffLL,
	51,	64,	41,	0xffffffffff8003ffLL,
	52,	64,	41,	0xffffffffff8007ffLL,
	53,	64,	41,	0xffffffffff800fffLL,
	54,	64,	41,	0xffffffffff801fffLL,
	55,	64,	41,	0xffffffffff803fffLL,
	56,	64,	41,	0xffffffffff807fffLL,
	57,	64,	41,	0xffffffffff80ffffLL,
	58,	64,	41,	0xffffffffff81ffffLL,
	59,	64,	41,	0xffffffffff83ffffLL,
	60,	64,	41,	0xffffffffff87ffffLL,
	61,	64,	41,	0xffffffffff8fffffLL,
	62,	64,	41,	0xffffffffff9fffffLL,
	63,	64,	41,	0xffffffffffbfffffLL,
	42,	64,	42,	0xffffffffffc00000LL,
	43,	64,	42,	0xffffffffffc00001LL,
	44,	64,	42,	0xffffffffffc00003LL,
	45,	64,	42,	0xffffffffffc00007LL,
	46,	64,	42,	0xffffffffffc0000fLL,
	47,	64,	42,	0xffffffffffc0001fLL,
	48,	64,	42,	0xffffffffffc0003fLL,
	49,	64,	42,	0xffffffffffc0007fLL,
	50,	64,	42,	0xffffffffffc000ffLL,
	51,	64,	42,	0xffffffffffc001ffLL,
	52,	64,	42,	0xffffffffffc003ffLL,
	53,	64,	42,	0xffffffffffc007ffLL,
	54,	64,	42,	0xffffffffffc00fffLL,
	55,	64,	42,	0xffffffffffc01fffLL,
	56,	64,	42,	0xffffffffffc03fffLL,
	57,	64,	42,	0xffffffffffc07fffLL,
	58,	64,	42,	0xffffffffffc0ffffLL,
	59,	64,	42,	0xffffffffffc1ffffLL,
	60,	64,	42,	0xffffffffffc3ffffLL,
	61,	64,	42,	0xffffffffffc7ffffLL,
	62,	64,	42,	0xffffffffffcfffffLL,
	63,	64,	42,	0xffffffffffdfffffLL,
	43,	64,	43,	0xffffffffffe00000LL,
	44,	64,	43,	0xffffffffffe00001LL,
	45,	64,	43,	0xffffffffffe00003LL,
	46,	64,	43,	0xffffffffffe00007LL,
	47,	64,	43,	0xffffffffffe0000fLL,
	48,	64,	43,	0xffffffffffe0001fLL,
	49,	64,	43,	0xffffffffffe0003fLL,
	50,	64,	43,	0xffffffffffe0007fLL,
	51,	64,	43,	0xffffffffffe000ffLL,
	52,	64,	43,	0xffffffffffe001ffLL,
	53,	64,	43,	0xffffffffffe003ffLL,
	54,	64,	43,	0xffffffffffe007ffLL,
	55,	64,	43,	0xffffffffffe00fffLL,
	56,	64,	43,	0xffffffffffe01fffLL,
	57,	64,	43,	0xffffffffffe03fffLL,
	58,	64,	43,	0xffffffffffe07fffLL,
	59,	64,	43,	0xffffffffffe0ffffLL,
	60,	64,	43,	0xffffffffffe1ffffLL,
	61,	64,	43,	0xffffffffffe3ffffLL,
	62,	64,	43,	0xffffffffffe7ffffLL,
	63,	64,	43,	0xffffffffffefffffLL,
	44,	64,	44,	0xfffffffffff00000LL,
	45,	64,	44,	0xfffffffffff00001LL,
	46,	64,	44,	0xfffffffffff00003LL,
	47,	64,	44,	0xfffffffffff00007LL,
	48,	64,	44,	0xfffffffffff0000fLL,
	49,	64,	44,	0xfffffffffff0001fLL,
	50,	64,	44,	0xfffffffffff0003fLL,
	51,	64,	44,	0xfffffffffff0007fLL,
	52,	64,	44,	0xfffffffffff000ffLL,
	53,	64,	44,	0xfffffffffff001ffLL,
	54,	64,	44,	0xfffffffffff003ffLL,
	55,	64,	44,	0xfffffffffff007ffLL,
	56,	64,	44,	0xfffffffffff00fffLL,
	57,	64,	44,	0xfffffffffff01fffLL,
	58,	64,	44,	0xfffffffffff03fffLL,
	59,	64,	44,	0xfffffffffff07fffLL,
	60,	64,	44,	0xfffffffffff0ffffLL,
	61,	64,	44,	0xfffffffffff1ffffLL,
	62,	64,	44,	0xfffffffffff3ffffLL,
	63,	64,	44,	0xfffffffffff7ffffLL,
	45,	64,	45,	0xfffffffffff80000LL,
	46,	64,	45,	0xfffffffffff80001LL,
	47,	64,	45,	0xfffffffffff80003LL,
	48,	64,	45,	0xfffffffffff80007LL,
	49,	64,	45,	0xfffffffffff8000fLL,
	50,	64,	45,	0xfffffffffff8001fLL,
	51,	64,	45,	0xfffffffffff8003fLL,
	52,	64,	45,	0xfffffffffff8007fLL,
	53,	64,	45,	0xfffffffffff800ffLL,
	54,	64,	45,	0xfffffffffff801ffLL,
	55,	64,	45,	0xfffffffffff803ffLL,
	56,	64,	45,	0xfffffffffff807ffLL,
	57,	64,	45,	0xfffffffffff80fffLL,
	58,	64,	45,	0xfffffffffff81fffLL,
	59,	64,	45,	0xfffffffffff83fffLL,
	60,	64,	45,	0xfffffffffff87fffLL,
	61,	64,	45,	0xfffffffffff8ffffLL,
	62,	64,	45,	0xfffffffffff9ffffLL,
	63,	64,	45,	0xfffffffffffbffffLL,
	46,	64,	46,	0xfffffffffffc0000LL,
	47,	64,	46,	0xfffffffffffc0001LL,
	48,	64,	46,	0xfffffffffffc0003LL,
	49,	64,	46,	0xfffffffffffc0007LL,
	50,	64,	46,	0xfffffffffffc000fLL,
	51,	64,	46,	0xfffffffffffc001fLL,
	52,	64,	46,	0xfffffffffffc003fLL,
	53,	64,	46,	0xfffffffffffc007fLL,
	54,	64,	46,	0xfffffffffffc00ffLL,
	55,	64,	46,	0xfffffffffffc01ffLL,
	56,	64,	46,	0xfffffffffffc03ffLL,
	57,	64,	46,	0xfffffffffffc07ffLL,
	58,	64,	46,	0xfffffffffffc0fffLL,
	59,	64,	46,	0xfffffffffffc1fffLL,
	60,	64,	46,	0xfffffffffffc3fffLL,
	61,	64,	46,	0xfffffffffffc7fffLL,
	62,	64,	46,	0xfffffffffffcffffLL,
	63,	64,	46,	0xfffffffffffdffffLL,
	47,	64,	47,	0xfffffffffffe0000LL,
	48,	64,	47,	0xfffffffffffe0001LL,
	49,	64,	47,	0xfffffffffffe0003LL,
	50,	64,	47,	0xfffffffffffe0007LL,
	51,	64,	47,	0xfffffffffffe000fLL,
	52,	64,	47,	0xfffffffffffe001fLL,
	53,	64,	47,	0xfffffffffffe003fLL,
	54,	64,	47,	0xfffffffffffe007fLL,
	55,	64,	47,	0xfffffffffffe00ffLL,
	56,	64,	47,	0xfffffffffffe01ffLL,
	57,	64,	47,	0xfffffffffffe03ffLL,
	58,	64,	47,	0xfffffffffffe07ffLL,
	59,	64,	47,	0xfffffffffffe0fffLL,
	60,	64,	47,	0xfffffffffffe1fffLL,
	61,	64,	47,	0xfffffffffffe3fffLL,
	62,	64,	47,	0xfffffffffffe7fffLL,
	63,	64,	47,	0xfffffffffffeffffLL,
	48,	64,	48,	0xffffffffffff0000LL,
	49,	64,	48,	0xffffffffffff0001LL,
	50,	64,	48,	0xffffffffffff0003LL,
	51,	64,	48,	0xffffffffffff0007LL,
	52,	64,	48,	0xffffffffffff000fLL,
	53,	64,	48,	0xffffffffffff001fLL,
	54,	64,	48,	0xffffffffffff003fLL,
	55,	64,	48,	0xffffffffffff007fLL,
	56,	64,	48,	0xffffffffffff00ffLL,
	57,	64,	48,	0xffffffffffff01ffLL,
	58,	64,	48,	0xffffffffffff03ffLL,
	59,	64,	48,	0xffffffffffff07ffLL,
	60,	64,	48,	0xffffffffffff0fffLL,
	61,	64,	48,	0xffffffffffff1fffLL,
	62,	64,	48,	0xffffffffffff3fffLL,
	63,	64,	48,	0xffffffffffff7fffLL,
	49,	64,	49,	0xffffffffffff8000LL,
	50,	64,	49,	0xffffffffffff8001LL,
	51,	64,	49,	0xffffffffffff8003LL,
	52,	64,	49,	0xffffffffffff8007LL,
	53,	64,	49,	0xffffffffffff800fLL,
	54,	64,	49,	0xffffffffffff801fLL,
	55,	64,	49,	0xffffffffffff803fLL,
	56,	64,	49,	0xffffffffffff807fLL,
	57,	64,	49,	0xffffffffffff80ffLL,
	58,	64,	49,	0xffffffffffff81ffLL,
	59,	64,	49,	0xffffffffffff83ffLL,
	60,	64,	49,	0xffffffffffff87ffLL,
	61,	64,	49,	0xffffffffffff8fffLL,
	62,	64,	49,	0xffffffffffff9fffLL,
	63,	64,	49,	0xffffffffffffbfffLL,
	50,	64,	50,	0xffffffffffffc000LL,
	51,	64,	50,	0xffffffffffffc001LL,
	52,	64,	50,	0xffffffffffffc003LL,
	53,	64,	50,	0xffffffffffffc007LL,
	54,	64,	50,	0xffffffffffffc00fLL,
	55,	64,	50,	0xffffffffffffc01fLL,
	56,	64,	50,	0xffffffffffffc03fLL,
	57,	64,	50,	0xffffffffffffc07fLL,
	58,	64,	50,	0xffffffffffffc0ffLL,
	59,	64,	50,	0xffffffffffffc1ffLL,
	60,	64,	50,	0xffffffffffffc3ffLL,
	61,	64,	50,	0xffffffffffffc7ffLL,
	62,	64,	50,	0xffffffffffffcfffLL,
	63,	64,	50,	0xffffffffffffdfffLL,
	51,	64,	51,	0xffffffffffffe000LL,
	52,	64,	51,	0xffffffffffffe001LL,
	53,	64,	51,	0xffffffffffffe003LL,
	54,	64,	51,	0xffffffffffffe007LL,
	55,	64,	51,	0xffffffffffffe00fLL,
	56,	64,	51,	0xffffffffffffe01fLL,
	57,	64,	51,	0xffffffffffffe03fLL,
	58,	64,	51,	0xffffffffffffe07fLL,
	59,	64,	51,	0xffffffffffffe0ffLL,
	60,	64,	51,	0xffffffffffffe1ffLL,
	61,	64,	51,	0xffffffffffffe3ffLL,
	62,	64,	51,	0xffffffffffffe7ffLL,
	63,	64,	51,	0xffffffffffffefffLL,
	52,	64,	52,	0xfffffffffffff000LL,
	53,	64,	52,	0xfffffffffffff001LL,
	54,	64,	52,	0xfffffffffffff003LL,
	55,	64,	52,	0xfffffffffffff007LL,
	56,	64,	52,	0xfffffffffffff00fLL,
	57,	64,	52,	0xfffffffffffff01fLL,
	58,	64,	52,	0xfffffffffffff03fLL,
	59,	64,	52,	0xfffffffffffff07fLL,
	60,	64,	52,	0xfffffffffffff0ffLL,
	61,	64,	52,	0xfffffffffffff1ffLL,
	62,	64,	52,	0xfffffffffffff3ffLL,
	63,	64,	52,	0xfffffffffffff7ffLL,
	53,	64,	53,	0xfffffffffffff800LL,
	54,	64,	53,	0xfffffffffffff801LL,
	55,	64,	53,	0xfffffffffffff803LL,
	56,	64,	53,	0xfffffffffffff807LL,
	57,	64,	53,	0xfffffffffffff80fLL,
	58,	64,	53,	0xfffffffffffff81fLL,
	59,	64,	53,	0xfffffffffffff83fLL,
	60,	64,	53,	0xfffffffffffff87fLL,
	61,	64,	53,	0xfffffffffffff8ffLL,
	62,	64,	53,	0xfffffffffffff9ffLL,
	63,	64,	53,	0xfffffffffffffbffLL,
	54,	64,	54,	0xfffffffffffffc00LL,
	55,	64,	54,	0xfffffffffffffc01LL,
	56,	64,	54,	0xfffffffffffffc03LL,
	57,	64,	54,	0xfffffffffffffc07LL,
	58,	64,	54,	0xfffffffffffffc0fLL,
	59,	64,	54,	0xfffffffffffffc1fLL,
	60,	64,	54,	0xfffffffffffffc3fLL,
	61,	64,	54,	0xfffffffffffffc7fLL,
	62,	64,	54,	0xfffffffffffffcffLL,
	63,	64,	54,	0xfffffffffffffdffLL,
	55,	64,	55,	0xfffffffffffffe00LL,
	56,	64,	55,	0xfffffffffffffe01LL,
	57,	64,	55,	0xfffffffffffffe03LL,
	58,	64,	55,	0xfffffffffffffe07LL,
	59,	64,	55,	0xfffffffffffffe0fLL,
	60,	64,	55,	0xfffffffffffffe1fLL,
	61,	64,	55,	0xfffffffffffffe3fLL,
	62,	64,	55,	0xfffffffffffffe7fLL,
	63,	64,	55,	0xfffffffffffffeffLL,
	56,	64,	56,	0xffffffffffffff00LL,
	57,	64,	56,	0xffffffffffffff01LL,
	58,	64,	56,	0xffffffffffffff03LL,
	59,	64,	56,	0xffffffffffffff07LL,
	60,	64,	56,	0xffffffffffffff0fLL,
	61,	64,	56,	0xffffffffffffff1fLL,
	62,	64,	56,	0xffffffffffffff3fLL,
	63,	64,	56,	0xffffffffffffff7fLL,
	57,	64,	57,	0xffffffffffffff80LL,
	58,	64,	57,	0xffffffffffffff81LL,
	59,	64,	57,	0xffffffffffffff83LL,
	60,	64,	57,	0xffffffffffffff87LL,
	61,	64,	57,	0xffffffffffffff8fLL,
	62,	64,	57,	0xffffffffffffff9fLL,
	63,	64,	57,	0xffffffffffffffbfLL,
	58,	64,	58,	0xffffffffffffffc0LL,
	59,	64,	58,	0xffffffffffffffc1LL,
	60,	64,	58,	0xffffffffffffffc3LL,
	61,	64,	58,	0xffffffffffffffc7LL,
	62,	64,	58,	0xffffffffffffffcfLL,
	63,	64,	58,	0xffffffffffffffdfLL,
	59,	64,	59,	0xffffffffffffffe0LL,
	60,	64,	59,	0xffffffffffffffe1LL,
	61,	64,	59,	0xffffffffffffffe3LL,
	62,	64,	59,	0xffffffffffffffe7LL,
	63,	64,	59,	0xffffffffffffffefLL,
	60,	64,	60,	0xfffffffffffffff0LL,
	61,	64,	60,	0xfffffffffffffff1LL,
	62,	64,	60,	0xfffffffffffffff3LL,
	63,	64,	60,	0xfffffffffffffff7LL,
	61,	64,	61,	0xfffffffffffffff8LL,
	62,	64,	61,	0xfffffffffffffff9LL,
	63,	64,	61,	0xfffffffffffffffbLL,
	62,	64,	62,	0xfffffffffffffffcLL,
	63,	64,	62,	0xfffffffffffffffdLL,
	63,	64,	63,	0xfffffffffffffffeLL,
};

Mask*
findmask(uvlong v)
{
	int top, bot, mid;
	Mask *m;

	bot = 0;
	top = nelem(bitmasks);
	while(bot < top){
		mid = (bot+top)/2;
		m = &bitmasks[mid];
		if(v == m->v)
			return m;
		if(v < m->v)
			top = mid;
		else
			bot = mid+1;
	}
	return nil;
}
