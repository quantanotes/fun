#|
Fun Flattening

Also ripped off from Matt Might.
|#

(begin
    (defun fun*?    (x) (and (pair? x) (eq (car x) 'fun*)))
    (defun funfun*? (x) (or (fun? x) (fun*? x)))    

    (defun freevals (exp)
        (cond
            ((symbol?  exp) (set exp))
            ((funfun*? exp) (freevals-fun exp))
            ((apply?   exp) (freevals-apply exp))))

    (defun freevals-fun (exp)
        (def args (fun-args exp))
        (def body (fun-body exp))
        (def fvsb (freevals body))
        (def args (set (quote-list args)))
        (difference fvsb args))

    (defun freevals-apply (exp)
        (def fn    (apply-fun exp))
        (def args  (apply-args exp))
        (def exps  (quote-list `(,fn ,@args)))
        (def fvs   (quote-list (map freevals exps)))
        (apply union fvs))

    (defun substitute (sub exp)
        ())

    ; Closure conversion
    (defun cc (exp)
        (def args  (fun-args exp))
        (def body  (fun-body exp))
        (def args* (con $env args))
        (def fa    (freevals exp))
        (def $env  (gensym 'env))
        
        )

    (defun ff (exp) '())

    (freevals '(fun (x) (+ x y))))