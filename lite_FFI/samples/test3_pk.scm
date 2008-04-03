main = (numtostring (foo 3 4 5))

foo a b c = (if (> a b) (* c a) (* c b))
