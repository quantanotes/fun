|#
This is just a brainstorm of how I want to create typed expressions in
Lisp.

Types are essentially predicates over sets hence boolean expressions apply as you would expect.

() Symbol expressions
[] Type expressions
{} Key value expressions {:age 10 :name 30}
as: coercsion
$: struct/record type
=>: lambda type
of: generics
(# ... ) array definition
#|

(begin
    ; or denotes sum type
    (deftype Number [or Integer
                        Real
                        Complex])

    ; $ denotes struct type
    (deftype Person [$ (name String)
                       (age Integer)
                       (friends [Array [Ref Person]])])

    ; . for accessing and .? for accessing nullable
    (let
        ([or Person nil] human { "bob" 13 (#) }) ; (bob has no friens)
        (.? human age))

    (deftype Nullable [intersects [or nil void false]])

    (declare add [=> (Number Number) Number])

    (deftype OneOrMore [or x (x x) (x nil)])
    (deftype Generic [=> [OneOrMore Type] Type])

    ; => denotes lambda type
    (defun add
        [=> Integer Integer Integer]
        (x y) 
        (+ x y)))

    (defun add
        [=> (Number Number) Number]
        ...)


    (with gpu
        (def x
            [Tensor (50 50)]
            (* (# 0) (50 50)))
        (def y
            [Tensor (10 10)]
            (* (# 0) (10 10)))
        (dot x y))

    ; coersion
    (as Float 3)

    ; define types over domain of function
    (deftype Addable [or (car (args add)) (cdr (args add))])

    (deftype Rational [of Pair [Integer (and Integer (neq 0))]])