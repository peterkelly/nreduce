import System(getArgs)

data Tree a = Leaf
            | Node (Tree a) (Tree a)

mktree :: Int -> Tree Int
mktree 0 = Leaf
mktree n = Node (mktree (n - 1)) (mktree (n - 1))

countnodes :: Tree a -> Int
countnodes Leaf = 0
countnodes (Node left right) = 1 + (countnodes left) + (countnodes right)

main2 n = (show (countnodes (mktree n)))

main = do
  args <- getArgs
  putStrLn (let n = if (length args) == 0 then
                        16
                    else
                        ((read (head args)) :: Int)
            in main2 n)
