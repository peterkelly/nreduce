import java.util.List;
import java.util.ArrayList;

public class HindleyMilner
{
  public enum ExpType { INT, IDE, COND, LAMB, APPL, BLOCK };
  public enum DeclType { DEF, SEQ, REC };
  public enum TypeExpType { VARTYPE, OPERTYPE };

  public static class Node
  {
    int dbgnum;
    TypeExp dbgtype;
    TypeExp dbgvartype;
  }

  public static class Exp extends Node
  {
    ExpType type;
    // INT
    int val;
    // IDE
    String ide;
    // COND
    Exp test;
    Exp ifTrue;
    Exp ifFalse;
    // LAMB
    String binder;
    Exp body;
    // APPL
    Exp fun;
    Exp arg;
    // BLOCK
    Decl decl;
    Exp scope;
  }

  public static class Decl extends Node
  {
    DeclType type;
    String binder;
    Exp def;
    Decl first;
    Decl second;
    Decl rec;
    TypeExp newTypeVar;
  }

  public static class TypeExp
  {
    TypeExpType type;
    TypeExp instance;
    String ide;
    TypeList args;
    boolean dbgrenamed;
    List<TypeExp> clones = new ArrayList<TypeExp>();
  }

  public static class TypeList
  {
    TypeExp head;
    TypeList tail;
  }

  public static class NonGenericVars extends TypeList
  {
  }

  // Implementation module TypeMod

  public static TypeExp NewTypeVar()
  {
    TypeExp r = new TypeExp();
    r.type = TypeExpType.VARTYPE;
    r.instance = null;
    r.ide = Inference.genVarName();
    return r;
  }

  public static TypeExp NewTypeOper(String ide, TypeList args)
  {
    TypeExp r = new TypeExp();
    r.type = TypeExpType.OPERTYPE;
    r.ide = ide;
    r.args = args;
    return r;
  }

  public static TypeList Extend(TypeExp head, TypeList tail)
  {
    TypeList r = new TypeList();
    r.head = head;
    r.tail = tail;
    return r;
  }

  public static boolean SameType(TypeExp typeExp1, TypeExp typeExp2)
  {
    return (typeExp1 == typeExp2);
  }

  public static TypeExp Prune(TypeExp typeExp)
  {
    switch (typeExp.type) {
    case VARTYPE:
      if (typeExp.instance == null) {
        return typeExp;
      }
      else {
        typeExp.instance = Prune(typeExp.instance);
        return typeExp.instance;
      }
    case OPERTYPE:
      return typeExp;
    }
    assert(false);
    return null;
  }

  public static boolean OccursInType(TypeExp typeVar, TypeExp typeExp)
  {
    typeExp = Prune(typeExp);
    switch (typeExp.type) {
    case VARTYPE:
      return SameType(typeVar,typeExp);
    case OPERTYPE:
      return OccursInTypeList(typeVar,typeExp.args);
    }
    assert(false);
    return false;
  }

  public static boolean OccursInTypeList(TypeExp typeVar, TypeList list)
  {
    if (list == null)
      return false;
    if (OccursInType(typeVar,list.head))
      return true;
    return OccursInTypeList(typeVar,list.tail);
  }

  public static boolean containsClone(TypeExp orig, TypeExp maybeClone)
  {
    for (int i = 0; i < orig.clones.size(); i++) {
      TypeExp clone = orig.clones.get(i);
      if (OccursInType(clone,maybeClone)) {
        return true;
      }
      else if (containsClone(clone,maybeClone)) {
        return true;
      }
    }
    return false;
  }

  public static boolean removeClone(TypeExp orig, TypeExp maybeClone)
  {
    for (int i = 0; i < orig.clones.size(); i++) {
      TypeExp clone = orig.clones.get(i);
      if (clone == maybeClone) {
        orig.clones.remove(i);
        return true;
      }
      else if (removeClone(clone,maybeClone)) {
        return true;
      }
    }
    return false;
  }

  public static void UnifyType(TypeExp typeExp1, TypeExp typeExp2, NonGenericVars list)
  {
    Inference.traceUnifyStart(typeExp1,typeExp2);
    typeExp1 = Prune(typeExp1);
    typeExp2 = Prune(typeExp2);

    switch (typeExp1.type) {
    case VARTYPE:
      if (OccursInType(typeExp1,typeExp2)) {
        if (!SameType(typeExp1,typeExp2))
          throw new RuntimeException("Type clash");
      }
      else {

        if (removeClone(typeExp1,typeExp2)) {
          Inference.message(Inference.typeString(typeExp2)+" was clone of "+
                            Inference.typeString(typeExp1));
        }

        if (containsClone(typeExp1,typeExp2)) {
          throw new RuntimeException("Recursion detected: Type is undecidable");
//           throw new RuntimeException(Inference.typeString(typeExp2)+" contains a clone of "+
//                                      Inference.typeString(typeExp1));
        }

        typeExp1.instance = typeExp2;

        // Propogate update to clones
        for (TypeExp clone : typeExp1.clones) {
          TypeExp ft = FreshType(typeExp2,list);
          UnifyType(clone,ft,list);
        }
        typeExp1.clones.clear();
      }
      break;
    case OPERTYPE:
      switch (typeExp2.type) {
      case VARTYPE:
        UnifyType(typeExp2,typeExp1,list);
        break;
      case OPERTYPE:
        if (typeExp1.ide.equals(typeExp2.ide))
          UnifyArgs(typeExp1.args,typeExp2.args,list);
        else
          throw new RuntimeException("type clash");
        break;
      }
      break;
    }
    Inference.traceUnifyEnd(typeExp1,typeExp2);
  }

  public static void UnifyArgs(TypeList list1, TypeList list2, NonGenericVars list)
  {
    if ((list1 == null) && (list2 == null))
      return;
    if ((list1 == null) || (list2 == null))
      throw new RuntimeException("Type clash");
    else {
      UnifyType(list1.head,list2.head,list);
      UnifyArgs(list1.tail,list2.tail,list);
    }
  }

  // Implementation module GenericVarMod

  public static NonGenericVars ExtendNGV(TypeExp head, NonGenericVars tail)
  {
    NonGenericVars r = new NonGenericVars();
    r.head = head;
    r.tail = tail;
    return r;
  }

  public static boolean IsGeneric(TypeExp typeVar, NonGenericVars list)
  {
    return !OccursInTypeList(typeVar,list);
  }

  public static class CopyEnv
  {
    TypeExp old;
    TypeExp newx;
    CopyEnv tail;
  }

  public static CopyEnv ExtendCopyEnv(TypeExp old, TypeExp newx, CopyEnv tail)
  {
    CopyEnv r;
    r = new CopyEnv();
    r.old = old;
    r.newx = newx;
    r.tail = tail;
    return r;
  }

  public static TypeExp FreshVar(TypeExp typeVar, CopyEnv scan, CopyEnv env[])
  {
    TypeExp newTypeVar;
    if (scan == null) {
      newTypeVar = NewTypeVar();
      env[0] = ExtendCopyEnv(typeVar,newTypeVar,env[0]);
      typeVar.clones.add(newTypeVar);
      return newTypeVar;
    }
    else if (SameType(typeVar,scan.old)) {
      return scan.newx;
    }
    else {
      return FreshVar(typeVar,scan.tail,env);
    }
  }

  public static TypeExp Fresh(TypeExp typeExp, NonGenericVars list, CopyEnv env[])
  {
    typeExp = Prune(typeExp);
    switch (typeExp.type) {
    case VARTYPE:
      if (IsGeneric(typeExp,list))
        return FreshVar(typeExp,env[0],env);
      else
        return typeExp;
    case OPERTYPE:
      return NewTypeOper(typeExp.ide,
                         FreshList(typeExp.args,list,env));
    }
    assert(false);
    return null;
  }

  public static TypeList FreshList(TypeList args, NonGenericVars list, CopyEnv env[])
  {
    if (args == null)
      return null;
    return Extend(Fresh(args.head,list,env),FreshList(args.tail,list,env));
  }

  public static TypeExp FreshType(TypeExp typeExp, NonGenericVars list)
  {
    CopyEnv[] env = new CopyEnv[]{null};
    return Fresh(typeExp,list,env);
  }

  // Implementation module EnvMod

  public static class Env
  {
    String ide;
    TypeExp typeExp;
    Env tail;
  }

  public static Env envExtend(String ide, TypeExp typeExp, Env tail)
  {
    Env r;
    r = new Env();
    r.ide = ide;
    r.typeExp = typeExp;
    r.tail = tail;
    return r;
  }

  public static TypeExp envRetrieve(String ide, Env env, NonGenericVars list)
  {
    Inference.traceEnvRetrieveStart(ide,env,list);
    TypeExp res = envRetrieve1(ide,env,list);
    Inference.traceEnvRetrieveEnd(ide,env,list,res);
    return res;
  }

  public static TypeExp envRetrieve1(String ide, Env env, NonGenericVars list)
  {
    if (env == null)
      throw new RuntimeException("Unbound ide: "+ide);
    else if (ide.equals(env.ide))
      return FreshType(env.typeExp,list);
    else
      return envRetrieve1(ide,env.tail,list);
  }

  // Implementation module TypecheckMod

  public static TypeExp FunType(TypeExp dom, TypeExp cod)
  {
    return NewTypeOper("->",Extend(dom,Extend(cod,null)));
  }

  public static TypeExp AnalyzeExp(Exp exp, Env env, NonGenericVars list)
  {
    Inference.traceAnalyzeStart(exp,env,list);
    exp.dbgtype = AnalyzeExp1(exp,env,list);
    Inference.traceAnalyzeEnd(exp,env,list);
    return exp.dbgtype;
  }

  public static TypeExp AnalyzeExp1(Exp exp, Env env, NonGenericVars list)
  {
    TypeExp typeOfThen, typeOfElse, typeOfBinder, typeOfBody, typeOfFun, typeOfArg, typeOfRes;
    Env bodyEnv, declEnv;
    NonGenericVars bodyList;

    switch (exp.type) {
    case INT:
      return IntType;
    case IDE:
      return envRetrieve(exp.ide,env,list);
    case COND:
      UnifyType(AnalyzeExp(exp.test,env,list),BoolType,list);
      typeOfThen = AnalyzeExp(exp.ifTrue,env,list);
      typeOfElse = AnalyzeExp(exp.ifFalse,env,list);
      UnifyType(typeOfThen,typeOfElse,list);
      return typeOfThen;
    case LAMB:
      typeOfBinder = NewTypeVar();
      exp.dbgvartype = typeOfBinder; // dbg
      bodyEnv = envExtend(exp.binder,typeOfBinder,env);
      bodyList = ExtendNGV(typeOfBinder,list);
      typeOfBody = AnalyzeExp(exp.body,bodyEnv,bodyList);
      return FunType(typeOfBinder,typeOfBody);
    case APPL:
      typeOfFun = AnalyzeExp(exp.fun,env,list);
      typeOfArg = AnalyzeExp(exp.arg,env,list);
      typeOfRes = NewTypeVar();
      UnifyType(typeOfFun,FunType(typeOfArg,typeOfRes),list);
      return typeOfRes;
    case BLOCK:
      declEnv = AnalyzeDecl(exp.decl,env,list);
      return AnalyzeExp(exp.scope,declEnv,list);
    default:
      assert(false);
    }
    return null;
  }

  public static Env AnalyzeDecl(Decl decl, Env env, NonGenericVars list)
  {
    switch (decl.type) {
    case DEF:
      return envExtend(decl.binder,AnalyzeExp(decl.def,env,list),env);
    case SEQ:
      return AnalyzeDecl(decl.second,AnalyzeDecl(decl.first,env,list),list);
    case REC: {
      Env[] envarr = new Env[]{env};
      NonGenericVars[] listarr = new NonGenericVars[]{list};
      AnalyzeRecDeclBind(decl.rec,envarr,listarr);
      AnalyzeRecDecl(decl.rec,envarr[0],listarr[0]);
      return envarr[0];
    }
    }
    assert(false);
    return null;
  }

  public static void AnalyzeRecDeclBind(Decl decl, Env env[], NonGenericVars list[])
  {
    switch (decl.type) {
    case DEF:
      decl.newTypeVar = NewTypeVar();
      decl.dbgvartype = decl.newTypeVar; // dbg
      env[0] = envExtend(decl.binder,decl.newTypeVar,env[0]);
      break;
    case SEQ:
      AnalyzeRecDeclBind(decl.first,env,list);
      AnalyzeRecDeclBind(decl.second,env,list);
      break;
    case REC:
      AnalyzeRecDeclBind(decl.rec,env,list);
      break;
    }
  }

  public static void AnalyzeRecDecl(Decl decl, Env env, NonGenericVars list)
  {
    switch (decl.type) {
    case DEF:
      UnifyType(decl.newTypeVar,AnalyzeExp(decl.def,env,list),list);
      break;
    case SEQ:
      AnalyzeRecDecl(decl.first,env,list);
      AnalyzeRecDecl(decl.second,env,list);
      break;
    case REC:
      AnalyzeRecDecl(decl.rec,env,list);
      break;
    }
  }

  public static TypeExp BoolType = NewTypeOper("bool",null);
  public static TypeExp IntType = NewTypeOper("int",null);

}
