#include "ctype.h"
#include "casing.h"

bool is_camel_case(const char *s) {
  if(strchr(s, '_'))
    return false;
  if(isupper(*s))
    return false;
  return true;
}

bool is_pascal_case(const char *s) {
  if(strchr(s, '_'))
    return false;
  if(islower(*s))
    return false;
  return true;
}

bool is_upper_case(const char *s) {
  for(size_t i = 0; i < strlen(s); i++) {
    if(!isupper(s[i]) && s[i] != '_' && !isdigit(s[i]))
      return false;
  }
  return true;
}

bool is_snake_case(const char *s) {
  for(size_t i = 0; i < strlen(s); i++) {
    if(isupper(s[i]) && s[i] != '_' && !isdigit(s[i]))
      return false;
  }
  return true;
}


#define SET(buf, j, value, len) \
do {                            \
  if(j++ >= len) return false;  \
  buf[j -1] = value;            \
} while(0)

bool fix_camel_case(const char *s, char *buf, size_t len) {
  bool needsCap = false;
  size_t j = 0;
  SET(buf, j, tolower(s[0]), len);
  for(size_t i = j; i < strlen(s); i++) {
    if(s[i] == '_') {
      needsCap = true;
      continue;
    } else {
      if(needsCap) {
        SET(buf, j, toupper(s[i]), len);
        needsCap = false;
        continue;
      }
      SET(buf, j, s[i], len);
    }
  }
  SET(buf, j, '\0', len);
  return true;
}

bool fix_pascal_case(const char *s, char *buf, size_t len) {
  bool needsCap = true;
  size_t j = 0;
  for(size_t i = 0; i < strlen(s); i++) {
    if(s[i] == '_') {
      needsCap = true;
      continue;
    } else {
      if(needsCap) {
        SET(buf, j, toupper(s[i]), len);
        needsCap = false;
        continue;
      }
      SET(buf, j, s[i], len);
    }
  }
  SET(buf, j, '\0', len);
  return true;
}

bool fix_upper_case(const char *s, char *buf, size_t len) {
  bool had_lower = false;
  size_t j = 0;
  for(size_t i = 0; i < strlen(s); i++) {
    if(!isupper(s[i]) && s[i] != '_' && !isdigit(s[i])) {
      SET(buf, j, toupper(s[i]), len);
      had_lower = true;
    } else {
      if(had_lower){
        SET(buf, j, '_', len);
        had_lower = false;
      }
      SET(buf, j, s[i], len);
    }
  }
  SET(buf, j, '\0', len);
  return true;
}

bool fix_snake_case(const char *s,  char *buf, size_t len) {
  bool had_lower = true;
  size_t j = 0;
  for(size_t i = 0; i < strlen(s); i++) {
    if(isupper(s[i])) {
      if(had_lower && j) {
        SET(buf, j, '_', len);
      }
      had_lower = false;
      SET(buf, j, tolower(s[i]), len);
    } else {
      SET(buf, j, s[i], len);
      had_lower = true;
    }
  }
  SET(buf, j, '\0', len);
  return true;
}


static const Casing camelCase = {
  .check = is_camel_case,
  .fix = fix_camel_case,
  .name = "camelCase"
};

static const Casing PascalCase = {
  .check = is_pascal_case,
  .fix = fix_pascal_case,
  .name = "PascalCase"
};

static const Casing UPPER_CASE = {
  .check = is_upper_case,
  .fix = fix_upper_case,
  .name = "UPPER_CASE"
};

static const Casing snake_case = {
  .check = is_snake_case,
  .fix = fix_snake_case,
  .name = "snake_case"
};

static const Casing *casings[4] = {
  &camelCase,
  &PascalCase,
  &UPPER_CASE,
  &snake_case
};

const Casing *get_casing(const CasingType t) {
  return casings[t];
}
