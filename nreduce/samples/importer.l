import importee

testfun a b = nil /* NOT the version we want to use */

map f lst =
(if lst
    (cons (f (head lst))
          (map f (tail lst)))
    nil)

flatten lst =
(if lst
    (append (head lst)
            (flatten (tail lst)))
    nil)

main =
(flatten (map importee:f (cons 1 (cons 2 (cons 3 nil)))))
