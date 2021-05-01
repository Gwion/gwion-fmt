enum char_type { cht_id, cht_op, cht_sp };

struct LintState {
  bool py;
  bool unpy;
  bool onlypy;
  bool minimize;
  bool color;
  bool builtin;
};

typedef struct {
  MemPool mp;
  Map macro;
  struct LintState *ls;
//  struct pos_t pos;
  unsigned int indent;
  unsigned int skip_indent;
  unsigned int nl;
  enum char_type last;
//  m_bool skip; // check_pos
  m_bool need_space;
} Lint;

ANN void lint(Lint *a, const m_str , ...);
ANN void lint_indent(Lint *a);
ANN void lint_sc(Lint *a);
ANN void lint_nl(Lint *a);
ANN void lint_comma(Lint *a);
ANN void lint_space(Lint *a);
ANN void lint_lparen(Lint *a);
ANN void lint_rparen(Lint *a);
ANN void lint_lbrace(Lint *a);
ANN void lint_rbrace(Lint *a);
ANN void lint_exp(Lint *a, Exp b);
ANN void lint_func_def(Lint *a, Func_Def b);
ANN void lint_class_def(Lint *a, Class_Def b);
ANN void lint_enum_def(Lint *a, Enum_Def b);
ANN void lint_union_def(Lint *a, Union_Def b);
ANN void lint_fptr_def(Lint *a, Fptr_Def b);
ANN void lint_type_def(Lint *a, Type_Def b);
ANN void lint_ast(Lint *a, Ast b);
