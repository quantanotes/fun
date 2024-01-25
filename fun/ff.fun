#|
Fun Flattening

Also ripped off from Matt Might.
|#

(begin
    (defun fun*?    (x) (and (pair? x) (eq? (car x) 'fun*)))
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
        (def fn   (apply-fun exp))
        (def args (apply-args exp))
        (def exps (quote-list `(,fn ,@args)))
        (def fvs  (quote-list (map freevals exps)))
        (apply union fvs))

    (defun subs (sub exp)
        (cond
            ((symbol? exp)  (subs-symbol sub exp))
            ((funfun*? exp) (subs-fun sub exp))
            ((apply? exp)   (subs-apply sub exp))
            (true exp)))

    (defun subs-symbol (sub exp)
        (def pair (assoc exp sub))
        (if pair (cdr pair) exp))

    (defun subs-fun (sub exp)
        (def args (fun-args exp))
        (def body (fun-body exp))
        (def sub* (filter (fun (pair) (not (has? args (car pair)))) sub))
        `(fun ,args ,(sub sub* body)))

    (defun subs-apply (sub exp)
        (def fn        (apply-fun exp))
        (def args      (apply-args exp))
        (def subs-args (map (fun (arg) (subs sub arg)) args))
        `(apply ,(subs sub fn) ,@subs-args))


    ; Closure conversion
    (defun cc (exp)
        (cond ((fun? exp)
            (def $env  (gensym 'env))
            (def args  (fun-args exp))
            (def body  (fun-body exp))
            (def args* (cons $env args))
            (def fvs   (freevals exp))
            (def env   (map (fun (v) (list v v)) fvs))
            (def sub   (map (fun (v) (list v `(env-ref ,$env ,v))) fvs))
            (def body* (subs sub body))
            `(make-closure (fun* ,args* ,body*) (make-env ,@env)))))

    ; Transform bottom up
    (defun tbup (f exp)
        (defun t (e) (tbup f e))
        (def exp* (cond
            ((symbol? exp)  exp)
            ((funfun*? exp) (tbup-fun t exp))
            ((apply? exp)   (tbup-apply t exp))))
        (f exp*))

    (defun tbup-fun (t exp)
        (def args (fun-args exp))
        (def body (fun-body exp))
        `(,(car exp) ,args ,(t body)))

    (defun tbup-apply (t exp)
        (def fn   (apply-fun exp))
        (def args (apply-args exp))
        (println 'fn: fn 'args: args)
        `(apply ,(t fn) ,@(map t args)))

    (defun ff (exp) (tbup cc exp))

    (ff '(fun (z) (z))))