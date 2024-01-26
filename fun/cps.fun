#|
Continuation Passing Style Transform

Unapologetically ripped off from Matt Might.

exp = aexp
    | (begin exp*)
    | (prim exp*)
    | (exp exp*)

aexp = (fun (symbol*) exp)
     | atom

cexp = (aexp aexp*)
     | ((cps prim) aexp*)
|#

(begin
    ; exp -> aexp
    (defun m (exp)
        (cond
            ((atom? exp) exp)
            ((fun?  exp) (m-fun exp))))

    (defun m-fun (x)
        (def args (fun-args exp))
        (def body (fun-body exp))
        (def $k   (gensym 'k))
        `(fun (,@args ,$k) ,(tc body $k)))

    ; exp | aexp -> cexp 
    (defun tc (exp c)
        (cond
            ((aexp?  exp) `(,c ,(m exp)))
            ((apply? exp) (tc-apply exp c))))

    (defun tc-apply (exp c)
        (def fn   (apply-fun exp))
        (def args (apply-args exp))
        (tk fn (fun ($fn)
            (t*k args (fun ($args)
                `(,$fn ,@$args ,c))))))
    
    ; exp | (aexp -> cexp) -> cexp
    (defun tk (exp k)
        (cond
            ((aexp?  exp) (k (m exp)))
            ((apply? exp) (tk-apply exp k))))

    (defun tk-apply (exp k)
        (def $r (gensym 'r))
        (def c `(fun (,$r) ,(k $r)))
        (tc exp c))

    ; exp* | (aexp* -> cexp) -> cexp
    (defun t*k (exp* k)
        (cond
            ((nil? exp*) (k nil))
            ((pair? exp*) (tk (car exp*) (fun (head)
                (t*k (cdr exp*) (fun (tail)
                    (k (cons head tail)))))))))

    (defun cps (exp)
        (cond
            ((aexp? exp)  (m exp))
            ((apply? exp) (tc exp 'halt)))))