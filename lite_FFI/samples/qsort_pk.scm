read_lines1 stream start !count =
(if stream
  (letrec
    c = (head stream)
    rest = (tail stream)
   in
    (if (== c '\n')
      (cons
        (prefix count start)
        (read_lines1 rest rest 0))
      (read_lines1 rest start (+ count 1))))
  (if (> count 0)
    (cons (prefix count start) nil)
    nil))

read_lines stream = (read_lines1 stream stream 0)

printstrings lst =
(if lst
  (append (head lst)
    (cons '\n'
      (printstrings (tail lst))))
  nil)


qsort2 cmp pivot lst before after =
(if lst
  (letrec
    item = (head lst)
    rest = (tail lst)
   in
    (if (< (cmp item pivot) 0)
      (qsort2 cmp pivot rest (cons item before) after)
      (qsort2 cmp pivot rest before (cons item after))))
  (append (qsort cmp before) (cons pivot (qsort cmp after))))

qsort cmp lst =
(if lst
  (letrec
    pivot = (head lst)
   in
    (qsort2 cmp pivot (tail lst) nil nil))
  nil)

main =
(letrec
  strings = (cons "Elephant"
            (cons "Door"
            (cons "Banana"
            (cons "Abacus"
            (cons "Castle"
            (cons "Gasp"
            (cons "Friend" nil)))))))
  sorted = (qsort strcmp strings)
 in
  (printstrings sorted))
