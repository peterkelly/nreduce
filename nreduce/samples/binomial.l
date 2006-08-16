(binom (n k) (if (and (= k 0) (>= n 0)) 1
             (if (and (< n k) (>= n 0)) 0
             (if (and (>= n k) (>= k 0))
                 (+ (binom (- n 1) k) (binom (- n 1) (- k 1)))
                 nil))))

(main () (binom 10 4))
