#pragma once

#include <stdbool.h>
#include <string.h>

typedef bool  (*casing_check_f)(const char *);
typedef bool (*casing_fix_f)(const char *s, char *, size_t);

typedef struct Casing {
  casing_check_f check;
  casing_fix_f   fix;
  const char    *name;
} Casing;

typedef enum CasingType {
  CAMELCASE,
  PASCALCASE,
  UPPERCASE,
  SNAKECASE
} CasingType;

const Casing *get_casing(const CasingType);
