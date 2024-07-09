#ifndef FUN_PRIM_H
#define FUN_PRIM_H

#include "any.h"
#include <stdarg.h>
#include <stdlib.h>

void fun_prim_add(fun_any_t a, fun_any_t b, fun_any_t k) {
  fun_as_proc(k)(fun_mk_int(fun_as_int(a) + fun_as_int(b)));
}

#endif // FUN_PRIM_H