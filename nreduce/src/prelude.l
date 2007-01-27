__printer1 partlen lst = (__printer (arrayskip partlen lst))

__printer lst = (if lst
                 (if (valarray? lst)
                     (letrec partlen = (arraysize lst) in
                             (seq (printarray partlen lst) (__printer1 partlen lst)))
                     (seq (print (head lst)) (__printer (tail lst))))
                     nil)

sparklist lst = (if lst
                     (parhead lst (sparklist (tail lst)))
                     nil)

spark val = (seq (sparklist val) val)
__start = (__printer main)

streamfd fd =  (readchunk fd (streamfd fd))

readb filename = (streamfd (openfd filename))

stdin = (streamfd 0)

readt filename = (readb filename)

len1 !total lst = (if lst
                     (letrec partlen = (arraysize lst) in
                              (len1 (+ total partlen) (arrayskip partlen lst)))
                              total)

len lst = (len1 0 lst)


skip n lst = (letrec partlen = (arraysize lst) in
                     (if (< n partlen)
                         (arrayskip n lst)
                     (skip (- n partlen) (arrayskip partlen lst))))

item n lst = (letrec partlen = (arraysize lst) in
                     (if (< n partlen)
                         (head (arrayskip n lst))
                     (item (- n partlen) (arrayskip partlen lst))))

prefix n lst = (if (<= n 0)
                   nil
               (letrec partlen = (arraysize lst) in
                       (if (<= n partlen)
                           (arrayprefix n lst nil)
                       (arrayprefix partlen lst
                                    (prefix (- n partlen) (arrayskip partlen lst))))))

sub !start !count lst = (prefix count (skip start lst))

append a b = (if a
                 (letrec partlen = (arraysize a) in
                          (arrayprefix partlen a (append (arrayskip partlen a) b)))
                 b)



appendn n a b = (if (= n 2)
                    (append a b)
                    (appendn (- n 1) (append a b)))

append1 a b = (append a (cons b nil))

listn n a = (if (= n 1)
                (cons a nil)
                (consn n (cons a nil)))

consn n a b = (if (= n 2)
                  (append1 a b)
                  (consn (- n 1) (append1 a b)))

streq a b = (if (and a b)
                (if (= (head a) (head b))
                    (streq (tail a) (tail b))
                    nil)
            (if (and (not a) (not b))
                1
                nil))

map f lst =
(if lst
    (cons (f (head lst))
          (map f (tail lst)))
    nil)

filter f lst =
(if lst
    (if (f (head lst))
        (cons (head lst) (filter f (tail lst)))
        (filter f (tail lst)))
    nil)
