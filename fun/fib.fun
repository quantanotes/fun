(begin
    (defun fib (n)
        (fib-tail n 1 0))

    (defun fib-tail (n a b)
        (cond
            ((= n 0) b)
            (true (fib-tail (- n 1) (+ a b) a))))

    (fib 5000))