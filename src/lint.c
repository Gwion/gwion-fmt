#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "lint.h"
#include "unpy.h"

#define BUF_SIZE 16

#define INDENT(a, b) { ++a->indent; {b;}; --a->indent; }

#define COLOR(a, b) { if(a->ls->color) printf(b); }

#define check_pos(a,b)


ANN static enum char_type cht(const char c) {
  if(isalnum(c) || c == '_')
    return cht_id;
  char *op = "?:$@+-/%~<>^|&!=*";
  do if(c == *op)
    return cht_op;
  while(++op);
  return cht_sp;
}

static void lint(Lint *a, const m_str fmt, ...) {
  a->nl = 0;
//  if(!a->skip) {
    va_list ap, aq;


   va_start(ap, fmt);
   int n = vsnprintf(NULL, 0, fmt, ap);
   va_end(ap);


    char buf[n+1];
    va_start(ap, fmt);


    vsprintf(buf, fmt, ap);
    printf(buf);
    if(a->need_space && a->last != cht_op && a->last == cht(buf[0]))
      printf(" ");
    a->last = cht(buf[n - 1]);
    va_end(ap);
    a->need_space = 0;
//  }
}

ANN static void lint_space(Lint *a) {
  if(!a->ls->minimize)
    lint(a, " ");
  else
    a->need_space = 1;
}

ANN static void lint_nl(Lint *a) {
  const unsigned int nl = a->nl + 1;
  if(!a->ls->minimize) {
    if(a->nl < 2)
      lint(a, "\n");
  }
  a->nl = nl;
}

ANN static void lint_lbrace(Lint *a) {
  if(!a->ls->py)
    lint(a, "{");
}

ANN static void lint_rbrace(Lint *a) {
  if(!a->ls->py)
    lint(a, "}");
}

ANN static void lint_sc(Lint *a) {
  if(!a->ls->py)
    lint(a, ";");
}

ANN static void lint_indent(Lint *a) {
  for(unsigned int i = 0; i < a->indent; ++i) {
    lint_space(a);
    lint_space(a);
  }
}
/*
static inline void check_pos(Lint *a, const struct pos_t *b) {
  if(!map_size(a->macro))
    return;
  const loc_t loc = (loc_t)VKEY(a->macro, 0);
  if(b->line >= a->pos.line || (b->line == a->pos.line && b->column + 1 >= a->pos.column)) {
    if(!a->skip) {
      Macro m = (Macro)VVAL(a->macro, 0);
      lint(a, m->name);
      if(b->line > a->pos.line)
        lint_nl(a);
      a->pos = loc->last;
      a->skip = 1;
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

#define _FLAG(a, b) (((a) & ae_flag_##b) == (ae_flag_##b))
ANN static void lint_flag(Lint *a, ae_flag b) {
  int state = 0;
  COLOR(a, "\033[32;2m");
  if(_FLAG(b, private) && (state = 1))
    lint(a, "private");
  else if(_FLAG(b, protect) && (state = 1))
    lint(a, "protect");
  if(state)
    lint_space(a);
  state = 0;
  COLOR(a, "\033[32;3m");
  if(_FLAG(b, static) && (state = 1))
    lint(a, "static");
  else if(_FLAG(b, global) && (state = 1))
    lint(a, "global");
  COLOR(a, "\033[0m");
  if(state)
    lint_space(a);
  state = 0;
  COLOR(a, "\033[32;4m");
  if(_FLAG(b, abstract) && (state = 1))
    lint(a, "abstract");
  else if(_FLAG(b, final) && (state = 1))
    lint(a, "final");
  COLOR(a, "\033[0m");
  if(state)
    lint_space(a);
}
#define lint_flag(a, b) lint_flag(a, b->flag)

ANN static void lint_symbol(Lint *a, Symbol b) {
  lint(a, s_name(b));
}

ANN static void lint_array_sub(Lint *a, Array_Sub b) {
  lint(a, "[");
  if(b->exp) {
    if(b->exp->next)
      lint_space(a);
    lint_exp(a, b->exp);
    if(b->exp->next)
      lint_space(a);
  }
  lint(a, "]");
}

#define NEXT(a,b,c) \
  if(b->next) {     \
    lint(a, ",");   \
    lint_space(a);  \
    c(a, b->next);  \
  }

ANN static void lint_id_list(Lint *a, ID_List b) {
  check_pos(a, &b->pos->first);
  lint_symbol(a, b->xid);
  check_pos(a, &b->pos->last);
  NEXT(a, b, lint_id_list)
}

ANN static void _lint_type_list(Lint *a, Type_List b) {
  lint_type_decl(a, b->td);
  NEXT(a, b, _lint_type_list);
}

ANN static void lint_type_list(Lint *a, Type_List b) {
  lint(a, ":[");
  lint_space(a);
  _lint_type_list(a, b);
  lint(a, "]");
}

ANN static void lint_tmpl(Lint *a, Tmpl *b) {
  if(b->list) {
    lint(a, ":[");
    lint_space(a);
    lint_id_list(a, b->list);
    lint_space(a);
    lint(a, "]");
  }
  if(b->call)
    lint_type_list(a, b->call);
}

ANN static void lint_range(Lint *a, Range *b) {
  lint(a, "[");
  if(b->start)
    lint_exp(a, b->start);
  lint_space(a);
  lint(a, ":");
  lint_space(a);
  if(b->end)
    lint_exp(a, b->end);
  lint(a, "]");
}

ANN static void lint_type_decl(Lint *a, Type_Decl *b) {
  check_pos(a, &b->pos->first);
  COLOR(a, "\33[32;1m")
  if(GET_FLAG(b, const)) {
    lint(a, "const");
    lint_space(a);
  }
  if(GET_FLAG(b, nonnull)) {
    lint(a, "nonnull");
    lint_space(a);
  }
  if(GET_FLAG(b, ref)) {
    lint(a, "ref");
    lint_space(a);
  }
  COLOR(a, "\033[0m");
  lint_flag(a, b);
  COLOR(a, "\033[31m")
  if(b->xid)
    lint_symbol(a, b->xid);
  if(b->types)
    lint_type_list(a, b->types);
  COLOR(a, "\033[0m")
  if(b->exp)
    lint_exp(a, b->exp);
  if(b->array)
    lint_array_sub2(a, b->array);
  if(b->next) {
    lint(a, ".");
    lint_type_decl(a, b->next);
  }
  lint_space(a);
  check_pos(a, &b->pos->last);
}

ANN static void lint_prim_id(Lint *a, Symbol *b) {
  lint_symbol(a, *b);
}

ANN static void lint_prim_num(Lint *a, m_uint *b) {
  lint(a, "%"UINT_F, *b);
}

ANN static void lint_prim_float(Lint *a, m_float *b) {
  if(*b == floor(*b))
    lint(a, "%li.", (m_int)*b);
  else
    lint(a, "%g", *b);
}

ANN static void lint_prim_str(Lint *a, m_str *b) {
  lint(a, "\"%s\"", *b);
}

ANN static void lint_prim_array(Lint *a, Array_Sub *b) {
  lint_array_sub(a, *b);
}

ANN static void lint_prim_range(Lint *a, Range* *b) {
  lint_range(a, *b);
}

ANN static void lint_prim_hack(Lint *a, Exp *b) {
  lint(a, "<<<");
  lint_space(a);
  lint_exp(a, *b);
  lint_space(a);
  lint(a, ">>>");
}

ANN static void lint_prim_typeof(Lint *a, Exp *b) {
  COLOR(a, "\033[33m")
  lint(a, "typeof");
  COLOR(a, "\033[0m")
  paren_exp(a, *b);
}

ANN static void lint_prim_char(Lint *a, m_str *b) {
  COLOR(a, "\033[35;1m")
  lint(a, "'%s'", *b);
  COLOR(a, "\033[0m")
}

ANN static void lint_prim_nil(Lint *a, void *b) {
  lint(a, "()");
}

DECL_PRIM_FUNC(lint, void, Lint*)
ANN static void lint_prim(Lint *a, Exp_Primary *b) {
  lint_prim_func[b->prim_type](a, &b->d);
}

ANN static void lint_var_decl(Lint *a, Var_Decl b) {
  check_pos(a, &b->pos->first);
  if(b->xid)
    lint_symbol(a, b->xid);
  if(b->array)
    lint_array_sub2(a, b->array);
  check_pos(a, &b->pos->last);
}

ANN static void lint_var_decl_list(Lint *a, Var_Decl_List b) {
  lint_var_decl(a, b->self);
  NEXT(a, b, lint_var_decl_list);
}

ANN static void lint_exp_decl(Lint *a, Exp_Decl *b) {
  if(b->td) {
    if(!(GET_FLAG(b->td, const) || GET_FLAG(b->td, nonnull) || GET_FLAG(b->td, ref))) {
      COLOR(a, "\33[32;1m")
      lint(a, "var");
      COLOR(a, "\33[0m")
      lint_space(a);
    }
    lint_type_decl(a, b->td);
  }
  lint_var_decl_list(a, b->list);
}

ANN static void lint_op(Lint *a, const Symbol b) {
  char *str = s_name(b), c;
  while((c = *str)) {
    if(c == '%')
      printf("%%");
    else
      printf("%c", c);
    ++str;
  }
}

ANN static void lint_exp_binary(Lint *a, Exp_Binary *b) {
  const unsigned int coloncolon = !strcmp(s_name(b->op), "::");
  maybe_paren_exp(a, b->lhs);
  if(!coloncolon)
    lint_space(a);
//  lint_symbol(a, b->op);
  lint_op(a, b->op);
  if(!coloncolon)
    lint_space(a);
  lint_exp(a, b->rhs);
}

ANN static int isop(const Symbol s) {
  char *name = s_name(s);
  for(size_t i = 0; i < strlen(name); ++i) {
    if(isalnum(name[i]))
      return 0;
  }
  return 1;
}

ANN static void lint_exp_unary(Lint *a, Exp_Unary *b) {
  if(s_name(b->op)[0] == '$' && !isop(b->op))
    lint(a, "(");
  lint_op(a, b->op);
  if(s_name(b->op)[0] == '$' && !isop(b->op))
    lint(a, ")");
  if(!isop(b->op))
    lint_space(a);
  if(b->exp)
    lint_exp(a, b->exp);
  if(b->td)
    lint_type_decl(a, b->td);
  if(b->code)
    lint_stmt_code(a, &b->code->d.stmt_code);
}

ANN static void lint_exp_cast(Lint *a, Exp_Cast *b) {
  if(b->exp->exp_type != ae_exp_decl)
    lint_exp(a, b->exp);
  else
    paren_exp(a, b->exp);
  lint_space(a);
  lint(a, "$");
  lint_space(a);
  lint_type_decl(a, b->td);
}

ANN static void lint_exp_post(Lint *a, Exp_Postfix *b) {
  lint_exp(a, b->exp);
  lint_op(a, b->op);
}

ANN static void lint_exp_call(Lint *a, Exp_Call *b) {
  if(b->func->exp_type != ae_exp_decl)
    lint_exp(a, b->func);
  else
    paren_exp(a, b->func);
  if(b->tmpl)
    lint_tmpl(a, b->tmpl);
  if(b->args)
    paren_exp(a, b->args);
  else
    lint(a, "()");
}

ANN static void lint_exp_array(Lint *a, Exp_Array *b) {
  lint_exp(a, b->base);
  lint_array_sub2(a, b->array);
}

ANN static void lint_exp_slice(Lint *a, Exp_Slice *b) {
  if(b->base->exp_type != ae_exp_primary &&
     b->base->exp_type != ae_exp_array   &&
     b->base->exp_type != ae_exp_call    &&
     b->base->exp_type != ae_exp_post    &&
     b->base->exp_type != ae_exp_dot)
    lint_exp(a, b->base);
  else
    paren_exp(a, b->base);
  lint_range(a, b->range);
}

ANN static void lint_exp_if(Lint *a, Exp_If *b) {
  lint_exp(a, b->cond);
  lint_space(a);
  lint(a, "?");
  if(b->if_exp) {
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
  do {
    lint_symbol(a, b->var_decl->xid);
    lint_space(a);
  } while((b = b->next));
}

ANN static void lint_exp_lambda(Lint *a, Exp_Lambda *b) {
  lint(a, "\\");
  if(b->def->base->args) {
    lint_lambda_list(a, b->def->base->args);
    lint_space(a);
  }
  if(b->def->d.code)
    lint_stmt_code(a, &b->def->d.code->d.stmt_code);
}

DECL_EXP_FUNC(lint, void, Lint*)
ANN static void lint_exp(Lint *a, Exp b) {
  check_pos(a, &b->pos->first);
  lint_exp_func[b->exp_type](a, &b->d);
  check_pos(a, &b->pos->last);
  NEXT(a, b, lint_exp)
}

ANN static void lint_prim_interp(Lint *a, Exp *b) {
  COLOR(a, "\033[37m")
  lint(a, "\"");
  Exp e = *b;
  while(e) {
    if(e->exp_type == ae_exp_primary && e->d.prim.prim_type == ae_prim_str) {
      lint(a, e->d.prim.d.str);
    } else {
      COLOR(a, "\033[0m")
      lint(a, "${"); // do not use rbace
      lint_space(a);
      check_pos(a, &e->pos->first);
      lint_exp_func[e->exp_type](a, &e->d);
      check_pos(a, &e->pos->last);
      lint_space(a);
      lint(a, "}");
      COLOR(a, "\033[37m")
    }
    e = e->next;
  }
  lint(a, "\"");
  COLOR(a, "\033[0m")
}

ANN static void lint_array_sub2(Lint *a, Array_Sub b) {
  Exp e = b->exp;
  for(m_uint i = 0; i < b->depth; ++i) {
    lint(a, "[");
    if(e) {
      check_pos(a, &e->pos->first);
      lint_exp_func[e->exp_type](a, &e->d);
      check_pos(a, &e->pos->last);
      e = e->next;
    }
    lint(a, "]");
  }
}

ANN static void paren_exp(Lint *a, Exp b) {
  lint(a, "(");
  lint_exp(a, b);
  lint(a, ")");
}

ANN static void maybe_paren_exp(Lint *a, Exp b) {
  if(b->next)
    paren_exp(a, b);
  else
    lint_exp(a, b);
}

ANN static void lint_stmt_exp(Lint *a, Stmt_Exp b) {
  if(b->val)
    lint_exp(a, b->val);
  lint_sc(a);
}

DECL_STMT_FUNC(lint, void, Lint*)
ANN static void lint_stmt_while(Lint *a, Stmt_Flow b) {
  if(!b->is_do) {
    COLOR(a, "\033[37m")
    lint(a, "while");
    COLOR(a, "\033[0m")
    paren_exp(a, b->cond);
  } else {
    COLOR(a, "\033[37m")
    lint(a, "do");
    COLOR(a, "\033[0m")
  }
  lint_space(a);
  lint_stmt_func[b->body->stmt_type](a, &b->body->d);
  if(b->is_do) {
    lint_space(a);
    COLOR(a, "\033[37m")
    lint(a, "while");
    COLOR(a, "\033[0m")
    paren_exp(a, b->cond);
    lint_sc(a);
  }
}

ANN static void lint_stmt_until(Lint *a, Stmt_Flow b) {
  if(!b->is_do) {
    COLOR(a, "\033[37m")
    lint(a, "until");
    COLOR(a, "\033[0m")
    paren_exp(a, b->cond);
  } else {
    COLOR(a, "\033[37m")
    lint(a, "do");
    COLOR(a, "\033[0m")
  }
  lint_space(a);
  lint_stmt_func[b->body->stmt_type](a, &b->body->d);
  if(b->is_do) {
    lint_space(a);
    COLOR(a, "\033[37m")
    lint(a, "until");
    COLOR(a, "\033[0m")
    paren_exp(a, b->cond);
    lint_sc(a);
  }
}

ANN static void lint_stmt_for(Lint *a, Stmt_For b) {
  COLOR(a, "\033[37m")
  lint(a, "for");
  COLOR(a, "\033[0m")
  lint(a, "(");
  lint_stmt_exp(a, &b->c1->d.stmt_exp);
  lint_space(a);
  const unsigned int py = a->ls->py;
  a->ls->py = 0;
  if(b->c2)
    lint_stmt_exp(a, &b->c2->d.stmt_exp);
  lint_space(a);
  a->ls->py = py;
  if(b->c3)
    lint_exp(a, b->c3);
  lint(a, ")");
  lint_stmt(a, b->body);
}

ANN static void lint_stmt_each(Lint *a, Stmt_Each b) {
  COLOR(a, "\033[37m")
  lint(a, "foreach");
  COLOR(a, "\033[0m")
  lint(a, "(");
  lint_symbol(a, b->sym);
  lint_space(a);
  lint(a, ":");
  lint_space(a);
  lint_exp(a, b->exp);
  lint(a, ")");
  if(b->body->stmt_type != ae_stmt_code)
    lint_nl(a);
  INDENT(a, lint_stmt(a, b->body))
}

ANN static void lint_stmt_loop(Lint *a, Stmt_Loop b) {
  COLOR(a, "\033[37m")
  lint(a, "repeat");
  COLOR(a, "\033[0m")
  paren_exp(a, b->cond);
  lint_stmt(a, b->body);
}


ANN static void lint_code(Lint *a, Stmt b) {
  if (b->stmt_type == ae_stmt_if ||
      b->stmt_type == ae_stmt_code ||
     (b->stmt_type == ae_stmt_exp && !b->d.stmt_exp.val)) {
    lint_space(a);
    lint_stmt_func[b->stmt_type](a, &b->d);
  } else {
    lint_nl(a);
    INDENT(a, lint_indent(a); lint_stmt_func[b->stmt_type](a, &b->d))
  }
}

ANN static void lint_stmt_if(Lint *a, Stmt_If b) {
  COLOR(a, "\033[37m")
  lint(a, "if");
  COLOR(a, "\033[0m")
  paren_exp(a, b->cond);
  lint_code(a, b->if_body);
  if(b->else_body) {
    lint_nl(a);
    lint_indent(a);
    COLOR(a, "\033[37m")
    lint(a, "else");
    COLOR(a, "\033[0m")
    lint_code(a, b->else_body);
  }
}


ANN static void lint_stmt_code(Lint *a, Stmt_Code b) {
  lint_lbrace(a);
  if(b->stmt_list) {
    INDENT(a, lint_nl(a); lint_stmt_list(a, b->stmt_list))
    lint_indent(a);
  }
  lint_rbrace(a);
}

ANN static void lint_stmt_varloop(Lint *a, Stmt_VarLoop b) {
  COLOR(a, "\033[37;2m")
  lint(a, "varloop");
  COLOR(a, "\033[0m")
  lint_space(a);
  lint_exp(a, b->exp);
  if(b->body->stmt_type != ae_stmt_code) {
    lint_nl(a);
    INDENT(a, lint_stmt(a, b->body))
  } else
    lint_stmt_code(a, &b->body->d.stmt_code);
}

ANN static void lint_stmt_break(Lint *a, Stmt_Exp b) {
  COLOR(a, "\033[34m")
  lint(a, "break");
  COLOR(a, "\033[0m")
  lint_sc(a);
}

ANN static void lint_stmt_continue(Lint *a, Stmt_Exp b) {
  COLOR(a, "\033[34m")
  lint(a, "continue");
  COLOR(a, "\033[0m")
  lint_sc(a);
}

ANN static void lint_stmt_return(Lint *a, Stmt_Exp b) {
  COLOR(a, "\033[34m")
  lint(a, "return");
  COLOR(a, "\033[0m")
  if(b->val) {
    lint_space(a);
    lint_exp(a, b->val);
  }
  lint_sc(a);
}

ANN static void lint_case_list(Lint *a, Stmt_List b) {
  lint_stmt_case(a, &b->stmt->d.stmt_match);
  if(b->next) {
    lint_nl(a);
    lint_case_list(a, b->next);
  }
}

ANN static void lint_stmt_match(Lint *a, Stmt_Match b) {
  COLOR(a, "\033[37;2m")
  lint(a, "match");
  COLOR(a, "\033[0m")
  lint_space(a);
  lint_exp(a, b->cond);
  lint_space(a);
  lint_lbrace(a);
  lint_nl(a);
  INDENT(a, lint_case_list(a, b->list))
  lint_indent(a);
  lint_nl(a);
  lint_rbrace(a);
  if(b->where) {
    lint_space(a);
    COLOR(a, "\033[37;2m")
    lint(a, "where");
    COLOR(a, "\033[0m")
    lint_space(a);
    lint_stmt(a, b->where);
  }
}

ANN static void lint_stmt_case(Lint *a, Stmt_Match b) {
  lint_indent(a);
  COLOR(a, "\033[37;2m")
  lint(a, "case");
  COLOR(a, "\033[0m")
  lint_space(a);
  lint_exp(a, b->cond);
  if(b->when) {
    lint_space(a);
    COLOR(a, "\033[37;2m")
    lint(a, "when");
    COLOR(a, "\033[0m")
    lint_space(a);
    lint_exp(a, b->when);
  }
//  lint_space(a);
  lint(a, ":");
  if(b->list->next)
    INDENT(a, lint_stmt_list(a, b->list))
  else {
    lint_space(a);
    lint_stmt_func[b->list->stmt->stmt_type](a, &b->list->stmt->d);
  }
}

ANN static void lint_stmt_jump(Lint *a, Stmt_Jump b) {
}

static const char *pp[] = {
  "!", "include", "define", "pragma", "undef", "ifdef", "ifndef", "else", "endif"
};

ANN static void lint_stmt_pp(Lint *a, Stmt_PP b) {
  COLOR(a, "\033[34;3m")
  if(b->pp_type != ae_pp_nl)
    lint(a, "#%s %s", pp[b->pp_type], b->data ?: "");
  COLOR(a, "\033[0m")
}

ANN static void lint_stmt(Lint *a, Stmt b) {
  check_pos(a, &b->pos->first);
  lint_indent(a);
  lint_stmt_func[b->stmt_type](a, &b->d);
  lint_nl(a);
  check_pos(a, &b->pos->last);
}

ANN static void lint_arg_list(Lint *a, Arg_List b) {
  if(b->td)
    lint_type_decl(a, b->td);
  lint_var_decl(a, b->var_decl);
  NEXT(a, b, lint_arg_list);
}

ANN static void lint_decl_list(Lint *a, Decl_List b) {
  lint_indent(a);
  UNSET_FLAG(b->self->d.exp_decl.td, ref);
  UNSET_FLAG(b->self->d.exp_decl.td, const);
  UNSET_FLAG(b->self->d.exp_decl.td, nonnull);
  lint_type_decl(a, b->self->d.exp_decl.td);
  lint_var_decl(a, b->self->d.exp_decl.list->self);
//  lint_exp(a, b->self);
  lint_sc(a);
  lint_nl(a);
  if(b->next)
    lint_decl_list(a, b->next);
}

ANN static inline int isnl(Stmt b) {
  return b->stmt_type == ae_stmt_pp &&
         b->d.stmt_pp.pp_type == ae_pp_nl;
}

ANN static void lint_stmt_list(Lint *a, Stmt_List b) {
  if(b->stmt->stmt_type != ae_stmt_exp || b->stmt->d.stmt_exp.val)
    lint_stmt(a, b->stmt);
  if(b->next)
    lint_stmt_list(a, b->next);
}

ANN static void lint_func_base(Lint *a, Func_Base *b) {
  lint_flag(a, b);
  if(fbflag(b, fbflag_unary)) {
    lint_op(a, b->xid);
    lint_space(a);
    if(b->tmpl)
      lint_tmpl(a, b->tmpl);
  }
  if(b->td)
    lint_type_decl(a, b->td);
  if(!fbflag(b, fbflag_unary)) {
    lint_symbol(a, b->xid);
    if(b->tmpl)
      lint_tmpl(a, b->tmpl);
  }
  lint(a, "(");
  if(b->args)
    lint_arg_list(a, b->args);
  if(fbflag(b, fbflag_variadic)) {
    if(b->args) {
      lint(a, ",");
      lint_space(a);
    }
    lint(a, "...");
  }
  lint(a, ")");
}

ANN static void lint_func_def(Lint *a, Func_Def b) {
  check_pos(a, &b->pos->first);
  if(!fbflag(b->base, fbflag_op))
    lint(a, "fun");
  else
    lint(a, "operator");
  lint_space(a);
  lint_func_base(a, b->base);
  lint_space(a);
  if(!GET_FLAG(b->base, abstract) && b->d.code)
    lint_stmt(a, b->d.code);
  check_pos(a, &b->pos->last);
}

ANN static void lint_class_def(Lint *a, Class_Def b) {
  check_pos(a, &b->pos->first);
  lint(a, "class");
  lint_space(a);
  lint_flag(a, b);
  lint_symbol(a, b->base.xid);
  if(b->base.tmpl)
    lint_tmpl(a, b->base.tmpl);
  lint_space(a);
  if(b->base.ext) {
    lint(a, "extends");
    lint_space(a);
    lint_type_decl(a, b->base.ext);
  }
  if(b->body) {
    lint_lbrace(a);
    lint_nl(a);
    INDENT(a, lint_ast(a, b->body))
    lint_rbrace(a);
  } else
    lint(a, "{}");
  check_pos(a, &b->pos->last);
  lint_nl(a);
}

ANN static void lint_enum_def(Lint *a, Enum_Def b) {
  lint(a, "enum");
  lint_space(a);
  lint_flag(a, b);
  if(b->xid) {
    lint_symbol(a, b->xid);
    lint_space(a);
  }
  lint_lbrace(a);
  lint_space(a);
  lint_id_list(a, b->list);
  lint_space(a);
  lint_rbrace(a);
}

ANN static void lint_union_def(Lint *a, Union_Def b) {
  check_pos(a, &b->pos->first);
  lint(a, "union");
  lint_space(a);
  lint_flag(a, b);
  if(b->type_xid) {
    lint_symbol(a, b->type_xid);
    lint_space(a);
  }
  if(b->tmpl)
    lint_tmpl(a, b->tmpl);
  lint_lbrace(a);
  lint_nl(a);
  INDENT(a, lint_decl_list(a, b->l))
  lint_rbrace(a);
  if(b->xid) {
    lint_space(a);
    lint_symbol(a, b->xid);
  }
  lint_sc(a);
  check_pos(a, &b->pos->last);
}

ANN static void lint_fptr_def(Lint *a, Fptr_Def b) {
//  check_pos(a, &b->pos->first);
  lint(a, "funcdef");
  lint_space(a);
  lint_func_base(a, b->base);
  lint_sc(a);
  lint_nl(a);
//  check_pos(a, &b->pos->last);
}

ANN static void lint_type_def(Lint *a, Type_Def b) {
//  check_pos(a, &b->pos->first);
  lint(a, "typedef");
  lint_space(a);
  if(b->ext)
    lint_type_decl(a, b->ext);
  lint_symbol(a, b->xid);
  if(b->tmpl)
    lint_tmpl(a, b->tmpl);
  lint_sc(a);
  lint_nl(a);
//  check_pos(a, &b->pos->last);
}

DECL_SECTION_FUNC(lint, void, Lint*)
ANN static void lint_section(Lint *a, Section *b) {
//  check_pos(a, &b->pos->first);
  if(b->section_type != ae_section_stmt)
    lint_indent(a);
  lint_section_func[b->section_type](a, *(void**)&b->d);
//  check_pos(a, &b->pos->last);
}

ANN static void lint_ast(Lint *a, Ast b) {
  lint_section(a, b->section);
  if(b->next) {
    lint_nl(a);
    lint_ast(a, b->next);
  }
}

ANN static void lint_gw(struct AstGetter_ *arg, struct LintState *ls) {
  const Ast ast = parse(arg);
  if(ast) {
    Lint lint = { .mp=arg->st->p, .ls=ls};
    lint_ast(&lint, ast);
    free_ast(lint.mp, ast);
  }
}

ANN static void read_py(struct AstGetter_ *arg, char **ptr, size_t *sz) {
  FILE *f = open_memstream(ptr, sz);
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
  struct PPArg_ ppa = { .lint = 1};
  pparg_ini(mp, &ppa);
  struct LintState ls = {};

  for(int i = 1; i < argc; ++i) {
    if(!strcmp(argv[i], "-p")) {
      ls.py = !ls.py;
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
      ls.onlypy = ls.unpy = 0;
      continue;
    } else if(!strcmp(argv[i], "-c")) {
      ls.color = !ls.color;
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
