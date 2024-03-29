/*
 * arm64
 */
#include <bio.h>
#include <link.h>
#include "../7l/7.out.h"

#ifndef	EXTERN
#define	EXTERN	extern
#endif

#undef	getc
#undef	ungetc
#undef	BUFSIZ

#define	getc	ccgetc
#define	ungetc	ccungetc

typedef	struct	Sym	Sym;
typedef	struct	Io	Io;

#define	MAXALIGN	7
#define	FPCHIP		1
#define	NSYMB		8192
#define	BUFSIZ		8192
#define	HISTSZ		20
#define	NINCLUDE	10
#define	NHUNK		10000
#define	EOF		(-1)
#define	IGN		(-2)
#define	GETC()		((--fi.c < 0)? filbuf(): *fi.p++ & 0xff)
#define	NHASH		503
#define	STRINGSZ	200
#define	NMACRO		10

struct	Sym
{
	Sym*	link;
	char*	macro;
	int32	value;
	ushort	type;
	char	*name;
	char*	labelname;
	char	sym;
};
#define	S	((Sym*)0)

EXTERN	struct
{
	char*	p;
	int	c;
} fi;

struct	Io
{
	Io*	link;
	char	b[BUFSIZ];
	char*	p;
	short	c;
	short	f;
};
#define	I	((Io*)0)

enum
{
	CLAST,
	CMACARG,
	CMACRO,
	CPREPROC,
};

EXTERN	int	debug[256];
EXTERN	Sym*	hash[NHASH];
EXTERN	char**	Dlist;
EXTERN	int	nDlist;
EXTERN	int	newflag;
EXTERN	char*	hunk;
EXTERN	char**	include;
EXTERN	Io*	iofree;
EXTERN	Io*	ionext;
EXTERN	Io*	iostack;
EXTERN	int32	lineno;
EXTERN	int	nerrors;
EXTERN	int32	nhunk;
EXTERN	int	ninclude;
EXTERN	int32	nsymb;
EXTERN	Addr	nullgen;
EXTERN	char*	outfile;
EXTERN	int	pass;
EXTERN	int32	pc;
EXTERN	int	peekc;
EXTERN	int	sym;
EXTERN	char*	symb;
EXTERN	int	thechar;
EXTERN	char*	thestring;
EXTERN	int32	thunk;
EXTERN	Biobuf	obuf;
EXTERN	Link*	ctxt;
EXTERN	Biobuf	bstdout;

void*	alloc(int32);
void*	allocn(void*, int32, int32);
void	ensuresymb(int32);
void	errorexit(void);
void	pushio(void);
void	newio(void);
void	newfile(char*, int);
Sym*	slookup(char*);
Sym*	lookup(void);
Sym*	labellookup(Sym*);
void	settext(LSym*);
void	syminit(Sym*);
int32	yylex(void);
int	getc(void);
int	getnsc(void);
void	unget(int);
int	escchar(int);
void	cinit(void);
void	pinit(char*);
void	cclean(void);
void	outcode(int, Addr*, int, Addr*);
void	outgcode(int, Addr*, int, Addr*, Addr*);
int	filbuf(void);
Sym*	getsym(void);
void	domacro(void);
void	macund(void);
void	macdef(void);
void	macexpand(Sym*, char*);
void	macinc(void);
void	maclin(void);
void	macprag(void);
void	macif(int);
void	macend(void);
void	dodefine(char*);
void	prfile(int32);
void	linehist(char*, int);
void	gethunk(void);
void	yyerror(char*, ...);
int	yyparse(void);
void	setinclude(char*);
int	assemble(char*);
