(begin
    (defun apply (f args ...)
        (eval (cons f (eval (cons 'list* (quote-list args))))))

    (defun assoc (key alist)
        (cond
            ((nil? alist) false)
            ((eq? key (caar alist)) (car alist))
            (true (assoc key (cdr alist)))))

    (defun quote-list   (xs) (map (fun (x) (list 'quote x)) xs))
    (defun quote-result (f)  (fun (args ...) `(quote ,(apply f args))))

    (defun cddr  (x) (cdr (cdr x)))
    (defun cadr  (x) (car (cdr x)))
    (defun caar  (x) (car (car x)))
    (defun caadr (x) (car (cadr x)))
    (defun caddr (x) (car (cddr x)))

    (defun fun?   (x) (and (pair? x) (eq? (car x) 'fun)))
    (defun apply? (x) (pair? x))
    (defun unary? (x) (and (pair? x) (eq? (length x) 2)))
    (defun aexp?  (x) (or (atom? x) (fun? x)))

    (defun fun-args (x) (cadr x))
    (defun fun-body (x) (caddr x))

    (defun apply-fun  (x) (car x))
    (defun apply-args (x) (cdr x))

    (defun unary-fun (x) (car x))
    (defun unary-arg (x) (cadr x))

    (defun list* (xs ...)
        (if (nil? xs)
            nil
            (append (butlast xs) (car (last xs)))))

    (defun set (xs)
        (_set xs '()))

    (defun union (xs ...)
        (set (apply append (quote-list xs))))

    (defun intersect (xs ...)
        (eval (reduce
            (quote-result _intersect)
            (cdr xs)
            '(car xs))))

    (defun difference (a b)
        (cond
            ((nil? a)         '())
            ((has? (car a) b) (difference (cdr a) b))
            (true             (cons (car a) (difference (cdr a) b)))))

    (defun _set (xs ys)
        (cond
            ((nil?  xs) ys)
            ((atom? xs) (if (has? xs ys) ys (cons xs ys)))
            (true       (_set (cdr xs) (_set (car xs) ys)))))
    
    (defun _intersect (a b)
        (cond
            ((nil? a)         '())
            ((has? (car a) b) (cons (car a) (_intersect (cdr a) b)))
            (true             (_intersect (cdr a) b)))))