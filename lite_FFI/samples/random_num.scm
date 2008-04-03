main = (map (!x.(cons (numtostring x) " ")) str)
       
str = (cons (randomnum 4) (cons (randomnum 3) (cons (randomnum 3) nil)))