import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.io.PrintWriter;
import java.io.StringWriter;

public class HindleyMilner {

  public static int nextvar = 0;

  final boolean SHOW_INSTANCE_CHAIN = false;

  enum ExpType { INT, IDE, COND, LAMB, APPL, BLOCK };

  abstract class Exp
  {
    ExpType type;

    Type AnalyzeExp(Env env, VarList nongen)
    {
      Inference.inst.traceAnalyzeStart(this,env,nongen);
      Type type = AnalyzeExp1(env,nongen);
      Inference.inst.nodeTypes.put(this,type);
      Inference.inst.traceAnalyzeEnd(this,env,nongen);
      return type;
    }

    abstract Type AnalyzeExp1(Env env, VarList nongen);
    abstract void dump(PrintWriter out, String indent, boolean lastChild);
  }

  class Int extends Exp
  {
    int val;

    Type AnalyzeExp1(Env env, VarList nongen)
    {
      return IntType;
    }

    void dump(PrintWriter out, String indent, boolean lastChild)
    {
      String add = lastChild ? "    " : "|   ";
      String prefix = lastChild ? "`-- " : "|-- ";
      Inference.inst.printTreeLine(out,indent+prefix+val,this);
    }
  }

  class Ide extends Exp
  {
    String ide;

    Type AnalyzeExp1(Env env, VarList nongen)
    {
      return envRetrieve(ide,env,nongen);
    }

    void dump(PrintWriter out, String indent, boolean lastChild)
    {
      String add = lastChild ? "    " : "|   ";
      String prefix = lastChild ? "`-- " : "|-- ";
      Inference.inst.printTreeLine(out,indent+prefix+ide,this);
    }
  }

  class Cond extends Exp
  {
    Exp test;
    Exp left;
    Exp right;

    Type AnalyzeExp1(Env env, VarList nongen)
    {
      UnifyType(test.AnalyzeExp(env,nongen),BoolType,nongen);
      Type typeOfThen = left.AnalyzeExp(env,nongen);
      Type typeOfElse = right.AnalyzeExp(env,nongen);
      UnifyType(typeOfThen,typeOfElse,nongen);
      return typeOfThen;
    }

    void dump(PrintWriter out, String indent, boolean lastChild)
    {
      String add = lastChild ? "    " : "|   ";
      String prefix = lastChild ? "`-- " : "|-- ";
      Inference.inst.printTreeLine(out,indent+prefix+"if",this);
      test.dump(out,indent+add,false);
      left.dump(out,indent+add,false);
      right.dump(out,indent+add,true);
    }
  }

  class Lamb extends Exp
  {
    String ide;
    Exp body;
    Type dbgvartype;

    Type AnalyzeExp1(Env env, VarList nongen)
    {
      VarType typeOfBinder = new VarType();
      dbgvartype = typeOfBinder; // dbg
      Env bodyEnv = new Env(ide,typeOfBinder,env);
      VarList bodyNonGen = new VarList(typeOfBinder,nongen);
      Type typeOfBody = body.AnalyzeExp(bodyEnv,bodyNonGen);
      return FunType(typeOfBinder,typeOfBody);
    }

    void dump(PrintWriter out, String indent, boolean lastChild)
    {
      String add = lastChild ? "    " : "|   ";
      String prefix = lastChild ? "`-- " : "|-- ";
      Inference.inst.printTreeLine(out,indent+prefix+"lambda "+ide,this);
      body.dump(out,indent+add,true);
    }
  }

  class Appl extends Exp
  {
    Exp left;
    Exp right;

    Type AnalyzeExp1(Env env, VarList nongen)
    {
      Type typeOfFun = left.AnalyzeExp(env,nongen);
      Type typeOfArg = right.AnalyzeExp(env,nongen);
      Type typeOfRes = new VarType();
      UnifyType(typeOfFun,FunType(typeOfArg,typeOfRes),nongen);
      return typeOfRes;
    }

    void dump(PrintWriter out, String indent, boolean lastChild)
    {
      String add = lastChild ? "    " : "|   ";
      String prefix = lastChild ? "`-- " : "|-- ";
      Inference.inst.printTreeLine(out,indent+prefix+"@",this);
      left.dump(out,indent+add,false);
      right.dump(out,indent+add,true);
    }
  }

  class Block extends Exp
  {
    Decl decls;
    Exp body;

    Type AnalyzeExp1(Env env, VarList nongen)
    {
      for (Decl decl = decls; decl != null; decl = decl.next) {
        decl.newTypeVar = new VarType();
        env = new Env(decl.binder,decl.newTypeVar,env);
      }
      for (Decl decl = decls; decl != null; decl = decl.next)
        UnifyType(decl.newTypeVar,decl.def.AnalyzeExp(env,nongen),nongen);
      return body.AnalyzeExp(env,nongen);
    }

    void dump(PrintWriter out, String indent, boolean lastChild)
    {
      String add = lastChild ? "    " : "|   ";
      String prefix = lastChild ? "`-- " : "|-- ";
      Inference.inst.printTreeLine(out,indent+"block",this);

      Object rec = new Object();
      Inference.inst.nodeNumbers.put(rec,Inference.inst.nodeNumbers.get(this).intValue()+1);
      Inference.inst.printTreeLine(out,indent+"|-- rec",rec);
      Inference.inst.nodeNumbers.remove(rec);

      for (Decl decl = decls; decl != null; decl = decl.next) {
        String declIndent = indent+"|   ";
        String add2 = (decl.next == null) ? "    " : "|   ";
        String prefix2 = (decl.next == null) ? "`-- " : "|-- ";
        Inference.inst.printTreeLine(out,declIndent+prefix2+"def "+decl.binder,decl);
        decl.def.dump(out,declIndent+add2,true);
      }

      body.dump(out,indent,true);
    }
  }

  class Decl
  {
    String binder;
    Exp def;
    Type newTypeVar;
    Decl next;
  }

  abstract class Type
  {
    Type instance;
    String ide;
    Type[] args;
    List<Type> clones = new ArrayList<Type>();

    public abstract void printType(PrintWriter out, boolean brackets);

    public String toString()
    {
      StringWriter sw = new StringWriter();
      PrintWriter out = new PrintWriter(sw);
      printType(out,false);
      return sw.toString();
    }

    abstract Type prune();

    private Type FreshVar(Type typeVar, Map<Type,Type> copymap)
    {
      if (copymap.containsKey(typeVar))
        return copymap.get(typeVar);

      Type newTypeVar = new VarType();
      copymap.put(typeVar,newTypeVar);
      typeVar.clones.add(newTypeVar);
      return newTypeVar;
    }

    private Type Fresh(VarList nongen, Map<Type,Type> copymap)
    {
      Type typeExp = prune();
      if ((typeExp instanceof VarType) && ((VarType)typeExp).IsGeneric(nongen))
        return FreshVar(typeExp,copymap);
      if (typeExp instanceof VarType)
        return typeExp;
      Type[] copy = new Type[typeExp.args.length];
      for (int i = 0; i < copy.length; i++)
        copy[i] = typeExp.args[i].Fresh(nongen,copymap);
      return new OperType(typeExp.ide,copy);
    }

    Type FreshType(VarList nongen)
    {
      Map<Type,Type> copymap = new HashMap<Type,Type>();
      return Fresh(nongen,copymap);
    }

    boolean containsClone(Type maybeClone)
    {
      for (Type clone : clones) {
        if (OccursInType(clone,maybeClone) || clone.containsClone(maybeClone))
          return true;
      }
      return false;
    }

    boolean removeClone(Type maybeClone)
    {
      for (int i = 0; i < clones.size(); i++) {
        Type clone = clones.get(i);
        if (clone == maybeClone) {
          clones.remove(i);
          return true;
        }
        else if (clone.removeClone(maybeClone))
          return true;
      }
      return false;
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

  class VarType extends Type
  {
    VarType() {
      ide = genVarName();
    }

    public void printType(PrintWriter out, boolean brackets)
    {
      Type t = this;
      if (t.instance != null) {
        if (SHOW_INSTANCE_CHAIN)
          out.print(t.ide+":");
        t.instance.printType(out,brackets);
      }
      else {
        out.print(t.ide);
      }
    }

    Type prune()
    {
      if (instance == null)
        return this;
      instance = instance.prune();
      return instance;
    }

    boolean IsGeneric(VarList nongen)
    {
      for (; nongen != null; nongen = nongen.tail) {
        if (OccursInType(this,nongen.head))
          return false;
      }
      return true;
    }

  }

  class OperType extends Type
  {
    OperType(String id, Type[] a) {
      ide = id;
      args = a;
    }

    public void printType(PrintWriter out, boolean brackets)
    {
      Type t = this;
      int nargs = t.args.length;
      if (nargs == 0) {
        out.print(t.ide);
      }
      else if (nargs == 1) {
        if (brackets)
          out.print("(");
        t.args[0].printType(out,true);
        out.print(" "+t.ide);
        if (brackets)
          out.print(")");
      }
      else if (nargs == 2) {
        if (brackets)
          out.print("(");
        t.args[0].printType(out,true);
        out.print(" "+t.ide+" ");
        t.args[1].printType(out,false);
        if (brackets)
          out.print(")");
      }
      else {
        if (brackets)
          out.print("(");
        out.print(t.ide);
        for (int i = 0; i < nargs; i++) {
          out.print(" ");
          t.args[i].printType(out,true);
        }
        if (brackets)
          out.print(")");
      }
    }

    Type prune()
    {
      return this;
    }
  }

  class VarList
  {
    VarType head;
    VarList tail;

    VarList(VarType h, VarList t) {
      head = h;
      tail = t;
    }
  }

  class Env
  {
    String ide;
    Type exp;
    Env tail;

    Env(String i, Type e, Env t) {
      ide = i;
      exp = e;
      tail = t;
    }
  }


  Type BoolType = new OperType("bool",new Type[]{});
  Type IntType = new OperType("int",new Type[]{});

  Type FunType(Type dom, Type cod)
  {
    return new OperType("->",new Type[]{dom,cod});
  }

  boolean OccursInType(Type typeVar, Type typeExp)
  {
    typeExp = typeExp.prune();
    if (typeExp instanceof VarType)
      return (typeVar == typeExp);
    for (Type arg : typeExp.args) {
      if (OccursInType(typeVar,arg))
        return true;
    }
    return false;
  }

  void UnifyType(Type typeExp1, Type typeExp2, VarList nongen)
  {
    typeExp1 = typeExp1.prune();
    typeExp2 = typeExp2.prune();

    if (typeExp1 instanceof VarType) {
      if (OccursInType(typeExp1,typeExp2)) {
        if (typeExp1 != typeExp2)
          throw new RuntimeException("Type clash");
      }
      else {
        typeExp1.removeClone(typeExp2);

        if (typeExp1.containsClone(typeExp2))
          throw new RuntimeException("Recursion detected: Type is undecidable");

        typeExp1.instance = typeExp2;

        for (Type clone : typeExp1.clones) // Propagate update
          UnifyType(clone,typeExp2.FreshType(nongen),nongen);
        typeExp1.clones.clear();
      }
    }
    else if (typeExp2 instanceof VarType)
      UnifyType(typeExp2,typeExp1,nongen);
    else if (typeExp1.ide.equals(typeExp2.ide)) {
      if (typeExp1.args.length != typeExp2.args.length)
        throw new RuntimeException("Type clash");
      for (int i = 0; i < typeExp1.args.length; i++)
        UnifyType(typeExp1.args[i],typeExp2.args[i],nongen);
    }
    else
      throw new RuntimeException("type clash");
  }

  Type envRetrieve(String ide, Env env, VarList nongen)
  {
    for (; env != null; env = env.tail) {
      if (ide.equals(env.ide))
        return env.exp.FreshType(nongen);
    }
    throw new RuntimeException("Unbound ide: "+ide);
  }
}
