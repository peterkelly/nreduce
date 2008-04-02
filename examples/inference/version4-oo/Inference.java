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
  int SCREEN_HEIGHT = 50;
  int SCREEN_WIDTH = 110;

  PrintWriter mainout = new PrintWriter(System.out,true);
  Exp traceRoot = null;
  Stack<Object> traceStack = new Stack<Object>();
  Stack<String> unifyStack = new Stack<String>();
  StringWriter message = null;
  PrintWriter messageOut = null;
  boolean traceEnabled = false;
  Map<Object,Integer> nodeNumbers = new HashMap<Object,Integer>();
  Map<Object,Type> nodeTypes = new HashMap<Object,Type>();
  Set<Type> renamed = new HashSet<Type>();

  public Exp intv(int v)
  {
    Int e = new Int();
    e.type = ExpType.INT;
    e.val = v;
    return e;
  }

  public Exp ident(String name)
  {
    Ide e = new Ide();
    e.type = ExpType.IDE;
    e.ide = name;
    return e;
  }

  public Exp app(Exp fun, Exp arg)
  {
    Appl e = new Appl();
    e.type = ExpType.APPL;
    e.left = fun;
    e.right = arg;
    return e;
  }

  public Exp cond(Exp test, Exp ifTrue, Exp ifFalse)
  {
    Cond e = new Cond();
    e.type = ExpType.COND;
    e.test = test;
    e.left = ifTrue;
    e.right = ifFalse;
    return e;
  }

  public Exp lambda(String binder, Exp body)
  {
    Lamb e = new Lamb();
    e.type = ExpType.LAMB;
    e.ide = binder;
    e.body = body;
    return e;
  }

  public Exp block(Decl dlist, Exp scope)
  {
    Block e = new Block();
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
    else if ((node instanceof Lamb) && (((Lamb)node).dbgvartype != null))
      out.print(((Lamb)node).ide+": "+typeString(((Lamb)node).dbgvartype));
    else if ((node instanceof Decl) && (((Decl)node).newTypeVar != null))
      out.print(((Decl)node).binder+": "+typeString(((Decl)node).newTypeVar));
    out.println();
  }

  public String typeString(Type type)
  {
    if (type == null)
      return "";
    else
      return type.toString();
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

  public void renameTypeVarsType(Type exp)
  {
    if (exp instanceof VarType) {
      VarType vt = (VarType)exp;
      if (vt.instance != null) {
        renameTypeVarsType(vt.instance);
      }
      else if (!renamed.contains(exp)) {
        vt.ide = HindleyMilner.genVarName();
        renamed.add(vt);
      }
    }
    else {
      OperType ot = (OperType)exp;
      for (Type arg : ot.args)
        renameTypeVarsType(arg);
    }
  }

  public void renameTypeVars(Exp exp)
  {
    renameTypeVarsType(nodeTypes.get(exp));
    if (exp instanceof Cond) {
      renameTypeVars(((Cond)exp).test);
      renameTypeVars(((Cond)exp).left);
      renameTypeVars(((Cond)exp).right);
    }
    else if (exp instanceof Lamb) {
      renameTypeVars(((Lamb)exp).body);
    }
    else if (exp instanceof Appl) {
      renameTypeVars(((Appl)exp).left);
      renameTypeVars(((Appl)exp).right);
    }
    else if (exp instanceof Block) {
      for (Decl decl = ((Block)exp).decls; decl != null; decl = decl.next)
        renameTypeVars(decl.def);
      renameTypeVars(((Block)exp).body);
    }
  }

  public void number(Exp exp, int[] num)
  {
    nodeNumbers.put(exp,num[0]++);
    if (exp instanceof Cond) {
      number(((Cond)exp).test,num);
      number(((Cond)exp).left,num);
      number(((Cond)exp).right,num);
    }
    else if (exp instanceof Lamb) {
      number(((Lamb)exp).body,num);
    }
    else if (exp instanceof Appl) {
      number(((Appl)exp).left,num);
      number(((Appl)exp).right,num);
    }
    else if (exp instanceof Block) {
      num[0]++; // for fake rec
      for (Decl decl = ((Block)exp).decls; decl != null; decl = decl.next) {
        nodeNumbers.put(decl,num[0]++);
        number(decl.def,num);
      }
      number(((Block)exp).body,num);
    }
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
    traceRoot.dump(dpw,"",true);
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

//   public Type AnalyzeExp(Exp exp, Env env, VarList nongen)
//   {
//     traceAnalyzeStart(exp,env,nongen);
//     Type type = super.AnalyzeExp(exp,env,nongen);
//     nodeTypes.put(exp,type);
//     traceAnalyzeEnd(exp,env,nongen);
//     return type;
//   }

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
    if (!(typeExp1 instanceof VarType) && (typeExp2 instanceof VarType)) {
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
    return new OperType("list",new Type[]{elem});
  }

  public Type PairType(Type fst, Type snd)
  {
    return new OperType("x",new Type[]{fst,snd});
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
    vt1 = new VarType();
    vt2 = new VarType();
    env = new Env("pair",FunType(vt1,FunType(vt2,PairType(vt1,vt2))),env);

    // fst
    vt1 = new VarType();
    vt2 = new VarType();
    env = new Env("fst",FunType(PairType(vt1,vt2),vt1),env);

    // snd
    vt1 = new VarType();
    vt2 = new VarType();
    env = new Env("snd",FunType(PairType(vt1,vt2),vt2),env);

    // nil
    vt1 = new VarType();
    lt = listType(vt1);
    env = new Env("nil",lt,env);

    // cons
    vt1 = new VarType();
    lt = listType(vt1);
    env = new Env("cons",FunType(vt1,FunType(lt,lt)),env);

    // hd
    vt1 = new VarType();
    lt = listType(vt1);
    env = new Env("hd",FunType(lt,vt1),env);

    // tl
    vt1 = new VarType();
    lt = listType(vt1);
    env = new Env("tl",FunType(lt,lt),env);

    // null
    vt1 = new VarType();
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
    Type result = exp.AnalyzeExp(env,null);
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
          exp.dump(mainout,"",false);
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
