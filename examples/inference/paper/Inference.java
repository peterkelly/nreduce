import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.Stack;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;
import java.util.List;
import java.util.ArrayList;

interface Visitor {
  public void visit(Exp exp);
}

class TypeClashException extends RuntimeException {
  TypeClashException(Type type1, Type type2) {
    super("Type clash "+Inference.inst.typeString(type1)+
          " and "+Inference.inst.typeString(type2));
  }
}

class Inference extends Section6
{
  final boolean SHOW_INSTANCE_CHAIN = false;
  final boolean SHOW_CLONES = false;
  final boolean SPECIALISATION_DEBUG = false;
  final boolean SHOW_DEPS = true; // tests expect true
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
  Set<Type> renamed = new HashSet<Type>();

  void printTreeLine(PrintWriter out, String str, Object node)
  {
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

    if (node != null) {
      if ((node instanceof Exp) && (((Exp)node).type != null))
        out.print(typeString(((Exp)node).type));
      else if ((node instanceof Exp) && (((Exp)node).binderType != null))
        out.print(((Exp)node).ident+": "+typeString(((Exp)node).binderType));
      else if ((node instanceof Decl) && (((Decl)node).binderType != null))
        out.print(((Decl)node).binder+": "+typeString(((Decl)node).binderType));
    }
    out.println();
  }

  void printType(Type t, PrintWriter out, boolean brackets, Stack<Type> printStack)
  {
    if (printStack.contains(t)) {
      out.print("RECURSION!");
      return;
    }
    printStack.push(t);
    if (t.isvar) {

//       if (SHOW_DEPS)
//         out.print("(");

      if ((t.function == null) || SHOW_INSTANCE_CHAIN || !SHOW_DEPS) {
        if (t.instance != null) {
          if (SHOW_INSTANCE_CHAIN)
            out.print(t.ident+":");
          printType(t.instance,out,brackets,printStack);
        }
        else {
          out.print(t.ident);
        }
      }

//       if (SHOW_DEPS)
//         out.print(")");

      if (SHOW_DEPS && (t.function != null)) {
        if (SHOW_INSTANCE_CHAIN)
          out.print("[");
        out.print(t.function.getClass().getSimpleName().toLowerCase()+"(");
        for (int i = 0; i < t.args.length; i++) {
          if (i > 0)
            out.print(",");
          printType(t.args[i],out,false,printStack);
        }
        out.print(")");
        if (SHOW_INSTANCE_CHAIN)
          out.print("]");
//         out.print("["+t.args.length+"]");
      }

    }
    else {
      int nargs = t.args.length;
      if (nargs == 0) {
        out.print(t.ident);
      }
      else if (nargs == 1) {
        if (brackets)
          out.print("(");
        printType(t.args[0],out,true,printStack);
        out.print(" "+t.ident);
        if (brackets)
          out.print(")");
      }
      else if (nargs == 2) {
        if (brackets)
          out.print("(");
        printType(t.args[0],out,true,printStack);
        out.print(" "+t.ident+" ");
        printType(t.args[1],out,false,printStack);
        if (brackets)
          out.print(")");
      }
      else {
        if (brackets)
          out.print("(");
        out.print(t.ident);
        for (int i = 0; i < nargs; i++) {
          out.print(" ");
          printType(t.args[i],out,true,printStack);
        }
        if (brackets)
          out.print(")");
      }
    }
    if (SHOW_CLONES) {
      out.print(cloneString(t));
    }
    printStack.pop();
  }

  public String typeString(Type type)
  {
    if (type == null)
      return "";

    StringWriter sw = new StringWriter();
    PrintWriter out = new PrintWriter(sw);
    Stack<Type> printStack = new Stack<Type>();
    printType(type,out,false,printStack);
    return sw.toString();
  }

  String cloneString(Type type)
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

  void dump(Exp exp, PrintWriter out, String indent, boolean lastChild)
  {
    String add = lastChild ? "    " : "|   ";
    String prefix = lastChild ? "`-- " : "|-- ";
    switch (exp.kind) {
    case CONST:
      if (exp.type == IntType)
        printTreeLine(out,indent+prefix+exp.value,exp);
      else
        printTreeLine(out,indent+prefix+exp.dvalue,exp);
      break;
    case VAR:
      printTreeLine(out,indent+prefix+exp.ident,exp);
      break;
    case COND:
      printTreeLine(out,indent+prefix+"if",exp);
      dump(exp.test,out,indent+add,false);
      dump(exp.left,out,indent+add,false);
      dump(exp.right,out,indent+add,true);
      break;
    case LAMBDA:

      String extra = "";
      if (SPECIALISATION_DEBUG && (debugReferences != null)) {
        int nrefs = 0;
        for (Reference ref : debugReferences)
          if (ref.to == exp)
            nrefs++;
        if (nrefs > 0)
          extra += " ["+nrefs+" refs]";
      }

      printTreeLine(out,indent+prefix+"lambda "+exp.ident+extra,exp);

      dump(exp.body,out,indent+add,true);
      break;
    case APPL:
      printTreeLine(out,indent+prefix+"@",exp);
      dump(exp.left,out,indent+add,false);
      dump(exp.right,out,indent+add,true);
      break;
    case LETREC:
      printTreeLine(out,indent+"block",exp);
      printTreeLine(out,indent+"|-- rec",null);

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

  List<Reference> debugReferences = null;

  void findReferences(Exp exp, List<Reference> refs)
  {
    debugReferences = refs;
    super.findReferences(exp,refs);
  }

  void renameTypeVarsType(Type exp)
  {
    if (exp.isvar) {
      if (exp.instance != null) {
        renameTypeVarsType(exp.instance);
      }
      else if (!renamed.contains(exp)) {
        exp.ident = genVarName();
        renamed.add(exp);
      }
    }
    else {
      for (Type arg : exp.args)
        renameTypeVarsType(arg);
    }
  }

  void renameTypeVars(Exp exp)
  {
    traverse(exp,new Visitor() {
        public void visit(Exp exp) {
          renameTypeVarsType(exp.type);
          if (exp.kind == ExpKind.LETREC) {
            for (Decl decl = exp.decls; decl != null; decl = decl.next)
              renameTypeVarsType(decl.binderType);
          }
        }
      });
  }

  void traverse(Exp exp, Visitor visitor)
  {
    visitor.visit(exp);
    switch (exp.kind) {
    case CONST:
      break;
    case VAR:
      break;
    case COND:
      traverse(exp.test,visitor);
      traverse(exp.left,visitor);
      traverse(exp.right,visitor);
      break;
    case LAMBDA:
      traverse(exp.body,visitor);
      break;
    case APPL:
      traverse(exp.left,visitor);
      traverse(exp.right,visitor);
      break;
    case LETREC:
      for (Decl decl = exp.decls; decl != null; decl = decl.next)
        traverse(decl.def,visitor);
      traverse(exp.body,visitor);
      break;
    }
  }

  void renameFunctions(Exp exp)
  {
    final Set<String> names = new HashSet<String>();
    final Map<String,String> replace = new HashMap<String,String>();

    // Collect existing names
    traverse(exp,new Visitor() {
        public void visit(Exp exp) {
          if (exp.kind == ExpKind.LETREC) {
            for (Decl decl = exp.decls; decl != null; decl = decl.next)
              names.add(decl.binder);
          }
          else if (exp.kind == ExpKind.LAMBDA) {
            names.add(exp.ident);
          }
        }
      });

    // Rename decls corresponding to copied functions
    traverse(exp,new Visitor() {
        public void visit(Exp exp) {
          if (exp.kind == ExpKind.LETREC) {
            for (Decl decl = exp.decls; decl != null; decl = decl.next) {
              String oldName = decl.binder;
              int index = oldName.indexOf("_");
              if (index >= 0) {
                String base = oldName.substring(0,index);
                int num = 1;
                while (names.contains(base+num))
                  num++;
                String newName = base+num;
                decl.binder = newName;
                replace.put(oldName,newName);
                names.add(newName);
              }
            }
          }
        }
      });

    // Change references to use new names
    traverse(exp,new Visitor() {
        public void visit(Exp exp) {
          if ((exp.kind == ExpKind.VAR) && replace.containsKey(exp.ident))
            exp.ident = replace.get(exp.ident);
        }
      });
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

  void resetMessages()
  {
    message = new StringWriter();
    messageOut = new PrintWriter(message,true);
  }

  public void message(String msg)
  {
    messageOut.println(msg);
  }

  int countLines(String str)
  {
    char[] chars = str.toCharArray();
    int count = 0;
    for (char c : chars)
      if (c == '\n')
        count++;
    return count;
  }

  void dumpState()
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

    mainout.println();
    mainout.print(messageStr);
    for (int i = 0; i < SCREEN_HEIGHT-dumpLines-messageLines-3; i++)
      mainout.println();
    resetMessages();
  }

  Type analyze(Exp exp)
  {
    traceAnalyzeStart(exp);
    super.analyze(exp);
    traceAnalyzeEnd(exp);
    return exp.type;
  }

  void traceAnalyzeStart(Exp n)
  {
    traceStack.push(n);
    dumpState();
  }

  void traceAnalyzeEnd(Exp n)
  {
    traceStack.pop();
    dumpState();
  }

  void unify(Type type1, Type type2, Exp context)
  {
    traceUnifyStart(type1,type2);
    super.unify(type1,type2,context);
    traceUnifyEnd(type1,type2);
  }

  void traceUnifyStart(Type type1, Type type2)
  {
    if (!type1.isvar && type2.isvar) {
      unifyStack.push(null);
      unifyStack.push(null);
    }
    else {
      unifyStack.push(typeString(type1));
      unifyStack.push(typeString(type2));
    }
  }

  void traceUnifyEnd(Type type1, Type type2)
  {
    String s2 = unifyStack.pop();
    String s1 = unifyStack.pop();
    if ((s1 != null) && (s2 != null)) {
      String res = typeString(type1);
      message(String.format("Unify   %-30s %-30s %s",s1,s2,res));
    }
  }

  Type listType(Type elem)
  {
    return typeCons("list",new Type[]{elem});
  }

  Type PairType(Type fst, Type snd)
  {
    return typeCons("x",new Type[]{fst,snd});
  }

  void initEnv() {
    for (String name : new String[]{"+","-","*","/","%"})
      builtins.put(name,fun(IntType,fun(IntType,IntType)));
    for (String name : new String[]{"==","!=","<","<=",">",">="})
      builtins.put(name,fun(IntType,fun(IntType,BoolType)));
    Type a = typeVar(), alist = listType(a);
    builtins.put("cons",fun(a,fun(alist,alist)));
    builtins.put("hd",fun(alist,a));
    builtins.put("tl",fun(alist,alist));
    builtins.put("null",fun(alist,BoolType));
    builtins.put("nil",alist);
    builtins.put("true",BoolType);
    builtins.put("false",BoolType);

    Type vt1;
    Type vt2;
    Type lt;

    // pair
    vt1 = typeVar();
    vt2 = typeVar();
    builtins.put("pair",fun(vt1,fun(vt2,PairType(vt1,vt2))));

    // fst
    vt1 = typeVar();
    vt2 = typeVar();
    builtins.put("fst",fun(PairType(vt1,vt2),vt1));

    // snd
    vt1 = typeVar();
    vt2 = typeVar();
    builtins.put("snd",fun(PairType(vt1,vt2),vt2));

    // nil
    vt1 = typeVar();
    lt = listType(vt1);
    builtins.put("nil",lt);

    // polymorphic arithmetic operators
    addPolyComp("p==");
    addPolyComp("p!=");
    addPolyComp("p<");
    addPolyComp("p<=");
    addPolyComp("p>");
    addPolyComp("p>=");

    addPolyOp("p+");
    addPolyOp("p-");
    addPolyOp("p*");
    addPolyOp("p/");
  }

  void addPolyComp(String name)
  {
    Type left = typeVar();
    Type right = typeVar();
    builtins.put(name,fun(left,fun(right,BoolType)));
  }

  void addPolyOp(String name)
  {
    Type left = typeVar();
    Type right = typeVar();
    Type ret = typeVar();
    ret.args = new Type[]{left,right};
    ret.function = new Arith();
    builtins.put(name,fun(left,fun(right,ret)));
  }

  Type doAnalyze(Exp exp, boolean spec)
  {
    setParents(exp,null);
    if (spec) {
      inferAndSpecialise(exp);
      message("After removing unused functions\n");
      dumpState();
    }
    else {
      analyze(exp);
    }

    nextvar = 0;
    renameTypeVars(exp);
    renameFunctions(exp);
    return exp.type;
  }

  void run(String[] args)
  {
    int r = 0;
    try {
      resetMessages();
      initEnv();

      boolean dump = false;
      boolean spec = false;
      int argno = 0;

      while (true) {
        if ((args.length > argno) && args[argno].equals("-spec")) {
          spec = true;
          argno++;
        }
        else if ((args.length > argno) && args[argno].equals("-trace")) {
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

      if (dump) {
        try {
          Type result = doAnalyze(exp,spec);
          dump(exp,mainout,"",false);
        } catch (RuntimeException e) {
          mainout.println("Error: "+e.getMessage());
//           e.printStackTrace();
          r = 1;
        }

      }
      else if (traceEnabled) {
        try {
          Type result = doAnalyze(exp,spec);

          message("result type = "+typeString(result));
          dumpState();
        } catch (Throwable e) {
          message("Error: "+e.getMessage());
          StringWriter sw = new StringWriter();
          PrintWriter pw = new PrintWriter(sw);
          e.printStackTrace(pw);
          pw.flush();
          message(sw.toString());
          dumpState();
          r = 1;
        }
      }
      else {
        try {
          Type result = doAnalyze(exp,spec);
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

  static Inference inst;

  public static void main(String[] args)
  {
    inst = new Inference();
    inst.run(args);
  }
}
