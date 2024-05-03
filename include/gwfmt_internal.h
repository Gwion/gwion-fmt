#include <gwion_util.h>
#include <gwion_ast.h>
#include <gwfmt.h>

ANN static void gwfmt_symbol(Gwfmt *a, Symbol b);
ANN static void gwfmt_array_sub(Gwfmt *a, Array_Sub b);
ANN static void gwfmt_id_list(Gwfmt *a, ID_List b);
ANN static void gwfmt_tmplarg_list(Gwfmt *a, TmplArg_List b);
ANN static void gwfmt_tmpl(Gwfmt *a, Tmpl *b);
ANN static void gwfmt_range(Gwfmt *a, Range *b);
ANN static void gwfmt_prim_id(Gwfmt *a, Symbol *b);
ANN static void gwfmt_prim_num(Gwfmt *a, struct gwint *b);
ANN static void gwfmt_prim_float(Gwfmt *a, m_float *b);
ANN static void gwfmt_prim_str(Gwfmt *a, struct AstString *b);
ANN static void gwfmt_prim_array(Gwfmt *a, Array_Sub *b);
ANN static void gwfmt_prim_range(Gwfmt *a, Range **b);
ANN static void gwfmt_prim_hack(Gwfmt *a, Exp* *b);
ANN static void gwfmt_prim_interp(Gwfmt *a, Exp* *b);
ANN static void gwfmt_prim_char(Gwfmt *a, m_str *b);
ANN static void gwfmt_prim_nil(Gwfmt *a, void *b);
ANN static void gwfmt_prim(Gwfmt *a, Exp_Primary *b);
ANN static void gwfmt_var_decl(Gwfmt *a, const Var_Decl *b);
ANN static void gwfmt_exp_decl(Gwfmt *a, Exp_Decl *b);
ANN static void gwfmt_exp_binary(Gwfmt *a, Exp_Binary *b);
ANN static void gwfmt_exp_unary(Gwfmt *a, Exp_Unary *b);
ANN static void gwfmt_exp_cast(Gwfmt *a, Exp_Cast *b);
ANN static void gwfmt_exp_post(Gwfmt *a, Exp_Postfix *b);
ANN static void gwfmt_exp_call(Gwfmt *a, Exp_Call *b);
ANN static void gwfmt_exp_array(Gwfmt *a, Exp_Array *b);
ANN static void gwfmt_exp_slice(Gwfmt *a, Exp_Slice *b);
ANN static void gwfmt_exp_if(Gwfmt *a, Exp_If *b);
ANN static void gwfmt_exp_dot(Gwfmt *a, Exp_Dot *b);
ANN static void gwfmt_exp_lambda(Gwfmt *a, Exp_Lambda *b);
ANN static void gwfmt_stmt_exp(Gwfmt *a, Stmt_Exp b);
ANN static void gwfmt_stmt_while(Gwfmt *a, Stmt_Flow b);
ANN static void gwfmt_stmt_until(Gwfmt *a, Stmt_Flow b);
ANN static void gwfmt_stmt_for(Gwfmt *a, Stmt_For b);
ANN static void gwfmt_stmt_each(Gwfmt *a, Stmt_Each b);
ANN static void gwfmt_stmt_loop(Gwfmt *a, Stmt_Loop b);
ANN static void gwfmt_stmt_if(Gwfmt *a, Stmt_If b);
ANN static void gwfmt_stmt_code(Gwfmt *a, Stmt_Code b);
ANN static void gwfmt_stmt_break(Gwfmt *a, Stmt_Index b);
ANN static void gwfmt_stmt_continue(Gwfmt *a, Stmt_Index b);
ANN static void gwfmt_stmt_return(Gwfmt *a, Stmt_Exp b);
ANN static void gwfmt_case_list(Gwfmt *a, Stmt_List b);
ANN static void gwfmt_stmt_match(Gwfmt *a, Stmt_Match b);
ANN static void gwfmt_stmt_case(Gwfmt *a, Stmt_Match b);
ANN static void gwfmt_stmt_pp(Gwfmt *a, Stmt_PP b);
ANN static void gwfmt_stmt_defer(Gwfmt *a, Stmt_Defer b);
ANN static void gwfmt_stmt(Gwfmt *a, Stmt* b);
ANN /*static */void gwfmt_arg_list(Gwfmt *a, Arg_List b, const bool);
ANN static void gwfmt_variable_list(Gwfmt *a, Variable_List b);
ANN static void gwfmt_stmt_list(Gwfmt *a, Stmt_List b);
ANN static void gwfmt_func_base(Gwfmt *a, Func_Base *b);
ANN static void gwfmt_section(Gwfmt *a, Section *b);
