import System(getArgs)

data Tree a = Leaf
            | Node (Tree a) (Tree a)


mandel :: Double -> Double -> Double -> Double -> Int -> Int
mandel zr zi cr ci res =
    if res == 4096 then
        res
    else
        let
            newzr = (zr * zr) - (zi * zi) + cr
            newzi = (2.0 * (zr * zi)) + ci
            mag = sqrt ((newzr * newzr) + (newzi * newzi))
        in  if mag > 2.0 then
                res
            else
                mandel newzr newzi cr ci (res + 1)

mandel0 :: Double -> Double -> Int
mandel0 cr ci = mandel 0.0 0.0 cr ci 0

printcell :: Int -> String
printcell num = if num > 40 then "  "
                else if num > 30 then "''"
                else if num > 20 then "--"
                else if num > 10 then "**"
                else "##"

mloop :: Double -> Double -> Double -> Double -> Double -> Double -> Double -> String
mloop minx maxx miny maxy incr x y =
    if x >= maxx then
        if (y + incr) >= maxy then
           "\n"
        else
            "\n" ++ (mloop minx maxx miny maxy incr minx (y + incr))
    else
        (printcell (mandel0 x y)) ++ (mloop minx maxx miny maxy incr (x + incr) y)

mandelrange :: Double -> Double -> Double -> Double -> Double -> String
mandelrange minx maxx miny maxy incr = mloop minx maxx miny maxy incr minx miny

main2 :: Int -> String
main2 n = let incr = 2 / (fromIntegral n)
          in (mandelrange (-1.5) 0.5 (-1.0) 1.0 incr)

main = do
  args <- getArgs
  putStr (let n = if (length args) == 0 then
                      32
                  else
                      ((read (head args)) :: Int)
          in main2 n)
