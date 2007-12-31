import java.util.*;

public class quicksort_inplace
{
  static String[] genlist(int max)
  {
    String[] res = new String[max];
    for (int i = 0; i < max; i++) {
      char[] chars = ((7*i)+"").toCharArray();
      char[] reversed = new char[chars.length];
      for (int c = 0; c < chars.length; c++)
        reversed[chars.length-1-c] = chars[c];
      res[i] = new String(reversed);
    }
    return res;
  }

  static void quicksort(String[] arr, int left, int right)
  {
    int store = left;
    String pivot = arr[right];
    for (int i = left; i < right; i++) {
      if (arr[i].compareTo(pivot) < 0) {
        String temp = arr[store];
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

  public static void main(String[] args)
    throws Exception
  {
    int n = 1000;
    if (args.length > 0)
      n = Integer.parseInt(args[0]);
    String[] strings = genlist(n);
    quicksort(strings,0,strings.length-1);
    for (String s : strings)
      System.out.println(s);
  }
}
