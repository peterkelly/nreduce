import System
import System.Directory
import System.IO.Unsafe
import Data.Char

data Posting = Posting Int Int String
data Hit = Hit String Int Int
data FileId = FileId String Int
data WordInfo = WordInfo String [Hit]

apmap :: (a -> [b]) -> [a] -> [b]
apmap f lst = foldr (++) [] (map f lst)

hash :: String -> Int -> Int
hash [] val = rem val 256
hash str val = hash (tail str) ((ord (head str)) + val)

tokenize1 :: String -> String -> Int -> [String]
tokenize1 [] _ _ = []
tokenize1 str start count =
  let c = head str
  in if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))) then
       (tokenize1 (tail str) start (count + 1))
     else if (count < 2) then
       (tokenize (tail str))
     else
       (take count start):(tokenize (tail str))

tokenize :: String -> [String]
tokenize str = tokenize1 str str 0

getpostings :: String -> FileId -> [Posting]
getpostings path (FileId filename id) =
  let content = (unsafePerformIO (readFile (path ++ filename)))
      tokens = (tokenize content)
  in  (map (\token -> Posting (hash token 0) id token) tokens)

wordsort :: [Posting] -> [Posting]
wordsort lst =
  mergesort (\(Posting _ _ a)  -> \(Posting _ _ b) -> compare a b) lst

wrap :: [a] -> [[a]]
wrap [] = []
wrap (x:xs) = [x]:(wrap xs)

mergesort :: (a -> a -> Ordering) -> [a] -> [a]
mergesort cmp lst = mergesort2 cmp (wrap lst)

mergesort2 _ [] = []
mergesort2 cmp (x:[]) = x
mergesort2 cmp (x:xs) = mergesort2 cmp (merge_pairs cmp (x:xs))

merge_pairs ::  (a -> a -> Ordering) -> [[a]] -> [[a]]
merge_pairs _ [] = []
merge_pairs cmp (xs:ys:xss) =
  (merge cmp xs ys):(merge_pairs cmp xss)
merge_pairs cmp xss = xss

merge :: (a -> a -> Ordering) -> [a] -> [a] -> [a]
merge cmp [] ylst = ylst
merge cmp xlst [] = xlst
merge cmp xlst ylst =
 case xlst of
   (x:xs) ->
     case ylst of
       (y:ys) ->
         if ((cmp x y) == GT) then
           y:(merge cmp xlst ys)
         else
           x:(merge cmp xs ylst)

hitcol1 :: Int -> [Posting] -> String -> Int -> [Hit]
hitcol1 docid [] prevword count = [Hit prevword count docid]
hitcol1 docid (x:xs) prevword count =
  case x of
    (Posting _ curdocid curword) ->
      if (curword == prevword) then
        (hitcol1 docid xs curword (count + 1))
      else
        (Hit prevword count docid):(hitcol1 docid xs curword 1)

hitcol :: [Posting] -> [Hit]
hitcol [] = []
hitcol postings =
  case (head postings) of
    (Posting _ docid word) ->
      hitcol1 docid postings word 1

gethits :: [Posting] -> [Hit]
gethits postings = hitcol postings

split :: Int -> [Posting] -> [Posting] -> [Posting] -> ([Posting],[Posting])
split _ [] lt ge = (lt,ge)
split n (x:xs) lt ge =
  case x of
    (Posting hash _ _) ->
      if hash < n then
        split n xs (x:lt) ge
      else
        split n xs lt (x:ge)

mkhashtable1 :: [Posting] -> Int -> Int -> [[Hit]]
mkhashtable1 tokens lo hi =
  if (lo == hi) then
    (gethits (wordsort tokens)):[]
  else if ((hi - lo) == 1) then
    let (lt,ge) = split hi tokens [] []
    in (mkhashtable1 lt lo lo) ++ (mkhashtable1 ge hi hi)
  else
    let mid = ((floor ((fromIntegral (lo + hi)) / 2)) :: Int)
        (lt,ge) = split mid tokens [] []
    in  (mkhashtable1 lt lo (mid - 1)) ++ (mkhashtable1 ge mid hi)

mkhashtable :: [Posting] -> [[Hit]]
mkhashtable tokens = mkhashtable1 tokens 0 255

idfiles :: [String] -> Int -> [FileId]
idfiles (x:xs) id = (FileId x id):(idfiles xs (id + 1))
idfiles [] _ = []

collate1 :: [Hit] -> String -> [Hit] -> [WordInfo]
collate1 (hit:rest) curword matched =
  case hit of
    (Hit word _ _) ->
      if (word == curword) then
        collate1 rest curword (hit:matched)
      else
        (WordInfo curword matched):(collate1 rest word [hit])

collate1 [] curword matched = [(WordInfo curword matched)]

collate :: [Hit] -> [WordInfo]
collate [] = []
collate (hit:rest) =
  case hit of
    (Hit word _ _) ->
      collate1 rest word [hit]

mergehash :: [[[Hit]]] -> Int -> [[Hit]]
mergehash hashtables n =
  if (n >= 256) then
    []
  else
    (foldr (++) [] (map head hashtables)):(mergehash (map tail hashtables) (n + 1))

showhits :: [Hit] -> String
showhits [] = []
showhits ((Hit word count docid):hits) =
  word ++ " x " ++ (show count) ++ " in " ++ (show docid) ++ "\n" ++
  (showhits hits)

showhashtable1 :: [[Hit]] -> Int -> String
showhashtable1 [] _ = []
showhashtable1 (bin:bins) n =
  "hash " ++ (show n) ++ ":\n" ++ (showhits bin) ++ (showhashtable1 bins (n + 1))

showhashtable ht = showhashtable1 ht 0

showhits2 :: [Hit] -> String
showhits2 [] = []
showhits2 ((Hit word count docid):rest) =
  " " ++ (show docid) ++ "(" ++ (show count) ++ ")" ++ (showhits2 rest)

showcollated :: [WordInfo] -> String
showcollated [] = []
showcollated ((WordInfo word hits):rest) =
  word ++ (showhits2 hits) ++ "\n" ++ (showcollated rest)

showfileids :: [FileId] -> String
showfileids lst = apmap (\(FileId filename id) ->
  filename ++ " - " ++ (show id) ++ "\n") lst

showposting :: Posting -> String
showposting (Posting hash docid word) = (show hash) ++ " - " ++ (show docid) ++ " - " ++ word ++ "\n"

showdocpostings :: [Posting] -> String
showdocpostings postings = apmap showposting postings

showallpostings :: [[Posting]] -> String
showallpostings lst = apmap showdocpostings lst

hitwordsort2 :: Hit -> [Hit] -> [Hit] -> [Hit] -> [Hit]
hitwordsort2 pivot [] before after =
  (hitwordsort before) ++ pivot:(hitwordsort after)
hitwordsort2 pivot (it:rest) before after =
  case it of
    (Hit word count docid) ->
      case pivot of
        (Hit pivword _ _) ->
          if ((compare word pivword) == LT) then
            hitwordsort2 pivot rest (it:before) after
          else
            hitwordsort2 pivot rest before (it:after)

hitwordsort :: [Hit] -> [Hit]
hitwordsort [] = []
hitwordsort (pivot:rest) = hitwordsort2 pivot rest [] []

indexpath :: String -> IO ()
indexpath path = do
  contents <- getDirectoryContents path
  files <- return $ (mergesort compare $ 
         filter (\x -> (unsafePerformIO (doesFileExist (path ++ x)))) contents)
--  putStr $ foldr (++) [] $ map (\x -> x ++ "\n") files
--  putStrLn "file ids: "
  fileids <- return $ idfiles files 0
  putStr (showfileids fileids)

  postings <- return $ map (\fid -> getpostings path fid) fileids
--  putStrLn "Postings"
--  putStr (showallpostings postings)
  hashtables <- return $ map mkhashtable postings
--  putStr (apmap showhashtable hashtables)
  merged <- return $ mergehash hashtables 0
  sorted <- return $ map hitwordsort merged
--  putStr (showhashtable sorted)
  collated <- return $ map collate sorted
  putStr (apmap showcollated collated)

main = do
  args <- getArgs
  case args of
    [path] -> indexpath path
    _ ->
      (putStrLn "Please specify directory to index (including /)")
