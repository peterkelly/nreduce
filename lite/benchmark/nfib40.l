nfib n =
(if (<= n 1)
    1
    (+ (nfib (- n 2)) (nfib (- n 1))))

loop n max =
(if (> n max)
    nil
    (append "nfib("
    (append (numtostring n)
    (append ") = "
    (append (numtostring (nfib n))
    (append "\n"
            (loop (+ n 1) max)))))))

main = (loop 0 40)
