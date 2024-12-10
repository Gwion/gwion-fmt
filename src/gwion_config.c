#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "arg.h"
#include "operator.h"
#include "instr.h"
#include "object.h"
#include "import.h"
#include "gwi.h"
#include "compile.h"

#define runtime_bool_func(name, value)                    \
static MFUN(fmt_##name) {                                 \
  struct GwfmtState *ls = **(struct GwfmtState***)MEM(0); \
  ls->value = *(bool*)MEM(SZ_INT);                        \
}
runtime_bool_func(pythonize, py);
runtime_bool_func(unpythonize, unpy);
runtime_bool_func(only_pythonize, onlypy);
runtime_bool_func(minimize, minimize);
runtime_bool_func(pretty, pretty);
runtime_bool_func(show_lines, show_line);
runtime_bool_func(header, header);
runtime_bool_func(use_tabs, use_tabs);
runtime_bool_func(fix_case, fix_case);
runtime_bool_func(check_case, check_case);

static MFUN(fmt_color) {
  struct GwfmtState *ls = **(struct GwfmtState***)MEM(0);
  ls->color = *(m_uint*)MEM(SZ_INT);
}

static MFUN(fmt_setcolor) {
  struct GwfmtState *ls = **(struct GwfmtState***)MEM(0);
  const FmtColor kind = (FmtColor)*(m_uint*)MEM(SZ_INT);
  const M_Object color = *(M_Object*)MEM(SZ_INT*2);
  set_color(ls, kind, STRING(color));
}


#define runtime_case_func(name, value)                             \
static MFUN(fmt_##name##_case) {                                   \
  struct GwfmtState *ls = **(struct GwfmtState***)MEM(0);          \
  ls->config->cases[value##Case] = *get_casing(*(CasingType*)MEM(SZ_INT)); \
}
runtime_case_func(type, Type);
runtime_case_func(function, Function);
runtime_case_func(variable, Variable);

GWION_IMPORT(GwFmt) {
  GWI_B(gwi_enum_ini(gwi, "ColorKind"));
    GWI_B(gwi_enum_add(gwi, "StringColor",     StringColor));
    GWI_B(gwi_enum_add(gwi, "KeywordColor",    KeywordColor));
    GWI_B(gwi_enum_add(gwi, "FlowColor",       FlowColor));
    GWI_B(gwi_enum_add(gwi, "FunctionColor",   FunctionColor));
    GWI_B(gwi_enum_add(gwi, "TypeColor",        TypeColor));
    GWI_B(gwi_enum_add(gwi, "VariableColor",    VariableColor));
    GWI_B(gwi_enum_add(gwi, "NumberColor",      NumberColor));
    GWI_B(gwi_enum_add(gwi, "OpColor",          OpColor));
    GWI_B(gwi_enum_add(gwi, "ModColor",         ModColor));
    GWI_B(gwi_enum_add(gwi, "PunctuationColor", PunctuationColor));
    GWI_B(gwi_enum_add(gwi, "PPColor",          PPColor));
    GWI_B(gwi_enum_add(gwi, "SpecialColor",     SpecialColor));
  GWI_B(gwi_enum_end(gwi));
 
  GWI_B(gwi_enum_ini(gwi, "ColorWhen"));
    GWI_B(gwi_enum_add(gwi, "Always", COLOR_ALWAYS)); 
    GWI_B(gwi_enum_add(gwi, "Auto",   COLOR_AUTO)); 
    GWI_B(gwi_enum_add(gwi, "Never",  COLOR_NEVER)); 
  GWI_B(gwi_enum_end(gwi));
 
  GWI_B(gwi_enum_ini(gwi, "Case"));
    GWI_B(gwi_enum_add(gwi, "PascalCase", PASCALCASE)); 
    GWI_B(gwi_enum_add(gwi, "camelCase",  CAMELCASE)); 
    GWI_B(gwi_enum_add(gwi, "UPPER_CASE", UPPERCASE)); 
    GWI_B(gwi_enum_add(gwi, "snake_case", SNAKECASE)); 
  GWI_B(gwi_enum_end(gwi));
  
  GWI_B(gwi_struct_ini(gwi, "GwFmt"));
  gwi->gwion->env->class_def->size = SZ_INT;

#define import_bool_fun(name)                         \
  GWI_B(gwi_func_ini(gwi, "void", #name));            \
    GWI_B(gwi_func_arg(gwi, "bool", "arg"));          \
  GWI_B(gwi_func_end(gwi, fmt_##name, ae_flag_none)); \

 import_bool_fun(pythonize); 
 import_bool_fun(unpythonize); 
 import_bool_fun(only_pythonize); 
 import_bool_fun(minimize); 
 import_bool_fun(pretty); 
 import_bool_fun(header); 
 import_bool_fun(show_lines); 
 import_bool_fun(use_tabs); 
 import_bool_fun(fix_case); 
 import_bool_fun(check_case); 

  GWI_B(gwi_func_ini(gwi, "void", "color"));
    GWI_B(gwi_func_arg(gwi, "ColorWhen", "arg"));
  GWI_B(gwi_func_end(gwi, fmt_color, ae_flag_none));


  GWI_B(gwi_func_ini(gwi, "void", "color"));
    GWI_B(gwi_func_arg(gwi, "ColorKind", "arg"));
  GWI_B(gwi_func_end(gwi, fmt_setcolor, ae_flag_none));


  #define import_case_fun(name)                             \
  GWI_B(gwi_func_ini(gwi, "void", #name "_case"));          \
    GWI_B(gwi_func_arg(gwi, "Case", "arg"));                \
  GWI_B(gwi_func_end(gwi, fmt_##name##_case, ae_flag_none));

  import_case_fun(type);
  import_case_fun(function);
  import_case_fun(variable);

  GWI_B(gwi_struct_end(gwi));

  GWI_B(gwi_item_ini(gwi, "GwFmt", "gwfmt"));
  GWI_B(gwi_item_end(gwi, ae_flag_const, obj, NULL));
  return true;
}

static bool init_gwion(Gwion gwion) {
  char *argv = (m_str)"gwfmt";
  CliArg arg = {
    .arg = { .argv = &argv, .argc = 1 },
    .thread_count = 1,
    .queue_size = 1,
  };
  const bool ret = gwion_ini(gwion, &arg);
  arg_release(&arg);
  return ret && gwi_run(gwion, gwimport_GwFmt);
}

static bool run(Gwion gwion, const char *filename) {
  const bool ret = compile_filename(gwion, filename, NULL);
  if(ret) 
    vm_force_run(gwion->vm);
  return ret;
}

bool run_config(struct GwfmtState *ls, const char *filename) {
  struct Gwion_ gwion;
  if(!init_gwion(&gwion))
    return false;
  Value value = nspc_lookup_value1(gwion.env->curr, insert_symbol(gwion.st, "gwfmt"));
  value->d.ptr = (m_uint*)ls;
  const bool ret = run(&gwion, filename);
  gwion_end(&gwion);
  return ret;
}
