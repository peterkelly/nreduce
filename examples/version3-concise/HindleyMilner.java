import java.util.*;

enum ExpType { INT, IDE, COND, LAMB, APPL, BLOCK };

class Exp {
  ExpType type;
  int val;
  String ide;
  Exp test, left, right, body;
  Decl decls;
  Type dbgvartype;
}

class Decl {
  String binder;
  Exp def;
  Type newTypeVar;
  Decl next;
}

class Type {
  boolean isvar;
  Type instance;
  String ide;
  Type[] args;
  List<Type> clones = new ArrayList<Type>();

  Type(boolean iv, String id, Type[] a)
  { isvar = iv; ide = id; args = a; }
}

class VarList {
  Type head;
  VarList tail;
  VarList(Type h, VarList t)
  { head = h; tail = t; }
}

class Env {
  String ide;
  Type exp;
  Env tail;
  Env(String i, Type e, Env t)
  { ide = i; exp = e; tail = t; }
}

public class HindleyMilner {

  Type BoolType = NewTypeOper("bool",new Type[]{});
  Type IntType = NewTypeOper("int",new Type[]{});

  Type NewTypeVar() {
    return new Type(true,Inference.inst.genVarName(),null);
  }

  Type NewTypeOper(String ide, Type[] args) {
    return new Type(false,ide,args);
  }

  Type FunType(Type dom, Type cod) {
    return NewTypeOper("->",new Type[]{dom,cod});
  }

  Type Prune(Type typeExp) {
    if (!typeExp.isvar || (typeExp.instance == null))
      return typeExp;
    typeExp.instance = Prune(typeExp.instance);
    return typeExp.instance;
  }

  boolean OccursInType(Type typeVar, Type typeExp) {
    typeExp = Prune(typeExp);
    if (typeExp.isvar)
      return (typeVar == typeExp);
    for (Type arg : typeExp.args) {
      if (OccursInType(typeVar,arg))
        return true;
    }
    return false;
  }

  boolean containsClone(Type orig, Type maybeClone) {
    for (Type clone : orig.clones) {
      if (OccursInType(clone,maybeClone) || containsClone(clone,maybeClone))
        return true;
    }
    return false;
  }

  boolean removeClone(Type orig, Type maybeClone) {
    for (int i = 0; i < orig.clones.size(); i++) {
      Type clone = orig.clones.get(i);
      if (clone == maybeClone) {
        orig.clones.remove(i);
        return true;
      }
      else if (removeClone(clone,maybeClone))
        return true;
    }
    return false;
  }

  void UnifyType(Type typeExp1, Type typeExp2, VarList nongen) {
    typeExp1 = Prune(typeExp1);
    typeExp2 = Prune(typeExp2);

    if (typeExp1.isvar) {
      if (OccursInType(typeExp1,typeExp2)) {
        if (typeExp1 != typeExp2)
          throw new RuntimeException("Type clash");
      }
      else {
        removeClone(typeExp1,typeExp2);

        if (containsClone(typeExp1,typeExp2))
          throw new RuntimeException("Recursion detected: Type is undecidable");

        typeExp1.instance = typeExp2;

        for (Type clone : typeExp1.clones) // Propagate update
          UnifyType(clone,FreshType(typeExp2,nongen),nongen);
        typeExp1.clones.clear();
      }
    }
    else if (typeExp2.isvar)
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

  boolean IsGeneric(Type typeVar, VarList nongen) {
    for (; nongen != null; nongen = nongen.tail) {
      if (OccursInType(typeVar,nongen.head))
        return false;
    }
    return true;
  }

  Type FreshVar(Type typeVar, Map<Type,Type> copymap) {
    if (copymap.containsKey(typeVar))
      return copymap.get(typeVar);

    Type newTypeVar = NewTypeVar();
    copymap.put(typeVar,newTypeVar);
    typeVar.clones.add(newTypeVar);
    return newTypeVar;
  }

  Type Fresh(Type typeExp, VarList nongen, Map<Type,Type> copymap) {
    typeExp = Prune(typeExp);
    if (typeExp.isvar && IsGeneric(typeExp,nongen)) 
      return FreshVar(typeExp,copymap);
    if (typeExp.isvar)
      return typeExp;
    Type[] copy = new Type[typeExp.args.length];
    for (int i = 0; i < copy.length; i++)
      copy[i] = Fresh(typeExp.args[i],nongen,copymap);
    return NewTypeOper(typeExp.ide,copy);
  }

  Type FreshType(Type typeExp, VarList nongen) {
    Map<Type,Type> copymap = new HashMap<Type,Type>();
    return Fresh(typeExp,nongen,copymap);
  }

  Type envRetrieve(String ide, Env env, VarList nongen) {
    for (; env != null; env = env.tail) {
      if (ide.equals(env.ide))
        return FreshType(env.exp,nongen);
    }
    throw new RuntimeException("Unbound ide: "+ide);
  }

  Type AnalyzeExp(Exp exp, Env env, VarList nongen) {
    switch (exp.type) {
    case INT:
      return IntType;
    case IDE:
      return envRetrieve(exp.ide,env,nongen);
    case COND:
      UnifyType(AnalyzeExp(exp.test,env,nongen),BoolType,nongen);
      Type typeOfThen = AnalyzeExp(exp.left,env,nongen);
      Type typeOfElse = AnalyzeExp(exp.right,env,nongen);
      UnifyType(typeOfThen,typeOfElse,nongen);
      return typeOfThen;
    case LAMB:
      Type typeOfBinder = NewTypeVar();
      exp.dbgvartype = typeOfBinder; // dbg
      Env bodyEnv = new Env(exp.ide,typeOfBinder,env);
      VarList bodyNonGen = new VarList(typeOfBinder,nongen);
      Type typeOfBody = AnalyzeExp(exp.body,bodyEnv,bodyNonGen);
      return FunType(typeOfBinder,typeOfBody);
    case APPL:
      Type typeOfFun = AnalyzeExp(exp.left,env,nongen);
      Type typeOfArg = AnalyzeExp(exp.right,env,nongen);
      Type typeOfRes = NewTypeVar();
      UnifyType(typeOfFun,FunType(typeOfArg,typeOfRes),nongen);
      return typeOfRes;
    case BLOCK:
      for (Decl decl = exp.decls; decl != null; decl = decl.next) {
        decl.newTypeVar = NewTypeVar();
        env = new Env(decl.binder,decl.newTypeVar,env);
      }
      for (Decl decl = exp.decls; decl != null; decl = decl.next)
        UnifyType(decl.newTypeVar,AnalyzeExp(decl.def,env,nongen),nongen);
      return AnalyzeExp(exp.body,env,nongen);
    }
    return null;
  }
}
