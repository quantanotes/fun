#|
Fun Flattening (Closure Conversion)
|#

(begin
    (defun fun*?    (x) (apply-of? 'fun* x))
    (defun funfun*? (x) (or (fun? x) (fun*? x)))
    
    (defun freevals (x)
        (cond
            ((symbol?   x)                (set x))
            ((funfun*?  x)                (freevals-fun x))
            ((apply-of? 'make-closure x)  (freevals-make-closure x))
            ((apply-of? 'make-env x)      (freevals-make-env x))
            ((apply-of? 'env-ref x)       (freevals-env-ref x))
            ((apply-of? 'apply-closure x) (freevals-apply-closure x))
            ((apply?    x)                (freevals-apply x))))

    (defun freevals-fun (x)
        (def args (fun-args x))
        (def body (fun-body x))
        (def fvsb (freevals body))
        (def args (set (quote-list args)))
        (difference fvsb args))

    (defun freevals-apply (x)
        (def fvs (quote-list (map freevals x)))
        (apply union fvs))

    (defun freevals-make-closure (x)
        (def fn  (cadr x))
        (def env (caddr x))
        (union (freevals fn) (freevals env)))

    (defun freevals-make-env (x)
        (defun mapping (x) (if (pair? x) (freevals (cadr x)) nil))
        (def   pairs   (cdr x))
        (def   fvs     (quote-list (map mapping pairs)))
        (apply union fvs))

    (defun freevals-env-ref (x)
        (def env (cadr x))
        (freevals env))

    (defun freevals-apply-closure (x)
        (def fvs (quote-list (map freevals (cdr x))))
        (apply union fvs))

    (defun subs (sub x)
        (cond
            ((symbol?   x)                (subs-symbol sub x))
            ((funfun*?  x)                (subs-fun sub x))
            ((apply-of? 'make-closure x)  (subs-make-closure sub x))
            ((apply-of? 'make-env x)      (subs-make-env sub x))
            ((apply-of? 'env-ref x)       (subs-env-ref sub x))
            ((apply-of? 'apply-closure x) (subs-apply-closure sub x))
            ((apply?    x)                (subs-apply sub x))
            (true x)))

    (defun subs-symbol (sub x)
        (def pair (assoc x sub))
        (if pair (cdr pair) x))

    (defun subs-fun (sub x)
        (def args (fun-args x))
        (def body (fun-body x))
        (def sub* (filter (fun (pair) (not (has? (car pair) args))) sub))
        (list 'fun args (subs sub* body)))

    (defun subs-apply (sub x)
        (map (subs-with sub) x))

    (defun subs-make-closure (sub x)
        (def fn  (cadr x))
        (def env (caddr x))
        (list 'make-closure (subs sub fn) (subs sub env)))
    
    (defun subs-make-env (sub x)
        (def   pairs   (cdr x))
        (defun mapping (pair) (cons (car pair) ((subs-with sub) (cadr pair))))
        (cons 'make-env (map mapping pairs)))
    
    (defun subs-env-ref (sub x)
        (def env (cadr x))
        (def var (caddr x))
        (list 'env-ref (subs sub env) var))

    (defun subs-apply-closure (sub x)
        (def x (cdr x))
        (cons 'apply-closure (map (subs-with sub) x)))

    (defun subs-with (sub) (fun (x) (subs sub x)))

    ; Closure conversion
    (defun cc (x)
        (cond
            ((fun?      x)                (cc-fun x))
            ((apply-of? 'make-closure x)  x)
            ((apply-of? 'make-env x)      x)
            ((apply-of? 'env-ref x)       x)
            ((apply-of? 'apply-closure x) x)
            ((apply?    x)                (cc-apply x))
            (true       x)))

    (defun cc-fun (x)
        (def $env  (gensym 'env))
        (def args  (fun-args x))
        (def body  (fun-body x))
        (def args* (cons $env args))
        (def fvs   (freevals x))
        (def env   (map (fun (v) (list v v)) fvs))
        (def sub   (map (fun (v) (list v `(env-ref ,$env ,v))) fvs))
        (def body* (subs sub body))
        `(make-closure (fun* ,args* ,body*) (make-env ,@env)))

    (defun cc-apply (x)
        (cons 'apply-closure x))

    ; Transform bottom up
    (defun tbup (f x)
        (defun t (e) (tbup f e))
        (def x* (cond
            ((symbol?   x)                x)
            ((funfun*?  x)                (tbup-fun t x))
            ((apply-of? 'make-closure x)  (tbup-make-closure t x))
            ((apply-of? 'make-env x)      (tbup-make-env t x))
            ((apply-of? 'env-ref x)       (tbup-env-ref t x))
            ((apply-of? 'apply-closure x) (tbup-apply-closure t x))
            ((apply?    x)                (tbup-apply t x))))
        (f x*))

    (defun tbup-fun (t x)
        (def args (fun-args x))
        (def body (fun-body x))
        (list (car x) args (t body)))

    (defun tbup-apply (t x)
        (def fn   (apply-fun x))
        (def args (apply-args x))
        (cons (t fn) (map t args)))

    (defun tbup-make-closure (t x)
        (def fn  (cadr x))
        (def env (caddr x))
        (list 'make-closure (t fn) (t env)))
    
    (defun tbup-make-env (t x)
        (def   pairs   (cdr x))
        (defun mapping (pair) (cons (car pair) (t (cadr pair))))
        (cons 'make-env (map mapping pairs)))
    
    (defun tbup-env-ref (t x)
        (def env (cadr x))
        (def var (caddr x))
        (list 'env-ref (t env) var))

    (defun tbup-apply-closure (t x)
        (cons 'apply-closure (map t (cdr x))))
    
    (defun ff (x) (tbup cc x)))
