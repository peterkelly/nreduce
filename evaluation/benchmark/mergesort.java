import java.util.*;

public class mergesort
{
  static List<String> genlist(int max)
  {
    List<String> res = new ArrayList<String>();
    for (int i = 0; i < max; i++) {
      char[] chars = ((7*i)+"").toCharArray();
      char[] reversed = new char[chars.length];
      for (int c = 0; c < chars.length; c++)
        reversed[chars.length-1-c] = chars[c];
      res.add(new String(reversed));
    }
    return res;
  }

  static void printList(List<String> list)
  {
    for (String s : list)
      System.out.println(s);
  }

  static List<String> mergesort(List<String> m)
  {
    if (m.size() <= 1)
      return m;

    int middle = m.size()/2;
    List<String> left = m.subList(0,middle);
    List<String> right = m.subList(middle,m.size());

    left = mergesort(left);
    right = mergesort(right);

    return merge(left,right);
  }

  static List<String> merge(List<String> xlst, List<String> ylst)
  {
    int x = 0;
    int y = 0;
    List<String> res = new ArrayList<String>();
    while ((x < xlst.size()) && (y < ylst.size())) {
      String xval = xlst.get(x);
      String yval = ylst.get(y);
      if (xval.compareTo(yval) < 0) {
        res.add(xval);
        x++;
      }
      else {
        res.add(yval);
        y++;
      }
    }
    if (x < xlst.size())
      res.addAll(xlst.subList(x,xlst.size()));
    if (y < ylst.size())
      res.addAll(ylst.subList(y,ylst.size()));
    return res;
  }

  public static void main(String[] args)
  {
    int n = 1000;
    if (args.length > 0)
      n = Integer.parseInt(args[0]);
    List<String> input = genlist(n);
    List<String> sorted = mergesort(input);
    printList(sorted);
  }

}
