import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.Stack;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;

public class Inference extends HindleyMilner
{
  final boolean SHOW_INSTANCE_CHAIN = false;
  int SCREEN_HEIGHT = 50;
  int SCREEN_WIDTH = 110;

  PrintWriter mainout = new PrintWriter(System.out,true);
  Exp traceRoot = null;
  Stack<Object> traceStack = new Stack<Object>();
  Stack<String> unifyStack = new Stack<String>();
  int nextvar = 0;
  StringWriter message = null;
  PrintWriter messageOut = null;
  boolean traceEnabled = false;
  Map<Object,Integer> nodeNumbers = new HashMap<Object,Integer>();
  Map<Object,Type> nodeTypes = new HashMap<Object,Type>();
  Set<Type> renamed = new HashSet<Type>();

  public Exp intv(int v)
  {
    Exp e = new Exp();
    e.type = ExpType.INT;
    e.val = v;
    return e;
  }

  public Exp ident(String name)
  {
    Exp e = new Exp();
    e.type = ExpType.IDE;
    e.ide = name;
    return e;
  }

  public Exp app(Exp fun, Exp arg)
  {
    Exp e = new Exp();
    e.type = ExpType.APPL;
    e.left = fun;
    e.right = arg;
    return e;
  }

  public Exp cond(Exp test, Exp ifTrue, Exp ifFalse)
  {
    Exp e = new Exp();
    e.type = ExpType.COND;
    e.test = test;
    e.left = ifTrue;
    e.right = ifFalse;
    return e;
  }

  public Exp lambda(String binder, Exp body)
  {
    Exp e = new Exp();
    e.type = ExpType.LAMB;
    e.ide = binder;
    e.body = body;
    return e;
  }

  public Exp block(Decl dlist, Exp scope)
  {
    Exp e = new Exp();
    e.type = ExpType.BLOCK;
    e.decls = dlist;
    e.body = scope;
    return e;
  }

  public Decl declList(String binder, Exp def)
  {
    Decl newdl = new Decl();
    newdl.binder = binder;
    newdl.def = def;
    newdl.next = null;
    return newdl;
  }

  public void printTreeLine(PrintWriter out, String str, Object node)
  {
//     out.format("%-6d %-60s %s\n",num,str,typeString(type));


    out.format("%-6d ",nodeNumbers.get(node));
    out.print(str);
    out.print(" ");
    if (!traceStack.isEmpty() && (traceStack.peek() == node)) {
      for (int i = 0; i < 60-str.length()-1; i++)
        out.print("#");
    }
    else {
      for (int i = 0; i < 60-str.length()-1; i++)
        out.print(" ");
    }
    out.print(" ");
    if (nodeTypes.containsKey(node))
      out.print(typeString(nodeTypes.get(node)));
    else if ((node instanceof Exp) && (((Exp)node).dbgvartype != null))
      out.print(((Exp)node).ide+": "+typeString(((Exp)node).dbgvartype));
    else if ((node instanceof Decl) && (((Decl)node).newTypeVar != null))
      out.print(((Decl)node).binder+": "+typeString(((Decl)node).newTypeVar));
    out.println();
  }

  public void printType(Type t, PrintWriter out, boolean brackets)
  {
    if (t.isvar) {
      if (t.instance != null) {
        if (SHOW_INSTANCE_CHAIN)
          out.print(t.ide+":");
        printType(t.instance,out,brackets);
      }
      else {
        out.print(t.ide);
      }
    }
    else {
      int nargs = t.args.length;
      if (nargs == 0) {
        out.print(t.ide);
      }
      else if (nargs == 1) {
        if (brackets)
          out.print("(");
        printType(t.args[0],out,true);
        out.print(" "+t.ide);
        if (brackets)
          out.print(")");
      }
      else if (nargs == 2) {
        if (brackets)
          out.print("(");
        printType(t.args[0],out,true);
        out.print(" "+t.ide+" ");
        printType(t.args[1],out,false);
        if (brackets)
          out.print(")");
      }
      else {
        if (brackets)
          out.print("(");
        out.print(t.ide);
        for (int i = 0; i < nargs; i++) {
          out.print(" ");
          printType(t.args[i],out,true);
        }
        if (brackets)
          out.print(")");
      }
    }
  }

  public String typeString(Type type)
  {
    if (type == null)
      return "";

    StringWriter sw = new StringWriter();
    PrintWriter out = new PrintWriter(sw);
    printType(type,out,false);
    return sw.toString();
  }

  public String cloneString(Type type)
  {
    if (type.clones.isEmpty())
      return "";
    String s = "{";
    for (int i = 0; i < type.clones.size(); i++) {
      if (i > 0)
        s += ",";
      s += typeString(type.clones.get(i));
    }
    s += "}";
    return s;
  }

  public void dump(Exp exp, PrintWriter out, String indent, boolean lastChild)
  {
    String add = lastChild ? "    " : "|   ";
    String prefix = lastChild ? "`-- " : "|-- ";
    switch (exp.type) {
    case INT:
      printTreeLine(out,indent+prefix+exp.val,exp);
      break;
    case IDE:
      printTreeLine(out,indent+prefix+exp.ide,exp);
      break;
    case COND:
      printTreeLine(out,indent+prefix+"if",exp);
      dump(exp.test,out,indent+add,false);
      dump(exp.left,out,indent+add,false);
      dump(exp.right,out,indent+add,true);
      break;
    case LAMB:
      printTreeLine(out,indent+prefix+"lambda "+exp.ide,exp);
      dump(exp.body,out,indent+add,true);
      break;
    case APPL:
      printTreeLine(out,indent+prefix+"@",exp);
      dump(exp.left,out,indent+add,false);
      dump(exp.right,out,indent+add,true);
      break;
    case BLOCK:
      printTreeLine(out,indent+"block",exp);

      Object rec = new Object();
      nodeNumbers.put(rec,nodeNumbers.get(exp).intValue()+1);
      printTreeLine(out,indent+"|-- rec",rec);
      nodeNumbers.remove(rec);

      for (Decl decl = exp.decls; decl != null; decl = decl.next) {
        String declIndent = indent+"|   ";
        String add2 = (decl.next == null) ? "    " : "|   ";
        String prefix2 = (decl.next == null) ? "`-- " : "|-- ";
        printTreeLine(out,declIndent+prefix2+"def "+decl.binder,decl);
        dump(decl.def,out,declIndent+add2,true);
      }

      dump(exp.body,out,indent,true);
      break;
    }
  }

  public void renameTypeVarsType(Type exp)
  {
    if (exp.isvar) {
      if (exp.instance != null) {
        renameTypeVarsType(exp.instance);
      }
      else if (!renamed.contains(exp)) {
        exp.ide = genVarName();
        renamed.add(exp);
      }
    }
    else {
      for (Type arg : exp.args)
        renameTypeVarsType(arg);
    }
  }

  public void renameTypeVars(Exp exp)
  {
    renameTypeVarsType(nodeTypes.get(exp));
    switch (exp.type) {
    case INT:
      break;
    case IDE:
      break;
    case COND:
      renameTypeVars(exp.test);
      renameTypeVars(exp.left);
      renameTypeVars(exp.right);
      break;
    case LAMB:
      renameTypeVars(exp.body);
      break;
    case APPL:
      renameTypeVars(exp.left);
      renameTypeVars(exp.right);
      break;
    case BLOCK:
      for (Decl decl = exp.decls; decl != null; decl = decl.next)
        renameTypeVars(decl.def);
      renameTypeVars(exp.body);
      break;
    }
  }

  public void number(Exp exp, int[] num)
  {
    nodeNumbers.put(exp,num[0]++);
    switch (exp.type) {
    case INT:
      break;
    case IDE:
      break;
    case COND:
      number(exp.test,num);
      number(exp.left,num);
      number(exp.right,num);
      break;
    case LAMB:
      number(exp.body,num);
      break;
    case APPL:
      number(exp.left,num);
      number(exp.right,num);
      break;
    case BLOCK:
      num[0]++; // for fake rec
      for (Decl decl = exp.decls; decl != null; decl = decl.next) {
        nodeNumbers.put(decl,num[0]++);
        number(decl.def,num);
      }
      number(exp.body,num);
      break;
    }
  }

  public String genVarName()
  {
    char[] data;
    if (nextvar < 26) {
      data = new char[1];
      data[0] = (char)('a'+nextvar);
    }
    else {
      data = new char[2];
      data[0] = (char)('a'+(nextvar/26)-1);
      data[1] = (char)('a'+(nextvar%26));
    }
    nextvar++;

    return new String(data);
  }

  public void resetMessages()
  {
    message = new StringWriter();
    messageOut = new PrintWriter(message,true);
  }

  public void message(String msg)
  {
//     while (msg.length() > 0) {
//       String cur;
//       if (msg.length() > SCREEN_WIDTH) {
//         cur = msg.substring(0,SCREEN_WIDTH);
//         msg = msg.substring(SCREEN_WIDTH);
//       }
//       else {
//         cur = msg;
//         msg = "";
//       }
      messageOut.println(msg);
//     }
  }

  public int countLines(String str)
  {
    char[] chars = str.toCharArray();
    int count = 0;
    for (char c : chars)
      if (c == '\n')
        count++;
    return count;
  }

  public void dumpState(Env env, VarList nongen)
  {
    if (!traceEnabled)
      return;
    messageOut.flush();
    StringWriter dsw = new StringWriter();
    PrintWriter dpw = new PrintWriter(dsw);
    dump(traceRoot,dpw,"",true);
    String dumpStr = dsw.toString();
    String messageStr = message.toString();
    int dumpLines = countLines(dumpStr);
    int messageLines = countLines(messageStr);

    mainout.print(dumpStr);
    mainout.println();

    mainout.print("nonGeneric:");
    for (VarList te = nongen; te != null; te = te.tail)
      mainout.print(" ["+typeString(te.head)+"]");
    mainout.println();

    mainout.println();
    mainout.print(messageStr);
    for (int i = 0; i < SCREEN_HEIGHT-dumpLines-messageLines-4; i++)
      mainout.println();
    resetMessages();
  }

  public Type AnalyzeExp(Exp exp, Env env, VarList nongen)
  {
    traceAnalyzeStart(exp,env,nongen);
    Type type = super.AnalyzeExp(exp,env,nongen);
    nodeTypes.put(exp,type);
    traceAnalyzeEnd(exp,env,nongen);
    return type;
  }

  public void traceAnalyzeStart(Object n, Env env, VarList nongen)
  {
    traceStack.push(n);
    dumpState(env,nongen);
  }

  public void traceAnalyzeEnd(Object n, Env env, VarList nongen)
  {
    traceStack.pop();
    dumpState(env,nongen);
  }

  public void UnifyType(Type typeExp1, Type typeExp2, VarList nongen)
  {
    traceUnifyStart(typeExp1,typeExp2);
    super.UnifyType(typeExp1,typeExp2,nongen);
    traceUnifyEnd(typeExp1,typeExp2);
  }

  public void traceUnifyStart(Type typeExp1, Type typeExp2)
  {
    if (!typeExp1.isvar && typeExp2.isvar) {
      unifyStack.push(null);
      unifyStack.push(null);
    }
    else {
      unifyStack.push(typeString(typeExp1));
      unifyStack.push(typeString(typeExp2));
    }
  }

  public String envRetrieveOrig = null;

  public Type envRetrieve(String ide, Env env, VarList nongen)
  {
    traceEnvRetrieveStart(ide,env,nongen);
    Type res = super.envRetrieve(ide,env,nongen);
    traceEnvRetrieveEnd(ide,env,nongen,res);
    return res;
  }

  public void traceEnvRetrieveStart(String ide, Env env, VarList nongen)
  {
    Type exp = null;
    for (; env != null; env = env.tail)
      if (ide.equals(env.ide))
        exp = env.exp;
    envRetrieveOrig = typeString(exp);
  }

  public void traceEnvRetrieveEnd(String ide, Env env, VarList nongen,
                                         Type res)
  {
    message("EnvRetrieve "+ide+": "+typeString(res)+"    [orig: "+envRetrieveOrig+"]");
  }

  public void traceUnifyEnd(Type typeExp1, Type typeExp2)
  {
    String s2 = unifyStack.pop();
    String s1 = unifyStack.pop();
    if ((s1 != null) && (s2 != null)) {
      String res = typeString(typeExp1);
      message(String.format("Unify   %-30s %-30s %s",s1,s2,res));
    }
  }

  public Type listType(Type elem)
  {
    return NewTypeOper("list",new Type[]{elem});
  }

  public Type PairType(Type fst, Type snd)
  {
    return NewTypeOper("x",new Type[]{fst,snd});
  }

  public Env initEnv()
  {
    Type vt1;
    Type vt2;
    Type lt;
    Env env = null;

    // + - * / %
    env = new Env("+",FunType(IntType,FunType(IntType,IntType)),env);
    env = new Env("-",FunType(IntType,FunType(IntType,IntType)),env);
    env = new Env("*",FunType(IntType,FunType(IntType,IntType)),env);
    env = new Env("/",FunType(IntType,FunType(IntType,IntType)),env);
    env = new Env("%",FunType(IntType,FunType(IntType,IntType)),env);

    // == != < <= > >=
    env = new Env("==",FunType(IntType,FunType(IntType,BoolType)),env);
    env = new Env("!=",FunType(IntType,FunType(IntType,BoolType)),env);
    env = new Env("<",FunType(IntType,FunType(IntType,BoolType)),env);
    env = new Env("<=",FunType(IntType,FunType(IntType,BoolType)),env);
    env = new Env(">",FunType(IntType,FunType(IntType,BoolType)),env);
    env = new Env(">=",FunType(IntType,FunType(IntType,BoolType)),env);

    // pair
    vt1 = NewTypeVar();
    vt2 = NewTypeVar();
    env = new Env("pair",FunType(vt1,FunType(vt2,PairType(vt1,vt2))),env);

    // fst
    vt1 = NewTypeVar();
    vt2 = NewTypeVar();
    env = new Env("fst",FunType(PairType(vt1,vt2),vt1),env);

    // snd
    vt1 = NewTypeVar();
    vt2 = NewTypeVar();
    env = new Env("snd",FunType(PairType(vt1,vt2),vt2),env);

    // nil
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = new Env("nil",lt,env);

    // cons
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = new Env("cons",FunType(vt1,FunType(lt,lt)),env);

    // hd
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = new Env("hd",FunType(lt,vt1),env);

    // tl
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = new Env("tl",FunType(lt,lt),env);

    // null
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = new Env("null",FunType(lt,BoolType),env);

    // true
    env = new Env("true",BoolType,env);

    // false
    env = new Env("false",BoolType,env);

    return env;
  }

  public void printEnv(Env env)
  {
    for (; env != null; env = env.tail)
      mainout.format("%-12s %s\n",env.ide,typeString(env.exp));
  }

  public Type doAnalyze(Exp exp, Env env)
  {
    Type result = AnalyzeExp(exp,env,null);
    nextvar = 0;
    renameTypeVars(exp);
    return result;
  }

  public void run(String[] args)
  {
    int r = 0;
    try {
      resetMessages();
      Env env = initEnv();

      boolean dump = false;
      int argno = 0;

      while (true) {
        if ((args.length > argno) && args[argno].equals("-trace")) {
          traceEnabled = true;
          argno++;
        }
        else if ((args.length > argno+1) && args[argno].equals("-lines")) {
          argno++;
          SCREEN_HEIGHT = Integer.parseInt(args[argno++]);
        }
        else if ((args.length > argno) && args[argno].equals("-dump")) {
          dump = true;
          argno++;
        }
        else {
          break;
        }
      }

      if (args.length == argno) {
        System.err.println("Usage: Inference <filename>");
        System.exit(1);
      }

      String filename = args[argno];

      Parser parser = new Parser(new FileInputStream(filename));
      Exp exp = parser.Expression();
      traceRoot = exp;

      int[] num = new int[]{0};
      number(exp,num);

      if (dump) {
        try {
          Type result = doAnalyze(exp,env);
          dump(exp,mainout,"",false);
        } catch (RuntimeException e) {
          mainout.println("Error: "+e.getMessage());
//           e.printStackTrace();
          r = 1;
        }

      }
      else if (traceEnabled) {
        try {
          Type result = doAnalyze(exp,env);

          message("result type = "+typeString(result));
          dumpState(env,null);
        } catch (RuntimeException e) {
          message("Error: "+e.getMessage());
          e.printStackTrace();
          dumpState(env,null);
          r = 1;
        }
      }
      else {
        try {
          Type result = doAnalyze(exp,env);
          mainout.println(typeString(result));
        } catch (RuntimeException e) {
          mainout.println("Error: "+e.getMessage());
//           e.printStackTrace();
          r = 1;
        }
      }

      mainout.flush();
    }
    catch (FileNotFoundException e) {
      System.err.println(e);
      System.exit(1);
    }
    catch (ParseException e) {
      System.err.println(e);
      System.exit(1);
    }

    System.exit(r);
  }

  public static Inference inst;

  public static void main(String[] args)
  {
    inst = new Inference();
    inst.run(args);
  }
}
