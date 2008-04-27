main = (printstrings (filteItems 3 string2))

main1 = (numtostring (item 1 string1))
        
string1 =
        (cons 1 (cons 4 nil))
        
string2 =
            (cons 10
            (cons 3
            (cons 8
            (cons 10
            (cons 66
            (cons 1
            (cons 33
            (cons 7 nil))))))))

string3 =
        (cons nil (cons 3 nil))


filteItems pivot numbers =
//                           (filter (!x.== x pivot) numbers)
                           (filter (!x.> x pivot) numbers)
 
        
printstrings str =
                  (if str
                     (map (!x.(cons (numtostring x) " ")) str)
                     nil)

selectLItems pivot numbers =
                           (if (>= 1 (len numbers))
                               (if (>= pivot (item 0 numbers))
                                     nil
                                     (cons (item 0 numbers) nil))
                               (letrec firstItem = (item 0 numbers)
                                 in
                                 (if (>= pivot firstItem)
                                     (selectLItems pivot (skip 1 numbers))
                                     (cons firstItem (selectLItems pivot (skip 1 numbers))))))
                  
                           
selectSItems pivot numbers = 
                           (if (>= 1 (len numbers)) 
                               (if (<= pivot (item 0 numbers))
                                   nil
                                   (cons (item 0 numbers) nil))
                               (if (<= pivot (item 0 numbers))
                                   (selectSItems pivot (skip 1 numbers))
                                   (cons (item 0 numbers) (selectSItems pivot (skip 1 numbers)))))
                           
                           
selectEItems pivot numbers =
                            (if (>= 1 (len numbers))
                                (if (== pivot (item 0 numbers))
                                    (cons (item 0 numbers) nil)
                                     nil)
                                (letrec firstItem = (item 0 numbers)
                                  in
                                      (if (== pivot firstItem)
                                           (cons firstItem (selectEItems pivot (skip 1 numbers)))
                                           (selectEItems pivot (skip 1 numbers)))))
                           