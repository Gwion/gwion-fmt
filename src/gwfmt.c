#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwfmt.h"
#include "unpy.h"
#include "cmdapp.h"

ANN static inline void gwfmt_file(Gwfmt *a, const m_str name) {
  gwfmt_util(a, "{-}File:{0} {+}%s{0}\n", name);
}

ANN static int gwfmt_gw(struct AstGetter_ *arg, struct GwfmtState *ls) {
  const Ast ast = parse(arg);
  if (!ast) return 1;
  Gwfmt l = {.mp = arg->st->p, .st = arg->st, .ls = ls, .line = 1, .last = cht_nl };
  if (!ls->pretty) {
    if (l.ls->header) {
      gwfmt_util(&l, "       {N}┏━━━{0} ");
      gwfmt_file(&l, arg->name);
      gwfmt_util(&l, "{0}  {-}     {N}┃{0}");
    }
    gwfmt_nl(&l);
  } else if (l.ls->header) {
    gwfmt_util(&l, "\n");
    gwfmt_util(&l, arg->name);
  }
  gwfmt_ast(&l, ast);
  free_ast(l.mp, ast);
  ls->mark = 0;
  if (!ls->pretty) {
    if(!ls->minimize)
      gwfmt_util(&l, "\b\b\b\b\b\b\b");
    if (l.ls->header) {
      gwfmt_util(&l, "{0}{-}     {N}┃{0}\n");
      gwfmt_util(&l, "       {N}┗━━━{0}\n");
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

ANN static int gwfmt_unpy(struct AstGetter_ *arg, struct GwfmtState *ls) {
  char * ptr;
  size_t sz;
  read_py(arg, &ptr, &sz);
  int ret = 0;
  if (!ls->onlypy) {
    FILE *            f       = fmemopen(ptr, sz, "r");
    struct AstGetter_ new_arg = {arg->name, f, arg->st, .ppa = arg->ppa};
    ret                       = gwfmt_gw(&new_arg, ls);
    fclose(f);
  } else
    printf("%s", ptr);
  free(ptr);
  return ret;
}
#ifndef GWFMT_VERSION
#define GWFMT_VERSION "N.A"
#endif


enum {
  INDENT,
  PRETTY,
  PY,
  UNPY,
  ONLYPY,
  HEADER,
  MARK,
  EXPAND,
  MINIFY,
  COLOR,
  NOPTIONS
};

static void setup_options(cmdapp_t *app, cmdopt_t *opt) {
  cmdapp_set(app, 'i', "indent", CMDOPT_TAKESARG, NULL, "set lenght of indent in spaces",
             "integer", &opt[INDENT]);
  cmdapp_set(app, 'n', "pretty", CMDOPT_TAKESARG, NULL, "enable or disable pretty mode",
             "bool", &opt[PRETTY]);
  cmdapp_set(app, 'p', "py", CMDOPT_TAKESARG, NULL, "{/}pythonify{0}",
             "bool", &opt[PY]);
  cmdapp_set(app, 'u', "unpy", CMDOPT_TAKESARG, NULL, "{/}unpythonify{0}",
             "bool", &opt[UNPY]);
  cmdapp_set(app, 'r', "onlypy", CMDOPT_TAKESARG, NULL, "enable or disable python mode only",
             "bool", &opt[HEADER]);
  cmdapp_set(app, 'h', "header", CMDOPT_TAKESARG, NULL, "enable or disable header mode",
             "bool", &opt[ONLYPY]);
  cmdapp_set(app, 'M', "mark", CMDOPT_TAKESARG, NULL, "mark a line",
             "integer", &opt[MARK]);
  cmdapp_set(app, 'e', "expand", CMDOPT_TAKESARG, NULL, "enable or disable lint mode",
             "bool", &opt[HEADER]);
  cmdapp_set(app, 'm', "minify", CMDOPT_TAKESARG, NULL, "minimize input",
             "bool", &opt[MINIFY]);
  cmdapp_set(app, 'c', "color", CMDOPT_TAKESARG, NULL, "enable or disable {R}c{G}o{B}l{M}o{Y}r{C}s{0}",
             "{+}auto{-}/never/always", &opt[COLOR]);
}

#define ARG2INT(a) strtol(a, NULL, 10)
ANN static inline bool arg2bool(const char *arg) {
  return !strcmp(arg, "true");
}

typedef struct GwArg {
  SymTable *st;
  struct PPArg_ *ppa;
  struct GwfmtState *ls;
} GwArg;

ANN static bool run(GwArg *arg, const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    gw_err("{R}gwfmt:{0} can't open file %s\n", filename);
    return false;
  }
  struct AstGetter_ getter = { 
    .name = (m_str)filename,
    .f = file,
    .st = arg->st,
    .ppa = arg->ppa
  };
  bool ret = (!arg->ls->unpy ? gwfmt_gw : gwfmt_unpy)(&getter, arg->ls);
  fclose(file);
  printf("%s", arg->ls->text.str);
  text_reset(&arg->ls->text);
  return ret;
}


static void myproc(void *data, cmdopt_t *option, const char *arg) {
  GwArg *gwarg = data;
  struct GwfmtState *ls = gwarg->ls;
  if (arg) run(gwarg, arg);
  else {
    switch (option->shorto) {
      case 'i': // indent
        ls->nindent = ARG2INT(option->value);
        break;
      case 'n': // pretty
        ls->pretty = !ls->pretty;
        break;
      case 'h': // header
        ls->header = !ls->header;
        break;
      case 'u': // unpy
        ls->unpy = !ls->unpy;
        break;
      case 'r': // unpy
        ls->onlypy = ls->unpy = arg2bool(option->value);
        break;
      case 'M': // mark
        ls->mark = ARG2INT(option->value);
        break;
      case 'e': // expand
        gwarg->ppa->fmt = !arg2bool(option->value);
        break;
      case 'm': // minify
        ls->minimize = arg2bool(option->value);
        break;
      case 'c': // color
        ls->color = arg2bool(option->value);
        tcol_override_color_checks(ls->color);
        break;
    }
  }
}

ANN static bool arg_parse(GwArg *a, int argc, char **argv) {
  cmdapp_t            app;
  const cmdapp_info_t info = {
      .program         = "gwion",
      .synopses        = NULL, // so it's automatic
      .version         = GWFMT_VERSION,
      .author          = "Jérémie Astor",
      .year            = 2016,
      .description     = "Pretty printer and minifier for the Gwion programming language.",
      .help_des_offset = 32,
      .ver_extra =
          "License GPLv3+: GNU GPL version 3 or later "
          "<https://gnu.org/licenses/gpl.html>\n"
          "This is free software: you are free to change and redistribute it.\n"
          "There is NO WARRANTY, to the extent permitted by law.\n"};
  cmdapp_init(&app, argc, argv, CMDAPP_MODE_SHORTARG, &info);
  cmdapp_enable_procedure(&app, myproc, a);
  cmdopt_t opt[NOPTIONS];
  setup_options(&app, opt);
  bool ret = cmdapp_run(&app) == EXIT_SUCCESS && cmdapp_should_exit(&app);
  //if (cmdapp_run(&app) == EXIT_SUCCESS && cmdapp_should_exit(&app))
  //  arg->arg->quit = 1;
  cmdapp_destroy(&app);
  return ret;
}

int main(int argc, char **argv) {
  MemPool       mp  = mempool_ini(sizeof(struct Exp_));
  SymTable *    st  = new_symbol_table(mp, 65347); // could be smaller
  struct PPArg_ ppa = {.fmt = 1};
  pparg_ini(mp, &ppa);
  struct GwfmtState ls = {.color = isatty(1), .show_line = true, .header = true, .nindent = 2, .ppa = &ppa};
  text_init(&ls.text, mp);
  int              ret = 0;
  tcol_override_color_checks(ls.color);
  GwArg arg =  { .ls = &ls, .st = st, .ppa = &ppa };
  argc--; argv++;
  arg_parse(&arg, argc, argv);
  text_release(&ls.text);
  pparg_end(&ppa);
  free_symbols(st);
  mempool_end(mp);
  return ret;
}
