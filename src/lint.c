#include <ctype.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#define LINT_IMPL
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

ANN static enum gwfmt_char_type cht(const char c) {
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

ANN void color(Gwfmt *a, const  char *buf) {
  char tmp[strlen(buf)*4];
  tcol_snprintf(tmp, strlen(buf)*4, buf);
  text_add(&a->ls->text, tmp);
}

ANN void reset_color(Gwfmt *a);

void COLOR(Gwfmt *a, const  char *b, const char *c) {
  color(a, b);
  gwfmt(a, c);
  reset_color(a);
}

ANN void reset_color(Gwfmt *a) {
  color(a, "{0}");
}

void keyword(Gwfmt *a, const  char *kw) {
  COLOR(a, a->ls->config->colors[KeywordColor], kw);
}

void flow(Gwfmt *a, const  char *kw) {
  COLOR(a, a->ls->config->colors[FlowColor], kw);
}
ANN static void check_tag(Gwfmt *a, const  Tag * tag, const CaseType ct,
                          char* color) {
  const Casing casing = a->ls->config->cases[ct];
  if(a->ls->check_case && !casing.check(s_name(tag->sym))) {
   a->ls->error = true;
   if(!a->ls->fix_case) {
      char main[256];
      char info[256];
      snprintf(main, sizeof(main) - 1, "invalid {Y+}%s{0} casing", ct_name[ct]);
      snprintf(info, sizeof(info) - 1, "should be {W+}%s{0} case", casing.name);
      printf("filename: %s\n", a->filename);
      gwlog_error(main, info, a->filename ?: "/dev/null", tag->loc, 0);
    } else {
      char buf[256];
      const char *name = s_name(tag->sym);
      if(strlen(name) > (MAX_COLOR_LENGTH - 2)) {
        gwlog_error("color too long", NULL, a->filename, tag->loc, 0);
        return;
      }
      if(casing.fix(name, buf, 256)) {
        COLOR(a, color, buf);
        return;
      }
      gwlog_error("can't convert case", NULL, a->filename, tag->loc, 0);
    }
  }
  // comments
  COLOR(a, color, s_name(tag->sym));
}

ANN void type_tag(Gwfmt *a, const Tag *tag) {
  check_tag(a, tag, TypeCase, a->ls->config->colors[TypeColor]);
}

ANN int gwfmt_util(Gwfmt *a, const  char *fmt, ...) {
  va_list ap;
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
  return ret;
}

ANN static void handle_space(Gwfmt *a, const char c) {
  if ((a->need_space && a->last != cht_delim && cht(c) != cht_delim && a->last == cht(c)) ||

      (a->last == cht_colon /*&& (*buf == cht_lbrack || *buf == cht_op)*/)) {
    text_add(&a->ls->text, " ");
    a->pos.column += 1;
  }
  a->need_space = 0;
}

ANN void gwfmt(Gwfmt *a, const  char *fmt, ...) {
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
  a->pos.column += n;
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
        gwfmt_util(a, " {-}% 4u{0}", a->pos.line);//root
//        if (a->ls->mark == a->pos.line)
//          gwfmt_util(a, " {+R}>{0}");
//        else
          gwfmt_util(a, "  ");
        gwfmt_util(a, "{N}┃{0} "); //
      }
    }
  }
  a->pos.column = a->ls->base_column;
  a->pos.line++;
  a->nl = nl;
}

ANN static m_str gwfmt_verbatim(Gwfmt *a, m_str b) {
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
     a->pos.column++;
  }
  a->last = cht(last);
  return NULL;
}

ANN static void gwfmt_op(Gwfmt *a, const  Symbol b) {
  m_str s = s_name(b);
  handle_space(a, *s);
  color(a, a->ls->config->colors[OpColor]);
  gwfmt_verbatim(a, s_name(b));
  reset_color(a);
}

ANN void modifier(Gwfmt *a, const  char *mod) {
  COLOR(a, a->ls->config->colors[ModColor], mod);
}

ANN void operator(Gwfmt *a, const  char *op) {
  COLOR(a, a->ls->config->colors[OpColor], op);
}


ANN void punctuation(Gwfmt *a, const  char *punct) {
  COLOR(a, a->ls->config->colors[PunctuationColor], punct);
}

ANN void gwfmt_lbrace(Gwfmt *a) {
  if (!a->ls->py) punctuation(a, "{");
}

ANN void gwfmt_rbrace(Gwfmt *a) {
  if (!a->ls->py) punctuation(a, "}");
}

ANN void gwfmt_sc(Gwfmt *a) {
  if (!a->ls->py) punctuation(a, ";");
}

ANN void gwfmt_comma(Gwfmt *a) {
  if (!a->ls->py) punctuation(a, ",");
}

ANN void gwfmt_lparen(Gwfmt *a) { punctuation(a, "("); }

ANN void gwfmt_rparen(Gwfmt *a) { punctuation(a, ")");
  a->last = cht_delim;

}

ANN static inline void gwfmt_lbrack(Gwfmt *a) { punctuation(a, "["); }

ANN static inline void gwfmt_rbrack(Gwfmt *a) { punctuation(a, "]"); }

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

ANN static void paren_exp(Gwfmt *a, const Exp* b);
ANN static void maybe_paren_exp(Gwfmt *a, const Exp* b);
ANN static void gwfmt_array_sub2(Gwfmt *a, const Array_Sub b);
ANN static void gwfmt_prim_interp(Gwfmt *a, const Exp* *b);

#define _FLAG(a, b) (((a)&ae_flag_##b) == (ae_flag_##b))
ANN static void gwfmt_flag(Gwfmt *a, const ae_flag b) {
  bool state = true;
  if (_FLAG(b, private))
    modifier(a, "private");
  else if (_FLAG(b, protect))
    modifier(a, "protect");
  else state = false;
  if (state) gwfmt_space(a);
  state = true;
  if (_FLAG(b, export)) {
    modifier(a, "export");
    gwfmt_space(a);
  }
  if (_FLAG(b, static))
    modifier(a, "static");
  else state = false;
  if (state) gwfmt_space(a);
  state = true;
  if (_FLAG(b, abstract))
    modifier(a, "abstract");
  else if (_FLAG(b, final))
    modifier(a, "final");
  else state = false;
  if (state) gwfmt_space(a);
}
#define gwfmt_flag(a, b) gwfmt_flag(a, b->flag)

ANN static void gwfmt_symbol(Gwfmt *a, const Symbol b) {
  const m_str s = s_name(b);
  if (!strcmp(s, "true") || !strcmp(s, "false") || !strcmp(s, "maybe") ||
      !strcmp(s, "adc") || !strcmp(s, "dac") || !strcmp(s, "now"))
    COLOR(a, a->ls->config->colors[SpecialColor], s_name(b));
  else
    gwfmt(a, s);
}

ANN static void gwfmt_tag(Gwfmt *a, const Tag *b) {
  // check comments
  gwfmt_symbol(a, b->sym);
}

ANN static void gwfmt_array_sub(Gwfmt *a, const Array_Sub b) {
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

ANN static void gwfmt_taglist(Gwfmt *a, const TagList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Tag tag = taglist_at(b, i);
    gwfmt_tag(a, &tag);
    if(i < b->len - 1) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  }
}

ANN void gwfmt_traits(Gwfmt *a, const TagList *b) {
  punctuation(a, ":");
  gwfmt_space(a);
  gwfmt_taglist(a, b);
  gwfmt_space(a);
}

ANN static void gwfmt_specialized_list(Gwfmt *a, const  SpecializedList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Specialized spec = specializedlist_at(b, i);
    if(spec.td) {
      modifier(a, "const");
      gwfmt_space(a);
      gwfmt_type_decl(a, spec.td);
      gwfmt_space(a);
    }
    COLOR(a, a->ls->config->colors[TypeColor], s_name(spec.tag.sym));
    if (spec.traits) {
      gwfmt_space(a);
      gwfmt_traits(a, spec.traits);
    }
    if(i < b->len - 1) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  }
}

ANN static void _gwfmt_tmplarg_list(Gwfmt *a, const  TmplArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const TmplArg targ = tmplarglist_at(b, i);
    if (targ.type == tmplarg_td)
     gwfmt_type_decl(a, targ.d.td);
    else gwfmt_exp(a, targ.d.exp);
    if(i < b->len - 1) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  }
}

ANN static inline void gwfmt_init_tmpl(Gwfmt *a) { punctuation(a, ":["); }

ANN static void gwfmt_tmplarg_list(Gwfmt *a, const  TmplArgList *b) {
  gwfmt_init_tmpl(a);
  gwfmt_space(a);
  _gwfmt_tmplarg_list(a, b);
  gwfmt_space(a);
  gwfmt_rbrack(a);
}

ANN static void gwfmt_tmpl(Gwfmt *a, const Tmpl *b) {
  if (!b->call) {
    gwfmt_init_tmpl(a);
    gwfmt_space(a);
    gwfmt_specialized_list(a, b->list);
    gwfmt_space(a);
    gwfmt_rbrack(a);
  }
  if (b->call) gwfmt_tmplarg_list(a, b->call);
}

ANN static void gwfmt_range(Gwfmt *a, const Range *b) {
  gwfmt_lbrack(a);
  if (b->start) gwfmt_exp(a, b->start);
  gwfmt_space(a);
  punctuation(a, ":");
  gwfmt_space(a);
  if (b->end) gwfmt_exp(a, b->end);
  gwfmt_rbrack(a);
}

ANN void gwfmt_exp1(Gwfmt *a, const Exp* b);
ANN static void gwfmt_prim_dict(Gwfmt *a, const Exp* *b) {
  const Exp* e = *b;
  gwfmt_lbrace(a);
  gwfmt_space(a);
  do {
    Exp* next  = e->next;
    Exp* nnext = next->next;
//    e->next = NULL;
//    next->next = NULL;
    gwfmt_exp1(a, e);
    gwfmt_space(a);
    punctuation(a, ":");
    gwfmt_space(a);
    gwfmt_exp1(a, next);
//    e->next = next;
//    next->next = nnext;
    if(nnext) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  } while ((e = e->next->next));
  gwfmt_space(a);
  gwfmt_rbrace(a);
}

ANN static void gwfmt_effect(Gwfmt *a, const Symbol b) {
  COLOR(a, a->ls->config->colors[FlowColor], s_name(b));
}

ANN static void gwfmt_perform(Gwfmt *a) {
  flow(a, "perform");
  gwfmt_space(a);
}

ANN static void gwfmt_prim_perform(Gwfmt *a, const Symbol *b) {
  flow(a, "perform");
  gwfmt_space(a);
  gwfmt_effect(a, *b);
}

ANN static void gwfmt_effects(Gwfmt *a, const struct Vector_ *b) {
  gwfmt_perform(a);
  for (m_uint i = 0; i < vector_size(b) - 1; i++) {
    gwfmt_effect(a, (Symbol)vector_at(b, i));
    gwfmt_space(a);
  }
  gwfmt_effect(a, (Symbol)vector_back(b));
}

ANN void gwfmt_type_decl(Gwfmt *a, const Type_Decl *b) {
  if (GET_FLAG(b, const)) {
    modifier(a, "const");
    gwfmt_space(a);
  }
  if (GET_FLAG(b, late)) {
    modifier(a, "late");
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
  } else if (b->tag.sym)
    COLOR(a, a->ls->config->colors[TypeColor], s_name(b->tag.sym));
  if (b->types) gwfmt_tmplarg_list(a, b->types);
  for (m_uint i = 0; i < b->option; ++i) gwfmt(a, "?");
  if (b->array) gwfmt_array_sub2(a, b->array);
  if (b->next) {
    gwfmt(a, ".");
    gwfmt_type_decl(a, b->next);
  }
}

ANN static void gwfmt_prim_id(Gwfmt *a, const Symbol *b) { gwfmt_symbol(a, *b); }


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

ANN static void gwfmt_decimal(Gwfmt *a, const m_int num) {
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

ANN static void gwfmt_binary(Gwfmt *a, const m_int num) {
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

ANN static void gwfmt_hexa(Gwfmt *a, const m_int num) {
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

ANN static void gwfmt_octal(Gwfmt *a, const m_int num) {
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

ANN static void gwfmt_prim_num(Gwfmt *a, const struct gwint *b) {
  color(a, a->ls->config->colors[NumberColor]);
  switch(b->int_type) {
    case gwint_decimal: gwfmt_decimal(a, b->num); break;
    case gwint_binary:  gwfmt_binary(a, b->num); break;
    case gwint_hexa:    gwfmt_hexa(a, b->num); break;
    case gwint_octal:   gwfmt_octal(a, b->num); break;
  }
  reset_color(a);
}

ANN static void gwfmt_prim_float(Gwfmt *a, const m_float *b) {
  color(a, a->ls->config->colors[NumberColor]);
  if (*b == floor(*b))
    gwfmt(a, "%li.", (m_int)*b);
  else
    gwfmt(a, "%g", *b);
  reset_color(a);
}

ANN static void gwfmt_string(Gwfmt *a, const m_str str) {
  m_str s = str;
  while((s = gwfmt_verbatim(a, s))) {
      reset_color(a);
      gwfmt_nl(a);
      color(a, a->ls->config->colors[StringColor]);
  }
}

ANN static void gwfmt_delim(Gwfmt *a, const uint16_t delim) {
  reset_color(a);
  color(a, a->ls->config->colors[StringColor]);
  if(delim) {
    gwfmt(a, "%.*c",  delim, '#');
  }
  gwfmt(a, "\"");
}

ANN static void gwfmt_delim2(Gwfmt *a, const uint16_t delim) {
  reset_color(a);
  color(a, a->ls->config->colors[StringColor]);
  gwfmt(a, "\"");
  if(delim) {
    gwfmt(a, "%.*c",  delim, '#');
  }
  reset_color(a);
}

ANN static void gwfmt_prim_str(Gwfmt *a, const struct AstString *b) {
  gwfmt_delim(a, b->delim);
  gwfmt_string(a, b->data);
  gwfmt_delim2(a, b->delim);
}

ANN static void gwfmt_prim_array(Gwfmt *a, const Array_Sub *b) {
  gwfmt_array_sub(a, *b);
}

ANN static void gwfmt_prim_range(Gwfmt *a, const Range **b) { gwfmt_range(a, *b); }

ANN static void gwfmt_prim_hack(Gwfmt *a, const Exp* *b) {
  reset_color(a);
  punctuation(a, "<<<");
  gwfmt_space(a);
  gwfmt_exp(a, *b);
  gwfmt_space(a);
  reset_color(a);
  punctuation(a, ">>>");
}

ANN static void gwfmt_prim_char(Gwfmt *a, const m_str *b) {
  color(a, a->ls->config->colors[StringColor]);
  gwfmt(a, "'%s'", *b);
  reset_color(a);
}

ANN static void gwfmt_prim_nil(Gwfmt *a, const void *b NUSED) {
  gwfmt_lparen(a);
  gwfmt_rparen(a);
}

ANN void gwfmt_prim_locale(Gwfmt *a, const Symbol *b) {
  gwfmt(a, "`");
  gwfmt_symbol(a, *b);
  gwfmt(a, "`");
}

DECL_PRIM_FUNC(gwfmt, void, Gwfmt *, const)
ANN static void gwfmt_prim(Gwfmt *a, const Exp_Primary *b) {
  gwfmt_prim_func[b->prim_type](a, &b->d);
}

ANN static void variable_tag(Gwfmt *a, const  Tag *b) {
    check_tag(a, b, VariableCase, a->ls->config->colors[VariableColor]);
}

ANN static void gwfmt_var_decl(Gwfmt *a, const  Var_Decl *b) {
  if (b->tag.sym) 
    variable_tag(a, &b->tag);
}

ANN static void gwfmt_exp_decl(Gwfmt *a, const Exp_Decl *b) {
  if (b->var.td) {
    if (!(GET_FLAG(b->var.td, const) || GET_FLAG(b->var.td, late))) {
      modifier(a, "var");
      gwfmt_space(a);
    }
    gwfmt_type_decl(a, b->var.td);
    if(b->args) paren_exp(a, b->args);
    gwfmt_space(a);
  }
  gwfmt_var_decl(a, &b->var.vd);
}

ANN static void gwfmt_exp_td(Gwfmt *a, const Type_Decl *b) {
  operator(a, "$");
  //  gwfmt_space(a);
  gwfmt_type_decl(a, b);
}

ANN static void gwfmt_exp_binary(Gwfmt *a, const Exp_Binary *b) {
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

ANN static void gwfmt_captures(Gwfmt *a, const CaptureList *b) {
  punctuation(a, ":");
  gwfmt_space(a);
  for (uint32_t i = 0; i < b->len; i++) {
    const Capture cap = capturelist_at(b, i);
    if(cap.is_ref) gwfmt(a, "&");
    gwfmt_var_decl(a, &cap.var);
    gwfmt_space(a);
  }
  punctuation(a, ":");
}

ANN static void gwfmt_exp_unary(Gwfmt *a, const Exp_Unary *b) {
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

ANN static void gwfmt_exp_cast(Gwfmt *a, const Exp_Cast *b) {
  if (b->exp->exp_type != ae_exp_decl)
    gwfmt_exp(a, b->exp);
  else
    paren_exp(a, b->exp);
  gwfmt_space(a);
  operator(a, "$");
  gwfmt_space(a);
  gwfmt_type_decl(a, b->td);
}

ANN static void gwfmt_exp_post(Gwfmt *a, const Exp_Postfix *b) {
  gwfmt_exp(a, b->exp);
  gwfmt_op(a, b->op);
}

ANN static void gwfmt_exp_call(Gwfmt *a, const Exp_Call *b) {
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

ANN static void gwfmt_exp_array(Gwfmt *a, const Exp_Array *b) {
  gwfmt_exp(a, b->base);
  gwfmt_array_sub2(a, b->array);
}

ANN static void gwfmt_exp_slice(Gwfmt *a, const Exp_Slice *b) {
  if (b->base->exp_type != ae_exp_primary &&
      b->base->exp_type != ae_exp_array && b->base->exp_type != ae_exp_call &&
      b->base->exp_type != ae_exp_post && b->base->exp_type != ae_exp_dot)
    gwfmt_exp(a, b->base);
  else
    paren_exp(a, b->base);
  gwfmt_range(a, b->range);
}

ANN static void gwfmt_exp_if(Gwfmt *a, const Exp_If *b) {
  gwfmt_exp(a, b->cond);
  gwfmt_space(a);
  gwfmt(a, "?");
  if (b->if_exp) {
    gwfmt_space(a);
    gwfmt_exp(a, b->if_exp);
    gwfmt_space(a);
  }
  punctuation(a, ":");
  gwfmt_exp(a, b->else_exp);
}

ANN static void gwfmt_exp_dot(Gwfmt *a, const Exp_Dot *b) {
  gwfmt_exp(a, b->base);
  gwfmt(a, ".");
  gwfmt_tag(a, &b->var.tag);
}

ANN static void gwfmt_lambda_list(Gwfmt *a, const ArgList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Arg arg = arglist_at(b, i);
    gwfmt_variable(a, &arg.var);
    gwfmt_space(a);
  }
}

ANN static void gwfmt_exp_lambda(Gwfmt *a, const Exp_Lambda *b) {
  gwfmt(a, "\\");
  if (b->def->base->args) {
    gwfmt_lambda_list(a, b->def->base->args);
  }
  if (b->def->captures) gwfmt_captures(a, b->def->captures);
  if (b->def->d.code) {
    StmtList *code = b->def->d.code;
    if(stmtlist_len(code) != 1)
      gwfmt_stmt_list(a, code); // brackets?
    else {
      gwfmt_lbrace(a);
      gwfmt_space(a);
      gwfmt_exp(a, stmtlist_at(code, 0).d.stmt_exp.val);
      gwfmt_space(a);
      gwfmt_rbrace(a);
    }
  }
}

ANN static void gwfmt_exp_named(Gwfmt *a, const Exp_Named *b) {
  gwfmt_tag(a, &b->tag);
  gwfmt_space(a);
  gwfmt_op(a, insert_symbol(a->st, "="));
  gwfmt_space(a);
  gwfmt_exp(a, b->exp);
}

DECL_EXP_FUNC(gwfmt, void, Gwfmt *, const)
ANN void gwfmt_exp1(Gwfmt *a, const Exp* b) {
  if(b->paren) gwfmt_lparen(a);
  gwfmt_exp_func[b->exp_type](a, &b->d);
  if(b->paren) gwfmt_rparen(a);
}

ANN void gwfmt_exp(Gwfmt *a, const Exp* b) {
  gwfmt_exp1(a, b);
  NEXT(a, b, gwfmt_exp)
}

ANN static void gwfmt_prim_interp(Gwfmt *a, const Exp* *b) {
  const Exp* e = *b;
  const uint16_t delim = e->d.prim.d.string.delim;
  gwfmt_delim(a, delim);
  color(a, a->ls->config->colors[PunctuationColor]);
  while (e) {
    if (e->exp_type == ae_exp_primary && e->d.prim.prim_type == ae_prim_str) {
      gwfmt_string(a,  e->d.prim.d.string.data);
    } else {
      reset_color(a);
      COLOR(a, a->ls->config->colors[PunctuationColor], "${");
      gwfmt_space(a);
      gwfmt_exp(a, e);
//      gwfmt_exp_func[e->exp_type](a, &e->d);
      gwfmt_space(a);
      COLOR(a, a->ls->config->colors[PunctuationColor], "}");
      color(a, a->ls->config->colors[StringColor]);
    }
    e = e->next;
  }
  gwfmt_delim2(a, delim);
  color(a, a->ls->config->colors[StringColor]);
}

ANN static void gwfmt_array_sub2(Gwfmt *a, const Array_Sub b) {
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

ANN static void paren_exp(Gwfmt *a, const Exp* b) {
  gwfmt_lparen(a);
  //if(b->exp_type != ae_exp_primary &&
  //   b->d.prim.prim_type != ae_prim_nil)
    gwfmt_exp(a, b);
  gwfmt_rparen(a);
}

ANN static void maybe_paren_exp(Gwfmt *a, const Exp* b) {
  if (b->next)
    paren_exp(a, b);
  else
    gwfmt_exp(a, b);
}

ANN static void gwfmt_stmt_exp(Gwfmt *a, const Stmt_Exp b) {
  if (b->val) gwfmt_exp(a, b->val);
  gwfmt_sc(a);
}

ANN static void gwfmt_stmt_retry(Gwfmt *a, const Stmt_Exp b NUSED) {
  flow(a, "retry;");
}

ANN static void gwfmt_handler_list(Gwfmt *a, const HandlerList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Handler handler = handlerlist_at(b, i);
    flow(a, "handle");
    gwfmt_space(a);
    if (handler.tag.sym) {
      Tag tag = handler.tag;
      type_tag(a, &tag);
      gwfmt_space(a);
    }
    const uint indent = a->skip_indent++;
    gwfmt_stmt(a, handler.stmt);
    a->skip_indent = indent;
  }
}

ANN static void gwfmt_stmt_try(Gwfmt *a, const Stmt_Try b) {
  flow(a, "try");
  gwfmt_space(a);
  const uint indent = a->skip_indent++;
  gwfmt_stmt(a, b->stmt);
  gwfmt_space(a);
  a->skip_indent = indent;
  gwfmt_handler_list(a, b->handler);
}

ANN static void gwfmt_stmt_spread(Gwfmt *a, const Spread_Def b) {
  punctuation(a, "...");
  gwfmt_space(a);
  variable_tag(a, &b->tag);
  gwfmt_space(a);
  punctuation(a, ":");
  gwfmt_space(a);
  gwfmt_taglist(a, b->list);
  gwfmt_space(a);
  gwfmt_lbrace(a);
  gwfmt_nl(a);
  a->indent++;
  PPArg ppa = {};
  pparg_ini(a->mp, &ppa);
  FILE *file = fmemopen(b->data, strlen(b->data), "r");
  struct AstGetter_ arg = {
    .name = "", 
    .f = file,
    .st = a->st, 
    .ppa = &ppa
  };
  Ast tmp =    parse(&arg);
  if(tmp) gwfmt_ast(a, tmp);
  pparg_end(&ppa);
  a->indent--;
  gwfmt_indent(a);
  gwfmt_rbrace(a);
  punctuation(a, "...");
  gwfmt_nl(a);
}

ANN static void gwfmt_stmt_using(Gwfmt *a, const Stmt_Using b) {
  COLOR(a, a->ls->config->colors[KeywordColor], "using");
  gwfmt_space(a);
  if(b->tag.sym) {
    gwfmt_tag(a, &b->tag);
    gwfmt_space(a);
    gwfmt(a, ":");
    gwfmt_exp(a, b->d.exp);
  } else
    gwfmt_type_decl(a, b->d.td);
  gwfmt_sc(a);
}

ANN static void gwfmt_import_list(Gwfmt *a, const UsingStmtList *b) {
  a->indent++;
  for(uint32_t i = 0; i < b->len; i++) {
    const UsingStmt item = usingstmtlist_at(b, i);
    gwfmt_nl(a);
    gwfmt_indent(a);
    variable_tag(a, &item.tag);
    if(item.d.exp) {
      gwfmt_space(a);
      gwfmt(a, ":");
      gwfmt_space(a);
      gwfmt_exp(a, item.d.exp);
    }
  }
  a->indent--;
}

ANN static void gwfmt_stmt_import(Gwfmt *a, const Stmt_Import b) {
  COLOR(a, a->ls->config->colors[KeywordColor], "import");
  gwfmt_space(a);
  type_tag(a, &b->tag);
  if(b->selection) {
    gwfmt_space(a);
    gwfmt(a, ":");
    gwfmt_import_list(a, b->selection);
  }
  gwfmt_sc(a);
  gwfmt_nl(a);
}

ANN static void gwfmt_stmt_require(Gwfmt *a, const Stmt_Require b) {
  COLOR(a, a->ls->config->colors[KeywordColor], "require");
  gwfmt_space(a);
  for(uint32_t i = 0; i < b->tags->len; i++) {
    if(i) gwfmt(a, ".");
    const Tag tag = taglist_at(b->tags, i);
    gwfmt_symbol(a, tag.sym);
  }
  gwfmt_sc(a);
  gwfmt_nl(a);
}


DECL_STMT_FUNC(gwfmt, void, Gwfmt *, const)
ANN static void gwfmt_stmt_while(Gwfmt *a, const Stmt_Flow b) {
  if (!b->is_do) {
    flow(a, "while");
    paren_exp(a, b->cond);
  } else
    flow(a, "do");
  if (b->body->stmt_type != ae_stmt_exp || b->body->d.stmt_exp.val)
    gwfmt_space(a);
  gwfmt_stmt_func[b->body->stmt_type](a, &b->body->d);
  if (b->is_do) {
    gwfmt_space(a);
    flow(a, "while");
    paren_exp(a, b->cond);
    gwfmt_sc(a);
  }
}

ANN static void gwfmt_stmt_until(Gwfmt *a, const Stmt_Flow b) {
  if (!b->is_do) {
    flow(a, "until");
    paren_exp(a, b->cond);
  } else
    flow(a, "do");
  gwfmt_space(a);
  gwfmt_stmt_func[b->body->stmt_type](a, &b->body->d);
  if (b->is_do) {
    gwfmt_space(a);
    flow(a, "until");
    paren_exp(a, b->cond);
    gwfmt_sc(a);
  }
}

ANN static void gwfmt_stmt_for(Gwfmt *a, const Stmt_For b) {
  flow(a, "for");
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

ANN static void gwfmt_stmt_each(Gwfmt *a, const Stmt_Each b) {
  flow(a, "foreach");
  gwfmt_lparen(a);
  if(b->idx.tag.sym) {
    gwfmt_var_decl(a, &b->idx);
    gwfmt_comma(a);
    gwfmt_space(a);
  }
  gwfmt_var_decl(a, &b->var);
  gwfmt_space(a);
  punctuation(a, ":");
  gwfmt_space(a);
  gwfmt_exp(a, b->exp);
  gwfmt_rparen(a);
  if (b->body->stmt_type != ae_stmt_code) gwfmt_nl(a);
  INDENT(a, gwfmt_stmt(a, b->body))
}

ANN static void gwfmt_stmt_loop(Gwfmt *a, const Stmt_Loop b) {
  flow(a, "repeat");
  gwfmt_lparen(a);
  if (b->idx.tag.sym) {
    gwfmt_var_decl(a, &b->idx);
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

ANN static void gwfmt_code(Gwfmt *a, const Stmt* b) {
  if (b->stmt_type == ae_stmt_if || b->stmt_type == ae_stmt_code ||
      (b->stmt_type == ae_stmt_exp && !b->d.stmt_exp.val)) {
    gwfmt_space(a);
    gwfmt_stmt_func[b->stmt_type](a, &b->d);
  } else {
    gwfmt_nl(a);
    INDENT(a, gwfmt_indent(a); gwfmt_stmt_func[b->stmt_type](a, &b->d))
  }
}

ANN static void gwfmt_stmt_if(Gwfmt *a, const Stmt_If b) {
  flow(a, "if");
  paren_exp(a, b->cond);
  gwfmt_code(a, b->if_body);
  if (b->else_body) {
    gwfmt_space(a);
    flow(a, "else");
    gwfmt_code(a, b->else_body);
  }
}

ANN static void gwfmt_stmt_code(Gwfmt *a, const Stmt_Code b) {
  gwfmt_lbrace(a);
  if (b->stmt_list) {
    INDENT(a, gwfmt_nl(a); gwfmt_stmt_list(a, b->stmt_list))
    gwfmt_indent(a);
  }
  gwfmt_rbrace(a);
}

ANN static void gwfmt_stmt_break(Gwfmt *a, const Stmt_Index b) {
  flow(a, "break");
  if (b->idx) {
    gwfmt_space(a);
    struct gwint gwint = GWINT(b->idx, gwint_decimal);
    gwfmt_prim_num(a, &gwint);
  }
  gwfmt_sc(a);
}

ANN static void gwfmt_stmt_continue(Gwfmt *a, const Stmt_Index b) {
  flow(a, "continue");
  if (b->idx) {
    gwfmt_space(a);
    struct gwint gwint = GWINT(b->idx, gwint_decimal);
    gwfmt_prim_num(a, &gwint);
  }
  gwfmt_sc(a);
}

ANN static void gwfmt_stmt_return(Gwfmt *a, const Stmt_Exp b) {
  flow(a, "return");
  if (b->val) {
    gwfmt_space(a);
    gwfmt_exp(a, b->val);
  }
  gwfmt_sc(a);
}

ANN static void gwfmt_case_list(Gwfmt *a, const StmtList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt stmt = stmtlist_at(b, i);
    gwfmt_stmt_case(a, &stmt.d.stmt_match);
    if(i < b->len - 1) gwfmt_nl(a);
  }
}

ANN static void gwfmt_stmt_match(Gwfmt *a, const struct Match *b) {
  flow(a, "match");
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
    flow(a, "where");
    gwfmt_space(a);
    gwfmt_stmt(a, b->where);
  }
}

ANN static void gwfmt_stmt_case(Gwfmt *a, const struct Match *b) {
  gwfmt_indent(a);
  flow(a, "case");
  gwfmt_space(a);
  gwfmt_exp(a, b->cond);
  if (b->when) {
    gwfmt_space(a);
    flow(a, "when");
    gwfmt_space(a);
    gwfmt_exp(a, b->when);
  }
  //  gwfmt_space(a);
  punctuation(a, ":");
  if (b->list->len > 1)
    INDENT(a, gwfmt_stmt_list(a, b->list))
  else {
    gwfmt_space(a);
    const Stmt stmt = stmtlist_at(b->list, 0);
    gwfmt_stmt_func[stmt.stmt_type](a, &stmt.d);
  }
}

static const char *pp[] = {"!",     "define", "pragma", "undef",
                           "ifdef", "ifndef",  "else",   "endif",  "locale"};

ANN static void force_nl(Gwfmt *a) {
  if(a->ls->minimize) {
    text_add(&a->ls->text, "\n");
    a->pos.line++;
  }
}

ANN static void gwfmt_stmt_pp(Gwfmt *a, const  Stmt_PP b) {
  if (b->pp_type == ae_pp_nl) return;
  if (a->last != cht_nl) gwfmt_nl(a);
  color(a, a->ls->config->colors[PPColor]);
//    color(a, "{-M}");
  gwfmt(a, "#%s", pp[b->pp_type]);
  reset_color(a);
  gwfmt_space(a);
  if (b->pp_type == ae_pp_locale) {
    COLOR(a, a->ls->config->colors[FunctionColor], s_name(b->xid));
    gwfmt(a, s_name(b->xid));
    gwfmt_space(a);
    if(b->exp) gwfmt_exp(a, b->exp);
  } else if(b->data)
     gwfmt(a, b->data);
  reset_color(a);
  force_nl(a);
  a->last = cht_nl;
}

ANN static void gwfmt_stmt_defer(Gwfmt *a, const  Stmt_Defer b) {
  flow(a, "defer");
  gwfmt_space(a);
  a->skip_indent++;
  gwfmt_stmt(a, b->stmt);
//  a->skip_indent--;
}

ANN static void gwfmt_stmt(Gwfmt *a, const  Stmt* b) {
  const uint skip_indent = a->skip_indent;
  if (b->stmt_type != ae_stmt_pp) gwfmt_indent(a);
  gwfmt_stmt_func[b->stmt_type](a, &b->d);
  if (!skip_indent) gwfmt_nl(a);
}

ANN void gwfmt_variable(Gwfmt *a, const  Variable *b) {
  if (b->td) {
    gwfmt_type_decl(a, b->td);
    if (b->vd.tag.sym) gwfmt_space(a);
   }
  gwfmt_var_decl(a, &b->vd);
}
ANN /*static */void gwfmt_arg_list(Gwfmt *a, const  ArgList *b, const bool locale) {
  for(uint32_t i = locale; i < b->len; i++) {
    const Arg arg = arglist_at(b, i);
    gwfmt_variable(a, &arg.var);
    if (arg.exp) {
      gwfmt_space(a);
      punctuation(a, ":");
      gwfmt_space(a);
      gwfmt_exp(a, arg.exp);
    }
    if(i < b->len - 1) {
      gwfmt_comma(a);
      gwfmt_space(a);
    }
  }
}

ANN static void gwfmt_variable_list(Gwfmt *a, const  VariableList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Variable um = variablelist_at(b, i);
    gwfmt_indent(a);
    gwfmt_variable(a, &um);
    gwfmt_sc(a);
    gwfmt_nl(a);
  }
}

ANN static void gwfmt_stmt_list(Gwfmt *a, const  StmtList *b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt stmt = stmtlist_at(b, i);
    if (stmt.stmt_type != ae_stmt_exp || stmt.d.stmt_exp.val)
      gwfmt_stmt(a, &stmt);
  }
}

ANN static void gwfmt_func_base(Gwfmt *a, const  Func_Base *b) {
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
    if (!fbflag(b, fbflag_op)) {
      check_tag(a, &b->tag, FunctionCase, 
a->ls->config->colors[FunctionColor]
                );
    } else gwfmt_op(a, b->tag.sym);
    if (b->tmpl) gwfmt_tmpl(a, b->tmpl);
  }
  if (fbflag(b, fbflag_op))
    gwfmt_space(a);
  gwfmt_lparen(a);
  if (b->args) gwfmt_arg_list(a, b->args, fbflag(b, fbflag_locale));
  gwfmt_rparen(a);
  if (b->effects.ptr) gwfmt_effects(a, &b->effects);
}

ANN void gwfmt_func_def(Gwfmt *a, const Func_Def b) {
  /*
  if(fbflag(b->base, fbflag_compt)) {
    modifier("const");
    gwfmt_space(a);
  }
  */
  if(fbflag(b->base, fbflag_locale))
    keyword(a, "locale");
  else if(!fbflag(b->base, fbflag_op) && strcmp(s_name(b->base->tag.sym), "new"))
    keyword(a, "fun");
  else
    keyword(a, "operator");
  gwfmt_space(a);
  gwfmt_func_base(a, b->base);
  if (a->ls->builtin) {
    gwfmt_sc(a);
    gwfmt_nl(a);
    return;
  }
  if (!GET_FLAG(b->base, abstract) && !b->builtin && b->d.code) {
    gwfmt_space(a);
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

ANN static void gwfmt_extends(Gwfmt *a, const  Type_Decl *td) {
    keyword(a, "extends");
    gwfmt_space(a);
    gwfmt_type_decl(a, td);
    gwfmt_space(a);
}

ANN void gwfmt_class_def(Gwfmt *a, const Class_Def b) {
  keyword(a, !cflag(b, cflag_struct) ? "class" : "struct");
  gwfmt_space(a);
  gwfmt_flag(a, b);
  type_tag(a, &b->base.tag);
  if (b->base.tmpl) gwfmt_tmpl(a, b->base.tmpl);
  gwfmt_space(a);
  if (b->base.ext)
    gwfmt_extends(a, b->base.ext);
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

ANN static void gwfmt_enum_list(Gwfmt *a, const EnumValueList *b) {
    gwfmt_nl(a);
  for(uint32_t i = 0; i < b->len; i++) {
    const EnumValue ev = enumvaluelist_at(b, i);
    gwfmt_indent(a);
    if(ev.set) {
      struct gwint num = ev.gwint;
      gwfmt_prim_num(a, &num);
      gwfmt_space(a);
      gwfmt_op(a, insert_symbol(a->st, ":=>"));
      gwfmt_space(a);
    }
    variable_tag(a, &ev.tag);
    if (!a->ls->minimize && (i == b->len - 1)) gwfmt_comma(a);
    gwfmt_nl(a);
  }
}

ANN void gwfmt_enum_def(Gwfmt *a, const Enum_Def b) {
  keyword(a, "enum");
  gwfmt_space(a);
  gwfmt_flag(a, b);
  type_tag(a, &b->tag);
  gwfmt_space(a);
  gwfmt_lbrace(a);
  a->indent++;
  gwfmt_enum_list(a, b->list);
  a->indent--;
  gwfmt_rbrace(a);
  gwfmt_nl(a);
}

ANN void gwfmt_union_def(Gwfmt *a, const Union_Def b) {
  keyword(a, "union");
  gwfmt_space(a);
  gwfmt_flag(a, b);
  type_tag(a, &b->tag);
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

ANN void gwfmt_fptr_def(Gwfmt *a, const Fptr_Def b) {
  keyword(a, "funptr");
  gwfmt_space(a);
  gwfmt_func_base(a, b->base);
  gwfmt_sc(a);
  gwfmt_nl(a);
}

ANN void gwfmt_type_def(Gwfmt *a, const Type_Def b) {
  keyword(a, !b->distinct ? "typedef" : "distinct");
  gwfmt_space(a);
  if (b->ext) {
    gwfmt_type_decl(a, b->ext);
    gwfmt_space(a);
  }
  type_tag(a, &b->tag);
  if (b->tmpl) gwfmt_tmpl(a, b->tmpl);
  if (b->when) {
    gwfmt_nl(a);
    a->indent += 2;
    gwfmt_indent(a);
    flow(a, "when");
    a->indent -= 2;
    gwfmt_space(a);
    gwfmt_exp(a, b->when);
  }
  gwfmt_sc(a);
  gwfmt_nl(a);
}

ANN static void gwfmt_extend_def(Gwfmt *a, const Extend_Def b) {
  gwfmt_extends(a, b->td);
  punctuation(a, ":");
  gwfmt_space(a);
  gwfmt_taglist(a, b->traits);
  gwfmt_sc(a);
  gwfmt_nl(a);
}

ANN static void gwfmt_trait_def(Gwfmt *a, const Trait_Def b) {
  keyword(a, "trait");
  gwfmt_space(a);
  type_tag(a, &b->tag);
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

ANN void gwfmt_prim_def(Gwfmt *a, const Prim_Def b) {
  keyword(a, "primitive");
  gwfmt_space(a);
  gwfmt_flag(a, b);
  type_tag(a, &b->tag);
  gwfmt_space(a);
  struct gwint gwint = GWINT(b->size, gwint_decimal);
  gwfmt_prim_num(a, &gwint);
  gwfmt_sc(a);
  gwfmt_nl(a);
}

DECL_SECTION_FUNC(gwfmt, void, Gwfmt *, const)
ANN static void gwfmt_section(Gwfmt *a, const  Section *b) {
  if (b->section_type != ae_section_stmt) gwfmt_indent(a);
  gwfmt_section_func[b->section_type](a, *(void **)&b->d);
}

ANN void gwfmt_ast(Gwfmt *a, const Ast b) {
  const m_uint sz = b->len;
  for(m_uint i = 0; i < sz; i++) {
    const Section section = sectionlist_at(b, i);
    gwfmt_section(a, &section);
    if(i < sz -1) gwfmt_nl(a);
  }
}

void set_color(struct GwfmtState *ls, const FmtColor b,
               const char *color) {
  const size_t len = strlen(color);
  ls->config->colors[b][0] = '{';
  strncpy(ls->config->colors[b] + 1, color, 61);
  ls->config->colors[b][len + 1] = '}';
}

static Config gwfmt_config;
void gwfmt_state_init(GwfmtState *ls) {
  ls->config = &gwfmt_config;
  ls->config->cases[TypeCase] = *get_casing(PASCALCASE);
  ls->config->cases[VariableCase] = *get_casing(CAMELCASE);
  ls->config->cases[FunctionCase] = *get_casing(SNAKECASE);
  set_color(ls, StringColor, "/Y");
  set_color(ls, KeywordColor, "+G");
  set_color(ls, FlowColor, "+/G");
  set_color(ls, FunctionColor, "+M");
  set_color(ls, TypeColor, "+Y");
  set_color(ls, VariableColor, "W");
  set_color(ls, NumberColor, "M");
  set_color(ls, OpColor, "-W");
  set_color(ls, ModColor, "-G");
  set_color(ls, PPColor, "-B");
  set_color(ls, SpecialColor, "B");
  set_color(ls, PunctuationColor, "-");
}
