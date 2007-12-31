import java.util.*;

public class mergesort_inplace
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

  static void mergesort(String[] arr, String[] aux, int lo, int hi)
  {
    if (lo >= hi)
      return;

    int mid = lo+(hi+1-lo)/2;
    mergesort(arr,aux,lo,mid-1);
    mergesort(arr,aux,mid,hi);

    int xlen = mid-lo;
    int ylen = hi+1-mid;
    int pos = 0;
    int x = lo;
    int y = mid;
    while ((x <= mid-1) && (y <= hi)) {
      String xval = arr[x];
      String yval = arr[y];
      if (xval.compareTo(yval) < 0) {
        aux[pos++] = xval;
        x++;
      }
      else {
        aux[pos++] = yval;
        y++;
      }
    }
    if (x <= mid-1) {
      System.arraycopy(arr,x,aux,pos,mid-x);
      pos += mid-x;
    }
    if (y <= hi) {
      System.arraycopy(arr,y,aux,pos,hi+1-y);
      pos += hi+1-y;
    }
    System.arraycopy(aux,0,arr,lo,hi+1-lo);
  }

  public static void main(String[] args)
    throws Exception
  {
    int n = 1000;
    if (args.length > 0)
      n = Integer.parseInt(args[0]);
    String[] strings = genlist(n);
    String[] temp = new String[strings.length];
    mergesort(strings,temp,0,strings.length-1);
    for (String s : strings)
      System.out.println(s);
  }

}
