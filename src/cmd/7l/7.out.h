/*
 * arm64
 */

#define	NSNAME		8
#define	NSYM		50
#define	NREG		32
#define	NFREG		32
#include "../ld/textflag.h"

/*c2go
enum
{
	NSNAME = 8,
	NSYM = 50,
	NREG = 32
};
*/

enum {
	REGENV	= 0,	/* environment for closures */
	REGTMP	= 1,	/* reserved for liblink */
	REGRT1	= 2,	/* reserved for runtime, duffzero and duffcopy */
	REGRT2	= 3,	/* reserved for runtime, duffcopy */
	REGMIN	= 7,	/* register variables allocated from here to REGMAX */
	REGMAX	= 27,
	REGSB	= 28,	/* static base register, unused in the Go toolchain */
	REGG	= 29,	/* G */
	REGLINK	= 30,
	REGSP	= 31,
	REGZERO	= 31,

	FREGRET		= 0,
	FREGMIN		= 7,	/* first register variable */
	FREGMAX		= 26,	/* last register variable for 7g only */
	FREGEXT		= 26,	/* first external register */
	FREGZERO	= 28,	/* both float and double */
	FREGHALF	= 29,	/* double */
	FREGONE		= 30,	/* double */
	FREGTWO		= 31	/* double */
/*
 * GENERAL:
 *
 * compiler allocates R4 up as temps
 * compiler allocates register variables R7-R27
 * compiler allocates external registers R29 down
 *
 * compiler allocates register variables F7-F26
 * compiler allocates external registers F26 down
 */
};

/* compiler allocates register variables F0 up */
/* compiler allocates external registers F15 down */

enum {
	BIG = 2048-8,
};

enum
{
	C_NONE,
	C_REG,
	C_RSP,		/* D_REG or D_SP */
	C_SHIFT,		/* D_SHIFT: shift type, amount, value */
	C_EXTREG,	/* D_EXTREG: reg, ext type, shift */
	C_FREG,
	C_SPR,
	C_COND,

	C_ZCON,		/* 0 (matching C_REG, not C_RSP, allowing REGZERO) */
	C_ADDCON0,	/* 12-bit unsigned, unshifted */
	C_ADDCON,	/* 12-bit unsigned, shifted left by 0 or 12 */
	C_MOVCON,	/* generated by a 16-bit constant, optionally inverted and/or shifted by multiple of 16 */
	C_BITCON,	/* bimm32 */
	C_ABCON,	/* could be C_ADDCON or C_BITCON */
	C_MBCON,	/* could be C_MOVCON or C_BITCON */
	C_LCON,		/* 32-bit constant */
	C_FCON,		/* floating-point constant */
	C_VCON,		/* 64-bit constant */

	C_AACON,	/* ADDCON offset in auto constant $a(FP) */
	C_LACON,	/* 32-bit offset in auto constant $a(FP) */

	C_AECON,	/* ADDCON offset in extern constant $e(SB) */

	C_SBRA,
	C_LBRA,

	C_NPAUTO,	/* -512 <= x < 0, 0 mod 8 */
	C_NSAUTO,	/* -256 <= x < 0 */
	C_PSAUTO,	/* 0 to 255 */
	C_PPAUTO,	/* 0 to 504, 0 mod 8 */
	C_UAUTO4K,	/* 0 to 4095 */
	C_UAUTO8K,	/* 0 to 8190, 0 mod 2 */
	C_UAUTO16K,	/* 0 to 16380, 0 mod 4 */
	C_UAUTO32K,	/* 0 to 32760, 0 mod 8 */
	C_UAUTO64K,	/* 0 to 65520, 0 mod 16 */
	C_LAUTO,		/* any other 32-bit constant */

	C_SEXT1,		/* 0 to 4095, direct */
	C_SEXT2,		/* 0 to 8190 */
	C_SEXT4,		/* 0 to 16380 */
	C_SEXT8,		/* 0 to 32760 */
	C_SEXT16,	/* 0 to 65520 */
	C_LEXT,

	C_NPOREG,	/* mirror NPAUTO etc, except for ZOREG */
	C_NSOREG,
	C_ZOREG,
	C_PSOREG,
	C_PPOREG,
	C_UOREG4K,
	C_UOREG8K,
	C_UOREG16K,
	C_UOREG32K,
	C_UOREG64K,
	C_LOREG,

	C_ADDR,		/* relocatable address for dynamic loading */
	C_ROFF,		/* register offset (inc register extended) */
	C_XPOST,
	C_XPRE,

	C_VREG,

	C_GOK,
	C_NCLASS,	/* must be last */
};

enum	as
{
	AXXX,

	AADC,
	AADCS,
	AADCSW,
	AADCW,
	AADD,
	AADDS,
	AADDSW,
	AADDW,
	AADR,
	AADRP,
	AAND,
	AANDS,
	AANDSW,
	AANDW,
	AASR,
	AASRW,
	AAT,
	AB,
	ABFI,
	ABFIW,
	ABFM,
	ABFMW,
	ABFXIL,
	ABFXILW,
	ABIC,
	ABICS,
	ABICSW,
	ABICW,
	ABL,
	ABRK,
	ACBNZ,
	ACBNZW,
	ACBZ,
	ACBZW,
	ACCMN,
	ACCMNW,
	ACCMP,
	ACCMPW,
	ACINC,
	ACINCW,
	ACINV,
	ACINVW,
	ACLREX,
	ACLS,
	ACLSW,
	ACLZ,
	ACLZW,
	ACMN,
	ACMNW,
	ACMP,
	ACMPW,
	ACNEG,
	ACNEGW,
	ACRC32B,
	ACRC32CB,
	ACRC32CH,
	ACRC32CW,
	ACRC32CX,
	ACRC32H,
	ACRC32W,
	ACRC32X,
	ACSEL,
	ACSELW,
	ACSET,
	ACSETM,
	ACSETMW,
	ACSETW,
	ACSINC,
	ACSINCW,
	ACSINV,
	ACSINVW,
	ACSNEG,
	ACSNEGW,
	ADC,
	ADCPS1,
	ADCPS2,
	ADCPS3,
	ADMB,
	ADRPS,
	ADSB,
	AEON,
	AEONW,
	AEOR,
	AEORW,
	AERET,
	AEXTR,
	AEXTRW,
	AHINT,
	AHLT,
	AHVC,
	AIC,
	AISB,
	ALDAR,
	ALDARB,
	ALDARH,
	ALDARW,
	ALDAXP,
	ALDAXPW,
	ALDAXR,
	ALDAXRB,
	ALDAXRH,
	ALDAXRW,
	ALDXR,
	ALDXRB,
	ALDXRH,
	ALDXRW,
	ALDXP,
	ALDXPW,
	ALSL,
	ALSLW,
	ALSR,
	ALSRW,
	AMADD,
	AMADDW,
	AMNEG,
	AMNEGW,
	AMOVK,
	AMOVKW,
	AMOVN,
	AMOVNW,
	AMOVZ,
	AMOVZW,
	AMRS,
	AMSR,
	AMSUB,
	AMSUBW,
	AMUL,
	AMULW,
	AMVN,
	AMVNW,
	ANEG,
	ANEGS,
	ANEGSW,
	ANEGW,
	ANGC,
	ANGCS,
	ANGCSW,
	ANGCW,
	ANOP,
	AORN,
	AORNW,
	AORR,
	AORRW,
	APRFM,
	APRFUM,
	ARBIT,
	ARBITW,
	AREM,
	AREMW,
	ARET,
	AREV,
	AREV16,
	AREV16W,
	AREV32,
	AREVW,
	AROR,
	ARORW,
	ASBC,
	ASBCS,
	ASBCSW,
	ASBCW,
	ASBFIZ,
	ASBFIZW,
	ASBFM,
	ASBFMW,
	ASBFX,
	ASBFXW,
	ASDIV,
	ASDIVW,
	ASEV,
	ASEVL,
	ASMADDL,
	ASMC,
	ASMNEGL,
	ASMSUBL,
	ASMULH,
	ASMULL,
	ASTXR,
	ASTXRB,
	ASTXRH,
	ASTXP,
	ASTXPW,
	ASTXRW,
	ASTLP,
	ASTLPW,
	ASTLR,
	ASTLRB,
	ASTLRH,
	ASTLRW,
	ASTLXP,
	ASTLXPW,
	ASTLXR,
	ASTLXRB,
	ASTLXRH,
	ASTLXRW,
	ASUB,
	ASUBS,
	ASUBSW,
	ASUBW,
	ASVC,
	ASXTB,
	ASXTBW,
	ASXTH,
	ASXTHW,
	ASXTW,
	ASYS,
	ASYSL,
	ATBNZ,
	ATBZ,
	ATLBI,
	ATST,
	ATSTW,
	AUBFIZ,
	AUBFIZW,
	AUBFM,
	AUBFMW,
	AUBFX,
	AUBFXW,
	AUDIV,
	AUDIVW,
	AUMADDL,
	AUMNEGL,
	AUMSUBL,
	AUMULH,
	AUMULL,
	AUREM,
	AUREMW,
	AUXTB,
	AUXTH,
	AUXTW,
	AUXTBW,
	AUXTHW,
	AWFE,
	AWFI,
	AYIELD,

	AMOVB,
	AMOVBU,
	AMOVH,
	AMOVHU,
	AMOVW,
	AMOVWU,
	AMOV,
	AMOVNP,
	AMOVNPW,
	AMOVP,
	AMOVPD,
	AMOVPQ,
	AMOVPS,
	AMOVPSW,
	AMOVPW,

/* 
 * Do not reorder or fragment the conditional branch 
 * opcodes, or the predication code will break 
 */ 
	ABEQ,
	ABNE,
	ABCS,
	ABHS,
	ABCC,
	ABLO,
	ABMI,
	ABPL,
	ABVS,
	ABVC,
	ABHI,
	ABLS,
	ABGE,
	ABLT,
	ABGT,
	ABLE,

	AFABSD,
	AFABSS,
	AFADDD,
	AFADDS,
	AFCCMPD,
	AFCCMPED,
	AFCCMPS,
	AFCCMPES,
	AFCMPD,
	AFCMPED,
	AFCMPES,
	AFCMPS,
	AFCVTSD,
	AFCVTDS,
	AFCVTZSD,
	AFCVTZSDW,
	AFCVTZSS,
	AFCVTZSSW,
	AFCVTZUD,
	AFCVTZUDW,
	AFCVTZUS,
	AFCVTZUSW,
	AFDIVD,
	AFDIVS,
	AFMOVD,
	AFMOVS,
	AFMULD,
	AFMULS,
	AFNEGD,
	AFNEGS,
	AFSQRTD,
	AFSQRTS,
	AFSUBD,
	AFSUBS,
	ASCVTFD,
	ASCVTFS,
	ASCVTFWD,
	ASCVTFWS,
	AUCVTFD,
	AUCVTFS,
	AUCVTFWD,
	AUCVTFWS,

	ATEXT,
	ADATA,
	AGLOBL,
	AHISTORY,
	ANAME,
	AWORD,
	ADYNT,
	AINIT,
	ABCASE,
	ACASE,
	ADWORD,
	ASIGNAME,
	AGOK,
	ARETURN,
	AEND,

	AFCSELS,
	AFCSELD,
	AFMAXS,
	AFMINS,
	AFMAXD,
	AFMIND,
	AFMAXNMS,
	AFMAXNMD,
	AFNMULS,
	AFNMULD,
	AFRINTNS,
	AFRINTND,
	AFRINTPS,
	AFRINTPD,
	AFRINTMS,
	AFRINTMD,
	AFRINTZS,
	AFRINTZD,
	AFRINTAS,
	AFRINTAD,
	AFRINTXS,
	AFRINTXD,
	AFRINTIS,
	AFRINTID,
	AFMADDS,
	AFMADDD,
	AFMSUBS,
	AFMSUBD,
	AFNMADDS,
	AFNMADDD,
	AFNMSUBS,
	AFNMSUBD,
	AFMINNMS,
	AFMINNMD,
	AFCVTDH,
	AFCVTHS,
	AFCVTHD,
	AFCVTSH,

	AAESD,
	AAESE,
	AAESIMC,
	AAESMC,
	ASHA1C,
	ASHA1H,
	ASHA1M,
	ASHA1P,
	ASHA1SU0,
	ASHA1SU1,
	ASHA256H,
	ASHA256H2,
	ASHA256SU0,
	ASHA256SU1,

	AUNDEF,
	AUSEFIELD,
	ATYPE,
	AFUNCDATA,
	APCDATA,
	ACHECKNIL,
	AVARDEF,
	AVARKILL,
	ADUFFCOPY,
	ADUFFZERO,
	
	ALAST,
};

/* form offset parameter to SYS; special register number */
#define	SYSARG5(op0,op1,Cn,Cm,op2)	((op0)<<19|(op1)<<16|(Cn)<<12|(Cm)<<8|(op2)<<5)
#define	SYSARG4(op1,Cn,Cm,op2)	SYSARG5(0,op1,Cn,Cm,op2)
/*c2go
int SYSARG5(int, int, int, int, int);
int SYSARG4(int, int, int, int);
*/

/* type/name */
enum
{
	D_GOK	= 0,
	D_NONE,

/* name */
	D_EXTERN,
	D_STATIC,
	D_AUTO,
	D_PARAM,

/* type */
	D_BRANCH,
	D_OREG,		/* offset(reg) */
	D_XPRE,		/* offset(reg)! - pre-indexed */
	D_XPOST,		/* (reg)offset! - post-indexed */
	D_CONST,	/* 32-bit constant */
	D_DCONST,	/* 64-bit constant */
	D_FCONST,	/* floating-point constant */
	D_SCONST,	/* short string constant */
	D_REG,		/* Rn = Wn or Xn depending on op */
	D_SP,		/* distinguish REGSP from REGZERO */
	D_FREG,		/* Fn = Sn or Dn depending on op */
	D_VREG,		/* Vn = SIMD register */
	D_SPR,		/* special processor register */
	D_FILE,
	D_OCONST,	/* absolute address constant (unused) */
	D_FILE1,

	D_SHIFT,		/* Rm{, ashift #imm} */
	D_PAIR,		/* pair of gprs */
	D_ADDR,		/* address constant (dynamic loading) */
	D_ADRP,		/* pc-relative addressing, page */
	D_ADRLO,	/* low-order 12 bits of external reference */
	D_EXTREG,	/* Rm{, ext #imm} */
	D_ROFF,		/* register offset Rn+ext(Rm)<<s */
	D_COND,		/* condition EQ, NE, etc */
	D_VLANE,		/* Vn lane */
	D_VSET,		/* set of Vn */

	D_LAST,
	
/* reg names for 7g OREGISTER */
	D_R0 = 0, // type is D_REG
	D_F0 = D_R0+NREG, // type is D_FREG

	/* offset iff type is D_SPR */
	D_DAIF	= SYSARG5(3,3,4,2,1),
	D_NZCV	= SYSARG5(3,3,4,2,0),
	D_FPSR	= SYSARG5(3,3,4,4,1),
	D_FPCR	= SYSARG5(3,3,4,4,0),
	D_SPSR_EL1 = SYSARG5(3,0,4,0,0),
	D_ELR_EL1 = SYSARG5(3,0,4,0,1),
	D_SPSR_EL2 = SYSARG5(3,4,4,0,0),
	D_ELR_EL2 = SYSARG5(3,4,4,0,1),
//	D_SPSR_EL3 = SYSARG5(3,x,4,x,x),
//	D_ELR_EL3 = SYSARG5(3,x,4,x,x),
//	D_LR_EL0 = SYSARG5(3,x,4,x,x),
	D_CurrentEL = SYSARG5(3,0,4,2,2),
	D_SP_EL0 = SYSARG5(3,0,4,1,0),
//	D_SP_EL1 = SYSARG5(3,x,4,x,x),
//	D_SP_EL2 = SYSARG5(3,x,4,x,x),
	D_SPSel	= SYSARG5(3,0,4,2,0),
//	D_SPSR_abt  = SYSARG5(3,x,4,x,x),
//	D_SPSR_fiq = SYSARG5(3,x,4,x,x),
//	D_SPSR_ieq = SYSARG5(3,x,4,x,x),
//	D_SPSR_und = SYSARG5(3,x,4,x,x),
	D_DAIFSet = (1<<30)|0,
	D_DAIFClr = (1<<30)|1
};

/*
 * this is the ranlib header
 */
#define	SYMDEF	"__.GOSYMDEF"
/*c2go extern char SYMDEF[]; */
