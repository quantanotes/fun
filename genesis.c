/*
 * GENESIS OF FUN
 *
 * The purpose of this file is to bootstrap our fun compiler. It contains a
 * minimalistic implementation of fun.
 *
 * Inspirations: Chicken, Ribbit, Femtolisp, Tinylisp and many Fmore.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static jmp_buf jb;

typedef uint64_t word_t;

#define TAG_MASK 0xF

#define FALSE 0x6
#define TRUE  0x16
#define NIL   0xE
#define VOID  0x1E

#define INTEGER_TAG   0x1
#define SYMBOL_TAG    0x2
#define BOOLEAN_TAG   0x6
#define PRIMITIVE_TAG 0xA

#define MAKE_OBJECT(x)    ((word_t)(x))
#define MAKE_INTEGER(x)   ((x) << 1 | INTEGER_TAG)
#define MAKE_SYMBOL(x)    ((word_t)(x) << 4 | SYMBOL_TAG)
#define MAKE_PRIMITIVE(x) ((word_t)(x) << 4 | PRIMITIVE_TAG)
#define MAKE_BOOLEAN(x)   ((word_t)(x) << 4 | BOOLEAN_TAG)

#define AS_OBJECT(x)    ((word_t*)(x))
#define AS_INTEGER(x)   ((x) >> 1)
#define AS_SYMBOL(x)    ((char*)((x) >> 4))
#define AS_PRIMITIVE(x) ((word_t(*)(word_t))((x) >> 4))
#define AS_BOOLEAN(x)   ((x) >> 4)

#define IS_OBJECT(x)    (((x)&0x3) == 0)
#define IS_INTEGER(x)   ((x)&INTEGER_TAG)
#define IS_SYMBOL(x)    (((x)&TAG_MASK) == SYMBOL_TAG)
#define IS_PRIMITIVE(x) (((x)&TAG_MASK) == PRIMITIVE_TAG)
#define IS_BOOLEAN(x)   (((x)&TAG_MASK) == BOOLEAN_TAG)

#define PAIR_TYPE    0x1
#define CLOSURE_TYPE 0x2

#define AS_CLOSURE(x) ((closure_t*)AS_OBJECT(x))

#define IS_PAIR(x)    (!IS_OBJECT(x) ? 0 : GET_OBJECT_TYPE(x) == PAIR_TYPE)
#define IS_CLOSURE(x) (!IS_OBJECT(x) ? 0 : GET_OBJECT_TYPE(x) == CLOSURE_TYPE)

#define FLAGS_SHIFT 0x3C
#define TYPE_SHIFT  0x38
#define SIZE_SHIFT  0x18

#define MAKE_OBJECT_HEADER(flags, type, size)                            \
    (((word_t)(flags) << FLAGS_SHIFT) | ((word_t)(type) << TYPE_SHIFT) | \
     ((word_t)(size) << SIZE_SHIFT))
#define SET_OBJECT_HEADER(x, flags, type, size)                              \
    (AS_OBJECT(x)[0] = ((GET_OBJECT_HEADER(x)) & ~(0xFULL << FLAGS_SHIFT)) | \
                       MAKE_OBJECT_HEADER(flags, type, size))

#define GET_OBJECT_HEADER(x) (AS_OBJECT(x)[0])
#define GET_OBJECT_FLAGS(x)  ((GET_OBJECT_HEADER(x) >> FLAGS_SHIFT) & 0xF)
#define GET_OBJECT_TYPE(x)   ((GET_OBJECT_HEADER(x) >> TYPE_SHIFT) & 0xF)
#define GET_OBJECT_SIZE(x)   ((GET_OBJECT_HEADER(x) >> SIZE_SHIFT) & 0xFFFFFFFF)

#define SET_OBJECT_FLAGS(x, flags) \
    ((GET_OBJECT_HEADER(x)) & ((flags) << FLAGS_SHIFT))

/*
 * The first bit in an object header forwarding information. I.e. has it
 * moved from the stack to the heap during eviction or from the fromspace to the
 * tospace during garbage collection. In the heap to heap case, the last 32 bits
 * are set to the new location.
 */

#define FORWARD_MASK        0xFFFFFFFF
#define SET_FORWARD_FLAG(x) (SET_OBJECT_FLAGS(x, GET_OBJECT_FLAGS(x) | 0x8))
#define MAKE_FORWARD(x, y)  (GET_OBJECT_HEADER(x) = (y), SET_FORWARD_FLAG(x))
#define GET_FORWARD(x)      (GET_OBJECT_HEADER(x) & FORWARD_MASK)
#define IS_FORWARD(x)       (GET_OBJECT_FLAGS(x) >> 3)

#define STACK_SIZE            4096
#define HEAP_SIZE             65536
#define SYMBOL_TABLE_SIZE     4096
#define ENV_STACK_SIZE        4096
#define ENV_SYMBOL_TABLE_SIZE 512
#define BUFFER_SIZE           4096

#define CAR(x) AS_OBJECT(x)[1]
#define CDR(x) AS_OBJECT(x)[2]

#define CAAR(x)  CAR(CAR(x))
#define CADR(x)  CAR(CDR(x))
#define CDAR(x)  CDR(CAR(x))
#define CDDR(x)  CDR(CDR(x))
#define CADDR(x) CAR(CDAR(x))
#define CAADR(x) CAR(CADR(x))
#define CDADR(x) CDR(CADR(x))

#define EACH_CONS(item, list) \
    for (word_t(item) = (list); (item) != NIL; (item) = CDR(item))

#define TAKE_CONS(item) for (; (item) != NIL; (item) = CDR(item))

#define NOT(x) (IS_BOOLEAN(x) ? !AS_BOOLEAN(x) : (x) == NIL)

typedef struct {
    word_t header;
    word_t car;
    word_t cdr;
} pair_t;

typedef struct {
    word_t symbol;
    word_t value;
} binding_t;

typedef struct {
    word_t header;
    word_t args;
    word_t body;
    int    macro;
} closure_t;

typedef struct env env_t;
struct env {
    size_t     size;
    word_t*    fp;
    binding_t* bindings;
};

static word_t  stack[STACK_SIZE];
static word_t* sp = stack;

static word_t  heap[2][HEAP_SIZE];
static word_t* fromspace = heap[0];
static word_t* tospace   = heap[1];
static word_t* hp        = heap[0];

static word_t symbol_table[SYMBOL_TABLE_SIZE];

static env_t  env_stack[ENV_STACK_SIZE];
static env_t* env = env_stack;

static word_t stack_alloc(size_t size);
static word_t heap_alloc(size_t size);
static word_t any_alloc(word_t** memory, size_t size, word_t* capacity);
static word_t alloc(size_t size);

static void   gc();
static word_t forward(word_t x);
static word_t evict(word_t x);

static word_t cons(word_t car, word_t cdr);
static word_t append(word_t x, word_t y);
static word_t intern(const char* s);
static word_t closure(word_t args, word_t body, int macro);
static word_t quote(word_t x);
static void   extend();
static void   unwind();
static void   bind(word_t symbol, word_t value);
static word_t lookup(word_t symbol);

static word_t eval(word_t x);
static word_t evlis(word_t x);
static word_t apply(word_t x, word_t y);
static word_t begin(word_t x);
static word_t quasi(word_t x);
static word_t capture(word_t x);

static char  buffer[BUFFER_SIZE];
static char* bp;

static word_t parse();
static word_t parse_expr();
static word_t parse_atom();
static word_t parse_list();
static word_t parse_quote();
static word_t parse_quasi();
static word_t parse_unquote();
static word_t parse_unquote_splicing();

static void print(word_t x);
static void print_list(word_t x);

static size_t
hashtable_search(word_t* entries, int as_str, size_t size, word_t key);

static void error(const char* format, ...);
static void panic(const char* format, ...);

/* Memory management */

/*
 * An object never contains a reference with an address above it's own
 * if it lives on the stack. Using this assertion we can forward any
 * objects which break this rule to the heap (see cons should not cons
 * its arguments - H.G Baker).
 *
 * This technique is potentially less efficient than just using a regular
 * Cheney's GC (which we do employ) due to copying from the stack to the heap.
 * I have implemented it anyways as an exercise for when I implement
 * continuation passing style optimisation which this style of memory management
 * is suited for.
 */

static word_t stack_alloc(size_t size) {
    return any_alloc(&sp, size, stack + STACK_SIZE);
}

static word_t heap_alloc(size_t size) {
    if (hp + size >= fromspace + HEAP_SIZE) {
        gc();
    }

    const word_t ptr = any_alloc(&hp, size, fromspace + HEAP_SIZE);
    if (ptr == VOID) {
        panic("heap overflow");
    }

    return ptr;
}

static word_t any_alloc(word_t** memory, size_t size, word_t* capacity) {
    if (*memory + size >= capacity) {
        return VOID;
    }

    const word_t ptr = (word_t)*memory;

    *memory += size;
    return ptr;
}

static word_t alloc(size_t size) {
    word_t ptr = stack_alloc(size);
    if (ptr != VOID) {
        return ptr;
    }
    return heap_alloc(size);
}

static inline int in_stack(word_t x) {
    return AS_OBJECT(x) < sp && AS_OBJECT(x) >= stack;
}

static inline int should_evict(word_t x) {
    if (!IS_OBJECT(x)) {
        return 0;
    }
    return !IS_FORWARD(x) && in_stack(x);
}

static inline word_t check_evict(word_t x) {
    if (should_evict(x)) {
        x = evict(x);
    }
    return x;
}

/*
 * Eviction recursively moves objects in the stack, which contain references
 * below in the stack or heap, to the heap.
 */
static word_t evict(word_t x) {
    if (!should_evict(x)) {
        return x;
    }

    const word_t* ptr  = AS_OBJECT(x);
    const word_t  type = GET_OBJECT_TYPE(x);
    const word_t  size = GET_OBJECT_SIZE(x);

    switch (type) {
        case PAIR_TYPE:
            word_t* car = &CAR(x);
            word_t* cdr = &CAR(x);
            *car        = check_evict(*car);
            *cdr        = check_evict(*cdr);
            break;
        case CLOSURE_TYPE:
            closure_t* closure = (closure_t*)ptr;
            closure->args      = check_evict(closure->args);
            closure->body      = check_evict(closure->body);
            break;
    }

    word_t* new_ptr = AS_OBJECT(heap_alloc(size));
    memcpy(new_ptr, ptr, size);

    SET_FORWARD_FLAG(x);
    ptr = new_ptr;

    return MAKE_OBJECT(x);
}

static void swap_heaps() {
    word_t* tmp = fromspace;
    fromspace   = tospace;
    tospace     = tmp;
    hp          = tospace;
}

static void gc() {
    swap_heaps();
    for (env_t* e = env_stack; e <= env; e++) {
        for (size_t i = 0; i < e->size; i++) {
            word_t* ptr = &(e->bindings[i].value);

            if (!IS_OBJECT(*ptr)) {
                continue;
            }

            if (IS_FORWARD(*ptr)) {
                *ptr = GET_FORWARD(ptr);
            } else {
                const word_t new_ptr = forward(*ptr);
                *ptr                 = MAKE_FORWARD(ptr, new_ptr);
            }
        }
    }
}

static word_t forward(word_t x) {
    if (!IS_OBJECT(x)) {
        return x;
    }

    const word_t* ptr  = AS_OBJECT(x);
    const word_t  type = GET_OBJECT_TYPE(x);
    const word_t  size = GET_OBJECT_SIZE(x);

    switch (type) {
        case PAIR_TYPE:
            CAR(ptr) = forward((CAR(ptr)));
            CDR(ptr) = forward((CDR(ptr)));
        case CLOSURE_TYPE:
            word_t* args = &((closure_t*)ptr)->args;
            word_t* body = &((closure_t*)ptr)->body;
            *args        = forward(*args);
            *body        = forward(*body);
            break;
    }

    word_t new_ptr = heap_alloc(size);
    memcpy(AS_OBJECT(new_ptr), ptr, size);
    return new_ptr;
}

/* Lisp */

static word_t cons(word_t car, word_t cdr) {
    car = check_evict(car);
    cdr = check_evict(cdr);

    pair_t* pair = (pair_t*)alloc(sizeof(pair_t));
    pair->header = MAKE_OBJECT_HEADER(0, PAIR_TYPE, sizeof(pair_t));
    pair->car    = car;
    pair->cdr    = cdr;

    return MAKE_OBJECT(pair);
}

static word_t append(word_t x, word_t y) {
    if (!IS_PAIR(x)) {
        return y;
    }
    return cons(CAR(x), append(CDR(x), y));
}

/*
 * Symbol interning means we allocate each symbol once. Symbol equality
 * becomes trivial as two same symbols will share an address.
 */
static word_t intern(const char* s) {
    const size_t index = hashtable_search(
        (word_t*)symbol_table, 1, SYMBOL_TABLE_SIZE, MAKE_SYMBOL(s)
    );

    if (index == -1) {
        panic("symbol table overflow");
    }

    if (symbol_table[index] == VOID) {
        word_t symbol       = MAKE_SYMBOL(_strdup(s));
        symbol_table[index] = symbol;
    }

    return symbol_table[index];
}

static word_t closure(word_t args, word_t body, int macro) {
    args = check_evict(args);
    body = check_evict(body);

    closure_t* closure = (closure_t*)alloc(sizeof(closure_t));
    closure->header    = MAKE_OBJECT_HEADER(0, CLOSURE_TYPE, sizeof(closure_t));
    closure->args      = args;
    closure->body      = capture(body);
    closure->macro     = macro;

    return MAKE_OBJECT(closure);
}

static word_t quote(word_t x) {
    return cons(intern("quote"), cons(x, NIL));
}

static void extend() {
    if (env >= env_stack + ENV_STACK_SIZE) {
        panic("environment stack overflow");
    }

    env++;
    env->size = ENV_SYMBOL_TABLE_SIZE;
    env->fp   = sp;
    env->bindings =
        (binding_t*)malloc(sizeof(binding_t) * ENV_SYMBOL_TABLE_SIZE);

    for (size_t i = 0; i < env->size; i++) {
        env->bindings[i] = (binding_t){VOID, VOID};
    }
}

static void unwind() {
    sp = env->fp;
    free(env->bindings);
    env--;
}

static void bind(word_t symbol, word_t value) {
    const size_t index =
        hashtable_search((word_t*)env->bindings, 0, env->size, symbol);

    if (index == -1) {
        panic("environment symbol table overflow");
    }

    env->bindings[index] = (binding_t){symbol, value};
}

static word_t lookup(word_t symbol) {
    for (env_t* e = env; e >= env_stack; e--) {
        const size_t index =
            hashtable_search((word_t*)e->bindings, 0, e->size, symbol);
        const binding_t binding = e->bindings[index];

        if (binding.symbol != VOID) {
            return binding.value;
        }
    }

    return VOID;
}

/* Evaluation */

static word_t eval(word_t x) {
    if (IS_PAIR(x)) {
        return apply(eval(CAR(x)), CDR(x));
    } else if (IS_SYMBOL(x)) {
        const word_t val = lookup(x);
        if (val == VOID && x != intern("void")) {
            error("unbound symbol %s", AS_SYMBOL(x));
        }
        return val;
    } else {
        return x;
    }
}

static word_t evlis(word_t x) {
    if (IS_PAIR(x)) {
        return cons(eval(CAR(x)), evlis(CDR(x)));
    } else if (IS_SYMBOL(x)) {
        return lookup(x);
    } else {
        return NIL;
    }
}

static word_t apply(word_t x, word_t y) {
    if (IS_PRIMITIVE(x)) {
        return AS_PRIMITIVE(x)(y);
    } else if (IS_CLOSURE(x)) {
        extend();
        const closure_t* closure = AS_CLOSURE(x);
        word_t           args    = closure->args;

        TAKE_CONS(y) {
            word_t arg = closure->macro ? CAR(y) : eval(CAR(y));
            bind(CAR(args), arg);
            args = CDR(args);
        }

        const word_t result = begin(closure->body);
        unwind();
        return result;
    }

    error("apply expects closure type");
    return VOID;
}

static word_t begin(word_t x) {
    word_t y = NIL;
    TAKE_CONS(x) {
        y = eval(CAR(x));
    }
    return y;
}

static word_t quasi(word_t x) {
    if (!IS_PAIR(x)) {
        return x;
    } else if (CAR(x) == intern("unquote")) {
        return eval(CADR(x));
    } else if (CAR(x) == intern("unquote-splicing")) {
        return append(eval(CADR(x)), quasi(CDDR(x)));
    } else if (CAR(x) == intern("quasi")) {
        return quasi(CDR(x));
    } else {
        if (IS_PAIR(CAR(x)) && CAAR(x) == intern("unquote-splicing")) {
            return append(eval(CADDR(x)), quasi(CDR(x)));
        } else {
            return cons(quasi(CAR(x)), quasi(CDR(x)));
        }
    }
}

/* Substitutes free variable pointers in to closures for environment capturing.
 * The gc handles the rest.
 */
static word_t capture(word_t x) {
    if (IS_PAIR(x)) {
        return cons(capture(CAR(x)), capture(CDR(x)));
    } else if (IS_SYMBOL(x)) {
        const word_t val = lookup(x);
        if (val == VOID || IS_PRIMITIVE(val)) {
            return x;
        }
        return quote(val);
    } else {
        return x;
    }
}

/* Parsing */

#define TOKEN_SIZE 128
static char token[TOKEN_SIZE];

#define IS_EOF(c)        ((c) == '\0')
#define IS_WHITESPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\n')
#define IS_SPECIAL(c)                                                       \
    ((c) == '(' || (c) == ')' || (c) == '\'' || (c) == '`' || (c) == ',' || \
     (c) == '@')

#define SCAN_NUMBER(fmt, num, len) \
    sscanf_s(token, fmt, &(num), &(len)) > 0 && (len) == strlen(token)

#define PARSE_QUOTE(sym) \
    next_token();        \
    return cons(intern(sym), cons(parse_expr(), NIL))

static char* next_token() {
    size_t i = 0;

    while (IS_WHITESPACE(*bp)) {
        bp++;
    }

    if (IS_SPECIAL(*bp)) {
        token[i++] = *bp++;
    } else {
        do {
            token[i++] = *bp++;
        } while (!IS_WHITESPACE(*bp) && !IS_SPECIAL(*bp) && !IS_EOF(*bp));
    }

    token[i] = '\0';
    return token;
}

static word_t parse() {
    bp = buffer;
    next_token();
    return parse_expr();
}

static word_t parse_expr() {
    switch (*token) {
        case '(':
            return parse_list();
        case '\'':
            return parse_quote();
        case '`':
            return parse_quasi();
        case ',':
            return parse_unquote();
        case '@':
            return parse_unquote_splicing();
        default:
            if (!IS_SPECIAL(*token)) {
                return parse_atom();
            }
            error("unexpected token %s", token);
    }
    return VOID;
}

static word_t parse_atom() {
    int     len;
    int64_t integer;
    float   real;

    if (SCAN_NUMBER("%lld%n", integer, len)) {
        return MAKE_INTEGER(integer);
    } else {
        return intern(token);
    }
}

static word_t parse_quote() {
    PARSE_QUOTE("quote");
}

static word_t parse_quasi() {
    PARSE_QUOTE("quasi");
}

static word_t parse_unquote() {
    PARSE_QUOTE("unquote");
}

static word_t parse_unquote_splicing() {
    PARSE_QUOTE("unquote-splicing");
}

static word_t parse_list() {
    next_token();

    if (*token == 0) {
        error("parentheses mismatched");
    }

    if (*token == ')') {
        return NIL;
    }

    const word_t car = parse_expr();
    const word_t cdr = parse_list();

    return cons(car, cdr);
}

static void print(word_t x) {
    if (x == NIL) {
        printf("nil");
    } else if (x == VOID) {
        printf("void");
    } else if (IS_BOOLEAN(x)) {
        AS_BOOLEAN(x) ? printf("true") : printf("false");
    } else if (IS_INTEGER(x)) {
        printf("%lld", AS_INTEGER(x));
    } else if (IS_SYMBOL(x)) {
        printf("%s", AS_SYMBOL(x));
    } else if (IS_PRIMITIVE(x)) {
        printf("<primitive>");
    } else if (IS_PAIR(x)) {
        print_list(x);
    } else if (IS_CLOSURE(x)) {
        AS_CLOSURE(x)->macro ? printf("<macro>") : printf("<fun>");
    }
}

static void print_list(word_t x) {
    putchar('(');

    while (1) {
        print(CAR(x));
        x = CDR(x);

        if (x == NIL) {
            break;
        }

        putchar(' ');
    }

    putchar(')');
}

/* Primitives */

#define PRIMITIVE_INT_BINARY_OP(list, op)                                  \
    do {                                                                   \
        (list)      = evlis(list);                                         \
        int64_t acc = AS_INTEGER(CAR(list));                               \
        EACH_CONS(item, CDR(list)) { acc = acc op AS_INTEGER(CAR(item)); } \
        return MAKE_INTEGER(acc);                                          \
    } while (0)

#define PRIMITIVE_INT_COMP_OP(list, op)                                    \
    do {                                                                   \
        (list)      = evlis(list);                                         \
        int64_t acc = AS_INTEGER(CAR(list));                               \
        EACH_CONS(item, CDR(list)) { acc = acc op AS_INTEGER(CAR(item)); } \
        return MAKE_BOOLEAN(acc ? 1 : 0);                                  \
    } while (0)

static word_t primitive_eval(word_t x) {
    return eval(CAR(evlis(x)));
}

static word_t primitive_begin(word_t x) {
    return begin(x);
}

static word_t primitive_quote(word_t x) {
    return CAR(x);
}

static word_t primitive_quasi(word_t x) {
    return quasi(CAR(x));
}

static word_t primitive_unquote(word_t x) {
    return eval(CAR(evlis(x)));
}

static word_t primitive_cons(word_t x) {
    x = evlis(x);
    return cons(CAR(x), CADR(x));
}

static word_t primitive_car(word_t x) {
    if (!IS_PAIR(x)) {
        error("cdr expects pair type");
    }
    return CAAR(evlis(x));
}

static word_t primitive_cdr(word_t x) {
    if (!IS_PAIR(x)) {
        error("cdr expects pair type");
    }
    return CDAR(evlis(x));
}

static word_t primitive_add(word_t x) {
    PRIMITIVE_INT_BINARY_OP(x, +);
}

static word_t primitive_sub(word_t x) {
    PRIMITIVE_INT_BINARY_OP(x, -);
}

static word_t primitive_mul(word_t x) {
    PRIMITIVE_INT_BINARY_OP(x, *);
}

static word_t primitive_div(word_t x) {
    PRIMITIVE_INT_BINARY_OP(x, /);
}

static word_t primitive_eqv(word_t x) {
    PRIMITIVE_INT_COMP_OP(x, ==);
}

static word_t primitive_gt(word_t x) {
    PRIMITIVE_INT_COMP_OP(x, >);
}

static word_t primitive_lt(word_t x) {
    PRIMITIVE_INT_COMP_OP(x, <);
}

static word_t primitive_gte(word_t x) {
    PRIMITIVE_INT_COMP_OP(x, >=);
}

static word_t primitive_lte(word_t x) {
    PRIMITIVE_INT_COMP_OP(x, <=);
}

static word_t primitive_eq(word_t x) {
    return MAKE_BOOLEAN(eval(CAR(x)) == eval(CADR(x)));
}

static word_t primitive_is_nil(word_t x) {
    return MAKE_BOOLEAN(eval(CAR(x)) == NIL);
}

static word_t primitive_is_atom(word_t x) {
    x = eval(CAR(x));
    return MAKE_BOOLEAN(IS_BOOLEAN(x) || IS_INTEGER(x) || IS_SYMBOL(x));
}

static word_t primitive_is_pair(word_t x) {
    return MAKE_BOOLEAN(IS_PAIR(eval(CAR(x))));
}

static word_t primitive_is_quote(word_t x) {
    return MAKE_BOOLEAN(CAR(x) == intern("quote"));
}

static word_t primitive_is_symbol(word_t x) {
    return MAKE_BOOLEAN(IS_SYMBOL(eval(CAR(x))));
}

static word_t primitive_is_integer(word_t x) {
    return MAKE_BOOLEAN(IS_INTEGER(eval(CAR(x))));
}

static word_t primitive_is_closure(word_t x) {
    x = eval(CAR(x));
    return MAKE_BOOLEAN(IS_PRIMITIVE(x) || IS_CLOSURE(x));
}

static word_t primitive_not(word_t x) {
    return MAKE_BOOLEAN(NOT(CAR(evlis(x))));
}

static word_t primitive_or(word_t x) {
    word_t y = FALSE;
    while (x != NIL && NOT(y = eval(CAR(x)))) {
        x = CDR(x);
    }
    return y;
}

static word_t primitive_and(word_t x) {
    word_t y = FALSE;
    while (x != NIL && !NOT(y = eval(CAR(x)))) {
        x = CDR(x);
    }
    return y;
}

static word_t primitive_cond(word_t x) {
    TAKE_CONS(x) {
        const word_t clause = CAR(x);
        const word_t cond   = eval(CAR(clause));
        const word_t exp    = CADR(clause);
        if (!NOT(cond)) {
            return eval(exp);
        }
    }
    return NIL;
}

static word_t primitive_let(word_t x) {
    word_t vals = NIL;
    EACH_CONS(bindings, CAR(x)) {
        const word_t binding = CAR(bindings);
        const word_t val     = eval(CADR(binding));
        CDR(binding)         = cons(val, NIL);
    }

    extend();
    EACH_CONS(bindings, CAR(x)) {
        const word_t sym = CAAR(bindings);
        const word_t val = CADDR(bindings);
        bind(sym, val);
    }

    word_t y = begin(CDR(x));
    unwind();
    return y;
}

static word_t primitive_lets(word_t x) {
    extend();
    EACH_CONS(bindings, CAR(x)) {
        const word_t sym = CAAR(bindings);
        const word_t val = CADDR(bindings);
        bind(sym, eval(val));
    }
    word_t y = begin(CDR(x));
    unwind();
    return y;
}

static word_t primitive_fun(word_t x) {
    return closure(CAR(x), CDR(x), 0);
}

static word_t primitive_macro(word_t x) {
    return closure(CAR(x), CDR(x), 1);
}

static word_t primitive_def(word_t x) {
    bind(CAR(x), eval(CADR(x)));
    return CAR(x);
}

static word_t primitive_defun(word_t x) {
    const word_t sym  = CAR(x);
    const word_t args = CADR(x);
    const word_t body = CDDR(x);
    const word_t fun  = closure(args, body, 0);
    bind(sym, fun);
    return fun;
}

static word_t primitive_defmacro(word_t x) {
    const word_t sym  = CAR(x);
    const word_t args = CADR(x);
    const word_t body = CDDR(x);
    const word_t fun  = closure(args, body, 1);
    bind(sym, fun);
    return fun;
}

static word_t primitive_list(word_t x) {
    return evlis(x);
}

static word_t primitive_append(word_t x) {
    word_t y    = NIL;
    word_t tail = NIL;

    TAKE_CONS(x) {
        word_t sublist = eval(CAR(x));
        TAKE_CONS(sublist) {
            if (y == NIL) {
                y    = cons(CAR(sublist), NIL);
                tail = y;
            } else {
                CDR(tail) = cons(CAR(sublist), NIL);
                tail      = CDR(tail);
            }
        }
    }

    return y;
}

static word_t primitive_length(word_t x) {
    x          = eval(CAR(x));
    uint64_t y = 0;
    TAKE_CONS(x) {
        y++;
    }
    return MAKE_INTEGER(y);
}

static word_t primitive_println(word_t x) {
    print(evlis(x));
    putchar('\n');
    return VOID;
}

int           gensym_counter = 0;
static word_t primitive_gensym(word_t x) {
    char sym[20];
    snprintf(sym, sizeof(sym), "$%d", gensym_counter++);
    return intern(sym);
}

#define PRIM(name)              #name, primitive_##name
#define PRIM_ALIAS(alias, name) #alias, primitive_##name

static const struct {
    const char* s;
    word_t (*f)(word_t);
} primitives[] = {
    PRIM(eval),
    PRIM(begin),
    PRIM(quote),
    PRIM(quasi),
    PRIM(unquote),
    PRIM(cons),
    PRIM(car),
    PRIM(cdr),
    PRIM_ALIAS(+, add),
    PRIM_ALIAS(-, sub),
    PRIM_ALIAS(*, mul),
    PRIM_ALIAS(/, div),
    PRIM_ALIAS(=, eqv),
    PRIM_ALIAS(>, gt),
    PRIM_ALIAS(<, lt),
    PRIM_ALIAS(>=, gte),
    PRIM_ALIAS(<=, lte),
    PRIM(eq),
    PRIM_ALIAS(nil?, is_nil),
    PRIM_ALIAS(atom?, is_atom),
    PRIM_ALIAS(pair?, is_pair),
    PRIM_ALIAS(quote?, is_quote),
    PRIM_ALIAS(symbol?, is_symbol),
    PRIM_ALIAS(integer?, is_integer),
    PRIM_ALIAS(closure?, is_closure),
    PRIM(not ),
    PRIM(or),
    PRIM(and),
    PRIM(cond),
    PRIM(let),
    PRIM_ALIAS(let*, lets),
    PRIM(fun),
    PRIM(macro),
    PRIM(def),
    PRIM(defun),
    PRIM(defmacro),
    PRIM(list),
    PRIM(append),
    PRIM(length),
    PRIM(println),
    PRIM(gensym),
    {0},
};

/* Hashtable */

static uint64_t hash_uint(uint64_t key) {
    return key ^ (key >> 16);
}

static uint64_t hash_str(const char* key) {
    int          c;
    unsigned int hash = 5381;

    while ((c = (unsigned char)*key++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

/*
 * This function has been heuristcally generalised for the two hashtable use
 * cases: symbol interning and env symbol look up. The as_str boolean when true
 * searches the hashtable as if it were a list of symbol pointers (for the
 * symbol table). When as_str is false the hashtable is searched as if it were
 * integer key value pairs (for environment lookup).
 */
static size_t
hashtable_search(word_t* entries, int as_str, size_t size, word_t key) {
    uint64_t h     = as_str ? hash_str((char*)AS_SYMBOL(key)) : hash_uint(key);
    uint64_t index = h % size;
    size_t   count = 0;

    while (count < size) {
        const word_t entry_key =
            as_str ? (word_t)entries[index] : entries[index * 2];

        if (entry_key == VOID) {
            return index;
        }

        const int found_str =
            as_str ? strcmp(AS_SYMBOL(entry_key), AS_SYMBOL(key)) == 0 : 0;

        if (entry_key == key || found_str) {
            return index;
        }

        index = (index + 1) % size;
        count++;
    }

    return -1;
}

/* Main */

static void init_symbol_table() {
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        symbol_table[i] = VOID;
    }
}

static void init_env() {
    env--;
    extend();
}

static void init_primitives() {
    for (size_t i = 0; primitives[i].s; ++i) {
        bind(intern(primitives[i].s), MAKE_PRIMITIVE(primitives[i].f));
    }
    bind(intern("true"), TRUE);
    bind(intern("false"), FALSE);
    bind(intern("nil"), NIL);
    bind(intern("void"), VOID);
}

static void open(const char* path) {
    FILE* file;
    fopen_s(&file, path, "r");

    if (file == NULL) {
        panic("io error");
    }

    fseek(file, 0L, SEEK_END);
    const long size = ftell(file);
    rewind(file);

    fread(buffer, 1, size, file);
    fclose(file);
    buffer[size] = '\0';

    print(eval(parse()));
}

static void init() {
    init_symbol_table();
    init_env();
    init_primitives();
    printf("have fun!\n");
}

static void repl() {
    while (1) {
        printf("> ");
        fgets(buffer, BUFFER_SIZE, stdin);

        if (strcmp(buffer, "quit\n") == 0) {
            break;
        }

        print(eval(parse()));
        putchar('\n');
    }

    printf("goodbye!\n");
}

int genesis_entry(const char* path) {
    if (!setjmp(jb)) {
        init();
    }
    if (path == NULL) {
        repl();
    }
    if (!setjmp(jb)) {
        open(path);
    }
    return 0;
}

static void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    putchar('\n');
    va_end(args);
}

/* Error handling */

static void error(const char* format, ...) {
    log_error(format);
    longjmp(jb, 1);
}

static void panic(const char* format, ...) {
    log_error(format);
    exit(EXIT_FAILURE);
}