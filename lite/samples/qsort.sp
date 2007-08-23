function read_lines1(stream,start,!count)
{
  if (stream) {
    let c = head(stream);
    let rest = tail(stream);
    if (c == '\n')
      cons(prefix(count,start),
           read_lines1(rest,rest,0));
    else
      read_lines1(rest,start,count+1);
  }
  else if (count > 0)
    cons(prefix(count,start),nil);
}

function read_lines(stream)
{
  read_lines1(stream,stream,0);
}

function printstrings(lst)
{
  if (lst)
    head(lst)."\n".printstrings(tail(lst));
}

function qsort2(cmp,pivot,lst,before,after)
{
  if (lst) {
    let item = head(lst);
    let rest = tail(lst);
    if (cmp(item,pivot) < 0)
      qsort2(cmp,pivot,rest,cons(item,before),after);
    else
      qsort2(cmp,pivot,rest,before,cons(item,after));
  }
  else
    qsort(cmp,before).cons(pivot,qsort(cmp,after));
}

function qsort(cmp,lst)
{
  if (lst) {
    let pivot = head(lst);
    qsort2(cmp,pivot,tail(lst),nil,nil);
  }
}

function main()
{
  let strings = read_lines(readb("strings"));
  let sorted = qsort(strcmp,strings);
  printstrings(sorted);
}
