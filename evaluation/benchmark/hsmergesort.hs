import System(getArgs)


genlist :: Int -> Int -> [String]
genlist i max = if i >= max then
                    []
                else
                    (reverse (show (7 * i))) : (genlist (i + 1) max)

flatten :: [String] -> String
flatten [] = []
flatten (h:t) = h ++ "\n" ++ (flatten t)

mergesort :: [String] -> [String]
mergesort m = if (length m) <= 1 then
                  m
              else
                  let middle = floor ((fromIntegral (length m)) / 2)
                      left = take middle m
                      right = drop middle m
                      left2 = mergesort left
                      right2 = mergesort right
                  in  merge left2 right2 []

merge :: [String] -> [String] -> [String] -> [String]
merge xlst ylst out = if xlst /= [] then
                          if ylst /= [] then
                              let x = head xlst
                                  y = head ylst
                              in  if x > y then
                                      merge xlst (tail ylst) (y:out)
                                  else
                                      merge (tail xlst) ylst (x:out)
                          else
                              merge (tail xlst) ylst ((head xlst):out)
                      else if ylst /= [] then
                               merge xlst (tail ylst) ((head ylst):out)
                           else
                               reverse out


main2 args = let n = if args /= [] then
                         (read (head args))
                     else
                         1000
                 input = genlist 0 n
                 sorted = mergesort input
             in  flatten sorted

main = do
  args <- getArgs
  putStr (main2 args)
