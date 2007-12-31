public class matmulti
{
  static int[][] createMatrix(int height, int width, int val)
  {
    int[][] matrix = new int[height][width];
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        matrix[y][x] = val%23;
        val++;
      }
    }
    return matrix;
  }

  static void printMatrix(int[][] matrix)
  {
    int height = matrix.length;
    int width = matrix[0].length;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++)
        System.out.format("%5d ",matrix[y][x]);
      System.out.println();
    }
  }

  static int[][] multiply(int[][] a, int[][] b)
  {
    int arows = a.length;
    int acols = a[0].length;
    int bcols = b[0].length;

    int[][] result = new int[arows][bcols];
    for (int y = 0; y < arows; y++)
      for (int x = 0; x < bcols; x++)
        for (int i = 0; i < acols; i++)
          result[y][x] += a[y][i] * b[i][x];
    return result;
  }

  static int matsum(int[][] matrix)
  {
    int total = 0;
    for (int y = 0; y < matrix.length; y++)
      for (int x = 0; x < matrix[0].length; x++)
        total += matrix[y][x];
    return total;
  }

  public static void main(String[] args)
  {
    int size = 10;
    if (args.length > 0)
      size = Integer.parseInt(args[0]);
    boolean print = (args.length != 1);

    int[][] a = createMatrix(size,size,1);
    int[][] b = createMatrix(size,size,2);
    if (print) {
      printMatrix(a);
      System.out.println("--");
      printMatrix(b);
      System.out.println("--");
    }

    int[][] c = multiply(a,b);
    if (print) {
      printMatrix(c);
      System.out.println();
    }
    System.out.println("sum = "+matsum(c));
  }
}
