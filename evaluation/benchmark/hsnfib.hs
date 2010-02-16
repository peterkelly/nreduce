import System(getArgs)

nfib :: Int -> Int
nfib n =
    if n <= 1 then
        1
    else
        (nfib (n - 2)) + (nfib (n - 1))


loop :: Int -> Int -> String
loop n max =
    if n > max then
       ""
    else
        "nfib(" ++ (show n) ++ ") = " ++
        (show (nfib n)) ++ "\n" ++ (loop (n + 1) max)


main2 n = (loop 0 n)

main = do
  args <- getArgs
  putStr (let n = if (length args) == 0 then
                      24
                  else
                      ((read (head args)) :: Int)
          in main2 n)
