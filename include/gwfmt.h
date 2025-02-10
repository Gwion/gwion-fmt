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

// TODO: use that!!!!!
typedef struct GwFmtMark {
  uint16_t line;
  char sign[16];
} GwFmtMark;
MK_VECTOR_TYPE(GwFmtMark, mark)

typedef struct Config {
  Casing       cases[LastCase];
  char         colors[LastColor][MAX_COLOR_LENGTH];
} Config;

typedef struct GwfmtState {
  GwText         text;
  PPArg         *ppa;
  Config        *config;
  unsigned int   nindent;
  GwFmtMarkList *marks;
  unsigned int   base_column;
  bool           py;
  bool           unpy;
  bool           onlypy;
  bool           minimize;
  bool           color;
  bool           builtin;
  bool           pretty;
  bool           show_line;
  bool           header;
  bool           use_tabs;
  bool           error;
  bool           fix_case;
  bool           check_case;
  bool           fmt;
} GwfmtState;

void gwfmt_state_init(GwfmtState *ls);

typedef struct {
  const char *filename;
  MemPool           mp;
  SymTable          *st; // only for spread
  Map               macro;
  struct GwfmtState *ls;
  CommentList   *comments;
  uint32_t       comment_index;
  //  struct pos_t pos;
  unsigned int   indent;
  unsigned int   skip_indent;
  unsigned int   nl;
  enum gwfmt_char_type last;
  pos_t          pos;
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
ANN void gwfmt_exp(Gwfmt *a, const Exp* b);
ANN void gwfmt_func_def(Gwfmt *a, const Func_Def b);
ANN void gwfmt_class_def(Gwfmt *a, const Class_Def b);
ANN void gwfmt_enum_def(Gwfmt *a, const Enum_Def b);
ANN void gwfmt_union_def(Gwfmt *a, const Union_Def b);
ANN void gwfmt_fptr_def(Gwfmt *a, const Fptr_Def b);
ANN void gwfmt_type_def(Gwfmt *a, const Type_Def b);
ANN void gwfmt_prim_def(Gwfmt *a, const Prim_Def b);
ANN void gwfmt_ast(Gwfmt *a, const Ast b);
ANN void gwfmt_type_decl(Gwfmt *a, const Type_Decl *b);
ANN void gwfmt_variable(Gwfmt *a, const Variable *b);


void set_color(struct GwfmtState *ls, const FmtColor b,
               const char *color);
bool run_config(struct GwfmtState *ls, const char *filename);
