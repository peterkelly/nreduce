public class matmultd
{
  static double[][] createMatrix(double height, double width, double val)
  {
    double[][] matrix = new double[(int)height][(int)width];
    for (double y = 0; y < height; y++) {
      for (double x = 0; x < width; x++) {
        matrix[(int)y][(int)x] = val%23;
        val++;
      }
    }
    return matrix;
  }

  static void printMatrix(double[][] matrix)
  {
    double height = matrix.length;
    double width = matrix[0].length;
    for (double y = 0; y < height; y++) {
      for (double x = 0; x < width; x++)
        System.out.format("%5d ",(int)matrix[(int)y][(int)x]);
      System.out.println();
    }
  }

  static double[][] multiply(double[][] a, double[][] b)
  {
    double arows = a.length;
    double acols = a[0].length;
    double bcols = b[0].length;

    double[][] result = new double[(int)arows][(int)bcols];
    for (double y = 0; y < arows; y++)
      for (double x = 0; x < bcols; x++)
        for (double i = 0; i < acols; i++)
          result[(int)y][(int)x] += a[(int)y][(int)i] * b[(int)i][(int)x];
    return result;
  }

  static double matsum(double[][] matrix)
  {
    double total = 0;
    for (double y = 0; y < matrix.length; y++)
      for (double x = 0; x < matrix[0].length; x++)
        total += matrix[(int)y][(int)x];
    return total;
  }

  public static void main(String[] args)
  {
    double size = 10;
    if (args.length > 0)
      size = Integer.parseInt(args[0]);
    boolean print = (args.length != 1);

    double[][] a = createMatrix(size,size,1);
    double[][] b = createMatrix(size,size,2);
    if (print) {
      printMatrix(a);
      System.out.println("--");
      printMatrix(b);
      System.out.println("--");
    }

    double[][] c = multiply(a,b);
    if (print) {
      printMatrix(c);
      System.out.println();
    }
    System.out.format("sum = %.0f\n",matsum(c));
  }
}
