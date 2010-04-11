import System(getArgs)
import Array

pad :: Int -> String
pad n = if n <= 0 then
            ""
        else
            " " ++ (pad (n - 1))

padnum n width = let str = (show (floor n))
                 in (pad (width - (length str))) ++ str


mkmatrix :: Int -> Int -> Int -> Array (Int,Int) Double
mkmatrix height width val =
    array ((0,0),(height-1,width-1))
              [((row,col),fromIntegral $ (val+row*width+col) `mod` 23)
                   | row <- [0..height-1],
                     col <- [0..width-1]]

getrows :: Array (Int,Int) Double -> Int
getrows matrix = (fst (snd (bounds matrix))) + 1

getcols :: Array (Int,Int) Double -> Int
getcols matrix = (snd (snd (bounds matrix))) + 1

printmatrix :: Array (Int,Int) Double -> String
printmatrix matrix =
    let nrows = getrows matrix
        ncols = getcols matrix
        rec = (\row col ->
                   if row >= nrows then
                       ""
                   else if col >= ncols then
                            "\n" ++ (rec (row+1) 0)
                   else
                       (padnum (matrix ! (row,col)) 5) ++ " " ++
                       (rec row (col+1)))
    in  rec 0 0

multiply :: Array (Int,Int) Double -> Array (Int,Int) Double -> Array (Int,Int) Double
multiply a b =
  let arows = (getrows a)
      acols = (getcols a)
      bcols = (getcols b)
  in  array ((0,0),(arows-1,bcols-1))
      [((y,x),sum [a!(y,i) * b!(i,x) | i <- [0..acols-1]])
       | y <- [0..arows-1],
         x <- [0..bcols-1]]

matsum :: Array (Int,Int) Double -> Double
matsum matrix =
    sum [matrix!(row,col)
             | row <- [0..(getrows matrix)-1],
               col <- [0..(getcols matrix)-1]]

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
                     ++ (show (floor (matsum c))) ++ "\n"
                 else
                     "sum = " ++ (show (floor (matsum c))) ++ "\n"

main = do
  args <- getArgs
  putStr (main2 args)
