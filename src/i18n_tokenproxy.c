/*
* proxy.c
* lexer proxy for Lua parser -- allows aliases
* Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
* Sun Nov 13 09:24:13 BRST 2016
* This code is hereby placed in the public domain.
* Add <<#include "proxy.c">> just before the definition of luaX_next in llex.c
*/
#define DEBUG 1
#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

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

/**
TODO:
 - how to make the lexem type scope confined in this file? adding static will make compilation fail…
 - manage unload of lexicon
 * */
typedef struct {
    char *name;
    int token;
} lexem;

typedef struct {
    lexem *lexems;
    int length;
} lexicon;

/* TODO moving this global variables to staticaly scoped within nexttoken variables and pass them around as needed */
static lexem *aliases;
static int alias_count;
static char *last_loaded_lexicon;

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
* Return the number of line contained in the file refered by lexicon_path or 0 if the file can't be opened
*/
static int lexicon_line_count(const char* lexicon_path) {
    FILE* lexicon = fopen(lexicon_path, "r");
    if (lexicon == NULL) {
        fprintf(stderr, "Can't open %s file. Lexicon wasn't changed\n", lexicon_path);
        return 0;
    }
    int ch, number_of_lines = 0;
    do {
        ch = fgetc(lexicon);
        if(ch == '\n')
            number_of_lines++;
    } while (ch != EOF);

    // last line doesn't end with a new line!
    // but there has to be a line at least before the last line
    if(ch != '\n' && number_of_lines != 0)
        number_of_lines++;
    fclose(lexicon);
    return number_of_lines;
}

static int get_token_value(char *keyword) {
    for(int i = FIRST_RESERVED; i < NUM_RESERVED; i++) {
        if(strcmp(luaX_tokens[i], keyword) == 0) {
            return i;
        }
    }
    //debug_print("no luke with reserved keywords for %s\n", keyword);
	if(strcmp(keyword, "==") == 0)
		return TK_EQ;
	if(strcmp(keyword, "~=") == 0)
		return TK_NE;
	if(strcmp(keyword, "<") == 0)
		return TK_LT;
	if(strcmp(keyword, "<=") == 0)
		return TK_LE;
	if(strcmp(keyword, ">=") == 0)
		return TK_GE;
	if(strcmp(keyword, ">") == 0)
		return TK_GT;
	if(strcmp(keyword, "+") == 0)
		return TK_ADD;
	if(strcmp(keyword, "&") == 0)
		return TK_BAND;
	if(strcmp(keyword, "~") == 0)
		return TK_BNOT;
	if(strcmp(keyword, "|") == 0)
		return TK_BOR;
	if(strcmp(keyword, "^") == 0)
		return TK_BXOR;
	if(strcmp(keyword, "/") == 0)
		return TK_DIV;
	if(strcmp(keyword, "//") == 0)
		return TK_IDIV;
	if(strcmp(keyword, "-") == 0)
		return TK_MINUS;
	if(strcmp(keyword, "%") == 0)
		return TK_MOD;
	if(strcmp(keyword, "^") == 0)
		return TK_POW;
	if(strcmp(keyword, "-") == 0)
		return TK_SUB;
	if(strcmp(keyword, "<<") == 0)
		return TK_SHR;
	if(strcmp(keyword, ">>") == 0)
		return TK_SHL;
    return -1; // No corresponding token
}

static void debug_print_lexicon(lexem *aliases, const int limit) {
    for(int i = 0; i < limit; i++)
        debug_print("%s have value %i\n", aliases[i].name, aliases[i].token);
}

static void free_aliases() {
    for(int i=0; i < alias_count; i++)
        free(aliases[i].name);
    free(aliases);
    alias_count = 0;
}
/**
* Return the number of entry scanned in lexicon_path and stored in the aliases array
// TODO this is a very rough parsing, which with a very strict format…
// Ideally this should take a lua file
*/
static int parse_lexicon_file(const char *lexicon_path, const int line_to_read) {
    if( aliases != 0)
        free_aliases(); // Only one lexicon can be loaded at once for now
    aliases = malloc(line_to_read * sizeof *aliases);
    const int alias_max_length = 128;
    const int keyword_max_length = 128;
    char alias[alias_max_length];
    char target_keyword[keyword_max_length];

    int entry = 0;
    //debug_print("ready to store at most %i entries from %s\n", line_to_read, lexicon_path);

    FILE* lexicon = fopen(lexicon_path, "r");
    // TODO check the file is readable
    int line_count = 0;
    while (!feof(lexicon)) {
        line_count++;
        const int scanned = fscanf(lexicon, "%s = '%[^']'", alias, target_keyword);
        if (scanned != 2) {
            //fdebug_print(stderr, "WARNING: line %i doesn't produce a lexical alias\n", line_count);
            continue; // didn't matched a single line affectation as expected, but next lines might, so don't break
        }
        const int token_value = get_token_value(target_keyword);
        //debug_print("ready to store %s \t\tas alias of \"%s\" \twith value %i\n", alias, target_keyword, token_value);
        // TODO: add a checking to validate alias as a valid token
        if(token_value > -1) {
            aliases[entry].name = strdup(alias);
            aliases[entry].token = token_value;
            entry++;
        }
    }
    fclose(lexicon);
    debug_print("The alias lexicon now have %i entries\n", entry);
    debug_print_lexicon(aliases, entry);
    aliases = realloc(aliases, entry * sizeof *aliases); // there might be more lines in the file than entries

    return entry;
}

/**
* Return true if the file path provided and the currently last loaded lexicon have the same content, false otherwise
*/
static int already_loaded(const char* new_lexicon_path) {
    // TODO: checking for a file hash equality rather than path equality
    if(last_loaded_lexicon == NULL)
        return 0;
    return strcmp(new_lexicon_path, last_loaded_lexicon) == 0;
}
/**
 *
 * length is assigned with the number of element in the returned table of lexem struct, because it seems that in C
 * there is no way to get this value. An other way to manage that would be to have a conventional value for the last
 * element. Or yet an other solution would be to create a structure which embed both the table and its length
 */
static lexem* load_lexicon(const char* lexicon_path) {
    if(aliases != 0 && already_loaded(lexicon_path))
    {
        return aliases;
    }

    debug_print("coucou from %i\n", __LINE__);
    const int number_of_lines = lexicon_line_count(lexicon_path);
    if(number_of_lines == 0)
    {
        debug_print("coucou from %i\n", __LINE__);
        return aliases;
    }
    if(last_loaded_lexicon != NULL)
        free(last_loaded_lexicon);
    last_loaded_lexicon = strdup(lexicon_path);
    alias_count = parse_lexicon_file(lexicon_path, number_of_lines);

    return aliases;
}


// #include "lua.h" is required to manipulate lua stack but is already included in llex.c which include the current file
#include "lualib.h"
#include "lauxlib.h"
static void set_alias_env(lua_State *L) {
    //luaL_openlibs(L);
    lua_newtable(L);
    for (int i = 0; i < alias_count; i++) {
        debug_print("pushing %s : %i\n", aliases[i].name, aliases[i].token);
        lua_pushstring(L, aliases[i].name);   /* Push the table cell key */
        lua_pushnumber(L, aliases[i].token); /* Push the cell value */
        lua_rawset(L, -3);      /* Stores the pair in the table */
        #ifdef prount
        #endif
    }

    /* By what name is the script going to reference our table? */
    lua_setglobal(L, "i18n");
}

// In Lua 5.0 reference manual is a table traversal example at page 29.
void PrintTable(lua_State *L)
{
    //lua_pushnil(L);
    int n = lua_gettop(L);
    debug_print("%i elements on the stack \n", n );
/*
    while(lua_next(L, -2) != 0)
    {
        if(lua_isstring(L, -1))
          printf("%s = %s\n", lua_tostring(L, -2), lua_tostring(L, -1));
        else if(lua_isnumber(L, -1))
          printf("%s = %d\n", lua_tostring(L, -2), lua_tonumber(L, -1));
        else if(lua_istable(L, -1))
          PrintTable(L);

        lua_pop(L, 1);
    }
    */
}

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void print_item(lua_State *L, int i, int as_key);

static int is_identifier(const char *s) {
  while (*s) {
    if (!isalnum(*s) && *s != '_') return 0;
    ++s;
  }
  return 1;
}

static int is_seq(lua_State *L, int i) {
      // stack = [..]
  lua_pushnil(L);
      // stack = [.., nil]
  int keynum = 1;
  while (lua_next(L, i)) {
      // stack = [.., key, value]
    lua_rawgeti(L, i, keynum);
      // stack = [.., key, value, t[keynum]]
    if (lua_isnil(L, -1)) {
      lua_pop(L, 3);
      // stack = [..]
      return 0;
    }
    lua_pop(L, 2);
      // stack = [.., key]
    keynum++;
  }
      // stack = [..]
  return 1;
}

static void print_seq(lua_State *L, int i) {
  printf("{");

  int k;
  for (k = 1;; ++k) {
        // stack = [..]
    lua_rawgeti(L, i, k);
        // stack = [.., t[k]]
    if (lua_isnil(L, -1)) break;
    if (k > 1) printf(", ");
    print_item(L, -1, 0);  // 0 --> as_key
    lua_pop(L, 1);
        // stack = [..]
  }
        // stack = [.., nil]
  lua_pop(L, 1);
        // stack = [..]

  printf("}");
}

static void print_table(lua_State *L, int i) {
  // Ensure i is an absolute index as we'll be pushing/popping things after it.
  if (i < 0) i = lua_gettop(L) + i + 1;

  const char *prefix = "{";
  if (is_seq(L, i)) {
    print_seq(L, i);  // This case includes all empty tables.
  } else {
        // stack = [..]
    lua_pushnil(L);
        // stack = [.., nil]
    while (lua_next(L, i)) {
      printf("%s", prefix);
        // stack = [.., key, value]
      print_item(L, -2, 1);  // 1 --> as_key
      printf(" = ");
      print_item(L, -1, 0);  // 0 --> as_key
      lua_pop(L, 1);  // So the last-used key is on top.
        // stack = [.., key]
      prefix = ", ";
    }
        // stack = [..]
    printf("}");
  }
}

static char *get_fn_string(lua_State *L, int i) {
  static char fn_name[1024];

  // Ensure i is an absolute index as we'll be pushing/popping things after it.
  if (i < 0) i = lua_gettop(L) + i + 1;

  // Check to see if the function has a global name.
      // stack = [..]
  lua_getglobal(L, "_G");
      // stack = [.., _G]
  lua_pushnil(L);
      // stack = [.., _G, nil]
  while (lua_next(L, -2)) {
      // stack = [.., _G, key, value]
    if (lua_rawequal(L, i, -1)) {
      snprintf(fn_name, 1024, "function:%s", lua_tostring(L, -2));
      lua_pop(L, 3);
      // stack = [..]
      return fn_name;
    }
      // stack = [.., _G, key, value]
    lua_pop(L, 1);
      // stack = [.., _G, key]
  }
  // If we get here, the function didn't have a global name; print a pointer.
      // stack = [.., _G]
  lua_pop(L, 1);
      // stack = [..]
  snprintf(fn_name, 1024, "function:%p", lua_topointer(L, i));
  return fn_name;
}

static void print_item(lua_State *L, int i, int as_key) {
  int ltype = lua_type(L, i);
  // Set up first, last and start and end delimiters.
  const char *first = (as_key ? "[" : "");
  const char *last  = (as_key ? "]" : "");
  switch(ltype) {

    case LUA_TNIL:
      printf("nil");  // This can't be a key, so we can ignore as_key here.
      return;

    case LUA_TNUMBER:
      printf("%s%g%s", first, lua_tonumber(L, i), last);
      return;

    case LUA_TBOOLEAN:
      printf("%s%s%s", first, lua_toboolean(L, i) ? "true" : "false", last);
      return;

    case LUA_TSTRING:
      {
        const char *s = lua_tostring(L, i);
        if (is_identifier(s) && as_key) {
          printf("%s", s);
        } else {
          printf("%s'%s'%s", first, s, last);
        }
      }
      return;

    case LUA_TTABLE:
      printf("%s", first);
      print_table(L, i);
      printf("%s", last);
      return;

    case LUA_TFUNCTION:
      printf("%s%s%s", first, get_fn_string(L, i), last);
      return;

    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
      printf("%suserdata:", first);
      break;

    case LUA_TTHREAD:
      printf("%sthread:", first);
      break;

    default:
      printf("<internal_error_in_print_stack_item!>");
      return;
  }

  // If we reach here, then we've got a type that we print as a pointer.
  printf("%p%s", lua_topointer(L, i), last);
}

void print_stack(lua_State *L) {
  int n = lua_gettop(L);
  printf("stack:");
  int i;
  for (i = 1; i <= n; ++i) {
    printf(" ");
    print_item(L, i, 0);  // 0 --> as_key
  }
  if (n == 0) printf(" <empty>");
  printf("\n");
}

static void get_alias_env(lua_State *L) {
    //luaL_openlibs(L);
    const char *i18n_keywords = "__i18n";
    //print_stack(L);
    int n = lua_gettop(L);
    printf("%i eroj\n", n);
    lua_getglobal(L, i18n_keywords);
    printf("%i eroj\n", n);
    print_stack(L);
     lua_pop(L, 1);
     printf("%i eroj\n", n);
    // PrintTable(L);
    // const size_t len = lua_rawlen(L,1);
    // debug_print("%s have %i elements\n", i18n_keywords, len);

    /* By what name is the script going to reference our table? */
    
}

static int nexttoken(LexState *ls, SemInfo *seminfo)
{
    const char *lexicon_path =  "i18n_eo.lua";
    //load_lexicon(lexicon_path);
    //set_alias_env(ls->L);
    get_alias_env(ls->L);
    //debug_print("lexicon stored at %p with %i elements\n", aliases, alias_count);
	int t=llex(ls,seminfo);
    // TODO treat that as a cas in the for loop, as the self keyword should also be modifiable from aliases
	if (t==TK_NAME && strcmp(getstr(seminfo->ts),"sia")==0) {
		seminfo->ts = luaS_new(ls->L,"self");
		return t;
	}
	if (t==TK_NAME) {
		int i;
		for (i=0; i<alias_count; i++) {
			if (strcmp(getstr(seminfo->ts),aliases[i].name)==0)
				return aliases[i].token;
		}
	}
	return t;
}

#define llex nexttoken
