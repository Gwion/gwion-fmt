#include <unistd.h>
#include <ctype.h>
#include <limits.h>
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

#define check_pos(a, b)

#define printf tcol_printf

ANN static enum char_type cht(const char c) {
  if (isalnum(c) || c == '_') return cht_id;
  char *op = "?:$@+-/%~<>^|&!=*";
  do
    if (c == *op) return cht_op;
  while (++op);
  return cht_sp;
}

ANN void lint(Lint *a, const m_str fmt, ...) {
  a->nl = 0;
  //  if(!a->skip) {
  va_list ap, aq;

  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);

  char buf[n + 1];
  va_start(ap, fmt);

  vsprintf(buf, fmt, ap);
  printf(buf);
  if (a->need_space && a->last != cht_op && a->last == cht(buf[0])) printf(" ");
  a->last = cht(buf[n - 1]);
  va_end(ap);
  a->need_space = 0;
  //  }
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
      lint(a, "\n");
      if (!a->ls->pretty && a->ls->show_line) {
        lint(a, " {-}% 4u{0}", a->line);
        if (a->mark == a->line)
          lint(a, " {+R}>{0}");
        else
          lint(a, "  ");
        lint(a, "{N}┃{0} ", a->line);
      }
    }
  }
  a->line++;
  a->nl = nl;
}

ANN void lint_lbrace(Lint *a) {
  if (!a->ls->py) lint(a, "{-}{{{0}");
}

ANN void lint_rbrace(Lint *a) {
  if (!a->ls->py) lint(a, "{-}}{0}");
}

ANN void lint_sc(Lint *a) {
  if (!a->ls->py) lint(a, "{-};{0}");
}

ANN void lint_comma(Lint *a) {
  if (!a->ls->py) lint(a, "{-},{0}");
}

ANN void lint_lparen(Lint *a) { lint(a, "{-}({0}"); }

ANN void lint_rparen(Lint *a) { lint(a, "{-}){0}"); }

ANN static inline void lint_lbrack(Lint *a) { lint(a, "{-}[{0}"); }

ANN static inline void lint_rbrack(Lint *a) { lint(a, "{-}]{0}"); }

ANN void lint_indent(Lint *a) {
  if (a->skip_indent > 0) {
    a->skip_indent--;
    return;
  }
  for (unsigned int i = 0; i < a->indent; ++i) {
    for (unsigned int j = 0; j < a->nindent; ++j)
      lint_space(a);
  }
}
/*
static inline void check_pos(Lint *a, const struct pos_t *b) {
  if(!map_size(a->macro))
    return;
  const loc_t loc = (loc_t)VKEY(a->macro, 0);
  if(b->line >= a->pos.line || (b->line == a->pos.line && b->column + 1 >=
a->pos.column)) { if(!a->skip) { Macro m = (Macro)VVAL(a->macro, 0); lint(a,
m->name); if(b->line > a->pos.line) lint_nl(a); a->pos = loc->last; a->skip = 1;
    } else {
      a->skip = 0;
      Macro m = (Macro)VVAL(a->macro, 0);
      xfree(m->name);
      mp_free(a->mp, Macro, m);
      map_remove(a->macro, (vtype)loc);
      mp_free(a->mp, loc_t, loc);
      const loc_t new_loc = VLEN(a->macro) ? (loc_t)VKEY(a->macro, 0) : NULL;
      if(new_loc)
        a->pos = new_loc->first;
      else
        a->pos.line = INT_MAX;
    }
  }
}
*/
ANN static void paren_exp(Lint *a, Exp b);
ANN static void maybe_paren_exp(Lint *a, Exp b);
ANN static void lint_array_sub2(Lint *a, Array_Sub b);
ANN static void lint_prim_interp(Lint *a, Exp *b);

#define _FLAG(a, b) (((a)&ae_flag_##b) == (ae_flag_##b))
ANN static void lint_flag(Lint *a, ae_flag b) {
  bool state = true;
  if (_FLAG(b, private))
    lint(a, "{-/G}private{0}");
  else if (_FLAG(b, protect))
    lint(a, "{-/G}protect{0}");
  else state = false;
  if (state) lint_space(a);
  state = true;
  if (_FLAG(b, static))
    lint(a, "{-G}static{0}");
  else if (_FLAG(b, global))
    lint(a, "{-G}global{0}");
  else state = false;
  if (state) lint_space(a);
  state = true;
  if (_FLAG(b, abstract))
    lint(a, "{-G}abstract{0}");
  else if (_FLAG(b, final))
    lint(a, "{-G}final{0}");
  else state = false;
  if (state) lint_space(a);
}
#define lint_flag(a, b) lint_flag(a, b->flag)

ANN static void lint_symbol(Lint *a, Symbol b) {
  const m_str s = s_name(b);
  if (!strcmp(s, "true") || !strcmp(s, "false") || !strcmp(s, "maybe") ||
      !strcmp(s, "adc") || !strcmp(s, "dac") || !strcmp(s, "now"))
    lint(a, "{B}%s{0}", s_name(b));
  else
    lint(a, s_name(b));
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
    check_pos(a, &b->pos->first);
    Symbol xid = *mp_vector_at(b, Symbol, i);
    lint_symbol(a, xid);
    if(i < b->len - 1) {
      lint_comma(a);
      lint_space(a);
    }
    check_pos(a, &b->pos->last);
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
    check_pos(a, &spec->pos->first);
    lint(a, "{-C}%s{0}", s_name(spec->xid));
    if (spec->traits) {
      lint_space(a);
      lint_traits(a, spec->traits);
    }
    if(i < b->len - 1) {
      lint_comma(a);
      lint_space(a);
    }
    check_pos(a, &spec->pos->last);
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

ANN static inline void lint_init_tmpl(Lint *a) { lint(a, "{-}:[{0}"); }

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
  lint(a, "{-}:{0}");
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
  lint(a, "{-/C}%s{0}", s_name(b));
}

ANN static void lint_perform(Lint *a) {
  lint(a, "{/M}perform{0}");
  lint_space(a);
}

ANN static void lint_prim_perform(Lint *a, Symbol *b) {
  lint(a, "{/M}perform{0}");
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
  check_pos(a, &b->pos->first);
  if (GET_FLAG(b, const)) {
    lint(a, "{+G}const{0}");
    lint_space(a);
  }
  if (GET_FLAG(b, late)) {
    lint(a, "{+/G}late{0}");
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
    //    lint(a, "{C}%s{0}", s_name(b->xid));
  } else if (b->xid)
    lint(a, "{C}%s{0}", s_name(b->xid));
  if (b->types) lint_type_list(a, b->types);
  for (m_uint i = 0; i < b->option; ++i) lint(a, "?");
  if (b->array) lint_array_sub2(a, b->array);
  if (b->next) {
    lint(a, ".");
    lint_type_decl(a, b->next);
  }
  check_pos(a, &b->pos->last);
}

ANN static void lint_prim_id(Lint *a, Symbol *b) { lint_symbol(a, *b); }

ANN static void lint_prim_num(Lint *a, m_int *b) {
  lint(a, "{M}%" INT_F "{0}", *b);
}

ANN static void lint_prim_float(Lint *a, m_float *b) {
  if (*b == floor(*b))
    lint(a, "{M}%li.{0}", (m_int)*b);
  else
    lint(a, "{M}%g{0}", *b);
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
      printf("{Y/}%s{0}", str);
      break;
    }
    printf("{Y/}%.*s{0}", next-str, str);
    pass += next - str + 1;
    str = next + 1;
  }
  a->nl = nl;
}

ANN static void lint_prim_str(Lint *a, struct AstString *b) {
  const uint16_t delim = b->delim > 0 ? b->delim - 1 : 0;
  if(delim)
    lint(a, "{-Y}%.*c\"{0}", delim, '#');
  else
    lint(a, "{-Y}\"{0}");
  lint_string(a, b->data);
  if(delim)
    lint(a, "{-Y}\"%.*c{0}", delim, '#');
  else
    lint(a, "{-Y}\"{0}");
}

ANN static void lint_prim_array(Lint *a, Array_Sub *b) {
  lint_array_sub(a, *b);
}

ANN static void lint_prim_range(Lint *a, Range **b) { lint_range(a, *b); }

ANN static void lint_prim_hack(Lint *a, Exp *b) {
  lint(a, "{-R}<<<{0}");
  lint_space(a);
  lint_exp(a, *b);
  lint_space(a);
  lint(a, "{-R}>>>{0}");
}

ANN static void lint_prim_typeof(Lint *a, Exp *b) {
  lint(a, "{+C}typeof{0}");
  paren_exp(a, *b);
}

ANN static void lint_prim_char(Lint *a, m_str *b) { lint(a, "{M}'%s'{0}", *b); }

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
  check_pos(a, &b->pos->first);
  if (b->xid) lint(a, "{W+}%s{0}", s_name(b->xid));
  check_pos(a, &b->pos->last);
}

ANN static void lint_exp_decl(Lint *a, Exp_Decl *b) {
  if (b->td) {
    if (!(GET_FLAG(b->td, const) || GET_FLAG(b->td, late))) {
      lint(a, "{+G}var{0}");
      lint_space(a);
    }
    lint_type_decl(a, b->td);
    if(b->args) paren_exp(a, b->args);
    lint_space(a);
  }
  lint_var_decl(a, &b->vd);
}

ANN static void lint_exp_td(Lint *a, Type_Decl *b) {
  lint(a, "{-G}${0}");
  //  lint_space(a);
  lint_type_decl(a, b);
}

ANN static void lint_op(Lint *a, const Symbol b) {
  lint(a, "{-G}%s{0}", s_name(b));
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
  } else if (b->unary_type == unary_code)
    lint_stmt_code(a, &b->code->d.stmt_code);
}

ANN static void lint_exp_cast(Lint *a, Exp_Cast *b) {
  if (b->exp->exp_type != ae_exp_decl)
    lint_exp(a, b->exp);
  else
    paren_exp(a, b->exp);
  lint_space(a);
  lint(a, "{-G}${0}");
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
    lint_space(a);
  }
  if (b->def->d.code) lint_stmt_code(a, &b->def->d.code->d.stmt_code);
}

DECL_EXP_FUNC(lint, void, Lint *)
ANN void lint_exp(Lint *a, Exp b) {
  check_pos(a, &b->pos->first);
  lint_exp_func[b->exp_type](a, &b->d);
  check_pos(a, &b->pos->last);
  NEXT(a, b, lint_exp)
}

ANN static void lint_prim_interp(Lint *a, Exp *b) {
  Exp e = *b;
  const uint16_t delim = e->d.prim.d.string.delim;
  if(delim > 1)
    lint(a, "{Y-}%.*c",  delim - 1, '#');
  lint(a, "{Y-}\"{0}{/Y}");
  while (e) {
    if (e->exp_type == ae_exp_primary && e->d.prim.prim_type == ae_prim_str) {
      lint_string(a,  e->d.prim.d.string.data);
    } else {
      lint(a, "{Y/-}${{{0}"); // do not use rbace
      lint_space(a);
      check_pos(a, &e->pos->first);
      lint_exp_func[e->exp_type](a, &e->d);
      check_pos(a, &e->pos->last);
      lint_space(a);
      lint(a, "{-/Y}}{0}{/Y}");
    }
    e = e->next;
  }
  if(delim > 1)
    lint(a, "{Y-}%.*c",  delim - 1, '#');
  lint(a, "{Y-}\"{0}{/Y}");
}

ANN static void lint_array_sub2(Lint *a, Array_Sub b) {
  Exp e = b->exp;
  for (m_uint i = 0; i < b->depth; ++i) {
    lint_lbrack(a);
    if (e) {
      check_pos(a, &e->pos->first);
      lint_exp_func[e->exp_type](a, &e->d);
      check_pos(a, &e->pos->last);
      e = e->next;
    }
    lint_rbrack(a);
  }
}

ANN static void paren_exp(Lint *a, Exp b) {
  lint_lparen(a);
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
  lint(a, "{+M}retry{0};");
}

ANN static void lint_handler_list(Lint *a, Handler_List b) {
  for(uint32_t i = 0; i < b->len; i++) {
    Handler *handler = mp_vector_at(b, Handler, i);
    lint(a, "{+M}handle{0}");
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
  lint(a, "{+M}try{0}");
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
    lint(a, "{+M}while{0}");
    paren_exp(a, b->cond);
  } else
    lint(a, "{+M}do{0}");
  if (b->body->stmt_type != ae_stmt_exp || b->body->d.stmt_exp.val)
    lint_space(a);
  lint_stmt_func[b->body->stmt_type](a, &b->body->d);
  if (b->is_do) {
    lint_space(a);
    lint(a, "{+M}while{0}");
    paren_exp(a, b->cond);
    lint_sc(a);
  }
}

ANN static void lint_stmt_until(Lint *a, Stmt_Flow b) {
  if (!b->is_do) {
    lint(a, "{+M}until{0}");
    paren_exp(a, b->cond);
  } else
    lint(a, "{+M}do{0}");
  lint_space(a);
  lint_stmt_func[b->body->stmt_type](a, &b->body->d);
  if (b->is_do) {
    lint_space(a);
    lint(a, "{+M}until{0}");
    paren_exp(a, b->cond);
    lint_sc(a);
  }
}

ANN static void lint_stmt_for(Lint *a, Stmt_For b) {
  lint(a, "{+M}for{0}");
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
  lint(a, "{+M}foreach{0}");
  lint_lparen(a);
  if(b->idx) {
    lint_symbol(a, b->idx->sym);
    lint_comma(a);
    lint_space(a);
  }
  lint_symbol(a, b->sym);
  lint_space(a);
  lint(a, "{-}:{0}");
  lint_space(a);
  lint_exp(a, b->exp);
  lint_rparen(a);
  if (b->body->stmt_type != ae_stmt_code) lint_nl(a);
  INDENT(a, lint_stmt(a, b->body))
}

ANN static void lint_stmt_loop(Lint *a, Stmt_Loop b) {
  lint(a, "{+M}repeat{0}");
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
  lint(a, "{+M}if{0}");
  paren_exp(a, b->cond);
  lint_code(a, b->if_body);
  if (b->else_body) {
    lint_nl(a);
    lint_indent(a);
    lint(a, "{+M}else{0}");
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
  lint(a, "{+M}break{0}");
  if (b->idx) {
    lint_space(a);
    lint_prim_num(a, &b->idx);
  }
  lint_sc(a);
}

ANN static void lint_stmt_continue(Lint *a, Stmt_Index b) {
  lint(a, "{+M}continue{0}");
  if (b->idx) {
    lint_space(a);
    lint_prim_num(a, &b->idx);
  }
  lint_sc(a);
}

ANN static void lint_stmt_return(Lint *a, Stmt_Exp b) {
  lint(a, "{+M}return{0}");
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
  lint(a, "{+M}match{0}");
  lint_space(a);
  lint_exp(a, b->cond);
  lint_space(a);
  lint_lbrace(a);
  lint_nl(a);
  INDENT(a, lint_case_list(a, b->list))
  lint_indent(a);
  lint_nl(a);
  lint_rbrace(a);
  if (b->where) {
    lint_space(a);
    lint(a, "{+M}where{0}");
    lint_space(a);
    lint_stmt(a, b->where);
  }
}

ANN static void lint_stmt_case(Lint *a, Stmt_Match b) {
  //for(uint32_t i = 0; i < b->len; i++) {
    lint_indent(a);
    lint(a, "{+M}case{0}");
    lint_space(a);
    lint_exp(a, b->cond);
    if (b->when) {
      lint_space(a);
      lint(a, "{+M}when{0}");
      lint_space(a);
      lint_exp(a, b->when);
    }
    //  lint_space(a);
    lint(a, "{-}:{0}");
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

ANN static void lint_stmt_pp(Lint *a, Stmt_PP b) {
  if (b->pp_type == ae_pp_nl) return;
  if (b->pp_type == ae_pp_locale) {
    lint(a, "{M/}#%s{0} %s{0}", pp[b->pp_type], pp_color[b->pp_type]);
    lint_symbol(a, b->xid);
    lint_space(a);
    if(b->exp) lint_exp(a, b->exp);
    return;
  }
  if (b->pp_type == ae_pp_include) {
    lint(a, "{M/}#%s{0} %s<%s>{0}", pp[b->pp_type], pp_color[b->pp_type],
         b->data ?: "");
  } else if (b->pp_type != ae_pp_comment) {
    lint(a, "{M/}#%s{0} %s%s{0}", pp[b->pp_type], pp_color[b->pp_type],
         b->data ?: "");
  } else {
    lint_indent(a);
//  a->skip_indent = indent;
//    if (!b->data || (*b->data != '-' && *b->data != '+'))
//      lint(a, "{/-}#%s%s{0}", pp[b->pp_type], b->data ?: "");
//    else
      lint(a, "{-/}#!%s{0}", b->data ?: "");
  }
}

ANN static void lint_stmt_defer(Lint *a, Stmt_Defer b) {
  lint(a, "{+M}defer{0}");
  lint_space(a);
  lint_stmt(a, b->stmt);
}

ANN static void lint_stmt(Lint *a, Stmt b) {
  check_pos(a, &b->pos->first);
  const uint skip_indent = a->skip_indent;
  if (b->stmt_type != ae_stmt_pp) lint_indent(a);
  lint_stmt_func[b->stmt_type](a, &b->d);
  if (!skip_indent) lint_nl(a);
  check_pos(a, &b->pos->last);
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
    lint(a, "{M}");
    lint_op(a, b->xid);
    lint(a, "{0}");
    lint_space(a);
    if (b->tmpl) lint_tmpl(a, b->tmpl);
  }
  if (b->td && !fbflag(b, fbflag_locale)) {
    lint_type_decl(a, b->td);
    lint_space(a);
  }
  if (!fbflag(b, fbflag_unary)) {
    lint(a, "{M}%s{0}", s_name(b->xid));
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
  check_pos(a, &b->pos->first);
  if(fbflag(b->base, fbflag_locale))
    lint(a, "{+C}locale{0}");
  else if(!fbflag(b->base, fbflag_op) && strcmp(s_name(b->base->xid), "new"))
    lint(a, "{+C}fun{0}");
  else
    lint(a, "{+C}operator{0}");
  lint_space(a);
  lint_func_base(a, b->base);
  if (a->ls->builtin) {
    lint_sc(a);
    lint_nl(a);
    return;
  }
  lint_space(a);
  a->skip_indent += 1;
  if (!GET_FLAG(b->base, abstract) && b->d.code)
    lint_stmt(a, b->d.code);
  else {
    lint_sc(a);
  }
  lint_nl(a);
  check_pos(a, &b->pos->last);
}

ANN void lint_class_def(Lint *a, Class_Def b) {
  check_pos(a, &b->pos->first);
  lint(a, "{+C}class{0}");
  lint_space(a);
  lint_flag(a, b);
  lint(a, "{+W}%s{0}", s_name(b->base.xid));
  if (b->base.tmpl) lint_tmpl(a, b->base.tmpl);
  lint_space(a);
  if (b->base.ext) {
    lint(a, "{+/C}extends{0}");
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
    check_pos(a, &b->pos->last);
  }
  lint_nl(a);
}

ANN void lint_enum_def(Lint *a, Enum_Def b) {
  lint(a, "{+C}enum{0}");
  lint_space(a);
  lint_flag(a, b);
  lint(a, "{/}%s{0}", s_name(b->xid));
  lint_space(a);
  lint_lbrace(a);
  lint_space(a);
  lint_id_list(a, b->list);
  lint_space(a);
  lint_rbrace(a);
  lint_nl(a);
}

ANN void lint_union_def(Lint *a, Union_Def b) {
  check_pos(a, &b->pos->first);
  lint(a, "{+C}union{0}");
  lint_space(a);
  lint_flag(a, b);
  lint(a, "{/}%s{0}", s_name(b->xid));
  lint_space(a);
  if (b->tmpl) lint_tmpl(a, b->tmpl);
  lint_space(a);
  lint_lbrace(a);
  lint_nl(a);
  INDENT(a, lint_union_list(a, b->l))
  lint_rbrace(a);
  lint_sc(a);
  lint_nl(a);
  check_pos(a, &b->pos->last);
}

ANN void lint_fptr_def(Lint *a, Fptr_Def b) {
  //  check_pos(a, &b->pos->first);
  lint(a, "{+C}funptr{0}");
  lint_space(a);
  lint_func_base(a, b->base);
  lint_sc(a);
  lint_nl(a);
  //  check_pos(a, &b->pos->last);
}

ANN void lint_type_def(Lint *a, Type_Def b) {
  //  check_pos(a, &b->pos->first);
  lint(a, "{+C}%s{0}", !b->distinct ? "typedef" : "distinct");
  lint_space(a);
  if (b->ext) {
    lint_type_decl(a, b->ext);
    lint_space(a);
  }
  lint(a, "{/}%s{0}", s_name(b->xid));
  if (b->tmpl) lint_tmpl(a, b->tmpl);
  if (b->when) {
    lint_nl(a);
    a->indent += 2;
    lint_indent(a);
    lint(a, "{M}when{0}");
    a->indent -= 2;
    lint_space(a);
    lint_exp(a, b->when);
  }
  lint_sc(a);
  lint_nl(a);
  //  check_pos(a, &b->pos->last);
}

ANN static void lint_extend_def(Lint *a, Extend_Def b) {
  //  check_pos(a, &b->pos->first);
  lint(a, "{+C}extends{0}");
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
  lint(a, "{+C}trait{0}");
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

DECL_SECTION_FUNC(lint, void, Lint *)
ANN static void lint_section(Lint *a, Section *b) {
  //  check_pos(a, &b->pos->first);
  if (b->section_type != ae_section_stmt) lint_indent(a);
  lint_section_func[b->section_type](a, *(void **)&b->d);
  //  check_pos(a, &b->pos->last);
}

ANN void lint_ast(Lint *a, Ast b) {
  const m_uint sz = b->len;
  for(m_uint i = 0; i < sz; i++) {
    Section *section = mp_vector_at(b, Section, i);
    lint_section(a, section);
    if(i < sz -1) lint_nl(a);
  }
}
