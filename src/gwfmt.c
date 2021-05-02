#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "lint.h"
#include "unpy.h"

ANN static void lint_gw(struct AstGetter_ *arg, struct LintState *ls) {
  const Ast ast = parse(arg);
  if(ast) {
    Lint l = { .mp=arg->st->p, .ls=ls, .line=1};
    if(!l.ls->pretty && l.ls->show_line) {
      lint(&l, "     {-Y}┏━━━{0} {+}FILE{-}:{0} {+B}%s{0}\n", arg->name); // todo: line count
      lint(&l, "{0}{-}     {Y}┃{0}\n");
      lint(&l, "{0}{-} % 4u{Y}┃{0} ", l.line); // todo: line count
    }
    lint_ast(&l, ast);
    free_ast(l.mp, ast);
    if(!l.ls->pretty && l.ls->show_line) {
      lint(&l, "\b\b\b\b\b\b\b");
      lint(&l, "{0}{-}     {Y}┃{0}\n");
      lint(&l, "     {-Y}┗━━━{0}\n");
    }
  }
}

ANN static void read_py(struct AstGetter_ *arg, char **ptr, size_t *sz) {
#ifndef BUILD_ON_WINDOWS
  FILE *f = open_memstream(ptr, sz);
#else
  FILE *f = tmpfile();
  fwrite(ptr, 1, sz, f);
#endif
  yyin = arg->f;
  yyout = f;
  yylex();
  fclose(f);
}

ANN static void lint_unpy(struct AstGetter_ *arg, struct LintState *ls) {
  char *ptr;
  size_t sz;
  read_py(arg, &ptr, &sz);
  if(!ls->onlypy) {
    FILE *f = fmemopen(ptr, sz, "r");
    struct AstGetter_ new_arg = { arg->name, f, arg->st , .ppa=arg->ppa };
    lint_gw(&new_arg, ls);
    fclose(f);
  } else
    printf(ptr);
  free(ptr);
}

int main(int argc, char **argv) {
  MemPool mp = mempool_ini(sizeof(struct Exp_));
  SymTable* st = new_symbol_table(mp, 65347); // could be smaller
  struct PPArg_ ppa = { .lint = 1 };
  pparg_ini(mp, &ppa);
  struct LintState ls = { .color=isatty(1), .show_line=1 };

  tcol_override_color_checks(ls.color);
  for(int i = 1; i < argc; ++i) {
    if(!strcmp(argv[i], "-p")) {
      ls.py = !ls.py;
      continue;
    } else if(!strcmp(argv[i], "-n")) {
      ls.pretty = !ls.pretty;
      continue;
    } else if(!strcmp(argv[i], "-u")) {
      ls.unpy = !ls.unpy;
      continue;
    } else if(!strcmp(argv[i], "-e")) { // expand
      ppa.lint = !ppa.lint;
      continue;
    } else if(!strcmp(argv[i], "-r")) { // only pythonify
      ls.onlypy = ls.unpy = !ls.onlypy;
      continue;
    } else if(!strcmp(argv[i], "-m")) {
      ls.minimize = 1;
      ls.pretty = ls.onlypy = ls.unpy = false;
      continue;
    } else if(!strcmp(argv[i], "-c")) {
      ls.color = !ls.color;
      tcol_override_color_checks(ls.color);
      continue;
    }
    FILE* file = fopen(argv[i], "r");
    if(!file)
      continue;
    struct AstGetter_ arg = { argv[i], file, st , .ppa=&ppa };
    (!ls.unpy ? lint_gw: lint_unpy)(&arg, &ls);
    fclose(file);
  }
  pparg_end(&ppa);
  free_symbols(st);
  mempool_end(mp);
}
