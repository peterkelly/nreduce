(__printer (lst) (if lst
                 (if (valarray? lst)
                     (letrec ((partlen (arraysize lst)))
                             (seq (printarray partlen lst) (__printer (arrayskip partlen lst))))
                     (seq (print (head lst)) (__printer (tail lst))))
                     nil))
(__sparklist (lst) (if lst
                     (parhead lst (__sparklist (tail lst)))
                     nil))
(__spark (val) (seq (__sparklist val) val))
(__start () (__printer main))

(streamfd   (fd)        (readchunk fd (streamfd fd)))

(readb      (filename)  (streamfd (openfd filename)))

(stdin () (streamfd 0))

(readt      (filename)  (readb filename))

(len1       (total lst) (if lst
                            (letrec ((partlen (arraysize lst)))
                                     (len1 (+ total partlen) (arrayskip partlen lst)))
                            total))

(len        (lst)       (len1 0 lst))


(skip       (n lst)     (letrec ((partlen (arraysize lst)))
                                (if (< n partlen)
                                    (arrayskip n lst)
                                (skip (- n partlen) (arrayskip partlen lst)))))

(item       (n lst)     (letrec ((partlen (arraysize lst)))
                                (if (< n partlen)
                                    (head (arrayskip n lst))
                                (item (- n partlen) (arrayskip partlen lst)))))

(prefix     (n lst)     (if (<= n 0)
                            nil
                           (letrec ((partlen (arraysize lst)))
                           (if (<= n partlen)
                               (arrayprefix n lst nil)
                               (arrayprefix partlen lst
                                            (prefix (- n partlen) (arrayskip partlen lst)))))))

(sub (start count lst)  (prefix count (skip start lst)))

(append     (a b)     (if a
                          (letrec ((partlen (arraysize a)))
                                  (arrayprefix partlen a (append (arrayskip partlen a) b)))
                          b))
