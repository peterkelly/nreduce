import java.util.*;

public class quicksort
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

  static List<String> quicksort(List<String> list)
  {
    String pivot = list.get(0);
    List<String> before = new ArrayList<String>();
    List<String> after = new ArrayList<String>();
    for (int pos = 1; pos < list.size(); pos++) {
      String cur = list.get(pos);
      if (cur.compareTo(pivot) < 0)
        before.add(cur);
      else
        after.add(cur);
    }
    List<String> res = new ArrayList<String>();
    if (!before.isEmpty())
      res.addAll(quicksort(before));
    res.add(pivot);
    if (!after.isEmpty())
      res.addAll(quicksort(after));
    return res;
  }

  public static void main(String[] args)
  {
    int n = 1000;
    if (args.length > 0)
      n = Integer.parseInt(args[0]);
    List<String> input = genlist(n);
    List<String> sorted = quicksort(input);
    printList(sorted);
  }
}
