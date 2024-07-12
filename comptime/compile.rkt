#lang nanopass

(define prims (set '+ '- '* '/ '=))

(define var? symbol?)
(define (lit? x) (or (boolean? x) (char? x) (number? x) (string? x)))
(define (prim? x) (set-member? prims x))

(define Var? var?)
(define Lit? lit?)
(define Prim? prim?)

(define-language L0
  (terminals
    (Var (var))
    (Lit (lit))
    (Prim (prim)))
  (Exp (exp body fun arg val)
    var
    lit
    (lam (var* ...) body)
    (if exp0 exp1 exp2)
    (fix ([var* val*] ...) body)
    (primcall prim arg* ...)
    (app fun arg* ...)))

(define-pass parse-scheme : * (exp) -> L0 ()
  (definitions
    (define (Exp* exp*) (map Exp exp*)))
    
  (Exp : * (exp) -> Exp ()
    (with-output-language L0
      (match exp
        [(? var?) `,exp]

        [(? lit?) `,exp]

        [`(fun (,var* ...) ,body)
         `(lam (,(Exp* var*) ...) ,(Exp body))]

        [`(if ,exp0 ,exp1 ,exp2)
         `(if ,(Exp exp0) ,(Exp exp1) ,(Exp exp2))]

        [`(letrec ([,var* ,val*] ...) ,body)
         `(fix ([,(Exp* var*) ,(Exp* val*)] ...) ,(Exp body))]

        [`(,(? prim?) ,arg* ...)
         `(primcall ,(car exp) ,(Exp* arg*) ...)]

        [`(,fun ,arg* ...)
         `(app ,(Exp fun) ,(Exp* arg*) ...)]

        [else (error "Unknown syntax" exp)])))
        
  (Exp exp))

(define-pass alpha-convert : L0 (exp) -> L0 ()
  (definitions
    (define (extend env vars new-vars)
      (append (map cons vars new-vars) env))
      
    (define (lookup env exp)
      (let ((binding (assoc exp env)))
        (if binding
          (cdr binding)
          exp))))

  (Exp : Exp (exp env) -> Exp ()
    [,var (or (lookup env var) var)]
    
    [(lam (,var* ...) ,body) 
     (define var*0 (map gensym var*))
     (define env0 (extend env var* var*0))
     (define body0 (Exp body env0))
    `(lam (,var*0 ...) ,body0)]

    [(if ,exp0 ,exp1 ,exp2)
    `(if ,(Exp exp0 env) 
         ,(Exp exp1 env) 
         ,(Exp exp2 env))]

    [(fix ([,var* ,val*] ...) ,body)
     (define var*0 (map gensym var*))
     (define env0 (extend env var* var*0))
     (define val*0 (map (lambda (val) (Exp val env0)) val*))
     (define body0 (Exp body env0))
    `(fix ([,var*0 ,val*0] ...) ,body0)]

    [(primcall ,prim ,arg* ...)
     (define arg*0 (map (lambda (arg) (Exp arg env)) arg*))
    `(primcall ,prim ,arg*0 ...)]

    [(app ,fun ,arg* ...)
     (define fun0 (Exp fun env))
     (define arg*0 (map (lambda (arg) (Exp arg env)) arg*))
    `(app ,fun0 ,arg*0 ...)])
      
    (Exp exp '()))

(define-pass cps-convert : L0 (exp) -> L0 ()
  (definitions
    ; Could move this in to another language definition
    ; Unused atm
    (define (aexp? exp)
      (nanopass-case (L0 Exp) exp
        [,var #t]
        [,lit #t]
        [(lam (,var* ...) ,body) #t]
        [else #f]))

    (define (Kont* exp* k)
      (cond
        [(empty? exp*) (k '())]
        [else (Kont (car exp*) (lambda (hd)
          (Kont* (cdr exp*) (lambda (tl)
            (k (cons hd tl))))))])))
        
    
    (Atomic : Exp (exp) -> Exp ()
      [(lam (,var* ...) ,body)
       (define $k (gensym 'k))
      `(lam (,var* ... ,$k) ,(Cont body $k))]
      
      [else exp])

    (Kont : Exp (exp k) -> Exp
      [(if ,exp0 ,exp1 ,exp2)
       (define $r (gensym 'r))
       (define c `(lam (,$r) ,(k $r)))
       (Kont exp1 (lambda (exp)
        `(if ,exp
          ,(Cont exp1 c)
          ,(Cont exp2 c))))]

      [(fix ([,var* ,val*] ...) ,body)
      `(fix ([,var* ,(map Atomic val*)] ...) ,(Kont body k))]

      [(primcall ,prim ,arg* ...)
       (define $r (gensym 'r))
       (define c `(lam (,$r) ,(k $r)))
       (Cont exp c)]

      [(app ,fun ,arg* ...)
       (define $r (gensym 'r))
       (define c `(lam (,$r) ,(k $r)))
       (Cont exp c)]
      
      [else (k (Atomic exp))])

  (Cont : Exp (exp c) -> Exp ()
    [(if ,exp0 ,exp1 ,exp2)
      (define $k (gensym 'k))
     `(app (lam (,$k)
      ,(Kont exp0 (lambda (exp)
        `(if ,exp ,(Cont exp1 $k) ,(Cont exp2 $k)))))
      ,c)]

    [(fix ([,var* ,val*] ...) ,body)
     `(fix ([,var* ,(map Atomic val*)] ...) 
     ,(Cont body c))]

    [(primcall ,prim ,arg* ...)
      (Kont* arg* (lambda (arg*0)
     `(primcall ,prim ,arg*0 ... ,c)))]

    [(app ,fun ,arg* ...)    
     (Kont fun (lambda (fun0)
     (Kont* arg* (lambda (arg*0)
    `(app ,fun0 ,arg*0 ... ,c)))))]
     
    [else `(app ,c ,(Atomic exp))])
        
  (Cont exp 'halt))

(define-language L1 (extends L0)
  (Exp (exp val)
    (+ (label var)
       (clos-ref var)
       (clos (var0* ...) (var1* ...) body)))) ; first set of vars are environment captures

(define-pass closure-convert : L0 (exp) -> L1 ()
  (definitions
    (define (Exp-with labels)
      (lambda (exp) (Exp exp labels)))

    (define (substitute exp env labels)
      (with-output-language (L1 Exp)
        (nanopass-case (L1 Exp) exp
          [,var
            (cond ((set-member? env exp) `(clos-ref ,exp))
                  ((set-member? labels exp) `(label ,exp))
                  (else exp))]

          [(lam (,var* ...) ,body)
           (define env0 (set-subtract env (apply set var*)))
          `(lam (,var* ...) ,(substitute body env0 labels))]

          [(if ,exp0 ,exp1 ,exp2)
          `(if ,(substitute exp0 env labels) ,(substitute exp1 env labels) ,(substitute exp2 env labels))]

          [(fix ([,var* ,val*] ...) ,body)
           (define labels0 (set-union labels (apply set var*)))
          `(fix ([,var* ,(map (substitute-with env labels0) val*)] ...) ,(substitute body env labels0))]

          [(primcall ,prim ,arg* ...)
          `(primcall ,prim ,(map (substitute-with env labels) arg*) ...)]

          [(app ,fun ,arg* ...)
          `(app ,(substitute fun env labels) ,(map (substitute-with env labels) arg*) ...)]

          [else exp])))

    (define (substitute-with env labels)
      (lambda (exp) (substitute exp env labels)))
    
    (define (free exp)
      (nanopass-case (L0 Exp) exp
        [,var (set var)]
      
        [(lam (,var* ...) ,body)
         (set-subtract (free body) (apply set var*))]
    
        [(if ,exp0 ,exp1 ,exp2)
         (set-union (free exp0) (free exp1) (free exp2))]
      
        [(fix ([,var* ,val*] ...) ,body)
       ; ((free body) U (free val*)) - var*
         (set-subtract (apply set-union (free body) (map free val*)) (apply set var*))]
      
        [(primcall ,prim ,arg* ...)
         (apply set-union (map free arg*))]

        [(app ,fun ,arg* ...)
         (apply set-union (map free (cons fun arg*)))]
      
      [else (set)])))

  (Exp : Exp (exp labels) -> Exp ()
    [(lam (,var* ...) ,body)
     (define env (set->list (set-subtract (free exp) labels)))
     (define body0 (substitute (Exp body labels) env labels))
     (if (set-empty? env)
    `(lam (,var* ...) ,body0)
    `(clos (,env ...) (,var* ...) ,body0))]

    [(if ,exp0 ,exp1 ,exp2)
    `(if ,(Exp exp0 labels) ,(Exp exp1 labels) ,(Exp exp2 labels))]

    [(fix ([,var* ,val*] ...) ,body)
     (define labels0 (set-union labels (apply set var*)))
     (define val*0 (map (lambda (v)
        (substitute (Exp v labels0) (set) labels0)) val*))
     (define body0 (substitute (Exp body labels0) (set) labels0))
    `(fix ([,var* ,val*0] ...) ,body0)]

    [(primcall ,prim ,arg* ...)
    `(primcall ,prim ,(map (Exp-with labels) arg*) ...)]

    [(app ,fun ,arg* ...)
    `(app ,(Exp fun labels) ,(map (Exp-with labels) arg*) ...)])
    
  (Exp exp (set)))

(define-language L2 (extends L1)
  (entry Prog)
  (Prog ()
    (+ (fix ([var* top*] ...) exp)))
  (Top (top)
    (+ exp ; exp is assumed to be atomic
       (lam (var* ...) body)
       (clos (var0* ...) (var1* ...) body)))
  (Exp (exp var val body)
    (- (fix ([var* val*] ...) body)
       (lam (var* ...) body)
       (clos (var0* ...) (var1* ...) body))
       
    (+ (make-clos (var* ...) var))))

(define-pass lift-top-level : L1 (exp) -> L2 ()
  (definitions
    (define-syntax build-T*
      (syntax-rules ()
        ((_ T* T t* env)
          (if (null? t*)
            (values '() env)
            (let*-values (((hd env0) (T (car t*) env))
                          ((tl env1) (T* (cdr t*) env0)))
              (values (cons hd tl) env1))))))

    (define (extend env var val)
      (cons (cons var val) env))

    (define (Exp* exp* env) (build-T* Exp* Exp exp* env))
    (define (Top* top* env) (build-T* Top* Top top* env)))

  (Exp : Exp (exp env) -> Top (env)
    [(label ,var) (values `(label ,var) env)]
    
    [(clos-ref ,var) (values `(clos-ref ,var) env)]

    [(lam (,var* ...) ,body)
     (let*-values ((($lam) (gensym 'lam))
                   ((exp0 env0) (Top exp env))
                   ((env1) (extend env0 $lam exp0)))
      (values $lam env1))]

    [(clos (,var0* ...) (,var1* ...) ,body)
     (let*-values ((($clos) (gensym 'lam))
                   ((exp0 env0) (Top exp env))
                   ((env1) (extend env0 $clos exp0)))
      (nanopass-case (L2 Top) exp0
        [(clos (,var0* ...) (,var1* ...) ,body)
         (values `(make-clos (,var0* ...) ,$clos) env1)]))]

    [(if ,exp0 ,exp1 ,exp2)
     (let*-values (((exp* env0) (Exp* (list exp0 exp1 exp2) env)))
      (values `(if ,(first exp*) ,(second exp*) ,(third exp*)) env0))]

    [(fix ([,var* ,val*] ...) ,body)
     (let*-values (((body0 env0) (Exp body env))
                   ((val*0 env1) (Top* val* env0))
                   ((env2) (if (pair? var*) 
                            (append (map cons var* val*0) env1)
                            env1)))
      (values body0 env2))]
    
    [(primcall ,prim ,arg* ...)
     (let*-values (((arg*0 env0) (Exp* arg* env)))
      (values `(primcall ,prim ,arg*0 ...) env0))]

    [(app ,fun ,arg* ...)
     (let*-values (((fun0 env0) (Exp fun env))
                   ((arg*0 env1) (Exp* arg* env0)))
      (values `(app ,fun0 ,arg*0 ...) env1))]
    
    [else (values `,exp env)])
    
  (Top : Exp (exp env) -> Top ()
    [,var (values var env)]

    [,lit (values lit env)]

    [(lam (,var* ...) ,body)
     (let-values (((body0 env0) (Exp body env)))
      (values `(lam (,var* ...) ,body0) env0))]

    [(clos (,var0* ...) (,var1* ...) ,body)
     (let-values (((body0 env0) (Exp body env)))
      (values `(clos (,var0* ...) (,var1* ...) ,body0) env0))]
      
    [else (error "fixpoint can only bind atomic values")])

  (let*-values (((body env) (Exp exp '()))
                ((var*) (map car env))
                ((val*) (map cdr env)))
    `(fix ([,var* ,val*] ...) ,body)))

(define-pass codegen-c : L2 (prog) -> * ()
  (definitions
    (define (~var var)
      (let ((s (symbol->string var)))
        (if (char-numeric? (string-ref s 0))
            (string-append "_" (regexp-replace* #rx"[^a-zA-Z0-9_]" s "_"))
            (regexp-replace* #rx"[^a-zA-Z0-9_]" s "_"))))

    (define (~lit lit)
      (cond
        [(eq? lit '#t) "fun_true"]
        [(eq? lit '#f) "fun_false"]
        [(number? lit) (string-append "fun_mk_int(" (number->string lit) ")")]))

    (define (~clos-ref var) (string-append "env." (~var var)))

    (define (~make-clos var* var)
      (string-append "fun_mk_clos(" var ", " (~list var*) ")"))

    (define (~if exp0 exp1 exp2)
      (string-append "(" "fun_as_bool(" exp0 ") ? (" exp1 ") : (" exp2 "))"))

    (define prims
      '((+ . "fun_prim_add")
        (- . "fun_prim_sub")
        (* . "fun_prim_mul")
        (/ . "fun_prim_div")
        (= . "fun_prim_eq")))

    (define (~primcall prim arg*)
      (define macro (cdr (assoc prim prims)))
      (string-append macro "(" (first arg*) ", " (second arg*) ", " (third arg*) ")"))

    (define (~app fun arg*)
      (string-append "fun_as_proc(" fun ")" "(" (~list arg*) ")"))

    (define (~top top)
      (apply string-append top))

    (define (~main body)
      (string-append "int main() {\n" body ";\nreturn 0;\n}"))
      
    (define (~list exp*)
      (string-join exp* ", ")))

  (Exp : Exp (exp) -> * ()
    [,var (~var var)]
    
    [,lit (~lit lit)]
    
    [(clos-ref ,var) (~clos-ref var)]
    
    [(label ,var) (~var var)]
    
    [(make-clos (,var* ...) ,var)
     (~make-clos (map Exp var*) (Exp var))]

    [(if ,exp0 ,exp1 ,exp2)
     (~if (Exp exp0) (Exp exp1) (Exp exp2))]
    
    [(primcall ,prim ,arg* ...)
     (define arg*0 (map Exp arg*))
     (~primcall prim arg*0)]
    
    [(app ,fun ,arg* ...)
     (define fun0 (Exp fun))
     (define arg*0 (map Exp arg*))
     (~app fun0 arg*0)])
    
  (Top : Top (top name) -> * ()
    [,exp
     (string-append "static const fun_any_t " (Exp name) " = " (Exp exp) ";\n")]
    
    [(lam (,var* ...) ,body)
     (define var*0 (string-join (map (lambda (v) (string-append "fun_any_t " (Exp v))) var*) ", "))
     (define body0 (Exp body))
     (string-append "void " name "_proc(" var*0 ") {\n" body0 ";\n}\n"
                    "static const fun_any_t " name " = fun_mk_proc(" name "_proc)\n")]
    
    [(clos (,var0* ...) (,var1* ...) ,body)
     (define var0*0 (string-join (map (lambda (v) (string-append "fun_any_t " (Exp v) ";")) var0*) "\n"))
     (define var1*0 (string-join (cons (string-append name "_env env") (map Exp var1*)) ", fun_any_t "))
     (define body0 (Exp body))
     (string-append "typedef struct {\n" var0*0 "\n} " name "_env;\n"
                    "void " name "_proc(" var1*0 ") {\n" body0 ";\n}\n")
                    "static const fun_any_t " name " = fun_mk_proc(" name "_proc)\n"])

  (Prog : Prog (prog) -> * ()
    [(fix ([,var* ,top*] ...) ,body)
     (let* ([var*0 (map Exp var*)]
            [top0 (~top (map Top top* var*0))]
            [main (~main (Exp body))])
       (string-append "#include \"runtime.h\"\n" top0 main))])
    
  (Prog prog))

(define steps
  (list parse-scheme
        alpha-convert
        cps-convert
        closure-convert
        lift-top-level
        codegen-c))

(define (apply-steps steps exp)
  (foldl (lambda (step exp) (step exp)) exp steps))

(define (compile exp)
  (apply-steps steps exp))