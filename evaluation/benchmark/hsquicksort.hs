import System(getArgs)


genlist :: Int -> Int -> [String]
genlist i max = if i >= max then
                    []
                else
                    (reverse (show (7 * i))) : (genlist (i + 1) max)

flatten :: [String] -> String
flatten [] = []
flatten (h:t) = h ++ "\n" ++ (flatten t)

quicksort2 :: String -> [String] -> [String] -> [String] -> [String]
quicksort2 pivot (item:rest) before after =
    if item < pivot then
        quicksort2 pivot rest (item:before) after
    else
        quicksort2 pivot rest before (item:after)
quicksort2 pivot [] before after =
    (quicksort before) ++ [pivot] ++ (quicksort after)

quicksort :: [String] -> [String]
quicksort [] = []
quicksort (h:t) = quicksort2 h t [] []

main2 args = let n = if args /= [] then
                         (read (head args))
                     else
                         1000
                 input = genlist 0 n
                 sorted = quicksort input
             in  flatten sorted

main = do
  args <- getArgs
  putStr (main2 args)
