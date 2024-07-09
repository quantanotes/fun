#ifndef FUN_ANY_H
#define FUN_ANY_H

#include <limits.h>
#include <stdint.h>

#define FUN_WORD_SIZE sizeof(fun_word_t)

typedef uintptr_t fun_word_t;

typedef enum {
  FUN_TAG_PTR,
  FUN_TAG_INT,
} fun_tag_t;

typedef struct {
  fun_word_t ptr : (FUN_WORD_SIZE* CHAR_BIT - 3);
  fun_tag_t tag : 3;
} fun_any_t;

typedef struct {
  fun_word_t mark : 1;
  fun_word_t size : 32;
  fun_tag_t tag : 4;
} fun_header_t;

typedef struct {
  fun_header_t header;
  fun_word_t data[];
} fun_obj_t;

typedef void (*fun_proc_t)(fun_any_t, ...);

static inline fun_any_t fun_mk_any(fun_tag_t t, fun_word_t x) {
  fun_any_t any;
  any.ptr = x;
  any.tag = t;
  return any;
}

static inline fun_any_t fun_mk_obj(void* x) {
  fun_any_t any;
  any.ptr = (fun_word_t)x;
  any.tag = FUN_TAG_PTR;
  return any;
}

static inline fun_any_t fun_mk_proc(fun_proc_t x) {
  return fun_mk_obj((void*)x);
}

static inline fun_any_t fun_mk_int(int x) {
  fun_any_t any;
  any.ptr = (fun_word_t)(x << 3);
  any.tag = FUN_TAG_INT;
  return any;
}

static inline fun_obj_t* fun_as_obj(fun_any_t any) {
  return (fun_obj_t*)any.ptr;
}

static inline fun_proc_t fun_as_proc(fun_any_t any) {
  return (fun_proc_t)any.ptr;
}

static inline int fun_as_int(fun_any_t any) {
  return (int)(any.ptr >> 3);
}

#endif // FUN_ANY_H