import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.Stack;

public class Inference extends HindleyMilner
{
  static final boolean SHOW_INSTANCE_CHAIN = false;
  static int SCREEN_HEIGHT = 50;
  static int SCREEN_WIDTH = 110;

  static PrintWriter mainout = new PrintWriter(System.out,true);
  static Exp traceRoot = null;
  static Stack<Node> traceStack = new Stack<Node>();
  static Stack<String> unifyStack = new Stack<String>();
  static int nextvar = 0;
  static StringWriter message = null;
  static PrintWriter messageOut = null;
  static boolean traceEnabled = false;

  public static Exp intv(int v)
  {
    Exp e = new Exp();
    e.type = ExpType.INT;
    e.val = v;
    return e;
  }

  public static Exp ident(String name)
  {
    Exp e = new Exp();
    e.type = ExpType.IDE;
    e.ide = name;
    return e;
  }

  public static Exp app(Exp fun, Exp arg)
  {
    Exp e = new Exp();
    e.type = ExpType.APPL;
    e.fun = fun;
    e.arg = arg;
    return e;
  }

  public static Exp cond(Exp test, Exp ifTrue, Exp ifFalse)
  {
    Exp e = new Exp();
    e.type = ExpType.COND;
    e.test = test;
    e.ifTrue = ifTrue;
    e.ifFalse = ifFalse;
    return e;
  }

  public static Exp lambda(String binder, Exp body)
  {
    Exp e = new Exp();
    e.type = ExpType.LAMB;
    e.binder = binder;
    e.body = body;
    return e;
  }

  public static Exp block(Decl decl, Exp scope)
  {
    Exp e = new Exp();
    e.type = ExpType.BLOCK;
    e.decl = decl;
    e.scope = scope;
    return e;
  }

  public static Decl def(String binder, Exp def)
  {
    Decl d = new Decl();
    d.type = DeclType.DEF;
    d.binder = binder;
    d.def = def;
    return d;
  }

  public static Decl seq(Decl first, Decl second)
  {
    Decl d = new Decl();
    d.type = DeclType.SEQ;
    d.first = first;
    d.second = second;
    return d;
  }

  public static Decl rec(Decl rec)
  {
    Decl d = new Decl();
    d.type = DeclType.REC;
    d.rec = rec;
    return d;
  }

  public static void printTreeLine(PrintWriter out, String str, Node n)
  {
//     out.format("%-6d %-60s %s\n",num,str,typeString(type));


    out.format("%-6d ",n.dbgnum);
    out.print(str);
    out.print(" ");
    if (!traceStack.isEmpty() && (traceStack.peek() == n)) {
      for (int i = 0; i < 60-str.length()-1; i++)
        out.print("#");
    }
    else {
      for (int i = 0; i < 60-str.length()-1; i++)
        out.print(" ");
    }
    out.print(" ");
    if (n.dbgtype != null)
      out.print(typeString(n.dbgtype));
    else if ((n instanceof Exp) && (n.dbgvartype != null))
      out.print(((Exp)n).binder+": "+typeString(n.dbgvartype));
    else if ((n instanceof Decl) && (n.dbgvartype != null))
      out.print(((Decl)n).binder+": "+typeString(n.dbgvartype));
    out.println();
  }

  public static void dumpDecl(Decl decl, PrintWriter out, String indent,
                              boolean lastChild, boolean top)
  {
    String add = lastChild ? "    " : "|   ";
    String prefix = lastChild ? "`-- " : "|-- ";
    switch (decl.type) {
    case DEF:
      printTreeLine(out,indent+prefix+"def "+decl.binder,decl);
      dump(decl.def,out,indent+add,true);
      break;
    case SEQ:
      dumpDecl(decl.first,out,indent,false,false);
      dumpDecl(decl.second,out,indent,top,false);
      break;
    case REC:
      printTreeLine(out,indent+prefix+"rec",decl);
      dumpDecl(decl.rec,out,indent+add,true,top);
      break;
    }
  }

  public static void printType(TypeExp t, PrintWriter out, boolean brackets)
  {
    switch (t.type) {
    case VARTYPE: {
      if (t.instance != null) {
        if (SHOW_INSTANCE_CHAIN)
          out.print(t.ide+":");
        printType(t.instance,out,brackets);
      }
      else {
        out.print(t.ide);
      }
      break;
    }
    case OPERTYPE: {
      int nargs = 0;
      for (TypeList a = t.args; a != null; a = a.tail)
        nargs++;
      if (nargs == 0) {
        out.print(t.ide);
      }
      else if (nargs == 1) {
        if (brackets)
          out.print("(");
        printType(t.args.head,out,true);
        out.print(" "+t.ide);
        if (brackets)
          out.print(")");
      }
      else if (nargs == 2) {
        if (brackets)
          out.print("(");
        printType(t.args.head,out,true);
        out.print(" "+t.ide+" ");
        printType(t.args.tail.head,out,false);
        if (brackets)
          out.print(")");
      }
      else {
        if (brackets)
          out.print("(");
        out.print(t.ide);
        for (TypeList a = t.args; a != null; a = a.tail) {
          out.print(" ");
          printType(a.head,out,true);
        }
        if (brackets)
          out.print(")");
      }
      break;
    }
    }
  }

  public static String typeString(TypeExp type)
  {
    if (type == null)
      return "";

    StringWriter sw = new StringWriter();
    PrintWriter out = new PrintWriter(sw);
    printType(type,out,false);
    return sw.toString();
  }

  public static String cloneString(TypeExp type)
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

  public static void dump(Exp exp, PrintWriter out, String indent, boolean lastChild)
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
      dump(exp.ifTrue,out,indent+add,false);
      dump(exp.ifFalse,out,indent+add,true);
      break;
    case LAMB:
      printTreeLine(out,indent+prefix+"lambda "+exp.binder,exp);
      dump(exp.body,out,indent+add,true);
      break;
    case APPL:
      printTreeLine(out,indent+prefix+"@",exp);
      dump(exp.fun,out,indent+add,false);
      dump(exp.arg,out,indent+add,true);
      break;
    case BLOCK:
      printTreeLine(out,indent+"block",exp);
      dumpDecl(exp.decl,out,indent,false,true);
      dump(exp.scope,out,indent,true);
      break;
    }
  }

  public static void renameTypeVarsType(TypeExp exp)
  {
    if (exp.type == TypeExpType.VARTYPE) {
      if (exp.instance != null) {
        renameTypeVarsType(exp.instance);
      }
      else if (!exp.dbgrenamed) {
        exp.ide = genVarName();
        exp.dbgrenamed = true;
      }
    }
    else if (exp.type == TypeExpType.OPERTYPE) {
      for (TypeList tl = exp.args; tl != null; tl = tl.tail)
        renameTypeVarsType(tl.head);
    }
  }

  public static void renameTypeVarsDecl(Decl decl)
  {
    switch (decl.type) {
    case DEF:
      renameTypeVars(decl.def);
      break;
    case SEQ:
      renameTypeVarsDecl(decl.first);
      renameTypeVarsDecl(decl.second);
      break;
    case REC:
      renameTypeVarsDecl(decl.rec);
      break;
    }
  }

  public static void renameTypeVars(Exp exp)
  {
    renameTypeVarsType(exp.dbgtype);
    switch (exp.type) {
    case INT:
      break;
    case IDE:
      break;
    case COND:
      renameTypeVars(exp.test);
      renameTypeVars(exp.ifTrue);
      renameTypeVars(exp.ifFalse);
      break;
    case LAMB:
      renameTypeVars(exp.body);
      break;
    case APPL:
      renameTypeVars(exp.fun);
      renameTypeVars(exp.arg);
      break;
    case BLOCK:
      renameTypeVarsDecl(exp.decl);
      renameTypeVars(exp.scope);
      break;
    }
  }

  public static void numberDecl(Decl decl, int[] num)
  {
    switch (decl.type) {
    case DEF:
      decl.dbgnum = num[0]++;
      number(decl.def,num);
      break;
    case SEQ:
      numberDecl(decl.first,num);
      numberDecl(decl.second,num);
      break;
    case REC:
      decl.dbgnum = num[0]++;
      numberDecl(decl.rec,num);
      break;
    }
  }

  public static void number(Exp exp, int[] num)
  {
    exp.dbgnum = num[0]++;
    switch (exp.type) {
    case INT:
      break;
    case IDE:
      break;
    case COND:
      number(exp.test,num);
      number(exp.ifTrue,num);
      number(exp.ifFalse,num);
      break;
    case LAMB:
      number(exp.body,num);
      break;
    case APPL:
      number(exp.fun,num);
      number(exp.arg,num);
      break;
    case BLOCK:
      numberDecl(exp.decl,num);
      number(exp.scope,num);
      break;
    }
  }

  public static String genVarName()
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

  public static void resetMessages()
  {
    message = new StringWriter();
    messageOut = new PrintWriter(message,true);
  }

  public static void message(String msg)
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

  public static int countLines(String str)
  {
    char[] chars = str.toCharArray();
    int count = 0;
    for (char c : chars)
      if (c == '\n')
        count++;
    return count;
  }

  public static void dumpState(Env env, NonGenericVars list)
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
    for (TypeList te = list; te != null; te = te.tail)
      mainout.print(" ["+typeString(te.head)+"]");
    mainout.println();

    mainout.println();
    mainout.print(messageStr);
    for (int i = 0; i < SCREEN_HEIGHT-dumpLines-messageLines-4; i++)
      mainout.println();
    resetMessages();
  }

  public static void traceAnalyzeStart(Node n, Env env, NonGenericVars list)
  {
    traceStack.push(n);
    dumpState(env,list);
  }

  public static void traceAnalyzeEnd(Node n, Env env, NonGenericVars list)
  {
    traceStack.pop();
    dumpState(env,list);
  }

  public static void traceUnifyStart(TypeExp typeExp1, TypeExp typeExp2)
  {
    if ((typeExp1.type == TypeExpType.OPERTYPE) &&
        (typeExp2.type == TypeExpType.VARTYPE)) {
      unifyStack.push(null);
      unifyStack.push(null);
    }
    else {
      unifyStack.push(typeString(typeExp1));
      unifyStack.push(typeString(typeExp2));
    }
  }

  public static String envRetrieveOrig = null;

  public static void traceEnvRetrieveStart(String ide, Env env, NonGenericVars list)
  {
    TypeExp exp = null;
    for (; env != null; env = env.tail)
      if (ide.equals(env.ide))
        exp = env.typeExp;
    envRetrieveOrig = typeString(exp);
  }

  public static void traceEnvRetrieveEnd(String ide, Env env, NonGenericVars list,
                                         TypeExp res)
  {
    message("EnvRetrieve "+ide+": "+typeString(res)+"    [orig: "+envRetrieveOrig+"]");
  }

  public static void traceUnifyEnd(TypeExp typeExp1, TypeExp typeExp2)
  {
    String s2 = unifyStack.pop();
    String s1 = unifyStack.pop();
    if ((s1 != null) && (s2 != null)) {
      String res = typeString(typeExp1);
      message(String.format("Unify   %-30s %-30s %s",s1,s2,res));
    }
  }

  public static TypeExp listType(TypeExp elem)
  {
    return NewTypeOper("list",Extend(elem,null));
  }

  public static TypeExp PairType(TypeExp fst, TypeExp snd)
  {
    return NewTypeOper("x",Extend(fst,Extend(snd,null)));
  }

  public static Env initEnv()
  {
    TypeExp vt1;
    TypeExp vt2;
    TypeExp lt;
    Env env = null;

    // + - * / %
    env = envExtend("+",FunType(IntType,FunType(IntType,IntType)),env);
    env = envExtend("-",FunType(IntType,FunType(IntType,IntType)),env);
    env = envExtend("*",FunType(IntType,FunType(IntType,IntType)),env);
    env = envExtend("/",FunType(IntType,FunType(IntType,IntType)),env);
    env = envExtend("%",FunType(IntType,FunType(IntType,IntType)),env);

    // == != < <= > >=
    env = envExtend("==",FunType(IntType,FunType(IntType,BoolType)),env);
    env = envExtend("!=",FunType(IntType,FunType(IntType,BoolType)),env);
    env = envExtend("<",FunType(IntType,FunType(IntType,BoolType)),env);
    env = envExtend("<=",FunType(IntType,FunType(IntType,BoolType)),env);
    env = envExtend(">",FunType(IntType,FunType(IntType,BoolType)),env);
    env = envExtend(">=",FunType(IntType,FunType(IntType,BoolType)),env);

    // pair
    vt1 = NewTypeVar();
    vt2 = NewTypeVar();
    env = envExtend("pair",FunType(vt1,FunType(vt2,PairType(vt1,vt2))),env);

    // fst
    vt1 = NewTypeVar();
    vt2 = NewTypeVar();
    env = envExtend("fst",FunType(PairType(vt1,vt2),vt1),env);

    // snd
    vt1 = NewTypeVar();
    vt2 = NewTypeVar();
    env = envExtend("snd",FunType(PairType(vt1,vt2),vt2),env);

    // nil
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = envExtend("nil",lt,env);

    // cons
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = envExtend("cons",FunType(vt1,FunType(lt,lt)),env);

    // hd
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = envExtend("hd",FunType(lt,vt1),env);

    // tl
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = envExtend("tl",FunType(lt,lt),env);

    // null
    vt1 = NewTypeVar();
    lt = listType(vt1);
    env = envExtend("null",FunType(lt,BoolType),env);

    // true
    env = envExtend("true",BoolType,env);

    // false
    env = envExtend("false",BoolType,env);

    return env;
  }

  public static void printEnv(Env env)
  {
    for (; env != null; env = env.tail)
      mainout.format("%-12s %s\n",env.ide,typeString(env.typeExp));
  }

  public static TypeExp doAnalyze(Exp exp, Env env)
  {
    TypeExp result = AnalyzeExp(exp,env,null);
    nextvar = 0;
    renameTypeVars(exp);
    return result;
  }

  public static void main(String[] args)
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
          TypeExp result = doAnalyze(exp,env);
          dump(exp,mainout,"",false);
        } catch (RuntimeException e) {
          mainout.println("Error: "+e.getMessage());
          r = 1;
        }

      }
      else if (traceEnabled) {
        try {
          TypeExp result = doAnalyze(exp,env);

          message("result type = "+typeString(result));
          dumpState(env,null);
        } catch (RuntimeException e) {
          message("Error: "+e.getMessage());
          dumpState(env,null);
          r = 1;
        }
      }
      else {
        try {
          TypeExp result = doAnalyze(exp,env);
          mainout.println(typeString(result));
        } catch (RuntimeException e) {
          mainout.println("Error: "+e.getMessage());
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
}
