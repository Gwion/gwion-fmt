%{
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define MAX_DEPTH 72

static int nesting;
static unsigned int indent_stack[MAX_DEPTH];
static int level = 0 ;

static unsigned int first = 1 ;

static unsigned int has_semi_colon;

void print_indent(void) ;
void process_indent(char* line) ;

static int unsigned start;
%}

ID  [A-Za-z_][A-Za-z0-9_]*
INT [0-9]+
OP [\?:\$@\+\-\*/%~\<\>\^|&\!=]*
%option noinput
%option noyywrap
%option outfile="src/unpy.c"
%option header-file="src/unpy.h"
%%


;(^[ ]*\n)*       { ++has_semi_colon ; start = 1; fprintf(yyout, ";\n");/* Ignore blank lines. */}
(^[ ]*\n)*       { --level ; fprintf(yyout, "\n");/* Ignore blank lines. */}
^[ ]*[^ \n]+  {int last = yyleng - 1;
               process_indent(yytext) ;
               while ((last >= 0) &&
                      (yytext[last] != ' ')) {
                unput(yytext[last]);
                last-- ;}
                print_indent();
}


"("  { fprintf(yyout, " "); ECHO; nesting++ ; }
")"  { ECHO; fprintf(yyout, " "); nesting-- ; }

\".*\" { if(!start) fprintf(yyout, " "); ECHO; printf(" "); }

{OP}|{ID}|{INT} {
  if(!(start || first))fprintf(yyout, " ") ;
  start = 0; ECHO; fprintf(yyout, " ");
}

[ \r] {}
\n    {}

<<EOF>>  { process_indent("") ; return 0 ; }

. { ECHO; start = 0; }

%%

unsigned int white_count(char* line) {
  unsigned int count = 0 ;
  while (*line == ' ')
    count++, line++ ;
  return count ;
}

void print_indent() {
  for(unsigned int i = 0; i < indent_stack[level]; ++i)
    fprintf(yyout, " ");
}

void process_indent(char* line) {
  if (nesting) /* Ignore indents while nested. */
    return ;
  start = 1;
  unsigned int indent = white_count(line) ;

  if (indent == indent_stack[level]) {
    if (!first && ! has_semi_colon)
      fprintf(yyout, ";\n") ;
    first = 0 ;
  start = 1;
    has_semi_colon = 0;
    return ;
  }

  if (indent > indent_stack[level]) {
    fprintf(yyout, " {\n") ;
    assert(level+1 < MAX_DEPTH) ;
    indent_stack[++level] = indent ;
    return ;
  }

  while (indent < indent_stack[level]) {
    --level ;
    if (!has_semi_colon) {
      fprintf(yyout, ";\n") ;
      has_semi_colon = 0;
    }
    print_indent();
    fprintf(yyout, "}\n") ;
  }
  assert(level >= 0) ;
}
