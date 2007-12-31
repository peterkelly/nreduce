function genlist(max)
{
  var res = new Array(max);
  for (var i = 0; i < max; i++) {
    var chars = (7*i)+"";
    var rev = "";
    for (var c = 0; c < chars.length; c++)
      rev += chars[chars.length-1-c];
    res[i] = rev;
  }
  return res;
}

function quicksort(list)
{
  var pivot = list[0];
  var before = new Array();
  var after = new Array();
  for (var pos = 1; pos < list.length; pos++) {
    var cur = list[pos];
    if (cur < pivot)
      before.push(cur);
    else
      after.push(cur);
  }
  var res = new Array();
  if (before.length > 0)
    res = res.concat(quicksort(before));
  res.push(pivot);
  if (after.length > 0)
    res = res.concat(quicksort(after));
  return res;
}

var n = 1000;
if (arguments.length > 0)
  n = arguments[0];
var strings = genlist(n);
var sorted = quicksort(strings);
for (var i = 0; i < sorted.length; i++)
  print(sorted[i]);
