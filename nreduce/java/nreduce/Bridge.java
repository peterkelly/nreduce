package nreduce;

import java.net.ServerSocket;
import java.net.Socket;
import java.net.InetAddress;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.InvocationTargetException;

import java.util.Map;
import java.util.HashMap;
import java.util.List;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Set;
import java.util.Iterator;
import java.util.TreeSet;

public class Bridge
{
    protected Map<Integer,Holder> objects = new HashMap<Integer,Holder>();
    protected Map<Holder,Integer> reverse = new HashMap<Holder,Integer>();
    protected int nextid = 1;

    public Bridge()
    {
    }

    protected Object demarshall(String arg)
        throws CommandException
    {
        char[] chars = arg.toCharArray();
        if ('"' == chars[0]) {
            String value = arg.substring(1,arg.length()-1);
            return Parser.unescape(value);
        }
        else if ('@' == chars[0]) {
            Integer id = Integer.parseInt(arg.substring(1));
            Holder h = objects.get(id);
            if (null == h)
                throw new CommandException("no such object @"+id);
            return h.getObj();
        }
        else if (arg.equals("nil")) {
            return null;
        }
        else {
            Double d = Double.parseDouble(arg);
            return d;
        }
    }

    protected void output(PrintWriter writer, String str)
    {
        System.out.println(str);
        writer.println(str);
    }

    protected void outputError(PrintWriter writer, String errmsg)
    {
        output(writer,"error: "+errmsg);
    }

    protected boolean checkCompatibility(Class[] types, Object[] params)
    {
        if (types.length != params.length)
            return false;
        int ok = 0;
        for (int i = 0; i < types.length; i++) {
            if ((Boolean.TYPE == types[i]) || (Boolean.class == types[i]))
                ok++;
            else if (types[i].isInstance(params[i]))
                ok++;
            else if ((params[i] instanceof Double) &&
                     ((Byte.TYPE == types[i]) ||
                      (Byte.class == types[i]) ||
                      (Short.TYPE == types[i]) ||
                      (Short.class == types[i]) ||
                      (Integer.TYPE == types[i]) ||
                      (Integer.class == types[i]) ||
                      (Long.TYPE == types[i]) ||
                      (Long.class == types[i]) ||
                      (Float.TYPE == types[i]) ||
                      (Float.class == types[i]) ||
                      (Double.TYPE == types[i]) ||
                      (Double.class == types[i])))
                ok++;
        }
        return (types.length == ok);
    }

    protected void convertParams(Class[] types, Object[] params)
    {
        for (int i = 0; i < params.length; i++) {
            if ((Boolean.TYPE == types[i]) || (Boolean.class == types[i])) {
                if (null == params[i])
                    params[i] = Boolean.FALSE;
                else
                    params[i] = Boolean.TRUE;
            }
            if (params[i] instanceof Double) {
                if ((Byte.TYPE == types[i]) || (Byte.class == types[i]))
                    params[i] = (byte)(((Double)params[i]).doubleValue());
                if ((Short.TYPE == types[i]) || (Short.class == types[i]))
                    params[i] = (short)(((Double)params[i]).doubleValue());
                if ((Integer.TYPE == types[i]) || (Integer.class == types[i]))
                    params[i] = (int)(((Double)params[i]).doubleValue());
                if ((Long.TYPE == types[i]) || (Long.class == types[i]))
                    params[i] = (long)(((Double)params[i]).doubleValue());
                if ((Float.TYPE == types[i]) || (Float.class == types[i]))
                    params[i] = (float)(((Double)params[i]).doubleValue());
            }
        }
    }

    protected void docall(String[] args, PrintWriter writer)
        throws InvocationTargetException, IllegalAccessException, ClassNotFoundException,
               CommandException
    {
        if (3 > args.length)
            throw new CommandException("insufficient arguments");
        String targetName = args[1];
        String methodName = args[2];
        Object target = null;
        boolean instance = false;
        Class c;

        // Obtain the class associated with the target object or class name
        if ('@' == targetName.charAt(0)) {
            Integer id = Integer.parseInt(targetName.substring(1));
            Holder h = objects.get(id);
            if (null == h)
                throw new CommandException("no such object @"+id);
            target = h.getObj();
            c = target.getClass();
            instance = true;
        }
        else {
            c = Class.forName(targetName);
        }

        Object[] params = new Object[args.length-3];
        for (int i = 0; i < params.length; i++)
            params[i] = demarshall(args[i+3]);

        // Find the appropriate method; throw an exception if none exists
        Method method = null;
        for (Method m : c.getMethods()) {
            if (m.getName().equals(methodName) &&
                checkCompatibility(m.getParameterTypes(),params)) {
                if (null != method)
                    throw new CommandException("multiple eligible methods");
                method = m;
            }
        }
        if (null == method)
            throw new CommandException("no suitable method found");

        if (!instance && !Modifier.isStatic(method.getModifiers()))
            throw new CommandException("non-static method; object required");

        // Invoke the method
        convertParams(method.getParameterTypes(),params);
        Object res = method.invoke(target,params);

        // Send back the results
        if (null == res) {
            output(writer,"nil");
        }
        else if (res instanceof String) {
            output(writer,"\""+Parser.escape((String)res)+"\"");
        }
        else if ((res instanceof Byte) ||
                 (res instanceof Short) ||
                 (res instanceof Integer) ||
                 (res instanceof Long) ||
                 (res instanceof Float) ||
                 (res instanceof Double) ||
                 (res instanceof Boolean)) {
            output(writer,res.toString());
        }
        else if (reverse.containsKey(res)) {
            Integer id = reverse.get(res);
            output(writer,"@"+id);
        }
        else {
            int id = addObject(res);
            output(writer,"@"+id);
        }
    }

    protected int addObject(Object res)
    {
        int id = nextid++;
        Holder h = new Holder(res);
        objects.put(id,h);
        reverse.put(h,id);
        return id;
    }

    protected void donew(String[] args, PrintWriter writer)
        throws InstantiationException, IllegalAccessException, ClassNotFoundException,
               CommandException, InvocationTargetException
    {
        if (2 > args.length)
            throw new CommandException("insufficient arguments");
        String className = args[1];
        Object target = null;
        Class c = Class.forName(className);

        Object[] params = new Object[args.length-2];
        for (int i = 0; i < params.length; i++)
            params[i] = demarshall(args[i+2]);

        // Find the appropriate constructor; throw an exception if none exists
        Constructor constructor = null;
        for (Constructor cons : c.getConstructors()) {
            if (checkCompatibility(cons.getParameterTypes(),params)) {
                if (null != constructor)
                    throw new CommandException("multiple eligible methods");
                constructor = cons;
            }
        }
        if (null == constructor)
            throw new CommandException("no suitable constructor found");

        // Invoke the method
        convertParams(constructor.getParameterTypes(),params);
        Object res = constructor.newInstance(params);

        int id = addObject(res);
        output(writer,"@"+id);
    }

    protected void dorelease(String[] args, PrintWriter writer)
        throws CommandException
    {
        int i;
        if (2 > args.length)
            throw new CommandException("insufficient arguments");

        for (i = 1; i < args.length; i++) {
            if ('@' != args[i].charAt(0))
                throw new CommandException("invalid object reference");
            Integer id = Integer.parseInt(args[i].substring(1));
            Holder h = objects.get(id);
            if (null == h)
                throw new CommandException("no such object @"+id);
            reverse.remove(h.getObj());
            objects.remove(id);
        }
        output(writer,"released "+(args.length-1)+" objects");
    }

    protected void doobjects(String[] args, PrintWriter writer)
    {
        // we use TreeSet to ensure the keys are sorted
        Set<Integer> keys = new TreeSet(objects.keySet());
        Iterator<Integer> iter = keys.iterator();
        while (iter.hasNext()) {
            Integer id = iter.next();
            String className = objects.get(id).getObj().getClass().getName();
            output(writer,String.format("%-6d %s",id,className));
        }
        output(writer,"-");
    }

    protected boolean processLine(String line, PrintWriter writer)
    {
        System.out.println(line);
        String[] args = Parser.parse(line);

        try {
            if (args[0].equals("call"))
                docall(args,writer);
            else if (args[0].equals("new"))
                donew(args,writer);
            else if (args[0].equals("release"))
                dorelease(args,writer);
            else if (args[0].equals("objects"))
                doobjects(args,writer);
            else if (args[0].equals("exit"))
                return true;
            else
                outputError(writer,"unknown command");
        }
        catch (ClassNotFoundException e) {
            outputError(writer,"unknown class");
            return false;
        }
        catch (InstantiationException e) {
            outputError(writer,"instantiation error");
            return false;
        }
        catch (IllegalAccessException e) {
            outputError(writer,"illegal access");
            return false;
        }
        catch (InvocationTargetException e) {
            outputError(writer,e.getTargetException().getClass().getName());
            return false;
        }
        catch (CommandException e) {
            outputError(writer,e.getMessage());
            return false;
        }
        catch (NumberFormatException e) {
            outputError(writer,"invalid number");
            return false;
        }
        return false;
    }


    public void run(Socket c)
    {
        try {
            InputStream cin = c.getInputStream();
            OutputStream cout = c.getOutputStream();
            PrintWriter writer = new PrintWriter(cout,true);
            InputStreamReader reader = new InputStreamReader(cin);
            BufferedReader breader = new BufferedReader(reader);

            String line;
            while ((line = breader.readLine()) != null) {
                if (processLine(line,writer)) {
                    c.close();
                    break;
                }
            }
            System.out.println("Connection closed; "+objects.size()+" remaining objects");
        }
        catch (Exception e) {
            System.out.println("Connection error: "+e.getClass().getName());
            try {
                c.close();
            }
            catch (Exception e2) {}
        }
    }

    public static void main(String[] args)
        throws Exception
    {
        if (args.length == 0) {
            System.out.println("Usage: Bridge <port>");
            System.exit(1);
        }
        int port = Integer.parseInt(args[0]);
        System.out.println("Usng port: "+port);

        InetAddress addr = InetAddress.getByAddress(new byte[]{127,0,0,1});
        ServerSocket s = new ServerSocket(port,1,addr);
        System.out.println("Started server socket");
        while (true) {
            Socket c = s.accept();
            System.out.println("Accepted connection");
            Bridge b = new Bridge();
            b.run(c);
        }
    }
}
