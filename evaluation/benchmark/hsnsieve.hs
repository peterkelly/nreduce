import System(getArgs)


merge :: Int -> Int -> [Int] -> [Int]
merge gn x ys = let y = head ys
                in  if x < y then
                        x:(merge gn (x + gn) ys)
                    else if x == y then
                             x:(merge gn (x + gn) (tail ys))
                         else y:(merge gn x (tail ys))

xfoldr1 :: [Int] -> [Int]
xfoldr1 lst = let p = head lst
                  gn = 2 * p
                  gp = p * p
              in  gp:(merge gn (gp + gn) (xfoldr1 (tail lst)))

diff :: Int -> [Int] -> [Int]
diff n ys = let y = head ys
            in   if n < y then
                     n:(diff (n + 2) ys)
                 else if n == y then
                          diff (n + 2) (tail ys)
                      else
                          diff n (tail ys)

primes :: [Int]
primes = 2:3:5:(diff 7 nonprimes)

nonprimes :: [Int]
nonprimes = xfoldr1 (tail primes)

listmult :: Int -> Int -> [Int]
listmult n p = p:(listmult n (p + n))

printnums :: [Int] -> String
printnums [] = ""
printnums (h:t) = (show h) ++ "\n" ++ (printnums t)

nupto :: Int -> [Int] -> Int -> Int
nupto n primes count = if (head primes) > n then
                           count
                       else
                           nupto n (tail primes) (count + 1)

showprimes :: Int -> Int -> String
showprimes n count = "Primes up to " ++ (show n) ++ ": " ++ (show count) ++ "\n"

main2 args = let n = if args /= [] then
                         (read (head args))
                     else
                         10000
                 count = nupto n primes 0
             in  showprimes n count

main = do
  args <- getArgs
  putStr (main2 args)
