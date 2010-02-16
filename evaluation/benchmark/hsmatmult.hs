import System(getArgs)

for :: Int -> Int -> (Int -> a) -> [a]
for i max f = if i < max then
                  (f i) : (for (i + 1) max f)
              else
                  []

mkrow :: Int -> Int -> [Int]
mkrow 0 _ = []
mkrow n val = (val `mod` 23) : (mkrow (n - 1) (val + 1))

mkmatrix :: Int -> Int -> Int -> [[Int]]
mkmatrix _ 0 _ = []
mkmatrix width height val = (mkrow width val) :
                            (mkmatrix width (height - 1) (val + width))

dosum :: Int -> [Int] -> [[Int]] -> Int -> Int
dosum _ [] _ total = total
dosum x aptr bptr total =
    dosum x (tail aptr) (tail bptr)
          (total + ((head aptr) * ((head bptr) !! x)))

multiply :: [[Int]] -> [[Int]] -> [[Int]]
multiply a b =
    let arows = length a
        acols = length (head a)
        bcols = length (head b)
        maxi = acols - 1
    in  for 0 arows (\y ->
          for 0 bcols (\x ->
            dosum x (a !! y) b 0))

pad :: Int -> String
pad n = if n <= 0 then
            ""
        else
            " " ++ (pad (n - 1))

padnum n width = let str = (show n)
                 in (pad (width - (length str))) ++ str

printrow :: [Int] -> String
printrow [] = "\n"
printrow (h:t) = (padnum h 5) ++ " " ++ (printrow t)

printmatrix :: [[Int]] -> String
printmatrix [] = ""
printmatrix (h:t) = (printrow h) ++ (printmatrix t)

matsum :: [[Int]] -> Int
matsum matrix = (sum (map sum matrix))


main2 :: [String] -> String
main2 args = let size = (if (length args) > 0 then
                             (read (head args))
                         else
                             10)
                 print = ((length args) > 1)
                 a = mkmatrix size size 1
                 b = mkmatrix size size 2
                 c = multiply a b
              in if (length args) == 0 then
                     (printmatrix a) ++ "--\n" ++
                     (printmatrix b) ++ "--\n" ++
                     (printmatrix c) ++ "\nsum = "
                     ++ (show (matsum c)) ++ "\n"
                 else
                     "sum = " ++ (show (matsum c)) ++ "\n"

main = do
  args <- getArgs
  putStr (main2 args)
