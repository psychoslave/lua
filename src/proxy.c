/*
* proxy.c
* lexer proxy for Lua parser -- allows aliases
* Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
* Sun Nov 13 09:24:13 BRST 2016
* This code is hereby placed in the public domain.
* Add <<#include "proxy.c">> just before the definition of luaX_next in llex.c
*/

#define TK_ADD		'+'
#define TK_BAND		'&'
#define TK_BNOT		'~'
#define TK_BOR		'|'
#define TK_BXOR		'^'
#define TK_DIV		'/'
#define TK_GT		'>'
#define TK_LT		'<'
#define TK_MINUS	'-'
#define TK_MOD		'%'
#define TK_POW		'^'
#define TK_SUB		'-'

static const struct {
    const char *name;
    int token;
} aliases[] = {
    { "nee",     TK_BNOT },
    { "disauxe", TK_BXOR },
    { "superas", TK_GT },
    { "malinfraas", TK_GT },
    { "suras", TK_GE },
    { "almenauxas", TK_GE },
    { "malsubas", TK_GE },
    { "egalas",  TK_EQ },
    { "samas",   TK_EQ },
    { "malsamas",TK_NE },
    { "neegalas",TK_NE },
    { "infraas", TK_LT },
    { "malsuperas", TK_LT },
    { "subas", TK_LE },
    { "malsuras", TK_LE },
    { "malalmenauxas", TK_LE },
    { "kaje", TK_BAND },
    { "auxe", TK_BOR },
    { "sobsxove", TK_SHR },
    { "sorsxove", TK_SHL },
    { "plus", TK_ADD },
    { "mal", TK_MINUS },
    { "kontraux", TK_MINUS },
    { "minus", TK_SUB },
    { "disige", TK_DIV },
    { "divide", TK_DIV },
    { "ozle", TK_DIV },
    { "onige", TK_IDIV },
    { "parte", TK_IDIV },
    { "pece", TK_IDIV },
    { "kvociente", TK_IDIV },
    { "module", TK_MOD },
    { "kongrue", TK_MOD },
    { "alt", TK_POW },
    { "potencige", TK_POW },
};

static int nexttoken(LexState *ls, SemInfo *seminfo)
{
	int t=llex(ls,seminfo);
	if (t==TK_NAME && strcmp(getstr(seminfo->ts),"sia")==0) {
		seminfo->ts = luaS_new(ls->L,"self");
		return t;
	}
	if (t==TK_NAME) {
		int i;
		int n = sizeof(aliases)/sizeof(aliases[0]);
		for (i=0; i<n; i++) {
			if (strcmp(getstr(seminfo->ts),aliases[i].name)==0)
				return aliases[i].token;
		}
	}
	return t;
}

#define llex nexttoken
