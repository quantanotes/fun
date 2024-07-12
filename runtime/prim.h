#ifndef FUN_PRIM_H
#define FUN_PRIM_H

#include "any.h"
#include <stdarg.h>
#include <stdlib.h>

static inline void fun_prim_add(fun_any_t a, fun_any_t b, fun_any_t k) {
  fun_as_proc(k)(fun_mk_int(fun_as_int(a) + fun_as_int(b)));
}

static inline void fun_prim_sub(fun_any_t a, fun_any_t b, fun_any_t k) {
  fun_as_proc(k)(fun_mk_int(fun_as_int(a) - fun_as_int(b)));
}

static inline void fun_prim_mul(fun_any_t a, fun_any_t b, fun_any_t k) {
  fun_as_proc(k)(fun_mk_int(fun_as_int(a) * fun_as_int(b)));
}

static inline void fun_prim_div(fun_any_t a, fun_any_t b, fun_any_t k) {
  fun_as_proc(k)(fun_mk_int(fun_as_int(a) / fun_as_int(b)));
}

static inline void fun_prim_eq(fun_any_t a, fun_any_t b, fun_any_t k) {
  fun_as_proc(k)(fun_mk_bool(fun_as_word(a) == fun_as_word(b)));
}

#endif // FUN_PRIM_H