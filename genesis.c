/* GENESIS OF FUN
 *
 * The purpose of this file is to bootstrap our fun compiler. It contains a
 * minimalistic implementation of fun.
 *
 * Inspirations: Chicken, Ribbit, Femtolisp, Minilisp, Tinylisp and many more.
 */

#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DATA REPRESENTATION
 *
 * Immediate values are represented using tagged pointers. The last 4 bits
 * indicate an object's type. Values which contain references to other objects
 * such as pairs are represented with a block of data containing a header
 * with type, gc and size information.
 */

typedef uint64_t word_t;

#define WORD_SIZE          8
#define BYTE_TO_WORD(size) (((size) + WORD_SIZE - 1) / WORD_SIZE * WORD_SIZE)

/* Special constants */

#define FALSE 0x6
#define TRUE  0x16
#define NIL   0xE
#define VOID  0x1E

/* Value tags */

#define TAG_MASK 0xF

#define INTEGER_TAG   0x1
#define SYMBOL_TAG    0x2
#define BOOLEAN_TAG   0x6
#define PRIMITIVE_TAG 0xA

/* Object tags */

#define PAIR_TYPE    0x1
#define VECTOR_TYPE  0x2
#define STRING_TYPE  0x3
#define ENV_TYPE     0x4
#define CLOSURE_TYPE 0x5

/* Object sizes */

#define PAIR_SIZE    2
#define ENV_SIZE     2
#define CLOSURE_SIZE 4

/* Cast to word */

#define MAKE_OBJECT(x)    ((word_t)(x))
#define MAKE_INTEGER(x)   ((x) << 1 | INTEGER_TAG)
#define MAKE_SYMBOL(x)    ((word_t)(x) << 4 | SYMBOL_TAG)
#define MAKE_BOOLEAN(x)   ((word_t)(x) << 4 | BOOLEAN_TAG)
#define MAKE_PRIMITIVE(x) ((word_t)(x) << 4 | PRIMITIVE_TAG)

/* Cast to type */

#define AS_OBJECT(x)    ((object_t*)(x))
#define AS_INTEGER(x)   ((x) >> 1)
#define AS_BOOLEAN(x)   ((x) >> 4)
#define AS_SYMBOL(x)    ((char*)((x) >> 4))
#define AS_PRIMITIVE(x) ((primitive_t*)((x) >> 4))

#define AS_PAIR(x)    ((pair_t*)AS_OBJECT(x)->data)
#define AS_VECTOR(x)  ((vector_t*)AS_OBJECT(x)->data)
#define AS_STRING(x)  ((string_t*)AS_OBJECT(x)->data)
#define AS_ENV(x)     ((env_t*)AS_OBJECT(x)->data)
#define AS_CLOSURE(x) ((closure_t*)AS_OBJECT(x)->data)

/* Type checks */

#define IS_OBJECT(x)    (((x)&0x3) == 0)
#define IS_INTEGER(x)   ((x)&INTEGER_TAG)
#define IS_BOOLEAN(x)   (((x)&TAG_MASK) == BOOLEAN_TAG)
#define IS_SYMBOL(x)    (((x)&TAG_MASK) == SYMBOL_TAG)
#define IS_PRIMITIVE(x) (((x)&TAG_MASK) == PRIMITIVE_TAG)

#define IS_TYPE(x, t) (!IS_OBJECT(x) ? 0 : GET_OBJECT_TYPE(x) == (t))
#define IS_PAIR(x)    IS_TYPE(x, PAIR_TYPE)
#define IS_VECTOR(x)  IS_TYPE(x, VECTOR_TYPE)
#define IS_STRING(x)  IS_TYPE(x, STRING_TYPE)
#define IS_ENV(x)     IS_TYPE(x, ENV_TYPE)
#define IS_CLOSURE(x) IS_TYPE(x, CLOSURE_TYPE)

/* Object header */

#define FLAGS_SHIFT 0x3C
#define TYPE_SHIFT  0x38
#define SIZE_SHIFT  0x18

#define MAKE_HEADER(flags, type, size)                                   \
    (((word_t)(flags) << FLAGS_SHIFT) | ((word_t)(type) << TYPE_SHIFT) | \
     ((word_t)(size) << SIZE_SHIFT))

#define SET_HEADER(x, flags, type, size)                            \
    (GET_HEADER(x) = ((GET_HEADER(x)) & ~(0xFULL << FLAGS_SHIFT)) | \
                     MAKE_HEADER(flags, type, size))

#define GET_HEADER(x)       (AS_OBJECT(x)->header)
#define GET_OBJECT_FLAGS(x) ((GET_HEADER(x) >> FLAGS_SHIFT) & 0xF)
#define GET_OBJECT_TYPE(x)  ((GET_HEADER(x) >> TYPE_SHIFT) & 0xF)
#define GET_OBJECT_SIZE(x)  ((GET_HEADER(x) >> SIZE_SHIFT) & 0xFFFFFFFF)

// TODO

/* FORWARD DECLARATIONS */

#define STACK_SIZE    4096
#define HEAP_SIZE     (65536 * 4)
#define ROOTS_SIZE    4096
#define SYMBOLS_SIZE  4096
#define BINDINGS_SIZE 512
#define BUFFER_SIZE   4096
#define TOKEN_SIZE    512

#define IS_ATOM(x) !IS_PAIR(x)
#define EQ(x, y)   ((x) == (y))
#define NOT(x)     (IS_BOOLEAN(x) ? !AS_BOOLEAN(x) : (x) == NIL)

#define CAR(x) AS_PAIR(x)->car
#define CDR(x) AS_PAIR(x)->cdr

#define CAAR(x)    CAR(CAR(x))
#define CADR(x)    CAR(CDR(x))
#define CDAR(x)    CDR(CAR(x))
#define CDDR(x)    CDR(CDR(x))
#define CADDR(x)   CAR(CDDR(x))
#define CAADR(x)   CAR(CADR(x))
#define CDADR(x)   CDR(CADR(x))
#define CADAR(x)   CAR(CDAR(x))
#define CDDDR(x)   CDR(CDDR(x))
#define CAAAR(x)   CAR(CAAR(x))
#define CDAAAR(x)  CDR(CAAAR(x))
#define CAADAR(x)  CAR(CADAR(x))
#define CDADAR(x)  CDR(CADAR(x))
#define CADADAR(x) CAR(CDADAR(x))

#define EACH_CONS(x, xs) for (word_t x = xs; (x) != NIL; (x) = CDR(x))
#define TAKE_CONS(x)     for (; (x) != NIL; (x) = CDR(x))
#define ZIP_EACH_CONS(x, y, xs, ys)                              \
    for (word_t(x) = (xs), (y) = (ys); (x) != NIL && (y) != NIL; \
         (x) = CDR(x), (y) = CDR(y))
#define ZIP_TAKE_CONS(x, y) \
    for (; (x) != NIL && (y) != NIL; (x) = CDR(x), (y) = CDR(y))

#define TAKE_CONS_UNTIL_ONE(x) for (; (x) != NIL && CDR(x) != NIL; (x) = CDR(x))

#define IS_EOF(c)        ((c) == '\0')
#define IS_WHITESPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\n')
#define IS_SPECIAL(c)                                                       \
    ((c) == '(' || (c) == ')' || (c) == '\'' || (c) == '`' || (c) == ',' || \
     (c) == '@')

#define SCAN_NUMBER(fmt, num, len) \
    sscanf_s(token, fmt, &(num), &(len)) > 0 && (len) == strlen(token)

#define PRIM(name, tailcall, macro)                                   \
#name, (primitive_t) {                                            \
        primitive_##name, MAKE_BOOLEAN(tailcall), MAKE_BOOLEAN(macro) \
    }

#define PRIMA(alias, name, tailcall, macro)                           \
#alias, (primitive_t) {                                           \
        primitive_##name, MAKE_BOOLEAN(tailcall), MAKE_BOOLEAN(macro) \
    }

#define PRIMITIVE_INTEGER_BINARY_OP(xs, op)                    \
    do {                                                       \
        int64_t y = AS_INTEGER(CAR(xs));                       \
        EACH_CONS(z, CDR(xs)) { y = y op AS_INTEGER(CAR(z)); } \
        return MAKE_INTEGER(y);                                \
    } while (0)

#define PRIMITIVE_INTEGER_COMP_OP(xs, op)                      \
    do {                                                       \
        int64_t y = AS_INTEGER(CAR(xs));                       \
        EACH_CONS(z, CDR(xs)) { y = y op AS_INTEGER(CAR(z)); } \
        return MAKE_BOOLEAN(y ? 1 : 0);                        \
    } while (0)

typedef struct {
    word_t* sp;
    word_t* rp;
    word_t  env;
} state_t;

typedef struct {
    word_t header;
    word_t data[];
} object_t;

typedef struct {
    word_t car;
    word_t cdr;
} pair_t;

typedef word_t vector_t[];

typedef word_t string_t[];

typedef struct {
    word_t parent;
    word_t bindings;
} env_t;

typedef struct {
    word_t env;
    word_t args;
    word_t body;
    word_t macro;
} closure_t;

typedef struct {
    word_t (*function)(word_t);
    word_t tailcall;
    word_t macro;
} primitive_t;

typedef enum {
    MOVE_EVICT,
    MOVE_FORWARD,
} move_t;

typedef enum {
    HASH_UINT,
    HASH_STRING,
} hash_t;

static word_t  stack[STACK_SIZE];
static word_t* sp = stack;

static word_t  heap[2][HEAP_SIZE];
static word_t* fromspace = heap[0];
static word_t* tospace   = heap[1];
static word_t* hp        = heap[0];

static word_t  roots[ROOTS_SIZE];
static word_t* rp = roots;

static state_t base;
static word_t  symbols;
static word_t  env;

static char  buffer[BUFFER_SIZE];
static char* bp;

static char token[TOKEN_SIZE];

/* Memory */

static word_t alloc(size_t size);
static void   gc();
static void   add_root(word_t root);

static word_t stack_alloc(size_t size);
static word_t heap_alloc(size_t size);
static word_t any_alloc(word_t** memory, size_t size, word_t* capacity);

static word_t evict(word_t x);
static word_t forward(word_t x);
static word_t move(word_t x, move_t mt);

static inline int    in_stack(word_t x);
static inline int    should_evict(word_t x);
static inline void   swap_heaps();
static inline size_t heap_size();

/* Constructors */

static object_t* new_object(word_t type, word_t size);
static word_t    cons(word_t car, word_t cdr);
static word_t    make_vector(size_t size, const word_t* data);
static word_t    make_string(const char* data);
static word_t    make_env(word_t parent);
static word_t    make_closure(word_t args, word_t body, int macro);

/* Symbols */

static word_t intern(const char* data);

/* List */

static word_t append(word_t head, word_t tail);

/* Environment */

static void    add_env(word_t x);
static void    extend();
static state_t save();
static void    unwind(state_t state);
static word_t  lookup(word_t var);
static void    bind(word_t var, word_t val);
static void    bind_args(word_t argvar, word_t argval);

/* Evaluation */

static word_t eval(word_t x);
static word_t evlis(word_t x);
static word_t evatom(word_t x);
static word_t evsym(word_t x);
static word_t quasi(word_t x);
static word_t begin(word_t x);

/* Parsing */

static word_t parse();
static word_t parse_expr();
static word_t parse_atom();
static word_t parse_list();
static word_t parse_quote(const char* type);

/* Printing */

static void print(word_t x);
static void print_vector(word_t x);
static void print_list(word_t x);
static void println(word_t x);

/* Hashtable */

static void init_hashtable(word_t* entries, size_t size);
static size_t
hashtable_search(const word_t* entries, size_t size, word_t key, hash_t ht);

/* Error handling */

static jmp_buf jb;
static void    error(const char* format, ...);
static void    panic(const char* format, ...);

/* MEMORY */

static word_t alloc(size_t size) {
    const word_t ptr = stack_alloc(size);

    if (ptr != VOID) {
        return ptr;
    }

    return heap_alloc(size);
}

static void gc() {
    // swap_heaps();

    // for (word_t* root = rp; root >= roots; root--) {
    //     move(*root, MOVE_FORWARD);
    // }
}

static void add_root(word_t root) {
    *rp++ = root;
}

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

static word_t forward(word_t x) {
    return move(x, MOVE_FORWARD);
}

static word_t evict(word_t x) {
    return move(x, MOVE_EVICT);
}

/* Moves an object to the heap. There are two types of moves, a forward and an
 * evict. A forward is for when the GC wants to copy to the new heap. An evict
 * is to move objects with hanging references from the stack to the heap.
 */
static word_t move(word_t x, move_t mt) {
    return x;
}

static inline void swap_heaps() {
    word_t* tmp = fromspace;
    fromspace   = tospace;
    tospace     = tmp;
    hp          = fromspace;
}

static inline size_t heap_size() {
    return (size_t)(hp - fromspace) * sizeof(word_t);
}

/* CONSTRUCTORS */

static object_t* new_object(word_t type, word_t size) {
    object_t* obj = (object_t*)alloc(size + 1);

    obj->header = MAKE_HEADER(0, type, size);

    return obj;
}

static word_t cons(word_t car, word_t cdr) {
    object_t* obj  = new_object(PAIR_TYPE, PAIR_SIZE);
    pair_t*   pair = AS_PAIR(obj);

    pair->car = evict(car);
    pair->cdr = evict(cdr);

    return MAKE_OBJECT(obj);
}

static word_t make_vector(size_t size, const word_t* data) {
    object_t* obj    = new_object(VECTOR_TYPE, size);
    vector_t* vector = AS_VECTOR(obj);

    if (data != NULL) {
        for (size_t i = 0; i < size; i++) {
            (*vector)[i] = evict(data[i]);
        }
    }

    return MAKE_OBJECT(obj);
}

static word_t make_string(const char* data) {
    const size_t size = strlen(data) + 1;

    object_t* obj = new_object(STRING_TYPE, BYTE_TO_WORD(size));

    strcpy_s((char*)obj->data, size, data);

    return MAKE_OBJECT(obj);
}

static word_t make_env(word_t parent) {
    object_t* obj = new_object(ENV_TYPE, ENV_SIZE);
    env_t*    env = (env_t*)obj->data;

    env->parent   = evict(parent);
    env->bindings = make_vector(BINDINGS_SIZE, 0);

    init_hashtable(*AS_VECTOR(env->bindings), BINDINGS_SIZE);

    return MAKE_OBJECT(obj);
}

static word_t make_closure(word_t args, word_t body, int macro) {
    object_t*  obj     = new_object(CLOSURE_TYPE, CLOSURE_SIZE);
    closure_t* closure = (closure_t*)obj->data;

    closure->env   = evict(env);
    closure->args  = evict(args);
    closure->body  = evict(body);
    closure->macro = MAKE_BOOLEAN(macro);

    return MAKE_OBJECT(obj);
}

/* SYMBOLS */

/* Debug key */
word_t* sa;

/* Interning ensures each symbol is allocated once. */
static word_t intern(const char* data) {
    vector_t* vector = AS_VECTOR(symbols);

    const size_t size = SYMBOLS_SIZE;
    const size_t index =
        hashtable_search(*vector, size, (word_t)data, HASH_STRING);

    if (index == -1) {
        panic("symbol table overflow");
    }

    word_t* symbol = &CAR((*vector)[index]);

    if (*symbol == VOID) {
        *symbol = make_string(data);
    }

    return MAKE_SYMBOL(AS_STRING(*symbol));
}

/* LIST */

static word_t append(word_t head, word_t tail) {
    if (head == NIL) {
        return tail;
    }

    if (IS_ATOM(head)) {
        return cons(head, tail);
    }

    return cons(CAR(head), append(CDR(head), tail));
}

/* ENVIRONMENT */

static void add_env(word_t x) {
    AS_ENV(x)->parent = env;
    env               = x;
}

static void extend() {
    env = make_env(env);
    add_root(env);
}

static state_t save() {
    return (state_t){
        .sp  = sp,
        .rp  = rp,
        .env = env,
    };
}

static void unwind(state_t state) {
    sp  = state.sp;
    rp  = state.rp;
    env = state.env;
}

static word_t lookup(word_t var) {
    for (word_t e = env; e != NIL; e = AS_ENV(e)->parent) {
        const vector_t* bindings = AS_VECTOR(AS_ENV(e)->bindings);

        const size_t index =
            hashtable_search(*bindings, BINDINGS_SIZE, var, HASH_UINT);

        const pair_t* binding = AS_PAIR((*bindings)[index]);

        if (binding->car != VOID) {
            return binding->cdr;
        }
    }

    return VOID;
}

static void bind(word_t var, word_t val) {
    const vector_t* bindings = AS_VECTOR(AS_ENV(env)->bindings);

    const size_t index =
        hashtable_search(*bindings, BINDINGS_SIZE, var, HASH_UINT);

    if (index == -1) {
        error("binding table overflow");
    }

    pair_t* binding = AS_PAIR((*bindings)[index]);

    binding->car = var;
    binding->cdr = val;
}

static void bind_args(word_t argvar, word_t argval) {
    ZIP_TAKE_CONS(argvar, argval) {
        const word_t var = CAR(argvar);
        const word_t val = CAR(argval);

        bind(var, val);
    }
}

/* EVALUATION */

static word_t eval(word_t x) {
    const state_t state = save();

start:
    if (IS_ATOM(x)) {
        x = evatom(x);
        goto end;
    }

    const word_t head = eval(CAR(x));
    x                 = CDR(x);

    if (IS_PRIMITIVE(head)) {
        const primitive_t* primitive = AS_PRIMITIVE(head);

        const int macro    = AS_BOOLEAN(primitive->macro);
        const int tailcall = AS_BOOLEAN(primitive->tailcall);

        if (!macro) {
            x = evlis(x);
        }

        x = primitive->function(x);

        if (tailcall) {
            goto start;
        }
    } else if (IS_CLOSURE(head)) {
        const closure_t* closure = AS_CLOSURE(head);

        const int macro = AS_BOOLEAN(closure->macro);

        if (!macro) {
            x = evlis(x);
        }

        env = closure->env;
        extend();
        bind_args(closure->args, x);

        x = begin(closure->body);

        goto start;
    } else {
        error("apply expects closure");
    }

end:
    unwind(state);
    return x;
}

static word_t evlis(word_t x) {
    if (IS_ATOM(x)) {
        return cons(eval(x), NIL);
    }

    word_t y    = NIL;
    word_t tail = NIL;

    TAKE_CONS(x) {
        const word_t new = cons(eval(CAR(x)), NIL);

        if (y == NIL) {
            tail = y = new;
        } else {
            CDR(tail) = new;
            tail      = CDR(tail);
        }
    }

    return y;
}

static word_t evatom(word_t x) {
    if (IS_SYMBOL(x)) {
        return evsym(x);
    } else {
        return x;
    }
}

static word_t evsym(word_t x) {
    const word_t val = lookup(x);

    if (val == VOID && x != intern("void")) {
        error("unbound symbol %s", AS_SYMBOL(x));
    }

    return val;
}

static word_t quasi(word_t x) {
    if (IS_ATOM(x)) {
        return x;
    }

    if (IS_PAIR(CAR(x)) && CAAR(x) == intern("splicing")) {
        return append(CADAR(x), quasi(CDR(x)));
    }

    if (IS_PAIR(CAR(x)) && CAAR(x) == intern("unquote")) {
        if (IS_PAIR(CADAR(x)) && CAADAR(x) == intern("splicing")) {
            return append(eval(CADADAR(x)), quasi(CDR(x)));
        }
    }

    if (CAR(x) == intern("unquote")) {
        return eval(CADR(x));
    }

    if (CAR(x) == intern("quasi")) {
        return quasi(CDR(x));
    }

    return cons(quasi(CAR(x)), quasi(CDR(x)));
}

static word_t begin(word_t x) {
    word_t y = NIL;

    TAKE_CONS_UNTIL_ONE(x) {
        y = eval(CAR(x));
    }

    return CAR(x);
}

/* PARSING */

static void skip_comments_and_whitespace() {
    while (IS_WHITESPACE(*bp) || *bp == ';') {
        if (*bp == ';') {
            while (*bp != '\0' && *bp != '\n') {
                bp++;
            }
        } else {
            bp++;
        }
    }
}

static char* next_token() {
    skip_comments_and_whitespace();

    size_t i = 0;

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
            return parse_quote("quote");
        case '`':
            return parse_quote("quasi");
        case ',':
            return parse_quote("unquote");
        case '@':
            return parse_quote("splicing");
        default:
            if (!IS_SPECIAL(*token)) {
                return parse_atom();
            }
            error("unexpected token %s", token);
    }

    return VOID;
}

static word_t parse_atom() {
    int     len     = 0;
    int64_t integer = 0;
    // float   real    = 0;

    if (SCAN_NUMBER("%lld%n", integer, len)) {
        return MAKE_INTEGER(integer);
    } else {
        return intern(token);
    }
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

static word_t parse_quote(const char* type) {
    next_token();
    return cons(intern(type), cons(parse_expr(), NIL));
}

/* PRINTING */

static void print(word_t x) {
    if (x == NIL) {
        printf("nil");
    } else if (x == VOID) {
        printf("void");
    } else if (IS_INTEGER(x)) {
        printf("%lld", AS_INTEGER(x));
    } else if (IS_BOOLEAN(x)) {
        AS_BOOLEAN(x) ? printf("true") : printf("false");
    } else if (IS_SYMBOL(x)) {
        printf("%s", AS_SYMBOL(x));
    } else if (IS_PRIMITIVE(x)) {
        printf("<primitive>");
    } else if (IS_PAIR(x)) {
        print_list(x);
    } else if (IS_VECTOR(x)) {
        print_vector(x);
    } else if (IS_STRING(x)) {
        printf("\"%s\"", (char*)AS_STRING(x));
    } else if (IS_CLOSURE(x)) {
        const closure_t* closure = AS_CLOSURE(x);
        AS_BOOLEAN(closure->macro) ? printf("<macro>") : printf("<fun>");
        printf("{ ");
        print(closure->args);
        printf(" :args ");
        print(closure->body);
        printf(" :body }");
    }
}

static void print_list(word_t x) {
    putchar('(');

    while (1) {
        print(CAR(x));
        x = CDR(x);

        if (IS_ATOM(x)) {
            if (x != NIL) {
                putchar(' ');
                print(x);
            }

            break;
        }

        putchar(' ');
    }

    putchar(')');
}

static void print_vector(word_t x) {
    const size_t    size   = GET_OBJECT_SIZE(x);
    const vector_t* vector = AS_VECTOR(x);

    putchar('[');

    for (int i = 0; i < size; i++) {
        print((*vector)[i]);

        if (i < size - 1) {
            putchar(' ');
        }
    }

    putchar(']');
}

static void println(word_t x) {
    print(x);
    putchar('\n');
}

/* PRIMITIVES */

static word_t primitive_eval(word_t x) {
    return CAR(x);
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
    return eval(CAR(x));
}

static word_t primitive_cons(word_t x) {
    return cons(CAR(x), CADR(x));
}

static word_t primitive_car(word_t x) {
    if (!IS_PAIR(x)) {
        error("cdr expects pair type");
    }

    return CAAR(x);
}

static word_t primitive_cdr(word_t x) {
    if (!IS_PAIR(x)) {
        error("cdr expects pair type");
    }

    return CDAR(x);
}

static word_t primitive_add(word_t x) {
    PRIMITIVE_INTEGER_BINARY_OP(x, +);
}

static word_t primitive_sub(word_t x) {
    PRIMITIVE_INTEGER_BINARY_OP(x, -);
}

static word_t primitive_mul(word_t x) {
    PRIMITIVE_INTEGER_BINARY_OP(x, *);
}

static word_t primitive_div(word_t x) {
    PRIMITIVE_INTEGER_BINARY_OP(x, /);
}

static word_t primitive_eqv(word_t x) {
    PRIMITIVE_INTEGER_COMP_OP(x, ==);
}

static word_t primitive_gt(word_t x) {
    PRIMITIVE_INTEGER_COMP_OP(x, >);
}

static word_t primitive_lt(word_t x) {
    PRIMITIVE_INTEGER_COMP_OP(x, <);
}

static word_t primitive_gte(word_t x) {
    PRIMITIVE_INTEGER_COMP_OP(x, >=);
}

static word_t primitive_lte(word_t x) {
    PRIMITIVE_INTEGER_COMP_OP(x, <=);
}

static word_t primitive_eq(word_t x) {
    return MAKE_BOOLEAN(CAR(x) == CADR(x));
}
static word_t primitive_not(word_t x) {
    return MAKE_BOOLEAN(NOT(CAR(x)));
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

static word_t primitive_is_atom(word_t x) {
    return MAKE_BOOLEAN(IS_ATOM(eval(CAR(x))));
}

static word_t primitive_is_nil(word_t x) {
    return MAKE_BOOLEAN(eval(CAR(x)) == NIL);
}

static word_t primitive_is_pair(word_t x) {
    return MAKE_BOOLEAN(IS_PAIR(eval(CAR(x))));
}

static word_t primitive_is_quote(word_t x) {
    return MAKE_BOOLEAN(eval(CAR(x)) == intern("quote"));
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

static word_t primitive_fun(word_t x) {
    const word_t args = CAR(x);
    const word_t body = CDR(x);

    return make_closure(args, body, 0);
}

static word_t primitive_macro(word_t x) {
    const word_t args = CAR(x);
    const word_t body = CDR(x);

    return make_closure(args, body, 1);
}

static word_t primitive_def(word_t x) {
    const word_t var = CAR(x);
    const word_t val = eval(CADR(x));

    bind(var, val);

    return CAR(x);
}

static word_t primitive_defun(word_t x) {
    const word_t var  = CAR(x);
    const word_t args = CADR(x);
    const word_t body = CDDR(x);
    const word_t fun  = make_closure(args, body, 0);

    bind(var, fun);

    return fun;
}

static word_t primitive_defmacro(word_t x) {
    const word_t var   = CAR(x);
    const word_t args  = CADR(x);
    const word_t body  = CDDR(x);
    const word_t macro = make_closure(args, body, 1);

    bind(var, macro);

    return macro;
}

static word_t primitive_cond(word_t x) {
    TAKE_CONS(x) {
        const word_t clause = CAR(x);
        const word_t cond   = eval(CAR(clause));
        const word_t exp    = CDR(clause);

        if (!NOT(cond)) {
            return begin(exp);
        }
    }

    return NIL;
}

static word_t primitive_let(word_t x) {
    /* Variables are bound in a separate environment, hence the two loops. One
     * for evaluating the variables, and the other for binding. */

    word_t bindings = NIL;

    /* _bindings = unevaluated bindings. */
    EACH_CONS(_bindings, CAR(x)) {
        const word_t binding = CAR(_bindings);
        const word_t var     = (CAR(binding));
        const word_t val     = eval(CADR(binding));

        bindings = cons(cons(var, val), bindings);
    }

    extend();

    TAKE_CONS(bindings) {
        const word_t binding = CAR(bindings);
        const word_t var     = CAR(binding);
        const word_t val     = CDR(binding);

        bind(var, val);
    }

    return begin(CDR(x));
}

static word_t primitive_lets(word_t x) {
    extend();

    EACH_CONS(bindings, CAR(x)) {
        const word_t var = CAAR(bindings);
        const word_t val = CADAR(bindings);

        bind(var, eval(val));
    }

    return begin(CDR(x));
}

static word_t primitive_list(word_t x) {
    return x;
}

static word_t primitive_length(word_t x) {
    uint64_t y = 0;
    x          = eval(CAR(x));

    TAKE_CONS(x) {
        y++;
    }

    return MAKE_INTEGER(y);
}

static word_t primitive_gensym(word_t x) {
    char       sym[TOKEN_SIZE];
    static int count = 0;

    if (CAR(x) == NIL) {
        snprintf(sym, sizeof(sym), "$%d", count++);
    } else {
        snprintf(sym, sizeof(sym), "$%s%d", AS_SYMBOL(CAR(x)), count++);
    }

    return intern(sym);
}

static word_t primitive_println(word_t x) {
    println(x);
    return VOID;
}

static const struct {
    const char* s;
    primitive_t p;
} primitives[] = {
    PRIM(eval, 1, 1),
    PRIM(begin, 1, 1),
    PRIM(quote, 0, 1),
    PRIM(quasi, 0, 1),
    PRIM(unquote, 0, 1),
    PRIM(cons, 0, 0),
    PRIM(car, 0, 0),
    PRIM(cdr, 0, 0),

    PRIMA(+, add, 0, 0),
    PRIMA(-, sub, 0, 0),
    PRIMA(*, mul, 0, 0),
    PRIMA(/, div, 0, 0),

    PRIMA(=, eqv, 0, 0),
    PRIMA(>, gt, 0, 0),
    PRIMA(<, lt, 0, 0),
    PRIMA(>=, gte, 0, 0),
    PRIMA(<=, lte, 0, 0),

    PRIM(eq, 0, 0),
    PRIM(not, 0, 0),
    PRIM(or, 0, 1),
    PRIM(and, 0, 1),

    PRIMA(nil?, is_nil, 0, 1),
    PRIMA(atom?, is_atom, 0, 1),
    PRIMA(pair?, is_pair, 0, 1),
    PRIMA(quote?, is_quote, 0, 1),
    PRIMA(symbol?, is_symbol, 0, 1),
    PRIMA(integer?, is_integer, 0, 1),
    PRIMA(closure?, is_closure, 0, 1),

    PRIM(fun, 0, 1),
    PRIM(macro, 0, 1),
    PRIM(def, 0, 1),
    PRIM(defun, 0, 1),
    PRIM(defmacro, 0, 1),
    
    PRIM(cond, 1, 1),
    PRIM(let, 1, 1),
    PRIMA(let*, lets, 1, 1),

    PRIM(list, 0, 0),
    PRIM(length, 0, 1),

    PRIM(gensym, 0, 0),
    PRIM(println, 0, 0),

    {0}
};

/* HASHTABLE */

static uint64_t hash_uint(uint64_t key) {
    return key ^ (key >> 16);
}

static uint64_t hash_str(const char* key) {
    int      c    = 0;
    uint64_t hash = 5342;

    while ((c = (unsigned char)*key++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

static void init_hashtable(word_t* entries, size_t size) {
    for (size_t i = 0; i < size; i++) {
        entries[i] = cons(VOID, VOID);
    }
}

/* Hashtable search generalises for string key and uint key. Assumes entries is
 * a vector of key-value cons pairs.
 */
static size_t
hashtable_search(const word_t* entries, size_t size, word_t key, hash_t ht) {
    uint64_t h = 0;
    switch (ht) {
        case HASH_UINT:
            h = hash_uint(key);
            break;
        case HASH_STRING:
            h = hash_str((char*)key);
            break;
    }

    size_t index = h % size;
    size_t count = 0;

    while (count < size) {
        const pair_t* entry = AS_PAIR(entries[index]);

        if (entry->car == VOID || entry->car == key) {
            return index;
        }

        const int found_str =
            ht == HASH_STRING
                ? strcmp((char*)AS_STRING(entry->car), (char*)key) == 0
                : 0;

        if (found_str) {
            return index;
        }

        count++;
        index++;
        index %= size;
    }

    return -1;
}

/* MAIN */

static void init_symbols() {
    symbols = make_vector(SYMBOLS_SIZE, NULL);

    init_hashtable(*AS_VECTOR(symbols), SYMBOLS_SIZE);

    add_root(symbols);
}

static void init_primitives() {
    for (size_t i = 0; primitives[i].s; ++i) {
        bind(intern(primitives[i].s), MAKE_PRIMITIVE(&primitives[i].p));
    }

    bind(intern("true"), TRUE);
    bind(intern("false"), FALSE);
    bind(intern("nil"), NIL);
    bind(intern("void"), VOID);
}

static void init_env() {
    env = make_env(NIL);
    add_root(env);
}

static void open(const char* path) {
    FILE* file = NULL;
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
    init_symbols();
    init_env();
    init_primitives();

    base = save();

    printf("have fun!\n");
}

static void repl() {
    while (1) {
        printf("> ");
        fgets(buffer, BUFFER_SIZE, stdin);

        if (strcmp(buffer, "quit\n") == 0) {
            break;
        }

        println(eval(parse()));
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

/* ERROR HANDLING */

static void log_error(const char* format, ...) {
    va_list args = NULL;
    va_start(args, format);
    vfprintf(stderr, format, args);
    putchar('\n');
    va_end(args);
}

static void error(const char* format, ...) {
    log_error(format);
    unwind(base);
    longjmp(jb, 1);
}

static void panic(const char* format, ...) {
    log_error(format);
    exit(EXIT_FAILURE);
}
