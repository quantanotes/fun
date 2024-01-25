|#
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
    (defun m (x)
        (cond
            ((atom? x) x)
            ((fun?  x) (m-fun x))))

    (defun m-fun (x)
        (def args  (fun-args x))
        (def body  (fun-body x))
        (def $k (gensym 'k))
        `(fun (,@args ,$k) ,(tc body $k)))

    ; exp | aexp -> cexp 
    (defun tc (x c)
        (cond
            ((aexp?  x) `(,c ,(m x)))
            ((apply? x) (tc-apply x c))))

    (defun tc-apply (x c)
        (def fn   (apply-fun x))
        (def args (apply-args x))
            (tk fn (fun ($fn)
                (t*k args (fun ($args ...)  ; Remove the n-ary if this doesn't work
                    `(,$fn ,@$args ,c)))))))
    
    ; exp | (aexp -> cexp) -> cexp
    (defun tk (x k)
        (cond
            ((aexp?  x) (k (m x)))
            ((apply? x) (tk-apply x k))))

    (defun tk-apply (x k)
        (def $r (gensym 'r))
        (def c `(fun (,$r) ,(k $r)))
        (tc x c))

    ; exp* | (aexp* -> cexp) -> cexp
    (defun t*k (xs k)
        (cond
            ((nil? xs) (k nil))
            ((pair? xs) (tk (car xs) (fun (head)
                (t*k (cdr xs) (fun (tail)
                    (k (cons head tail)))))))))

    (defun cps (x)
        (cond
            ((aexp? x)  (m x))
            ((apply? x) (tc x 'halt)))))