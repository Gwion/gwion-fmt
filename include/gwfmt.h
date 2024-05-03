#pragma once

#include "gwion_util.h"
#include "gwion_ast.h"
#include "casing.h"

enum gwfmt_char_type { cht_id, cht_colon, cht_lbrack, cht_nl, cht_op, cht_delim, cht_sp };

typedef enum CaseType {
  TypeCase,
  VariableCase,
  FunctionCase,
  LastCase
} CaseType;

#ifdef LINT_IMPL
static const char* ct_name[LastCase] = {
  "Type",
  "Variable",
  "Function",
};
#endif

typedef enum FmtColor {
  StringColor,
  KeywordColor,
  FlowColor,
  FunctionColor,
  TypeColor,
  VariableColor,
  NumberColor,
  OpColor,
  ModColor,
  PunctuationColor,
  PPColor,
  SpecialColor,
  LastColor,
} FmtColor; 

#define MAX_COLOR_LENGTH 8

typedef struct GwFmtMark {
  uint16_t line;
  char sign[16];
} GwFmtMark;

typedef struct GwfmtState {
  GwText       text;
  PPArg       *ppa;
  Casing       cases[LastCase];
  char         colors[LastColor][MAX_COLOR_LENGTH];
  unsigned int nindent;
  MP_Vector   *marks;        // NOTE: make it a vector?
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
  bool         error;
  bool         fix;
} GwfmtState;

void gwfmt_state_init(GwfmtState *ls);

typedef struct {
  const char *filename;
  MemPool           mp;
  SymTable          *st; // only for spread
  Map               macro;
  struct GwfmtState *ls;
  //  struct pos_t pos;
  unsigned int   indent;
  unsigned int   skip_indent;
  unsigned int   nl;
  enum gwfmt_char_type last;
  unsigned int   line;
  unsigned int   column;
  //  bool skip; // check_pos
  bool need_space;
} Gwfmt;

ANN int  gwfmt_util(Gwfmt *a, const char *, ...);
ANN void gwfmt(Gwfmt *a, const char*, ...);
ANN void gwfmt_indent(Gwfmt *a);
ANN void gwfmt_sc(Gwfmt *a);
ANN void gwfmt_nl(Gwfmt *a);
ANN void gwfmt_comma(Gwfmt *a);
ANN void gwfmt_space(Gwfmt *a);
ANN void gwfmt_lparen(Gwfmt *a);
ANN void gwfmt_rparen(Gwfmt *a);
ANN void gwfmt_lbrace(Gwfmt *a);
ANN void gwfmt_rbrace(Gwfmt *a);
ANN void gwfmt_exp(Gwfmt *a, Exp* b);
ANN void gwfmt_func_def(Gwfmt *a, Func_Def b);
ANN void gwfmt_class_def(Gwfmt *a, Class_Def b);
ANN void gwfmt_enum_def(Gwfmt *a, Enum_Def b);
ANN void gwfmt_union_def(Gwfmt *a, Union_Def b);
ANN void gwfmt_fptr_def(Gwfmt *a, Fptr_Def b);
ANN void gwfmt_type_def(Gwfmt *a, Type_Def b);
ANN void gwfmt_prim_def(Gwfmt *a, Prim_Def b);
ANN void gwfmt_ast(Gwfmt *a, Ast b);
ANN void gwfmt_type_decl(Gwfmt *a, const Type_Decl *b);
ANN void gwfmt_variable(Gwfmt *a, const Variable *b);


void set_color(struct GwfmtState *ls, const FmtColor b,
               const char *color);
bool run_config(struct GwfmtState *ls, const char *filename);
