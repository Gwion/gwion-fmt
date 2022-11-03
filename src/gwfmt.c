#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwfmt.h"
#include "unpy.h"

ANN static inline void lint_file(Lint *a, const m_str name) {
  lint(a, "{-}File:{0} {+}%s{0}\n", name);
}

ANN static int lint_gw(struct AstGetter_ *arg, struct LintState *ls) {
  const Ast ast = parse(arg);
  if (!ast) return 1;
  Lint l = {.mp = arg->st->p, .st = arg->st, .ls = ls, .line = 1 };
  if (!ls->pretty) {
    if (l.ls->header) {
      lint(&l, "       {N}┏━━━{0} ");
      lint_file(&l, arg->name);
      lint(&l, "{0}  {-}     {N}┃{0}");
    }
    lint_nl(&l);
  } else if (l.ls->header) {
    lint(&l, "\n");
    lint_file(&l, arg->name);
  }
  lint_ast(&l, ast);
  free_ast(l.mp, ast);
  ls->mark = 0;
  if (!ls->pretty) {
    lint(&l, "\b\b\b\b\b\b\b");
    if (l.ls->header) {
      lint(&l, "{0}{-}     {N}┃{0}\n");
      lint(&l, "       {N}┗━━━{0}\n");
    }
  }
  return 0;
}

ANN static void read_py(struct AstGetter_ *arg, char **ptr, size_t *sz) {
#ifndef BUILD_ON_WINDOWS
  FILE *f = open_memstream(ptr, sz);
#else
  FILE *f = tmpfile();
  fwrite(ptr, 1, sz, f);
#endif
  yyin  = arg->f;
  yyout = f;
  yylex();
  fclose(f);
}

ANN static int lint_unpy(struct AstGetter_ *arg, struct LintState *ls) {
  char * ptr;
  size_t sz;
  read_py(arg, &ptr, &sz);
  int ret = 0;
  if (!ls->onlypy) {
    FILE *            f       = fmemopen(ptr, sz, "r");
    struct AstGetter_ new_arg = {arg->name, f, arg->st, .ppa = arg->ppa};
    ret                       = lint_gw(&new_arg, ls);
    fclose(f);
  } else
    printf("%s", ptr);
  free(ptr);
  return ret;
}

int main(int argc, char **argv) {
  MemPool       mp  = mempool_ini(sizeof(struct Exp_));
  SymTable *    st  = new_symbol_table(mp, 65347); // could be smaller
  struct PPArg_ ppa = {.lint = 1};
  pparg_ini(mp, &ppa);
  struct LintState ls = {.color = isatty(1), .show_line = true, .header = true, .nindent = 2};
  int              ret = 0;
  tcol_override_color_checks(ls.color);
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-p")) {
      ls.py = !ls.py;
      continue;
    } else if (!strcmp(argv[i], "-n")) {
      ls.pretty = !ls.pretty;
      continue;
    } else if (!strcmp(argv[i], "-h")) {
      ls.header = !ls.header;
      continue;
    } else if (!strncmp(argv[i], "-M", 2)) {
      ls.mark = atoi(argv[i] + 2);
      continue;
    } else if (!strcmp(argv[i], "-u")) {
      ls.unpy = !ls.unpy;
      continue;
    } else if (!strcmp(argv[i], "-e")) { // expand
      ppa.lint = !ppa.lint;
      continue;
    } else if (!strcmp(argv[i], "-r")) { // only pythonify
      ls.onlypy = ls.unpy = !ls.onlypy;
      continue;
    } else if (!strcmp(argv[i], "-m")) {
      ls.minimize = 1;
      ls.pretty = ls.onlypy = ls.unpy = false;
      continue;
    } else if (!strcmp(argv[i], "-c")) {
      ls.color = !ls.color;
      tcol_override_color_checks(ls.color);
      continue;
    } else if (!strncmp(argv[i], "-i", 2)) {
      ls.nindent = atoi(argv[i] + 2);
      continue;
    }
    FILE *file = fopen(argv[i], "r");
    if (!file) continue;
    struct AstGetter_ arg = {argv[i], file, st, .ppa = &ppa};
    ret                   = (!ls.unpy ? lint_gw : lint_unpy)(&arg, &ls);
    fclose(file);
  }
  pparg_end(&ppa);
  free_symbols(st);
  mempool_end(mp);
  return ret;
}
