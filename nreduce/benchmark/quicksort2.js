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

function quicksort(arr,left,right)
{
  var store = left;
  var pivot = arr[right];
  for (var i = left; i < right; i++) {
    if (arr[i] < pivot) {
      var temp = arr[store];
      arr[store] = arr[i];
      arr[i] = temp;
      store++;
    }
  }
  arr[right] = arr[store];
  arr[store] = pivot;

  if (left < store-1)
    quicksort(arr,left,store-1);
  if (right > store+1)
    quicksort(arr,store+1,right);
}

var n = 1000;
if (arguments.length > 0)
  n = arguments[0];
var strings = genlist(n);
quicksort(strings,0,strings.length-1);
for (var i = 0; i < strings.length; i++)
  print(strings[i]);
