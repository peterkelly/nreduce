public class Pi
{
    public static void main(String[] args)
    {
        if (args.length == 0) {
            System.err.println("Usage: Pi <N>");
            System.exit(-1);
        }

        double N = (double)Integer.parseInt(args[0]);

        double pi = 0.0;
        for (int i = 1; i <= N; i++) {
            double k = (double)i;
            pi += 4.0*N/(Math.pow(N,2)+Math.pow(k-0.5,2));
            System.out.println("pi = "+pi);
        }
    }
}
