enum char_type { cht_id, cht_colon, cht_lbrack, cht_nl, cht_op, cht_delim, cht_sp };

struct GwfmtState {
  GwText       text;
  struct PPArg_ *ppa;
  unsigned int nindent;
  unsigned int mark;
  unsigned int base_column;
  bool         py;
  bool         unpy;
  bool         onlypy;
  bool         minimize;
  bool         color;
  bool         builtin;
  bool         pretty;
  bool         show_line;
  bool         header;
  bool         use_tabs;
};

typedef struct {
  MemPool           mp;
  SymTable          *st; // only for spread
  Map               macro;
  struct GwfmtState *ls;
  //  struct pos_t pos;
  unsigned int   indent;
  unsigned int   skip_indent;
  unsigned int   nl;
  enum char_type last;
  unsigned int   line;
  unsigned int   column;
  //  bool skip; // check_pos
  bool need_space;
} Gwfmt;

ANN void gwfmt(Gwfmt *a, const m_str, ...);
ANN void gwfmt_util(Gwfmt *a, const m_str, ...);
ANN void gwfmt_indent(Gwfmt *a);
ANN void gwfmt_sc(Gwfmt *a);
ANN void gwfmt_nl(Gwfmt *a);
ANN void gwfmt_comma(Gwfmt *a);
ANN void gwfmt_space(Gwfmt *a);
ANN void gwfmt_lparen(Gwfmt *a);
ANN void gwfmt_rparen(Gwfmt *a);
ANN void gwfmt_lbrace(Gwfmt *a);
ANN void gwfmt_rbrace(Gwfmt *a);
ANN void gwfmt_exp(Gwfmt *a, Exp b);
ANN void gwfmt_func_def(Gwfmt *a, Func_Def b);
ANN void gwfmt_class_def(Gwfmt *a, Class_Def b);
ANN void gwfmt_enum_def(Gwfmt *a, Enum_Def b);
ANN void gwfmt_union_def(Gwfmt *a, Union_Def b);
ANN void gwfmt_fptr_def(Gwfmt *a, Fptr_Def b);
ANN void gwfmt_type_def(Gwfmt *a, Type_Def b);
ANN void gwfmt_prim_def(Gwfmt *a, Prim_Def b);
ANN void gwfmt_ast(Gwfmt *a, Ast b);
