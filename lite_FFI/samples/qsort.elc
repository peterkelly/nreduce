printstrings str =
                 (if str
                     (map (!x.(cons (numtostring x) " ")) str)
                     nil)

//Return a list containing elements smaller than pivot
selectSItems pivot numbers = 
                           (filter (!x.< x pivot) numbers)

//Return a list containing elements equal pivot
selectEItems pivot numbers =
                           (filter (!x.== x pivot) numbers)

//Return a list containing elements larger than pivot
selectLItems pivot numbers =
                           (filter (!x.> x pivot) numbers)


quick_sort numbers =
                   (if (== 0 (len numbers))
                       nil
                       (if (>= 1 (len numbers))
                       (cons (head numbers) nil)
                       (letrec pivot = (head numbers)
                               leftItems = (selectSItems pivot numbers)
                               equalItems = (selectEItems pivot numbers)
                               rightItems = (selectLItems pivot numbers)
                         in
                               (append (append (quick_sort leftItems) equalItems) (quick_sort rightItems)))))

//quick sort
main =
(letrec
  strings = 
            (cons 10
            (cons 3
            (cons 8
            (cons 10
            (cons 66
            (cons 1
            (cons 33
            (cons 7 nil))))))))
 in
  (printstrings (quick_sort strings)))
