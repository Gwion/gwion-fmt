#include <ctype.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwfmt.h"
#include "gwfmt_internal.h"
#include "unpy.h"

#define INDENT(a, b)                                                           \
  {                                                                            \
    ++a->indent;                                                               \
    { b; };                                                                    \
    --a->indent;                                                               \
  }

int gwfmt_printf(const char * fmt, ...) {
  return tcol_printf(fmt);
}

int (*_print)(const char *, ...) = gwfmt_printf;

ANN void gwfmt_set_func(int (*f)(const char*, ...)) {
  _print = f;
}

ANN static enum char_type cht(const char c) {
  if (isalnum(c) || c == '_') return cht_id;
  if (c == ':') return cht_colon;
  if (c == '[') return cht_lbrack;
  if (c == '\n') return cht_nl;
  char *op = "?$@+-/%~<>^|&!=*";
  do
    if (c == *op) return cht_op;
  //while (++op);
  while (*op++);
  char * delim = "(){},;`]";
  do
    if (c == *delim) return cht_delim;
  while (*delim++);
  return cht_sp;
}

ANN void color(Gwfmt *a, const m_str buf) {
  char tmp[strlen(buf)*4];
  tcol_snprintf(tmp, strlen(buf)*4, buf);
  text_add(&a->ls->text, tmp);
}

void COLOR(Gwfmt *a, const m_str b, const m_str c) {
  color(a, b);
  gwfmt(a, c);
  color(a, "{0}");
}

ANN void gwfmt_util(Gwfmt *a, const m_str fmt, ...) {
  va_list ap, aq;
  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);

  char * buf = mp_malloc2(a->mp, n + 1);
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  char tmp[strlen(buf)*4];
  int ret = tcol_snprintf(tmp, strlen(buf)*4, buf);
  text_add(&a->ls->text, tmp);
  mp_free2(a->mp, n + 1, buf);
  va_end(ap);
}

ANN static void handle_space(Gwfmt *a, char c) {
  if ((a->need_space && a->last != cht_delim && cht(c) != cht_delim && a->last == cht(c)) ||

      (a->last == cht_colon /*&& (*buf == cht_lbrack || *buf == cht_op)*/)) {
    text_add(&a->ls->text, " ");
    a->column += 1;
  }
  a->need_space = 0;
}
ANN void gwfmt(Gwfmt *a, const m_str fmt, ...) {
  a->nl = 0;
  va_list ap;

  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);

  char * buf = mp_malloc2(a->mp, n + 1);
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  handle_space(a, buf[0]);
  text_add(&a->ls->text, buf);
  a->last = cht(buf[n - 1]);
  mp_free2(a->mp, n + 1, buf);
  va_end(ap);
  a->column += n;
}

ANN void gwfmt_space(Gwfmt *a) {
  if (!a->ls->minimize)
    gwfmt(a, " ");
  else
    a->need_space = 1;
}

ANN void gwfmt_nl(Gwfmt *a) {
  const unsigned int nl = a->nl + 1;
  if (!a->ls->minimize) {
    if (a->nl < 2) {
      gwfmt_util(a, "\n");
      if (!a->ls->pretty && a->ls->show_line) {
        gwfmt_util(a, " {-}% 4u{0}", a->line);//root
        if (a->ls->mark == a->line)
          gwfmt_util(a, " {+R}>{0}");
        else
          gwfmt_util(a, "  ");
        gwfmt_util(a, "{N}┃{0} "); //
      }
    }
  }
  a->column = a->ls->base_column;
  a->line++;
  a->nl = nl;
}

ANN void gwfmt_lbrace(Gwfmt *a) {
  if (!a->ls->py) COLOR(a, "{-}", "{");
}

ANN void gwfmt_rbrace(Gwfmt *a) {
  if (!a->ls->py) COLOR(a, "{-}", "}");
}

ANN void gwfmt_sc(Gwfmt *a) {
  if (!a->ls->py) COLOR(a, "{-}", ";");
}

ANN void gwfmt_comma(Gwfmt *a) {
  if (!a->ls->py) COLOR(a, "{-}", ",");
}

ANN void gwfmt_lparen(Gwfmt *a) { COLOR(a, "{-}","("); }

ANN void gwfmt_rparen(Gwfmt *a) { COLOR(a, "{-}", ")");
  a->last = cht_delim;

}

ANN static inline void gwfmt_lbrack(Gwfmt *a) { COLOR(a, "{-}", "["); }

ANN static inline void gwfmt_rbrack(Gwfmt *a) { COLOR(a, "{-}", "]"); }

ANN void gwfmt_indent(Gwfmt *a) {
  if(a->ls->minimize && !a->ls->py)
    return;
  if (a->skip_indent > 0) {
    a->skip_indent--;
    return;
  }
  if(!a->ls->use_tabs) {
    for (unsigned int i = 0; i < a->indent; ++i) {
      for (unsigned int j = 0; j < a->ls->nindent; ++j)
        gwfmt_space(a);
    }
  } else {
    for (unsigned int i = 0; i < a->indent; ++i)
      gwfmt(a, "\t");
  }
}

ANN static void paren_exp(Gwfmt *a, Exp* b);
ANN static void maybe_paren_exp(Gwfmt *a, Exp* b);
ANN static void gwfmt_array_sub2(Gwfmt *a, Array_Sub b);
ANN static void gwfmt_prim_interp(Gwfmt *a, Exp* *b);

#define _FLAG(a, b) (((a)&ae_flag_##b) == (ae_flag_##b))
ANN static void gwfmt_flag(Gwfmt *a, ae_flag b) {
  bool state = true;
  if (_FLAG(b, private))
    COLOR(a, "{-/G}", "private");
  else if (_FLAG(b, protect))
    COLOR(a, "{-/G}", "protect");
  else state = false;
  if (state) gwfmt_space(a);
  state = true;
  if (_FLAG(b, static))
    COLOR(a, "{-G}", "static");
  else if (_FLAG(b, global))
    COLOR(a, "{-G}", "global");
  else state = false;
  if (state) gwfmt_space(a);
  state = true;
  if (_FLAG(b, abstract))
    COLOR(a, "{-G}", "abstract");
  else if (_FLAG(b, final))
    COLOR(a, "{-G}", "final");
  else state = false;
  if (state) gwfmt_space(a);
}
#define gwfmt_flag(a, b) gwfmt_flag(a, b->flag)

ANN static void gwfmt_symbol(Gwfmt *a, Symbol b) {
  const m_str s = s_name(b);
  if (!strcmp(s, "true") || !strcmp(s, "false") || !strcmp(s, "maybe") ||
      !strcmp(s, "adc") || !strcmp(s, "dac") || !strcmp(s, "now"))
    COLOR(a, "{B}", s_name(b));
  else
    gwfmt(a, s);
}

ANN static void gwfmt_array_sub(Gwfmt *a, Array_Sub b) {
  gwfmt_lbrack(a);
  gwfmt_space(a);
  gwfmt_exp(a, b->exp);
  if (!a->ls->minimize) gwfmt_comma(a);
  gwfmt_space(a);
  gwfmt_rbrack(a);
}

#define NEXT(a, b, c)                                                          \
  if (b->next) {                                                               \
    gwfmt_comma(a);                                                             \
    gwfmt_space(a);                                                             \
    c(a, b->next);                                                             \
  }

ANN static void gwfmt_id_list(Gwfmt *a, ID_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Symbol xid = *mp_vector_at(b, Symbol, i);
    gwfmt_symbol(a, xid);
    if(i < b->len - 1) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  }
}

ANN void gwfmt_traits(Gwfmt *a, ID_List b) {
  gwfmt(a, ":");
  gwfmt_space(a);
  gwfmt_id_list(a, b);
  gwfmt_space(a);
}

ANN static void gwfmt_specialized_list(Gwfmt *a, Specialized_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Specialized *spec = mp_vector_at(b, Specialized, i);
    if(spec->td) {
      COLOR(a, "{+G}", "const");
      gwfmt_space(a);
      gwfmt_type_decl(a, spec->td);
      gwfmt_space(a);
    }
    COLOR(a, "{-C}", s_name(spec->tag.sym));
    if (spec->traits) {
      gwfmt_space(a);
      gwfmt_traits(a, spec->traits);
    }
    if(i < b->len - 1) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  }
}

ANN static void _gwfmt_tmplarg_list(Gwfmt *a, TmplArg_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    TmplArg targ = *mp_vector_at(b, TmplArg, i);
    if (targ.type == tmplarg_td)
     gwfmt_type_decl(a, targ.d.td);
    else gwfmt_exp(a, targ.d.exp);
    if(i < b->len - 1) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  }
}

ANN static inline void gwfmt_init_tmpl(Gwfmt *a) { COLOR(a, "{-}",":["); }

ANN static void gwfmt_tmplarg_list(Gwfmt *a, TmplArg_List b) {
  gwfmt_init_tmpl(a);
  gwfmt_space(a);
  _gwfmt_tmplarg_list(a, b);
  gwfmt_space(a);
  gwfmt_rbrack(a);
}

ANN static void gwfmt_tmpl(Gwfmt *a, Tmpl *b) {
  if (!b->call) {
    gwfmt_init_tmpl(a);
    gwfmt_space(a);
    gwfmt_specialized_list(a, b->list);
    gwfmt_space(a);
    gwfmt_rbrack(a);
  }
  if (b->call) gwfmt_tmplarg_list(a, b->call);
}

ANN static void gwfmt_range(Gwfmt *a, Range *b) {
  gwfmt_lbrack(a);
  if (b->start) gwfmt_exp(a, b->start);
  gwfmt_space(a);
  COLOR(a, "{-}", ":");
  gwfmt_space(a);
  if (b->end) gwfmt_exp(a, b->end);
  gwfmt_rbrack(a);
}

ANN static void gwfmt_prim_dict(Gwfmt *a, Exp* *b) {
  Exp* e = *b;
  gwfmt_lbrace(a);
  gwfmt_space(a);
  do {
    Exp* next  = e->next;
    Exp* nnext = next->next;
    e->next = NULL;
    next->next = NULL;
    gwfmt_exp(a, e);
    gwfmt_space(a);
    gwfmt(a, ":");
    gwfmt_space(a);
    gwfmt_exp(a, next);
    e->next = next;
    next->next = nnext;
    if(nnext) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  } while ((e = e->next->next));
  gwfmt_space(a);
  gwfmt_rbrace(a);
}
ANN static void gwfmt_effect(Gwfmt *a, Symbol b) {
  COLOR(a, "{-/C}", s_name(b));
}

ANN static void gwfmt_perform(Gwfmt *a) {
  COLOR(a, "{/M}", "perform");
  gwfmt_space(a);
}

ANN static void gwfmt_prim_perform(Gwfmt *a, Symbol *b) {
  COLOR(a, "{/M}", "perform");
  gwfmt_space(a);
  gwfmt_effect(a, *b);
}

ANN static void gwfmt_effects(Gwfmt *a, Vector b) {
  gwfmt_perform(a);
  for (m_uint i = 0; i < vector_size(b) - 1; i++) {
    gwfmt_effect(a, (Symbol)vector_at(b, i));
    gwfmt_space(a);
  }
  gwfmt_effect(a, (Symbol)vector_back(b));
}

ANN static void gwfmt_type_decl(Gwfmt *a, Type_Decl *b) {
  if (GET_FLAG(b, const)) {
    COLOR(a, "{+G}", "const");
    gwfmt_space(a);
  }
  if (GET_FLAG(b, late)) {
    COLOR(a, "{+/G}", "late");
    gwfmt_space(a);
  }
  gwfmt_flag(a, b);
  if (b->ref) gwfmt(a, "&");
  if (b->fptr) {
    const Fptr_Def fptr = b->fptr;
    gwfmt_lparen(a);
    if (b->fptr->base->flag != ae_flag_none) {
      gwfmt_flag(a, fptr->base);
      gwfmt_space(a);
    }
    gwfmt_type_decl(a, fptr->base->td);
    gwfmt_lparen(a);
    if (fptr->base->args) gwfmt_arg_list(a, fptr->base->args, 0);
    gwfmt_rparen(a);
    if (fptr->base->effects.ptr) gwfmt_effects(a, &fptr->base->effects);
    gwfmt_rparen(a);
    //    COLOR(a, "{C}", s_name(b->xid));
  } else if (b->tag.sym)
    COLOR(a, "{C}", s_name(b->tag.sym));
  if (b->types) gwfmt_tmplarg_list(a, b->types);
  for (m_uint i = 0; i < b->option; ++i) gwfmt(a, "?");
  if (b->array) gwfmt_array_sub2(a, b->array);
  if (b->next) {
    gwfmt(a, ".");
    gwfmt_type_decl(a, b->next);
  }
}

ANN static void gwfmt_prim_id(Gwfmt *a, Symbol *b) { gwfmt_symbol(a, *b); }


#define INTEGER_ADVANCE(a, tgt, src, n) \
do {                                    \
  memcpy(tgt, src, n);                  \
  gwfmt(a, tgt);                        \
  str += n;                             \
} while(0)

#define INTEGER_PAD(a, t, n)            \
do {                                    \
  for(int i = 0; i < (t - n); i++)      \
    gwfmt(a, "0");                      \
} while(0)

ANN static void gwfmt_decimal(Gwfmt *a, m_int num) {
  char c[1024];
  char *str = c;
  snprintf(str, 1023, "%" INT_F, num);
  const size_t sz = strlen(str);
  int start = sz % 3;
  char tmp[4] = {};
  if(start) INTEGER_ADVANCE(a, tmp, str, start);
  for(size_t i = start; i < sz; i += 3) {
    if(start++) gwfmt_space(a);
    INTEGER_ADVANCE(a, tmp, str, 3);
  }
}

static char *tobinary(char *tmp, m_uint a) {
  if (a > 1)
      tmp = tobinary(tmp, a / 2);
  *tmp = (a % 2) + '0';
  return tmp + 1;
}

ANN static void gwfmt_binary(Gwfmt *a, m_int num) {
  gwfmt(a, "0b");
  char c[1024] = {};
  char *str = c;
  tobinary(c, num);
  const size_t sz = strlen(str);
  int start = sz % 4;
  char tmp[5] = {};
  if(start) {
    INTEGER_PAD(a, 4, start);
    INTEGER_ADVANCE(a, tmp, str, start);
  }
  for(size_t i = start; i < sz; i += 4) {
    if(start++) gwfmt_space(a);
    INTEGER_ADVANCE(a, tmp, str, 4);
  }
}

ANN static void gwfmt_hexa(Gwfmt *a, m_int num) {
  gwfmt(a, "0x");
  char c[1024] = {};
  char *str = c;
  snprintf(str, 1023, "%lX", num);
  const size_t sz = strlen(str);
  int start = sz % 2;
  char tmp[3] = {};
  if(start) {
    INTEGER_PAD(a, 2, start);
    INTEGER_ADVANCE(a, tmp, str, start);
  }
  for(size_t i = start; i < sz; i += 2) {
    if(start++) gwfmt_space(a);
    INTEGER_ADVANCE(a, tmp, str, 2);
  }
}

ANN static void gwfmt_octal(Gwfmt *a, m_int num) {
  gwfmt(a, "0o");
  char c[1024] = {};
  char *str = c;
  snprintf(str, 1023, "%lo", num);
  const size_t sz = strlen(str);
  int start = sz % 3;
  char tmp[4] = {};
  if(start) {
    INTEGER_PAD(a, 3, start);
    INTEGER_ADVANCE(a, tmp, str, start);
  }
  for(size_t i = start; i < sz; i += 3) {
    if(start++) gwfmt_space(a);
    INTEGER_ADVANCE(a, tmp, str, 3);
  }
}

ANN static void gwfmt_prim_num(Gwfmt *a, struct gwint *b) {
  color(a, "{M}");
  switch(b->int_type) {
    case gwint_decimal: gwfmt_decimal(a, b->num); break;
    case gwint_binary:  gwfmt_binary(a, b->num); break;
    case gwint_hexa:    gwfmt_hexa(a, b->num); break;
    case gwint_octal:   gwfmt_octal(a, b->num); break;
  }
  color(a, "{0}");
}

ANN static void gwfmt_prim_float(Gwfmt *a, m_float *b) {
  if (*b == floor(*b)) {
    color(a, "{M}");
    gwfmt(a, "%li.", (m_int)*b);
    color(a, "{0}");
  } else {
    color(a, "{M}");
    gwfmt(a, "%g", *b);
    color(a, "{0}");
  }
}

ANN static m_str gwfmt_verbatim(Gwfmt *a, m_str b) {
  const size_t len = strlen(b);
  bool escape = false;
  char last = *b;
  while(*b) {
     char c[2] = { *b, 0 };
     escape = *b == '\\';
     last = *b;
     b++;
     if(!escape && *c == '\n') {
       a->last = cht(last);
       return b;
     }
     text_add(&a->ls->text, c); 
     a->column++;
  }
  a->last = cht(last);
  return NULL;
}

ANN static void gwfmt_string(Gwfmt *a, m_str str) {
  m_str s = str;
  while((s = gwfmt_verbatim(a, s))) {
      color(a, "{0}");
      gwfmt_nl(a);
      color(a, "{/Y}");
  }
}

ANN static void gwfmt_delim(Gwfmt *a, uint16_t delim) {
  if(delim) {
    color(a, "{Y-}");
    gwfmt(a, "%.*c",  delim, '#');
  }
  COLOR(a, "{0}{Y-}", "\"");
  color(a, "{/Y}");
}

ANN static void gwfmt_delim2(Gwfmt *a, uint16_t delim) {
  COLOR(a, "{0}{Y-}", "\"");
  if(delim) {
    color(a, "{Y-}");
    gwfmt(a, "%.*c",  delim, '#');
  }
  color(a, "{0}");
}

ANN static void gwfmt_prim_str(Gwfmt *a, struct AstString *b) {
  gwfmt_delim(a, b->delim);
  gwfmt_string(a, b->data);
  gwfmt_delim2(a, b->delim);
}

ANN static void gwfmt_prim_array(Gwfmt *a, Array_Sub *b) {
  gwfmt_array_sub(a, *b);
}

ANN static void gwfmt_prim_range(Gwfmt *a, Range **b) { gwfmt_range(a, *b); }

ANN static void gwfmt_prim_hack(Gwfmt *a, Exp* *b) {
  COLOR(a, "{-R}", "<<<");
  gwfmt_space(a);
  gwfmt_exp(a, *b);
  gwfmt_space(a);
  COLOR(a, "{-R}", ">>>");
}

ANN static void gwfmt_prim_char(Gwfmt *a, m_str *b) {
  color(a, "{M}");
  gwfmt(a, "'%s'", *b);
  color(a, "{0}");
}

ANN static void gwfmt_prim_nil(Gwfmt *a, void *b NUSED) {
  gwfmt_lparen(a);
  gwfmt_rparen(a);
}

ANN void gwfmt_prim_locale(Gwfmt *a, Symbol *b) {
  gwfmt(a, "`");
  gwfmt_symbol(a, *b);
  gwfmt(a, "`");
}

DECL_PRIM_FUNC(gwfmt, void, Gwfmt *)
ANN static void gwfmt_prim(Gwfmt *a, Exp_Primary *b) {
  gwfmt_prim_func[b->prim_type](a, &b->d);
}

ANN static void gwfmt_var_decl(Gwfmt *a, Var_Decl *b) {
  if (b->tag.sym) COLOR(a, "{W+}", s_name(b->tag.sym));
}

ANN static void gwfmt_exp_decl(Gwfmt *a, Exp_Decl *b) {
  if (b->var.td) {
    if (!(GET_FLAG(b->var.td, const) || GET_FLAG(b->var.td, late))) {
      COLOR(a, "{+G}", "var");
      gwfmt_space(a);
    }
    gwfmt_type_decl(a, b->var.td);
    if(b->args) paren_exp(a, b->args);
    gwfmt_space(a);
  }
  gwfmt_var_decl(a, &b->var.vd);
}

ANN static void gwfmt_exp_td(Gwfmt *a, Type_Decl *b) {
  COLOR(a, "{-G}", "$");
  //  gwfmt_space(a);
  gwfmt_type_decl(a, b);
}

ANN static void gwfmt_op(Gwfmt *a, const Symbol b) {
  m_str s = s_name(b);
  handle_space(a, *s);
  color(a, "{-G}");
  gwfmt_verbatim(a, s_name(b));
  color(a, "{0}");
}

ANN static void gwfmt_exp_binary(Gwfmt *a, Exp_Binary *b) {
  const unsigned int coloncolon = !strcmp(s_name(b->op), "::");
  maybe_paren_exp(a, b->lhs);
  if (!coloncolon) gwfmt_space(a);
  else a->last = cht_sp;
  gwfmt_op(a, b->op);
  if (!coloncolon) gwfmt_space(a);
  else a->last = cht_sp;
  gwfmt_exp(a, b->rhs);
}

ANN static int isop(const Symbol s) {
  char *name = s_name(s);
  for (size_t i = 0; i < strlen(name); ++i) {
    if (isalnum(name[i])) return 0;
  }
  return 1;
}

ANN static void gwfmt_captures(Gwfmt *a, Capture_List b) {
  gwfmt(a, ":");
  gwfmt_space(a);
  for (uint32_t i = 0; i < b->len; i++) {
    Capture *cap = mp_vector_at(b, Capture, i);
    if(cap->is_ref) gwfmt(a, "&");
    gwfmt_symbol(a, cap->var.tag.sym);
    gwfmt_space(a);
  }
  gwfmt(a, ":");
}

ANN static void gwfmt_exp_unary(Gwfmt *a, Exp_Unary *b) {
  if (s_name(b->op)[0] == '$' && !isop(b->op)) gwfmt_lparen(a);
  gwfmt_op(a, b->op);
  if (s_name(b->op)[0] == '$' && !isop(b->op)) gwfmt_rparen(a);
  if (b->captures) gwfmt_captures(a, b->captures);
  if (!isop(b->op)) gwfmt_space(a);
  if (b->unary_type == unary_exp)
    gwfmt_exp(a, b->exp);
  else if (b->unary_type == unary_td) {
    gwfmt_type_decl(a, b->ctor.td);
    if(b->ctor.exp) {
      gwfmt_lparen(a);
      if (b->ctor.exp->exp_type != ae_exp_primary ||
          b->ctor.exp->d.prim.prim_type != ae_prim_nil)
        gwfmt_exp(a, b->ctor.exp);
      gwfmt_rparen(a);
    }
  } else if (b->unary_type == unary_code) {
    gwfmt_lbrace(a);
    gwfmt_nl(a);
    INDENT(a, gwfmt_stmt_list(a, b->code));
    gwfmt_indent(a);
    gwfmt_rbrace(a);
  }
}

ANN static void gwfmt_exp_cast(Gwfmt *a, Exp_Cast *b) {
  if (b->exp->exp_type != ae_exp_decl)
    gwfmt_exp(a, b->exp);
  else
    paren_exp(a, b->exp);
  gwfmt_space(a);
  COLOR(a, "{-G}", "$");
  gwfmt_space(a);
  gwfmt_type_decl(a, b->td);
}

ANN static void gwfmt_exp_post(Gwfmt *a, Exp_Postfix *b) {
  gwfmt_exp(a, b->exp);
  gwfmt_op(a, b->op);
}

ANN static void gwfmt_exp_call(Gwfmt *a, Exp_Call *b) {
  if (b->func->exp_type != ae_exp_decl)
    gwfmt_exp(a, b->func);
  else
    paren_exp(a, b->func);
  if (b->tmpl) gwfmt_tmpl(a, b->tmpl);
  if (b->args)
    paren_exp(a, b->args);
  else
    gwfmt_prim_nil(a, b);
}

ANN static void gwfmt_exp_array(Gwfmt *a, Exp_Array *b) {
  gwfmt_exp(a, b->base);
  gwfmt_array_sub2(a, b->array);
}

ANN static void gwfmt_exp_slice(Gwfmt *a, Exp_Slice *b) {
  if (b->base->exp_type != ae_exp_primary &&
      b->base->exp_type != ae_exp_array && b->base->exp_type != ae_exp_call &&
      b->base->exp_type != ae_exp_post && b->base->exp_type != ae_exp_dot)
    gwfmt_exp(a, b->base);
  else
    paren_exp(a, b->base);
  gwfmt_range(a, b->range);
}

ANN static void gwfmt_exp_if(Gwfmt *a, Exp_If *b) {
  gwfmt_exp(a, b->cond);
  gwfmt_space(a);
  gwfmt(a, "?");
  if (b->if_exp) {
    gwfmt_space(a);
    gwfmt_exp(a, b->if_exp);
    gwfmt_space(a);
  }
  gwfmt(a, ":");
  gwfmt_exp(a, b->else_exp);
}

ANN static void gwfmt_exp_dot(Gwfmt *a, Exp_Dot *b) {
  gwfmt_exp(a, b->base);
  gwfmt(a, ".");
  gwfmt_symbol(a, b->xid);
}

ANN static void gwfmt_lambda_list(Gwfmt *a, Arg_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Arg *arg = mp_vector_at(b, Arg, i);
    gwfmt_symbol(a, arg->var.vd.tag.sym);
    gwfmt_space(a);
  }
}

ANN static void gwfmt_exp_lambda(Gwfmt *a, Exp_Lambda *b) {
  gwfmt(a, "\\");
  if (b->def->base->args) {
    gwfmt_lambda_list(a, b->def->base->args);
  }
  if (b->def->captures) gwfmt_captures(a, b->def->captures);
  if (b->def->d.code) {
    Stmt_List code = b->def->d.code;
    if(mp_vector_len(code) != 1)
      gwfmt_stmt_list(a, code); // brackets?
    else {
      gwfmt_lbrace(a);
      gwfmt_space(a);
      gwfmt_exp(a, mp_vector_at(code, Stmt, 0)->d.stmt_exp.val);
      gwfmt_space(a);
      gwfmt_rbrace(a);
    }
  }
}

DECL_EXP_FUNC(gwfmt, void, Gwfmt *)
ANN void gwfmt_exp(Gwfmt *a, Exp* b) {
  if(b->paren) gwfmt_lparen(a);
  gwfmt_exp_func[b->exp_type](a, &b->d);
  if(b->paren) gwfmt_rparen(a);
  NEXT(a, b, gwfmt_exp)
}

ANN static void gwfmt_prim_interp(Gwfmt *a, Exp* *b) {
  Exp* e = *b;
  const uint16_t delim = e->d.prim.d.string.delim;
  gwfmt_delim(a, delim);
  color(a, "{/Y}");
  while (e) {
    if (e->exp_type == ae_exp_primary && e->d.prim.prim_type == ae_prim_str) {
      gwfmt_string(a,  e->d.prim.d.string.data);
    } else {
      COLOR(a, "{Y/-}", "${");
      gwfmt_space(a);
      gwfmt_exp_func[e->exp_type](a, &e->d);
      gwfmt_space(a);
      COLOR(a, "{-/Y}", "}");
      color(a, "{Y/}");
    }
    e = e->next;
  }
  gwfmt_delim2(a, delim);
  color(a, "{/Y}");
}

ANN static void gwfmt_array_sub2(Gwfmt *a, Array_Sub b) {
  Exp* e = b->exp;
  for (m_uint i = 0; i < b->depth; ++i) {
    gwfmt_lbrack(a);
    if (e) {
      gwfmt_exp_func[e->exp_type](a, &e->d);
      e = e->next;
    }
    gwfmt_rbrack(a);
  }
}

ANN static void paren_exp(Gwfmt *a, Exp* b) {
  gwfmt_lparen(a);
  //if(b->exp_type != ae_exp_primary &&
  //   b->d.prim.prim_type != ae_prim_nil)
    gwfmt_exp(a, b);
  gwfmt_rparen(a);
}

ANN static void maybe_paren_exp(Gwfmt *a, Exp* b) {
  if (b->next)
    paren_exp(a, b);
  else
    gwfmt_exp(a, b);
}

ANN static void gwfmt_stmt_exp(Gwfmt *a, Stmt_Exp b) {
  if (b->val) gwfmt_exp(a, b->val);
  gwfmt_sc(a);
}

ANN static void gwfmt_stmt_retry(Gwfmt *a, Stmt_Exp b NUSED) {
  COLOR(a, "{+M}", "retry;");
}

ANN static void gwfmt_handler_list(Gwfmt *a, Handler_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Handler *handler = mp_vector_at(b, Handler, i);
    COLOR(a, "{+M}", "handle");
    gwfmt_space(a);
    if (handler->tag.sym) {
      gwfmt_effect(a, handler->tag.sym);
      gwfmt_space(a);
    }
    const uint indent = a->skip_indent++;
    gwfmt_stmt(a, handler->stmt);
    a->skip_indent = indent;
  }
}

ANN static void gwfmt_stmt_try(Gwfmt *a, Stmt_Try b) {
  COLOR(a, "{+M}", "try");
  gwfmt_space(a);
  const uint indent = a->skip_indent++;
//  const uint indent = a->indent++;
  gwfmt_stmt(a, b->stmt);
  gwfmt_space(a);
//  a->indent--;
// = indent;
  a->skip_indent = indent;
  gwfmt_handler_list(a, b->handler);
}

ANN static void gwfmt_stmt_spread(Gwfmt *a, Spread_Def b) {
  gwfmt(a, "...");
  gwfmt_space(a);
  gwfmt_symbol(a, b->tag.sym);
  gwfmt_space(a);
  gwfmt(a, ":");
  gwfmt_space(a);
  gwfmt_id_list(a, b->list);
  gwfmt_space(a);
  gwfmt_lbrace(a);
  gwfmt_nl(a);
  a->indent++;
  struct PPArg_ ppa = {};
  pparg_ini(a->mp, &ppa);
  FILE *file = fmemopen(b->data, strlen(b->data), "r");
  struct AstGetter_ arg = {"", file, a->st, .ppa = &ppa};
  Ast tmp =    parse(&arg);
  if(tmp) gwfmt_ast(a, tmp);
  pparg_end(&ppa);
  a->indent--;
  gwfmt_indent(a);
  gwfmt_rbrace(a);
  gwfmt_nl(a);
}

DECL_STMT_FUNC(gwfmt, void, Gwfmt *)
ANN static void gwfmt_stmt_while(Gwfmt *a, Stmt_Flow b) {
  if (!b->is_do) {
    COLOR(a, "{+M}", "while");
    paren_exp(a, b->cond);
  } else
    COLOR(a, "{+M}", "do");
  if (b->body->stmt_type != ae_stmt_exp || b->body->d.stmt_exp.val)
    gwfmt_space(a);
  gwfmt_stmt_func[b->body->stmt_type](a, &b->body->d);
  if (b->is_do) {
    gwfmt_space(a);
    COLOR(a, "{+M}", "while");
    paren_exp(a, b->cond);
    gwfmt_sc(a);
  }
}

ANN static void gwfmt_stmt_until(Gwfmt *a, Stmt_Flow b) {
  if (!b->is_do) {
    COLOR(a, "{+M}", "until");
    paren_exp(a, b->cond);
  } else
    COLOR(a, "{+M}", "do");
  gwfmt_space(a);
  gwfmt_stmt_func[b->body->stmt_type](a, &b->body->d);
  if (b->is_do) {
    gwfmt_space(a);
    COLOR(a, "{+M}", "until");
    paren_exp(a, b->cond);
    gwfmt_sc(a);
  }
}

ANN static void gwfmt_stmt_for(Gwfmt *a, Stmt_For b) {
  COLOR(a, "{+M}", "for");
  gwfmt_lparen(a);
  gwfmt_stmt_exp(a, &b->c1->d.stmt_exp);
  gwfmt_space(a);
  const unsigned int py = a->ls->py;
  a->ls->py             = 0;
  if (b->c2) gwfmt_stmt_exp(a, &b->c2->d.stmt_exp);
  a->ls->py = py;
  if (b->c3) {
    gwfmt_space(a);
    gwfmt_exp(a, b->c3);
  }
  gwfmt_rparen(a);
  if (b->body->stmt_type == ae_stmt_code)
    gwfmt_space(a);
  else if (!(b->body->stmt_type == ae_stmt_exp && !b->body->d.stmt_exp.val)) {
    gwfmt_space(a);
    gwfmt_nl(a);
    a->indent++;
    gwfmt_indent(a);
    a->indent--;
  }
  gwfmt_stmt(a, b->body);
}

ANN static void gwfmt_stmt_each(Gwfmt *a, Stmt_Each b) {
  COLOR(a, "{+M}", "foreach");
  gwfmt_lparen(a);
  if(b->idx) {
    gwfmt_symbol(a, b->idx->var.tag.sym);
    gwfmt_comma(a);
    gwfmt_space(a);
  }
  gwfmt_symbol(a, b->tag.sym);
  gwfmt_space(a);
  COLOR(a, "{-}", ":");
  gwfmt_space(a);
  gwfmt_exp(a, b->exp);
  gwfmt_rparen(a);
  if (b->body->stmt_type != ae_stmt_code) gwfmt_nl(a);
  INDENT(a, gwfmt_stmt(a, b->body))
}

ANN static void gwfmt_stmt_loop(Gwfmt *a, Stmt_Loop b) {
  COLOR(a, "{+M}", "repeat");
  gwfmt_lparen(a);
  if (b->idx) {
    gwfmt_symbol(a, b->idx->var.tag.sym);
    gwfmt_comma(a);
    gwfmt_space(a);
  }
  gwfmt_exp(a, b->cond);
  gwfmt_rparen(a);
  if (b->body->stmt_type == ae_stmt_code ||
      (b->body->stmt_type == ae_stmt_exp && !b->body->d.stmt_exp.val))
    gwfmt_stmt(a, b->body);
  else {
    gwfmt_nl(a);
    INDENT(a, gwfmt_stmt(a, b->body))
  }
}

ANN static void gwfmt_code(Gwfmt *a, Stmt* b) {
  if (b->stmt_type == ae_stmt_if || b->stmt_type == ae_stmt_code ||
      (b->stmt_type == ae_stmt_exp && !b->d.stmt_exp.val)) {
    gwfmt_space(a);
    gwfmt_stmt_func[b->stmt_type](a, &b->d);
  } else {
    gwfmt_nl(a);
    INDENT(a, gwfmt_indent(a); gwfmt_stmt_func[b->stmt_type](a, &b->d))
  }
}

ANN static void gwfmt_stmt_if(Gwfmt *a, Stmt_If b) {
  COLOR(a, "{+M}", "if");
  paren_exp(a, b->cond);
  gwfmt_code(a, b->if_body);
  if (b->else_body) {
    gwfmt_space(a);
    COLOR(a, "{+M}", "else");
    gwfmt_code(a, b->else_body);
  }
}

ANN static void gwfmt_stmt_code(Gwfmt *a, Stmt_Code b) {
  gwfmt_lbrace(a);
  if (b->stmt_list) {
    INDENT(a, gwfmt_nl(a); gwfmt_stmt_list(a, b->stmt_list))
    gwfmt_indent(a);
  }
  gwfmt_rbrace(a);
}

ANN static void gwfmt_stmt_break(Gwfmt *a, Stmt_Index b) {
  COLOR(a, "{+M}", "break");
  if (b->idx) {
    gwfmt_space(a);
    struct gwint gwint = GWINT(b->idx, gwint_decimal);
    gwfmt_prim_num(a, &gwint);
  }
  gwfmt_sc(a);
}

ANN static void gwfmt_stmt_continue(Gwfmt *a, Stmt_Index b) {
  COLOR(a, "{+M}", "continue");
  if (b->idx) {
    gwfmt_space(a);
    struct gwint gwint = GWINT(b->idx, gwint_decimal);
    gwfmt_prim_num(a, &gwint);
  }
  gwfmt_sc(a);
}

ANN static void gwfmt_stmt_return(Gwfmt *a, Stmt_Exp b) {
  COLOR(a, "{+M}", "return");
  if (b->val) {
    gwfmt_space(a);
    gwfmt_exp(a, b->val);
  }
  gwfmt_sc(a);
}

ANN static void gwfmt_case_list(Gwfmt *a, Stmt_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Stmt* stmt = mp_vector_at(b, Stmt, i);
    gwfmt_stmt_case(a, &stmt->d.stmt_match);
    if(i < b->len - 1) gwfmt_nl(a);
  }
}

ANN static void gwfmt_stmt_match(Gwfmt *a, Stmt_Match b) {
  COLOR(a, "{+M}", "match");
  gwfmt_space(a);
  gwfmt_exp(a, b->cond);
  gwfmt_space(a);
  gwfmt_lbrace(a);
  gwfmt_nl(a);
  INDENT(a, gwfmt_case_list(a, b->list))
  gwfmt_indent(a);
  gwfmt_nl(a);
  gwfmt_indent(a);
  gwfmt_rbrace(a);
  if (b->where) {
    gwfmt_space(a);
    COLOR(a, "{+M}", "where");
    gwfmt_space(a);
    gwfmt_stmt(a, b->where);
  }
}

ANN static void gwfmt_stmt_case(Gwfmt *a, Stmt_Match b) {
  gwfmt_indent(a);
  COLOR(a, "{+M}", "case");
  gwfmt_space(a);
  gwfmt_exp(a, b->cond);
  if (b->when) {
    gwfmt_space(a);
    COLOR(a, "{+M}", "when");
    gwfmt_space(a);
    gwfmt_exp(a, b->when);
  }
  //  gwfmt_space(a);
  COLOR(a, "{-}", ":");
  if (b->list->len > 1)
    INDENT(a, gwfmt_stmt_list(a, b->list))
  else {
    gwfmt_space(a);
    Stmt* stmt = mp_vector_at(b->list, Stmt, 0);
    gwfmt_stmt_func[stmt->stmt_type](a, &stmt->d);
  }
}

static const char *pp[] = {"!",     "include", "define", "pragma", "undef",
                           "ifdef", "ifndef",  "else",   "endif",  "import", "locale"};

static const char *pp_color[] = {"{-}", "{Y}", "{G}", "{R}", "{W}",
                                 "{B}", "{B}", "{B}", "{B}", "{+C}", "{+C}"};

ANN static inline m_str strip(m_str str) {
  while(isspace(*str))str++;
  return str;
}

ANN static void force_nl(Gwfmt *a) {
  if(a->ls->minimize) {
    text_add(&a->ls->text, "\n");
    a->line++;
  }
}

ANN static void gwfmt_stmt_pp(Gwfmt *a, Stmt_PP b) {
  if (b->pp_type == ae_pp_nl) return;
  if (a->last != cht_nl && b->pp_type != ae_pp_include) gwfmt(a, "\n");
  if (b->pp_type == ae_pp_locale) {
    COLOR(a, "{M/}", "#locale ");
    COLOR(a, "{+C}", s_name(b->xid));
    gwfmt_space(a);
    if(b->exp) gwfmt_exp(a, b->exp);
    return;
  } else if (b->pp_type == ae_pp_include) {
    if(a->ls->ppa->fmt) {
      COLOR(a, "{M/}", "#include");
      gwfmt_space(a);
      color(a, "{Y}");
      gwfmt(a, "<%s>", b->data);
      color(a, "{0}");
    } else return;
  } else if (b->pp_type != ae_pp_comment) {
    color(a, "{M/}");
    gwfmt(a, "#%s ", pp[b->pp_type]);
    COLOR(a, (m_str)pp_color[b->pp_type], b->data ?: "");
  } else if(!a->ls->minimize) {
    gwfmt_indent(a);
    color(a, "{M/}");
    gwfmt(a, "#! ");
    COLOR(a, "{-}", b->data ?: "");
  }
  force_nl(a);
  a->last = cht_nl;
}

ANN static void gwfmt_stmt_defer(Gwfmt *a, Stmt_Defer b) {
  COLOR(a, "{+M}", "defer");
  gwfmt_space(a);
  a->skip_indent++;
  gwfmt_stmt(a, b->stmt);
//  a->skip_indent--;
}

ANN static void gwfmt_stmt(Gwfmt *a, Stmt* b) {
  const uint skip_indent = a->skip_indent;
  if (b->stmt_type != ae_stmt_pp) gwfmt_indent(a);
  gwfmt_stmt_func[b->stmt_type](a, &b->d);
  if (!skip_indent) gwfmt_nl(a);
}

ANN /*static */void gwfmt_arg_list(Gwfmt *a, Arg_List b, const bool locale) {
  for(uint32_t i = locale; i < b->len; i++) {
    Arg *arg = mp_vector_at(b, Arg, i);
    if (arg->var.td) {
      gwfmt_type_decl(a, arg->var.td);
      if (arg->var.vd.tag.sym) gwfmt_space(a);
    }
    gwfmt_var_decl(a, &arg->var.vd);
    if (arg->exp) {
      gwfmt_space(a);
      gwfmt(a, ":");
      gwfmt_space(a);
      gwfmt_exp(a, arg->exp);
    }
    if(i < b->len - 1) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  }
}

ANN static void gwfmt_variable_list(Gwfmt *a, Variable_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Variable *um = mp_vector_at(b, Variable, i);
    gwfmt_indent(a);
    gwfmt_type_decl(a, um->td);
    gwfmt_space(a);
    gwfmt_var_decl(a, &um->vd);
    gwfmt_sc(a);
    gwfmt_nl(a);
  }
}

ANN static void gwfmt_stmt_list(Gwfmt *a, Stmt_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Stmt* stmt = mp_vector_at(b, Stmt, i);
    if (stmt->stmt_type != ae_stmt_exp || stmt->d.stmt_exp.val)
      gwfmt_stmt(a, stmt);
  }
}

ANN static void gwfmt_func_base(Gwfmt *a, Func_Base *b) {
  gwfmt_flag(a, b);
  if (fbflag(b, fbflag_unary)) {
    gwfmt_op(a, b->tag.sym);
    gwfmt_space(a);
    if (b->tmpl) gwfmt_tmpl(a, b->tmpl);
  }
  if (b->td && !fbflag(b, fbflag_locale)) {
    gwfmt_type_decl(a, b->td);
    gwfmt_space(a);
  }
  if (!fbflag(b, fbflag_unary)) {
    if (!fbflag(b, fbflag_op))
      COLOR(a, "{M}", s_name(b->tag.sym));
    else gwfmt_op(a, b->tag.sym);
    if (b->tmpl) gwfmt_tmpl(a, b->tmpl);
  }
  if (fbflag(b, fbflag_op))
    gwfmt_space(a);
  gwfmt_lparen(a);
  if (b->args) gwfmt_arg_list(a, b->args, fbflag(b, fbflag_locale));
  gwfmt_rparen(a);
  if (b->effects.ptr) gwfmt_effects(a, &b->effects);
}

ANN void gwfmt_func_def(Gwfmt *a, Func_Def b) {
  /*
  if(fbflag(b->base, fbflag_compt)) {
    COLOR(a, "{+G}", "const");
    gwfmt_space(a);
  }
  */
  if(fbflag(b->base, fbflag_locale))
    COLOR(a, "{+C}", "locale");
  else if(!fbflag(b->base, fbflag_op) && strcmp(s_name(b->base->tag.sym), "new"))
    COLOR(a, "{+C}", "fun");
  else
    COLOR(a, "{+C}", "operator");
  gwfmt_space(a);
  gwfmt_func_base(a, b->base);
  if (a->ls->builtin) {
    gwfmt_sc(a);
    gwfmt_nl(a);
    return;
  }
  gwfmt_space(a);
  if (!GET_FLAG(b->base, abstract) && b->d.code) {
    gwfmt_lbrace(a);
    gwfmt_nl(a);
    INDENT(a, gwfmt_stmt_list(a, b->d.code));
    gwfmt_indent(a);
    gwfmt_rbrace(a);
  } else {
    gwfmt_sc(a);
  }
  gwfmt_nl(a);
}

ANN void gwfmt_class_def(Gwfmt *a, Class_Def b) {
  COLOR(a, "{+C}", !cflag(b, cflag_struct) ? "class" : "struct");
  gwfmt_space(a);
  gwfmt_flag(a, b);
  COLOR(a, "{+W}", s_name(b->base.tag.sym));
  if (b->base.tmpl) gwfmt_tmpl(a, b->base.tmpl);
  gwfmt_space(a);
  if (b->base.ext) {
    COLOR(a, "{+/C}", "extends");
    gwfmt_space(a);
    gwfmt_type_decl(a, b->base.ext);
    gwfmt_space(a);
  }
  if (b->traits) gwfmt_traits(a, b->traits);
  gwfmt_lbrace(a);
  if (!a->ls->builtin) {
    if (b->body) {
      gwfmt_nl(a);
      INDENT(a, gwfmt_ast(a, b->body))
      gwfmt_indent(a);
    }
    gwfmt_rbrace(a);
  }
  gwfmt_nl(a);
}

ANN static void gwfmt_enum_list(Gwfmt *a, EnumValue_List b) {
    gwfmt_nl(a);
  for(uint32_t i = 0; i < b->len; i++) {
    EnumValue *ev = mp_vector_at(b, EnumValue, i);
    gwfmt_indent(a);
    if(ev->set) {
      gwfmt_prim_num(a, &ev->gwint);
      gwfmt_space(a);
      gwfmt_op(a, insert_symbol(a->st, ":=>"));
      gwfmt_space(a);
    }
    gwfmt_symbol(a, ev->tag.sym);
    if (!a->ls->minimize && (i == b->len - 1)) gwfmt_comma(a);
    gwfmt_nl(a);
  }
}

ANN void gwfmt_enum_def(Gwfmt *a, Enum_Def b) {
  COLOR(a, "{+C}", "enum");
  gwfmt_space(a);
  gwfmt_flag(a, b);
  COLOR(a, "{/}", s_name(b->tag.sym));
  gwfmt_space(a);
  gwfmt_lbrace(a);
  a->indent++;
  gwfmt_enum_list(a, b->list);
  a->indent--;
  gwfmt_rbrace(a);
  gwfmt_nl(a);
}

ANN void gwfmt_union_def(Gwfmt *a, Union_Def b) {
  COLOR(a, "{+C}", "union");
  gwfmt_space(a);
  gwfmt_flag(a, b);
  COLOR(a, "{/}", s_name(b->tag.sym));
  gwfmt_space(a);
  if (b->tmpl) gwfmt_tmpl(a, b->tmpl);
  gwfmt_space(a);
  gwfmt_lbrace(a);
  gwfmt_nl(a);
  INDENT(a, gwfmt_variable_list(a, b->l))
  gwfmt_rbrace(a);
  gwfmt_sc(a);
  gwfmt_nl(a);
}

ANN void gwfmt_fptr_def(Gwfmt *a, Fptr_Def b) {
  COLOR(a, "{+C}", "funptr");
  gwfmt_space(a);
  gwfmt_func_base(a, b->base);
  gwfmt_sc(a);
  gwfmt_nl(a);
}

ANN void gwfmt_type_def(Gwfmt *a, Type_Def b) {
  COLOR(a, "{+C}", !b->distinct ? "typedef" : "distinct");
  gwfmt_space(a);
  if (b->ext) {
    gwfmt_type_decl(a, b->ext);
    gwfmt_space(a);
  }
  COLOR(a, "{/}", s_name(b->tag.sym));
  if (b->tmpl) gwfmt_tmpl(a, b->tmpl);
  if (b->when) {
    gwfmt_nl(a);
    a->indent += 2;
    gwfmt_indent(a);
    COLOR(a, "{M}", "when");
    a->indent -= 2;
    gwfmt_space(a);
    gwfmt_exp(a, b->when);
  }
  gwfmt_sc(a);
  gwfmt_nl(a);
}

ANN static void gwfmt_extend_def(Gwfmt *a, Extend_Def b) {
  COLOR(a, "{+C}", "extends");
  gwfmt_space(a);
  gwfmt_type_decl(a, b->td);
  gwfmt_space(a);
  gwfmt(a, ":");
  gwfmt_space(a);
  gwfmt_id_list(a, b->traits);
  gwfmt_sc(a);
  gwfmt_nl(a);
}

ANN static void gwfmt_trait_def(Gwfmt *a, Trait_Def b) {
  COLOR(a, "{+C}", "trait");
  gwfmt_space(a);
  gwfmt_symbol(a, b->tag.sym);
  gwfmt_space(a);
  if (b->body) {
    gwfmt_lbrace(a);
    gwfmt_nl(a);
    INDENT(a, gwfmt_ast(a, b->body))
    gwfmt_indent(a);
    gwfmt_rbrace(a);
  } else gwfmt_sc(a);
  gwfmt_nl(a);
}

ANN void gwfmt_prim_def(Gwfmt *a, Prim_Def b) {
  COLOR(a, "{+C}", "primitive");
  gwfmt_space(a);
  gwfmt_flag(a, b);
  COLOR(a, "{+W}", s_name(b->tag.sym));
  gwfmt_space(a);
  struct gwint gwint = GWINT(b->size, gwint_decimal);
  gwfmt_prim_num(a, &gwint);
  gwfmt_sc(a);
  gwfmt_nl(a);
}

DECL_SECTION_FUNC(gwfmt, void, Gwfmt *)
ANN static void gwfmt_section(Gwfmt *a, Section *b) {
  if (b->section_type != ae_section_stmt) gwfmt_indent(a);
  gwfmt_section_func[b->section_type](a, *(void **)&b->d);
}

ANN void gwfmt_ast(Gwfmt *a, Ast b) {
  const m_uint sz = b->len;
  for(m_uint i = 0; i < sz; i++) {
    Section *section = mp_vector_at(b, Section, i);
    gwfmt_section(a, section);
    if(i < sz -1) gwfmt_nl(a);
  }
}
