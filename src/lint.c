#include <ctype.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwfmt.h"
#include "lint_internal.h"
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

ANN void lint_set_func(int (*f)(const char*, ...)) {
  _print = f;
}

ANN static enum char_type cht(const char c) {
  if (isalnum(c) || c == '_') return cht_id;
  char *op = "?:$@+-/%~<>^|&!=*";
  do
    if (c == *op) return cht_op;
  //while (++op);
  while (*op++);
  char * delim = "(){},;`";
  do
    if (c == *delim) return cht_delim;
  //while (++delim)";
  while (*delim++);
  return cht_sp;
}

ANN void color(Lint *a, const m_str buf) {
  char tmp[strlen(buf)*4];
  tcol_snprintf(tmp, strlen(buf)*4, buf);
  text_add(&a->ls->text, tmp);
}

void COLOR(Lint *a, const m_str b, const m_str c) {
  color(a, b);         
  lint(a, c);    
  color(a, "{0}");
}

ANN void lint_util(Lint *a, const m_str fmt, ...) {
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

ANN void lint(Lint *a, const m_str fmt, ...) {

  a->nl = 0;
  va_list ap;

  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);

  char * buf = mp_malloc2(a->mp, n + 1);
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  if (a->need_space && a->last != cht_delim && a->last == cht(buf[0])) {
    text_add(&a->ls->text, " ");
    a->column += 1;
  }
  text_add(&a->ls->text, buf);
  a->last = cht(buf[n - 1]);
  mp_free2(a->mp, n + 1, buf);
  va_end(ap);
  a->need_space = 0;
  a->column += n;
}

ANN void lint_space(Lint *a) {
  if (!a->ls->minimize)
    lint(a, " ");
  else
    a->need_space = 1;
}

ANN void lint_nl(Lint *a) {
  const unsigned int nl = a->nl + 1;
  if (!a->ls->minimize) {
    if (a->nl < 2) {
      lint_util(a, "\n");
      if (!a->ls->pretty && a->ls->show_line) {
        lint_util(a, " {-}% 4u{0}", a->line);//root
        if (a->ls->mark == a->line)
          lint_util(a, " {+R}>{0}");
        else
          lint_util(a, "  ");
        lint_util(a, "{N}┃{0} "); //
      }
    }
  }
  a->column = a->ls->base_column;
  a->line++;
  a->nl = nl;
}

ANN void lint_lbrace(Lint *a) {
  if (!a->ls->py) COLOR(a, "{-}", "{");
}

ANN void lint_rbrace(Lint *a) {
  if (!a->ls->py) COLOR(a, "{-}", "}");
}

ANN void lint_sc(Lint *a) {
  if (!a->ls->py) COLOR(a, "{-}", ";");
}

ANN void lint_comma(Lint *a) {
  if (!a->ls->py) COLOR(a, "{-}", ",");
}

ANN void lint_lparen(Lint *a) { COLOR(a, "{-}","("); }

ANN void lint_rparen(Lint *a) { COLOR(a, "{-}", ")");
  a->last = cht_delim;

}

ANN static inline void lint_lbrack(Lint *a) { COLOR(a, "{-}", "["); }

ANN static inline void lint_rbrack(Lint *a) { COLOR(a, "{-}", "]"); }

ANN void lint_indent(Lint *a) {
  if(a->ls->minimize && !a->ls->py)
    return;
  if (a->skip_indent > 0) {
    a->skip_indent--;
    return;
  }
  if(!a->ls->use_tabs) {
    for (unsigned int i = 0; i < a->indent; ++i) {
      for (unsigned int j = 0; j < a->ls->nindent; ++j)
        lint_space(a);
    }
  } else {
    for (unsigned int i = 0; i < a->indent; ++i)
      lint(a, "\t");
  }
}

ANN static void paren_exp(Lint *a, Exp b);
ANN static void maybe_paren_exp(Lint *a, Exp b);
ANN static void lint_array_sub2(Lint *a, Array_Sub b);
ANN static void lint_prim_interp(Lint *a, Exp *b);

#define _FLAG(a, b) (((a)&ae_flag_##b) == (ae_flag_##b))
ANN static void lint_flag(Lint *a, ae_flag b) {
  bool state = true;
  if (_FLAG(b, private))
    COLOR(a, "{-/G}", "private");
  else if (_FLAG(b, protect))
    COLOR(a, "{-/G}", "protect");
  else state = false;
  if (state) lint_space(a);
  state = true;
  if (_FLAG(b, static))
    COLOR(a, "{-G}", "static");
  else if (_FLAG(b, global))
    COLOR(a, "{-G}", "global");
  else state = false;
  if (state) lint_space(a);
  state = true;
  if (_FLAG(b, abstract))
    COLOR(a, "{-G}", "abstract");
  else if (_FLAG(b, final))
    COLOR(a, "{-G}", "final");
  else state = false;
  if (state) lint_space(a);
}
#define lint_flag(a, b) lint_flag(a, b->flag)

ANN static void lint_symbol(Lint *a, Symbol b) {
  const m_str s = s_name(b);
  if (!strcmp(s, "true") || !strcmp(s, "false") || !strcmp(s, "maybe") ||
      !strcmp(s, "adc") || !strcmp(s, "dac") || !strcmp(s, "now"))
    COLOR(a, "{B}", s_name(b));
  else
    lint(a, s);
}

ANN static void lint_array_sub(Lint *a, Array_Sub b) {
  lint_lbrack(a);
  if (b->exp) {
    if (b->exp->next) lint_space(a);
    lint_exp(a, b->exp);
    if (b->exp->next) lint_space(a);
  }
  lint_rbrack(a);
}

#define NEXT(a, b, c)                                                          \
  if (b->next) {                                                               \
    lint_comma(a);                                                             \
    lint_space(a);                                                             \
    c(a, b->next);                                                             \
  }

ANN static void lint_id_list(Lint *a, ID_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Symbol xid = *mp_vector_at(b, Symbol, i);
    lint_symbol(a, xid);
    if(i < b->len - 1) {
      lint_comma(a);
      lint_space(a);
    }
  }
}

ANN void lint_traits(Lint *a, ID_List b) {
  lint(a, ":");
  lint_space(a);
  lint_id_list(a, b);
  lint_space(a);
}

ANN static void lint_specialized_list(Lint *a, Specialized_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Specialized *spec = mp_vector_at(b, Specialized, i);
    COLOR(a, "{-C}", s_name(spec->xid));
    if (spec->traits) {
      lint_space(a);
      lint_traits(a, spec->traits);
    }
    if(i < b->len - 1) {
      lint_comma(a);
      lint_space(a);
    }
  }
}

ANN static void _lint_type_list(Lint *a, Type_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Type_Decl *td = *mp_vector_at(b, Type_Decl*, i);
    lint_type_decl(a, td);
    if(i < b->len - 1) {
      lint_comma(a);
      lint_space(a);
    }
  }
}

ANN static inline void lint_init_tmpl(Lint *a) { COLOR(a, "{-}",":["); }

ANN static void lint_type_list(Lint *a, Type_List b) {
  lint_init_tmpl(a);
  lint_space(a);
  _lint_type_list(a, b);
  lint_space(a);
  lint_rbrack(a);
}

ANN static void lint_tmpl(Lint *a, Tmpl *b) {
  if (!b->call) {
    lint_init_tmpl(a);
    lint_space(a);
    lint_specialized_list(a, b->list);
    lint_space(a);
    lint_rbrack(a);
  }
  if (b->call) lint_type_list(a, b->call);
}

ANN static void lint_range(Lint *a, Range *b) {
  lint_lbrack(a);
  if (b->start) lint_exp(a, b->start);
  lint_space(a);
  COLOR(a, "{-}", ":");
  lint_space(a);
  if (b->end) lint_exp(a, b->end);
  lint_rbrack(a);
}

ANN static void lint_prim_dict(Lint *a, Exp *b) {
  Exp e = *b;
  lint_lbrace(a);
  lint_space(a);
  do {
    const Exp next  = e->next;
    const Exp nnext = next->next;
    e->next = NULL;
    next->next = NULL;
    lint_exp(a, e);
    lint_space(a);
    lint(a, ":");
    lint_space(a);
    lint_exp(a, next);
    e->next = next;
    next->next = nnext;
    if(nnext) {
      lint_comma(a);
      lint_space(a);
    }
  } while ((e = e->next->next));
  lint_space(a);
  lint_rbrace(a);
}
ANN static void lint_effect(Lint *a, Symbol b) {
  COLOR(a, "{-/C}", s_name(b));
}

ANN static void lint_perform(Lint *a) {
  COLOR(a, "{/M}", "perform");
  lint_space(a);
}

ANN static void lint_prim_perform(Lint *a, Symbol *b) {
  COLOR(a, "{/M}", "perform");
  lint_space(a);
  lint_effect(a, *b);
}

ANN static void lint_effects(Lint *a, Vector b) {
  lint_perform(a);
  for (m_uint i = 0; i < vector_size(b) - 1; i++) {
    lint_effect(a, (Symbol)vector_at(b, i));
    lint_space(a);
  }
  lint_effect(a, (Symbol)vector_back(b));
}

ANN static void lint_type_decl(Lint *a, Type_Decl *b) {
  if (GET_FLAG(b, const)) {
    COLOR(a, "{+G}", "const");
    lint_space(a);
  }
  if (GET_FLAG(b, late)) {
    COLOR(a, "{+/G}", "late");
    lint_space(a);
  }
  lint_flag(a, b);
  if (b->ref) lint(a, "&");
  if (b->fptr) {
    const Fptr_Def fptr = b->fptr;
    lint_lparen(a);
    if (b->fptr->base->flag != ae_flag_none) {
      lint_flag(a, fptr->base);
      lint_space(a);
    }
    lint_type_decl(a, fptr->base->td);
    lint_lparen(a);
    if (fptr->base->args) lint_arg_list(a, fptr->base->args, 0);
    lint_rparen(a);
    if (fptr->base->effects.ptr) lint_effects(a, &fptr->base->effects);
    lint_rparen(a);
    //    COLOR(a, "{C}", s_name(b->xid));
  } else if (b->xid)
    COLOR(a, "{C}", s_name(b->xid));
  if (b->types) lint_type_list(a, b->types);
  for (m_uint i = 0; i < b->option; ++i) lint(a, "?");
  if (b->array) lint_array_sub2(a, b->array);
  if (b->next) {
    lint(a, ".");
    lint_type_decl(a, b->next);
  }
}

ANN static void lint_prim_id(Lint *a, Symbol *b) { lint_symbol(a, *b); }

ANN static void lint_prim_num(Lint *a, m_int *b) {
  color(a, "{M}");
  lint(a, "%" INT_F, *b);
  color(a, "{0}");
}

ANN static void lint_prim_float(Lint *a, m_float *b) {
  if (*b == floor(*b)) {
    color(a, "{M}");
    lint(a, "%li.", (m_int)*b);
    color(a, "{0}");
  } else {
    color(a, "{M}");
    lint(a, "%g", *b);
    color(a, "{0}");
  }
}

ANN static void lint_string(Lint *a, m_str str) {
  const size_t len = strlen(str);
  size_t pass = 0;
  const m_uint nl = a->nl;
  while(pass < len) {
    if(pass)
      lint_nl(a);
    a->nl = 0;
    const m_str next = strchr(str, '\n');
    if(!next) {
      COLOR(a, "{Y/}", str);
      break;
    }
    color(a, "{Y/}");
    lint(a, "%.*s", next-str, str);
    color(a, "{0}");
    pass += next - str + 1;
    str = next + 1;
  }
  a->nl = nl;
}

ANN static void lint_delim(Lint *a, uint16_t delim) {
  if(delim) {
    color(a, "{Y-}");
    lint(a, "%.*c",  delim - 1, '#');
  }
  COLOR(a, "{0}{Y-}", "\"");
  color(a, "{/Y}");
}

ANN static void lint_delim2(Lint *a, uint16_t delim) {
  COLOR(a, "{0}{Y-}", "\"");
  if(delim) {
    color(a, "{Y-}");
    lint(a, "%.*c",  delim - 1, '#');
  }
  color(a, "{0}");
}

ANN static void lint_prim_str(Lint *a, struct AstString *b) {
  const uint16_t delim = b->delim > 0 ? b->delim - 1 : 0;
  lint_delim(a, delim);
  lint_string(a, b->data);
  lint_delim2(a, delim);
}

ANN static void lint_prim_array(Lint *a, Array_Sub *b) {
  lint_array_sub(a, *b);
}

ANN static void lint_prim_range(Lint *a, Range **b) { lint_range(a, *b); }

ANN static void lint_prim_hack(Lint *a, Exp *b) {
  COLOR(a, "{-R}", "<<<");
  lint_space(a);
  lint_exp(a, *b);
  lint_space(a);
  COLOR(a, "{-R}", ">>>");
}

ANN static void lint_prim_typeof(Lint *a, Exp *b) {
  COLOR(a, "{+C}", "typeof");
  paren_exp(a, *b);
}

ANN static void lint_prim_char(Lint *a, m_str *b) {
  color(a, "{M}");
  lint(a, "'%s'", *b);
  color(a, "{0}");
}

ANN static void lint_prim_nil(Lint *a, void *b NUSED) {
  lint_lparen(a);
  lint_rparen(a);
}

ANN void lint_prim_locale(Lint *a, Symbol *b) {
  lint(a, "`");
  lint_symbol(a, *b);
  lint(a, "`");
}

DECL_PRIM_FUNC(lint, void, Lint *)
ANN static void lint_prim(Lint *a, Exp_Primary *b) {
  lint_prim_func[b->prim_type](a, &b->d);
}

ANN static void lint_var_decl(Lint *a, Var_Decl *b) {
  if (b->xid) COLOR(a, "{W+}", s_name(b->xid));
}

ANN static void lint_exp_decl(Lint *a, Exp_Decl *b) {
  if (b->td) {
    if (!(GET_FLAG(b->td, const) || GET_FLAG(b->td, late))) {
      COLOR(a, "{+G}", "var");
      lint_space(a);
    }
    lint_type_decl(a, b->td);
    if(b->args) paren_exp(a, b->args);
    lint_space(a);
  }
  lint_var_decl(a, &b->vd);
}

ANN static void lint_exp_td(Lint *a, Type_Decl *b) {
  COLOR(a, "{-G}", "$");
  //  lint_space(a);
  lint_type_decl(a, b);
}

ANN static void lint_op(Lint *a, const Symbol b) {
  COLOR(a, "{-G}", s_name(b));
}

ANN static void lint_exp_binary(Lint *a, Exp_Binary *b) {
  const unsigned int coloncolon = !strcmp(s_name(b->op), "::");
  maybe_paren_exp(a, b->lhs);
  if (!coloncolon) lint_space(a);
  //  lint_symbol(a, b->op);
  lint_op(a, b->op);
  if (!coloncolon) lint_space(a);
  lint_exp(a, b->rhs);
}

ANN static int isop(const Symbol s) {
  char *name = s_name(s);
  for (size_t i = 0; i < strlen(name); ++i) {
    if (isalnum(name[i])) return 0;
  }
  return 1;
}

ANN static void lint_exp_unary(Lint *a, Exp_Unary *b) {
  if (s_name(b->op)[0] == '$' && !isop(b->op)) lint_lparen(a);
  lint_op(a, b->op);
  if (s_name(b->op)[0] == '$' && !isop(b->op)) lint_rparen(a);
  if (!isop(b->op)) lint_space(a);
  if (b->unary_type == unary_exp)
    lint_exp(a, b->exp);
  else if (b->unary_type == unary_td) {
    lint_type_decl(a, b->ctor.td);
    if(b->ctor.exp) {
      lint_lparen(a);
      if (b->ctor.exp->exp_type != ae_exp_primary ||
          b->ctor.exp->d.prim.prim_type != ae_prim_nil)
        lint_exp(a, b->ctor.exp);
      lint_rparen(a);
    }
  } else if (b->unary_type == unary_code) {
    lint_lbrace(a);
    lint_nl(a);
    INDENT(a, lint_stmt_list(a, b->code));
    lint_indent(a);
    lint_rbrace(a);
  }
}

ANN static void lint_exp_cast(Lint *a, Exp_Cast *b) {
  if (b->exp->exp_type != ae_exp_decl)
    lint_exp(a, b->exp);
  else
    paren_exp(a, b->exp);
  lint_space(a);
  COLOR(a, "{-G}", "$");
  lint_space(a);
  lint_type_decl(a, b->td);
}

ANN static void lint_exp_post(Lint *a, Exp_Postfix *b) {
  lint_exp(a, b->exp);
  lint_op(a, b->op);
}

ANN static void lint_exp_call(Lint *a, Exp_Call *b) {
  if (b->func->exp_type != ae_exp_decl)
    lint_exp(a, b->func);
  else
    paren_exp(a, b->func);
  if (b->tmpl) lint_tmpl(a, b->tmpl);
  if (b->args)
    paren_exp(a, b->args);
  else
    lint_prim_nil(a, b);
}

ANN static void lint_exp_array(Lint *a, Exp_Array *b) {
  lint_exp(a, b->base);
  lint_array_sub2(a, b->array);
}

ANN static void lint_exp_slice(Lint *a, Exp_Slice *b) {
  if (b->base->exp_type != ae_exp_primary &&
      b->base->exp_type != ae_exp_array && b->base->exp_type != ae_exp_call &&
      b->base->exp_type != ae_exp_post && b->base->exp_type != ae_exp_dot)
    lint_exp(a, b->base);
  else
    paren_exp(a, b->base);
  lint_range(a, b->range);
}

ANN static void lint_exp_if(Lint *a, Exp_If *b) {
  lint_exp(a, b->cond);
  lint_space(a);
  lint(a, "?");
  if (b->if_exp) {
    lint_space(a);
    lint_exp(a, b->if_exp);
  }
  lint(a, ":");
  lint_space(a);
  lint_exp(a, b->else_exp);
}

ANN static void lint_exp_dot(Lint *a, Exp_Dot *b) {
  lint_exp(a, b->base);
  lint(a, ".");
  lint_symbol(a, b->xid);
}

ANN static void lint_lambda_list(Lint *a, Arg_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Arg *arg = mp_vector_at(b, Arg, i);
    lint_symbol(a, arg->var_decl.xid);
    lint_space(a);
  }
}

ANN static void lint_exp_lambda(Lint *a, Exp_Lambda *b) {
  lint(a, "\\");
  if (b->def->base->args) {
    lint_lambda_list(a, b->def->base->args);
  }
  if (b->def->d.code) {
    Stmt_List code = b->def->d.code;
    if(mp_vector_len(code) != 1)
      lint_stmt_list(a, code); // brackets?
    else {
      lint_lbrace(a);
      lint_space(a);
      lint_exp(a, mp_vector_at(code, struct Stmt_, 0)->d.stmt_exp.val);
      lint_space(a);
      lint_rbrace(a);
    }
  }
}

DECL_EXP_FUNC(lint, void, Lint *)
ANN void lint_exp(Lint *a, Exp b) {
  if(b->paren) lint_lparen(a);
  lint_exp_func[b->exp_type](a, &b->d);
  if(b->paren) lint_rparen(a);
  NEXT(a, b, lint_exp)
}

ANN static void lint_prim_interp(Lint *a, Exp *b) {
  Exp e = *b;
  const uint16_t delim = e->d.prim.d.string.delim;
  lint_delim(a, delim);
  color(a, "{/Y}");
  while (e) {
    if (e->exp_type == ae_exp_primary && e->d.prim.prim_type == ae_prim_str) {
      lint_string(a,  e->d.prim.d.string.data);
    } else {
      COLOR(a, "{Y/-}", "${");
      lint_space(a);
      lint_exp_func[e->exp_type](a, &e->d);
      lint_space(a);
      COLOR(a, "{-/Y}", "}");
      color(a, "{Y/}");
    }
    e = e->next;
  }
  lint_delim2(a, delim);
  color(a, "{/Y}");
}

ANN static void lint_array_sub2(Lint *a, Array_Sub b) {
  Exp e = b->exp;
  for (m_uint i = 0; i < b->depth; ++i) {
    lint_lbrack(a);
    if (e) {
      lint_exp_func[e->exp_type](a, &e->d);
      e = e->next;
    }
    lint_rbrack(a);
  }
}

ANN static void paren_exp(Lint *a, Exp b) {
  lint_lparen(a);
  if(b->exp_type != ae_exp_primary &&
     b->d.prim.prim_type != ae_prim_nil)
    lint_exp(a, b);
  lint_rparen(a);
}

ANN static void maybe_paren_exp(Lint *a, Exp b) {
  if (b->next)
    paren_exp(a, b);
  else
    lint_exp(a, b);
}

ANN static void lint_stmt_exp(Lint *a, Stmt_Exp b) {
  if (b->val) lint_exp(a, b->val);
  lint_sc(a);
}

ANN static void lint_stmt_retry(Lint *a, Stmt_Exp b NUSED) {
  COLOR(a, "{+M}", "retry;");
}

ANN static void lint_handler_list(Lint *a, Handler_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Handler *handler = mp_vector_at(b, Handler, i);
    COLOR(a, "{+M}", "handle");
    lint_space(a);
    if (handler->xid) {
      lint_effect(a, handler->xid);
      lint_space(a);
    }
    const uint indent = a->skip_indent++;
    lint_stmt(a, handler->stmt);
    a->skip_indent = indent;
  }
}

ANN static void lint_stmt_try(Lint *a, Stmt_Try b) {
  COLOR(a, "{+M}", "try");
  lint_space(a);
  const uint indent = a->skip_indent++;
//  const uint indent = a->indent++;
  lint_stmt(a, b->stmt);
  lint_space(a);
//  a->indent--;
// = indent;
  a->skip_indent = indent;
  lint_handler_list(a, b->handler);
}

ANN static void lint_stmt_spread(Lint *a, Spread_Def b) {
  lint(a, "...");
  lint_space(a);
  lint_symbol(a, b->xid);
  lint_space(a);
  lint(a, ":");
  lint_space(a);
  lint_id_list(a, b->list);
  lint_space(a);
  lint_lbrace(a);
  lint_nl(a);
  a->indent++;
  struct PPArg_ ppa = {};
  pparg_ini(a->mp, &ppa);
  FILE *file = fmemopen(b->data, strlen(b->data), "r");
  struct AstGetter_ arg = {"", file, a->st, .ppa = &ppa};
  Ast tmp =    parse(&arg);
  if(tmp) lint_ast(a, tmp);
  pparg_end(&ppa);
  a->indent--;
  lint_indent(a);
  lint_rbrace(a);
  lint_nl(a);
}

DECL_STMT_FUNC(lint, void, Lint *)
ANN static void lint_stmt_while(Lint *a, Stmt_Flow b) {
  if (!b->is_do) {
    COLOR(a, "{+M}", "while");
    paren_exp(a, b->cond);
  } else
    COLOR(a, "{+M}", "do");
  if (b->body->stmt_type != ae_stmt_exp || b->body->d.stmt_exp.val)
    lint_space(a);
  lint_stmt_func[b->body->stmt_type](a, &b->body->d);
  if (b->is_do) {
    lint_space(a);
    COLOR(a, "{+M}", "while");
    paren_exp(a, b->cond);
    lint_sc(a);
  }
}

ANN static void lint_stmt_until(Lint *a, Stmt_Flow b) {
  if (!b->is_do) {
    COLOR(a, "{+M}", "until");
    paren_exp(a, b->cond);
  } else
    COLOR(a, "{+M}", "do");
  lint_space(a);
  lint_stmt_func[b->body->stmt_type](a, &b->body->d);
  if (b->is_do) {
    lint_space(a);
    COLOR(a, "{+M}", "until");
    paren_exp(a, b->cond);
    lint_sc(a);
  }
}

ANN static void lint_stmt_for(Lint *a, Stmt_For b) {
  COLOR(a, "{+M}", "for");
  lint_lparen(a);
  lint_stmt_exp(a, &b->c1->d.stmt_exp);
  lint_space(a);
  const unsigned int py = a->ls->py;
  a->ls->py             = 0;
  if (b->c2) lint_stmt_exp(a, &b->c2->d.stmt_exp);
  a->ls->py = py;
  if (b->c3) {
    lint_space(a);
    lint_exp(a, b->c3);
  }
  lint_rparen(a);
  if (b->body->stmt_type == ae_stmt_code)
    lint_space(a);
  else if (!(b->body->stmt_type == ae_stmt_exp && !b->body->d.stmt_exp.val)) {
    lint_space(a);
    lint_nl(a);
    a->indent++;
    lint_indent(a);
    a->indent--;
  }
  lint_stmt(a, b->body);
}

ANN static void lint_stmt_each(Lint *a, Stmt_Each b) {
  COLOR(a, "{+M}", "foreach");
  lint_lparen(a);
  if(b->idx) {
    lint_symbol(a, b->idx->sym);
    lint_comma(a);
    lint_space(a);
  }
  lint_symbol(a, b->sym);
  lint_space(a);
  COLOR(a, "{-}", ":");
  lint_space(a);
  lint_exp(a, b->exp);
  lint_rparen(a);
  if (b->body->stmt_type != ae_stmt_code) lint_nl(a);
  INDENT(a, lint_stmt(a, b->body))
}

ANN static void lint_stmt_loop(Lint *a, Stmt_Loop b) {
  COLOR(a, "{+M}", "repeat");
  lint_lparen(a);
  if (b->idx) {
    lint_symbol(a, b->idx->sym);
    lint_comma(a);
    lint_space(a);
  }
  lint_exp(a, b->cond);
  lint_rparen(a);
  if (b->body->stmt_type == ae_stmt_code ||
      (b->body->stmt_type == ae_stmt_exp && !b->body->d.stmt_exp.val))
    lint_stmt(a, b->body);
  else {
    lint_nl(a);
    INDENT(a, lint_stmt(a, b->body))
  }
}

ANN static void lint_code(Lint *a, Stmt b) {
  if (b->stmt_type == ae_stmt_if || b->stmt_type == ae_stmt_code ||
      (b->stmt_type == ae_stmt_exp && !b->d.stmt_exp.val)) {
    lint_space(a);
    lint_stmt_func[b->stmt_type](a, &b->d);
  } else {
    lint_nl(a);
    INDENT(a, lint_indent(a); lint_stmt_func[b->stmt_type](a, &b->d))
  }
}

ANN static void lint_stmt_if(Lint *a, Stmt_If b) {
  COLOR(a, "{+M}", "if");
  paren_exp(a, b->cond);
  lint_code(a, b->if_body);
  if (b->else_body) {
    lint_space(a);
    COLOR(a, "{+M}", "else");
    lint_code(a, b->else_body);
  }
}

ANN static void lint_stmt_code(Lint *a, Stmt_Code b) {
  lint_lbrace(a);
  if (b->stmt_list) {
    INDENT(a, lint_nl(a); lint_stmt_list(a, b->stmt_list))
    lint_indent(a);
  }
  lint_rbrace(a);
}

ANN static void lint_stmt_break(Lint *a, Stmt_Index b) {
  COLOR(a, "{+M}", "break");
  if (b->idx) {
    lint_space(a);
    lint_prim_num(a, &b->idx);
  }
  lint_sc(a);
}

ANN static void lint_stmt_continue(Lint *a, Stmt_Index b) {
  COLOR(a, "{+M}", "continue");
  if (b->idx) {
    lint_space(a);
    lint_prim_num(a, &b->idx);
  }
  lint_sc(a);
}

ANN static void lint_stmt_return(Lint *a, Stmt_Exp b) {
  COLOR(a, "{+M}", "return");
  if (b->val) {
    lint_space(a);
    lint_exp(a, b->val);
  }
  lint_sc(a);
}

ANN static void lint_case_list(Lint *a, Stmt_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    const Stmt stmt = mp_vector_at(b, struct Stmt_, i);
    lint_stmt_case(a, &stmt->d.stmt_match);
    if(i < b->len - 1) lint_nl(a);
  }
}

ANN static void lint_stmt_match(Lint *a, Stmt_Match b) {
  COLOR(a, "{+M}", "match");
  lint_space(a);
  lint_exp(a, b->cond);
  lint_space(a);
  lint_lbrace(a);
  lint_nl(a);
  INDENT(a, lint_case_list(a, b->list))
  lint_indent(a);
  lint_nl(a);
  lint_indent(a);
  lint_rbrace(a);
  if (b->where) {
    lint_space(a);
    COLOR(a, "{+M}", "where");
    lint_space(a);
    lint_stmt(a, b->where);
  }
}

ANN static void lint_stmt_case(Lint *a, Stmt_Match b) {
  lint_indent(a);
  COLOR(a, "{+M}", "case");
  lint_space(a);
  lint_exp(a, b->cond);
  if (b->when) {
    lint_space(a);
    COLOR(a, "{+M}", "when");
    lint_space(a);
    lint_exp(a, b->when);
  }
  //  lint_space(a);
  COLOR(a, "{-}", ":");
  if (b->list->len > 1)
    INDENT(a, lint_stmt_list(a, b->list))
  else {
    lint_space(a);
    const Stmt stmt = mp_vector_at(b->list, struct Stmt_, 0);
    lint_stmt_func[stmt->stmt_type](a, &stmt->d);
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

ANN static void force_nl(Lint *a) {
  if(a->ls->minimize) {
    text_add(&a->ls->text, "\n");
    a->line++;
  }
}

ANN static void lint_stmt_pp(Lint *a, Stmt_PP b) {
  if (b->pp_type == ae_pp_nl) return;
  if (b->pp_type == ae_pp_locale) {
    COLOR(a, "{M/}", "#locale ");
    COLOR(a, "{+C}", s_name(b->xid));
    lint_space(a);
    if(b->exp) lint_exp(a, b->exp);
    return;
  } else if (b->pp_type == ae_pp_include) {
    if(a->ls->ppa->lint) {
      COLOR(a, "{M/}", "#include");
      lint_space(a);
      color(a, "{Y}");
      lint(a, "<%s>", b->data);
      color(a, "{0}");
    } else return;
  } else if (b->pp_type != ae_pp_comment) {
    color(a, "{M/}");
    lint(a, "#%s ", pp[b->pp_type]);
    COLOR(a, (m_str)pp_color[b->pp_type], b->data ?: "");
  } else if(!a->ls->minimize) {
    lint_indent(a);
    color(a, "{M/}");
    lint(a, "#! ");
    COLOR(a, "{-}", b->data ?: "");
  }
  force_nl(a);
}

ANN static void lint_stmt_defer(Lint *a, Stmt_Defer b) {
  COLOR(a, "{+M}", "defer");
  lint_space(a);
  a->skip_indent++;
  lint_stmt(a, b->stmt);
//  a->skip_indent--;
}

ANN static void lint_stmt(Lint *a, Stmt b) {
  const uint skip_indent = a->skip_indent;
  if (b->stmt_type != ae_stmt_pp) lint_indent(a);
  lint_stmt_func[b->stmt_type](a, &b->d);
  if (!skip_indent) lint_nl(a);
}

ANN static void lint_arg_list(Lint *a, Arg_List b, const bool locale) {
  for(uint32_t i = locale; i < b->len; i++) {
    Arg *arg = mp_vector_at(b, Arg, i);
    if (arg->td) {
      lint_type_decl(a, arg->td);
      if (arg->var_decl.xid) lint_space(a);
    }
    lint_var_decl(a, &arg->var_decl);
    if (arg->exp) {
      lint_space(a);
      lint(a, ":");
      lint_space(a);
      lint_exp(a, arg->exp);
    }
    if(i < b->len - 1) {
      lint_comma(a);
      lint_space(a);
    }
  }
}

ANN static void lint_union_list(Lint *a, Union_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Union_Member *um = mp_vector_at(b, Union_Member, i);
    lint_indent(a);
    lint_type_decl(a, um->td);
    lint_space(a);
    lint_var_decl(a, &um->vd);
    lint_sc(a);
    lint_nl(a);
  }
}

ANN static void lint_stmt_list(Lint *a, Stmt_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Stmt stmt = mp_vector_at(b, struct Stmt_, i);
    if (stmt->stmt_type != ae_stmt_exp || stmt->d.stmt_exp.val)
      lint_stmt(a, stmt);
  }
}

ANN static void lint_func_base(Lint *a, Func_Base *b) {
  lint_flag(a, b);
  if (fbflag(b, fbflag_unary)) {
    lint_op(a, b->xid);
    lint_space(a);
    if (b->tmpl) lint_tmpl(a, b->tmpl);
  }
  if (b->td && !fbflag(b, fbflag_locale)) {
    lint_type_decl(a, b->td);
    lint_space(a);
  }
  if (!fbflag(b, fbflag_unary)) {
    COLOR(a, "{M}", s_name(b->xid));
    if (b->tmpl) lint_tmpl(a, b->tmpl);
  }
  if (fbflag(b, fbflag_op))
    lint_space(a);
  lint_lparen(a);
  if (b->args) lint_arg_list(a, b->args, fbflag(b, fbflag_locale));
  lint_rparen(a);
  if (b->effects.ptr) lint_effects(a, &b->effects);
}

ANN void lint_func_def(Lint *a, Func_Def b) {
  if(fbflag(b->base, fbflag_locale))
    COLOR(a, "{+C}", "locale");
  else if(!fbflag(b->base, fbflag_op) && strcmp(s_name(b->base->xid), "new"))
    COLOR(a, "{+C}", "fun");
  else
    COLOR(a, "{+C}", "operator");
  lint_space(a);
  lint_func_base(a, b->base);
  if (a->ls->builtin) {
    lint_sc(a);
    lint_nl(a);
    return;
  }
  lint_space(a);
  if (!GET_FLAG(b->base, abstract) && b->d.code) {
    lint_lbrace(a);
    lint_nl(a);
    INDENT(a, lint_stmt_list(a, b->d.code));
    lint_indent(a);
    lint_rbrace(a);
  } else {
    lint_sc(a);
  }
  lint_nl(a);
}

ANN void lint_class_def(Lint *a, Class_Def b) {
  COLOR(a, "{+C}", !cflag(b, cflag_struct) ? "class" : "struct");
  lint_space(a);
  lint_flag(a, b);
  COLOR(a, "{+W}", s_name(b->base.xid));
  if (b->base.tmpl) lint_tmpl(a, b->base.tmpl);
  lint_space(a);
  if (b->base.ext) {
    COLOR(a, "{+/C}", "extends");
    lint_space(a);
    lint_type_decl(a, b->base.ext);
    lint_space(a);
  }
  if (b->traits) lint_traits(a, b->traits);
  lint_lbrace(a);
  if (!a->ls->builtin) {
    if (b->body) {
      lint_nl(a);
      INDENT(a, lint_ast(a, b->body))
      lint_indent(a);
    }
    lint_rbrace(a);
  }
  lint_nl(a);
}

ANN void lint_enum_def(Lint *a, Enum_Def b) {
  COLOR(a, "{+C}", "enum");
  lint_space(a);
  lint_flag(a, b);
  COLOR(a, "{/}", s_name(b->xid));
  lint_space(a);
  lint_lbrace(a);
  lint_space(a);
  lint_id_list(a, b->list);
  lint_space(a);
  lint_rbrace(a);
  lint_nl(a);
}

ANN void lint_union_def(Lint *a, Union_Def b) {
  COLOR(a, "{+C}", "union");
  lint_space(a);
  lint_flag(a, b);
  COLOR(a, "{/}", s_name(b->xid));
  lint_space(a);
  if (b->tmpl) lint_tmpl(a, b->tmpl);
  lint_space(a);
  lint_lbrace(a);
  lint_nl(a);
  INDENT(a, lint_union_list(a, b->l))
  lint_rbrace(a);
  lint_sc(a);
  lint_nl(a);
}

ANN void lint_fptr_def(Lint *a, Fptr_Def b) {
  COLOR(a, "{+C}", "funptr");
  lint_space(a);
  lint_func_base(a, b->base);
  lint_sc(a);
  lint_nl(a);
}

ANN void lint_type_def(Lint *a, Type_Def b) {
  COLOR(a, "{+C}", !b->distinct ? "typedef" : "distinct");
  lint_space(a);
  if (b->ext) {
    lint_type_decl(a, b->ext);
    lint_space(a);
  }
  COLOR(a, "{/}", s_name(b->xid));
  if (b->tmpl) lint_tmpl(a, b->tmpl);
  if (b->when) {
    lint_nl(a);
    a->indent += 2;
    lint_indent(a);
    COLOR(a, "{M}", "when");
    a->indent -= 2;
    lint_space(a);
    lint_exp(a, b->when);
  }
  lint_sc(a);
  lint_nl(a);
}

ANN static void lint_extend_def(Lint *a, Extend_Def b) {
  COLOR(a, "{+C}", "extends");
  lint_space(a);
  lint_type_decl(a, b->td);
  lint_space(a);
  lint(a, ":");
  lint_space(a);
  lint_id_list(a, b->traits);
  lint_sc(a);
  lint_nl(a);
}

ANN static void lint_trait_def(Lint *a, Trait_Def b) {
  COLOR(a, "{+C}", "trait");
  lint_space(a);
  lint_symbol(a, b->xid);
  lint_space(a);
  if (b->body) {
    lint_lbrace(a);
    lint_nl(a);
    INDENT(a, lint_ast(a, b->body))
    lint_indent(a);
    lint_rbrace(a);
  } else lint_sc(a);
  lint_nl(a);
}

ANN void lint_prim_def(Lint *a, Prim_Def b) {
  COLOR(a, "{+C}", "primitive");
  lint_space(a);
  lint_flag(a, b);
  COLOR(a, "{+W}", s_name(b->name));
  lint_space(a);
  lint_prim_num(a, (m_int*)&b->size);
  lint_sc(a);
  lint_nl(a);
}

DECL_SECTION_FUNC(lint, void, Lint *)
ANN static void lint_section(Lint *a, Section *b) {
  if (b->section_type != ae_section_stmt) lint_indent(a);
  lint_section_func[b->section_type](a, *(void **)&b->d);
}

ANN void lint_ast(Lint *a, Ast b) {
  const m_uint sz = b->len;
  for(m_uint i = 0; i < sz; i++) {
    Section *section = mp_vector_at(b, Section, i);
    lint_section(a, section);
    if(i < sz -1) lint_nl(a);
  }
}
