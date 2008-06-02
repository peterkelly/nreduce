import java.util.*;

enum ExpKind { CONST, VAR, APPL, COND, LAMBDA, LETREC };

class Exp {
  ExpKind kind;
  String ident;
  int value;
  Exp test, left, right, body;
  Decl decls;

  Type type;
  Type binderType;
  Exp parent;

  public Exp(ExpKind k)
  { kind = k; }

  double dvalue;
  List<Derivative> derivatives = new ArrayList<Derivative>();
}

class Decl {
  String binder;
  Exp def;
  Type binderType;
  Decl next;
  boolean marked = false;
}

class Type {
  boolean isvar;
  String ident;
  Type[] args;
  Type instance;
  List<Type> clones = new ArrayList<Type>();
  TypeFunction function;

  Type(boolean v, String i, Type[] a)
  { isvar = v; ident = i; args = a; instance = null; }
}

public abstract class Section3 {

  public abstract String genVarName();
  public abstract void message(String msg);
  public abstract String typeString(Type type);

  Map<String,Type> builtins = new HashMap<String,Type>();

  Type BoolType = typeCons("bool",new Type[]{});
  Type IntType = typeCons("int",new Type[]{});
  Type DoubleType = typeCons("double",new Type[]{});
  Type AnyType = typeCons("*",new Type[]{});

  public Exp iconst(final int i)
  { return new Exp(ExpKind.CONST){{ type = IntType; value = i; }}; }

  public Exp var(final String name)
  { return new Exp(ExpKind.VAR){{ ident = name; }}; }

  public Exp appl(final Exp fun, final Exp arg)
  { return new Exp(ExpKind.APPL){{ left = fun; right = arg; }}; }

  public Exp cond(final Exp t, final Exp l, final Exp r)
  { return new Exp(ExpKind.COND){{ test = t; left = l; right = r; }}; }

  public Exp lambda(final String binder, final Exp b)
  { return new Exp(ExpKind.LAMBDA){{ ident = binder; body = b; }}; }

  public Exp letrec(final Decl dlist, final Exp scope)
  { return new Exp(ExpKind.LETREC){{ decls = dlist; body = scope; }}; }

  public Exp dconst(final double d)
  { return new Exp(ExpKind.CONST){{ type = DoubleType; dvalue = d; }}; }

  public Decl decl(final String b, final Exp d, final Decl n)
  { return new Decl(){{ binder = b; def = d; next = n; }}; }

  void setParents(Exp exp, Exp parent) {
    if (exp == null)
      return;
    assert(exp.parent == null);
    exp.parent = parent;
    setParents(exp.test,exp);
    setParents(exp.left,exp);
    setParents(exp.right,exp);
    setParents(exp.body,exp);
    for (Decl decl = exp.decls; decl != null; decl = decl.next)
      setParents(decl.def,exp);
  }

  Type typeVar()
  { return new Type(true,genVarName(),null); }

  Type typeCons(String oper, Type[] args)
  { return new Type(false,oper,args); }

  Type fun(Type dom, Type cod)
  { return typeCons("->",new Type[]{dom,cod}); }

  Type prune(Type type) {
    if (!type.isvar || (type.instance == null))
      return type;
    type.instance = prune(type.instance);
    return type.instance;
  }

  // overridden in Mycroft to ignore letrec bindings
  boolean isGeneric(Type var, Exp context) {
    for (; context != null; context = context.parent) {
      if ((context.kind == ExpKind.LAMBDA) &&
          occursIn(var,context.binderType))
        return false;
      // FIXME: test this
      Exp parent = context.parent;
      if ((parent != null) &&
          (parent.kind == ExpKind.LETREC) &&
          (parent.body != context)) {
        for (Decl decl = parent.decls; decl != null; decl = decl.next)
          if (occursIn(var,decl.binderType))
            return false;
      }
    }
    return true;
  }

  boolean occursIn(Type needle, Type haystack) {
    haystack = prune(haystack);
    if (haystack.isvar)
      return (needle == haystack);
    for (Type arg : haystack.args) {
      if (occursIn(needle,arg))
        return true;
    }
    return false;
  }

  // overridden in Mycroft to update clones list
  Type freshVar(Type var, Map<Type,Type> copymap, Exp context) {
    if (copymap.containsKey(var))
      return copymap.get(var);

    Type newVar = typeVar();
    copymap.put(var,newVar);
    return newVar;
  }

  Type fresh(Type type, Exp context, Map<Type,Type> copymap) {
    type = prune(type);
    if (type.isvar) {
      if (isGeneric(type,context))
        return freshVar(type,copymap,context);
      else
        return type;
    }
    else {
      Type[] copy = new Type[type.args.length];
      for (int i = 0; i < copy.length; i++)
        copy[i] = fresh(type.args[i],context,copymap);
      return typeCons(type.ident,copy);
    }
  }

  Type freshType(Type type, Exp context) {
    Map<Type,Type> copymap = new HashMap<Type,Type>();
    return fresh(type,context,copymap);
  }

  Type lookupType(String ident, Exp context) {
    // FIXME: should we start with context.parent?
    // (won't make any difference to functionality, since context will always be a VAR, but
    //  more accurately represents the use of a "different" type environment for the lambda
    //  abstraction rule)
    for (; context != null; context = context.parent) {
      if ((context.kind == ExpKind.LAMBDA) &&
          (context.ident.equals(ident))) {
        return context.binderType;
      }
      else if (context.kind == ExpKind.LETREC) {
        for (Decl decl = context.decls; decl != null; decl = decl.next)
          if (decl.binder.equals(ident))
            return decl.binderType;
      }
    }

    if (!builtins.containsKey(ident))
      throw new RuntimeException("Unbound identifier: "+ident);
    else
      return builtins.get(ident);
  }

  void instantiate(Type type1, Type type2, Exp context) {
    type1.instance = type2;
  }

  void unify(Type type1, Type type2, Exp context) {
    type1 = prune(type1);
    type2 = prune(type2);

//     message("Unify "+typeString(type1)+" and "+typeString(type2));

    if (type1 == type2)
      return;
    else if (type1.isvar && occursIn(type1,type2))
      throw new TypeClashException(type1,type2);
    else if (type1.isvar)
      instantiate(type1,type2,context);
    else if (type2.isvar)
      unify(type2,type1,context);
    else {
      if (!type1.ident.equals(type2.ident))
        throw new TypeClashException(type1,type2);
      if (type1.args.length != type2.args.length)
        throw new TypeClashException(type1,type2);
      for (int i = 0; i < type1.args.length; i++)
        unify(type1.args[i],type2.args[i],context);
    }
  }

  Type analyze(Exp exp) {
    switch (exp.kind) {
    case CONST:
      return exp.type; // already assigned by parser
    case VAR:
      return exp.type = freshType(lookupType(exp.ident,exp),exp);
    case APPL:
      Type funType = analyze(exp.left);
      Type argType = analyze(exp.right);
      Type resType = typeVar();
      unify(funType,fun(argType,resType),exp);
      return exp.type = resType;
    case COND:
      unify(analyze(exp.test),BoolType,exp);
      Type thenType = analyze(exp.left);
      Type elseType = analyze(exp.right);
      unify(thenType,elseType,exp);
      return exp.type = thenType;
    case LAMBDA:
      if (exp.binderType == null)
        exp.binderType = typeVar();
      Type bodyType = analyze(exp.body);
      return exp.type = fun(exp.binderType,bodyType);
    case LETREC:
      for (Decl decl = exp.decls; decl != null; decl = decl.next)
        decl.binderType = typeVar();
      for (Decl decl = exp.decls; decl != null; decl = decl.next)
        unify(decl.binderType,analyze(decl.def),exp);
      return exp.type = analyze(exp.body);
    }
    return null;
  }
}
