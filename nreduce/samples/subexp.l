letrec
  loop   (!n.if (= n 0) 0 (loop (- n 1)))
  mklist (!len.!val.if (= len 0) nil (cons val (mklist (- len 1) val)))
in
  mklist 100 (loop 100000)
