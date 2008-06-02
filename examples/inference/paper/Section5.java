import java.util.List;
import java.util.ArrayList;
import java.util.Map;

class Derivative {
  String name;
  Type funType;
  Exp exp;
  Derivative(String n, Type ft, Exp e)
  { name = n; funType = ft; exp = e; }
}

class Reference
{
  Exp from;
  Exp to;
  Reference(Exp f, Exp t)
  { from = f; to = t; }
}

public abstract class Section5 extends Section4 {

  void inferAndSpecialise(Exp exp) {
    List<Reference> refs = new ArrayList<Reference>();
    analyze(exp);
    findReferences(exp,refs);
    while (!refs.isEmpty()) {
      Reference current = refs.remove(0);
      specialise(current.from,current.to,refs);
    }
    removeUnusedFunctions(exp);
  }

  void findReferences(Exp exp, List<Reference> refs) {
    if (exp == null)
      return;
    if (exp.kind == ExpKind.VAR) {
      Decl decl = lookupDecl(exp.ident,exp);
      if ((decl != null) && (decl.def.kind == ExpKind.LAMBDA))
        refs.add(new Reference(exp,decl.def));
    }
    findReferences(exp.test,refs);
    findReferences(exp.left,refs);
    findReferences(exp.right,refs);
    findReferences(exp.body,refs);
    for (Decl decl = exp.decls; decl != null; decl = decl.next)
      findReferences(decl.def,refs);
  }

  Decl lookupDecl(String ide, Exp ctx) {
    for (; ctx != null; ctx = ctx.parent) {
      if ((ctx.kind == ExpKind.LAMBDA) && (ctx.ident.equals(ide)))
        return null;
      if (ctx.kind == ExpKind.LETREC) {
        for (Decl decl = ctx.decls; decl != null; decl = decl.next)
          if (decl.binder.equals(ide))
            return decl;
      }
    }
    return null;
  }

  boolean typeEquiv(Type type1, Type type2) {
    type1 = prune(type1);
    type2 = prune(type2);
    if (type1.isvar != type2.isvar)
      return false;
    if (type1.isvar)
      return (isCloneOf(type1,type2) || isCloneOf(type2,type1));
    if (!type1.ident.equals(type2.ident) ||
        (type1.args.length != type2.args.length))
      return false;
    for (int i = 0; i < type1.args.length; i++) {
      if (!typeEquiv(type1.args[i],type2.args[i]))
        return false;
    }
    return true;
  }

  boolean isCloneOf(Type maybeClone, Type orig) {
    maybeClone = prune(maybeClone);
    orig = prune(orig);
    if (maybeClone == orig)
      return true;
    for (Type clone : orig.clones) {
      if (isCloneOf(maybeClone,clone))
        return true;
    }
    return false;
  }

  public void specialise(Exp ref, Exp fun, List<Reference> refs) {
    assert(ref.kind == ExpKind.VAR); // debug
    assert(fun.parent != null); // debug
    assert(fun.parent.kind == ExpKind.LETREC); // debug
    if (typeEquiv(fun.type,ref.type))
      return;
    for (Derivative existing : fun.derivatives) {
      if (typeEquiv(ref.type,existing.funType)) {
        ref.ident = existing.name;
        return;
      }
    }
    ref.ident = addDerivative(fun,ref.type,ref.ident,refs);
  }

  public String addDerivative(Exp fun, Type type, String origName,
                              List<Reference> refs) {
    assert(fun.parent != null);
    assert(fun.parent.kind == ExpKind.LETREC);

    Exp letrec = fun.parent;
    Exp copy = dup(fun);
    setParents(copy,letrec);

    String newName = origName+"_"+genVarName();
    Decl newDecl = letrec.decls = decl(newName,copy,letrec.decls);
    newDecl.binderType = type;

    analyze(copy);
    unify(copy.type,type,letrec);
    findReferences(copy,refs);

    fun.derivatives.add(new Derivative(newName,type,copy));
    return newName;
  }

  Exp dup(Exp e) {
    switch (e.kind) {
    case CONST: return (e.type == IntType) ? iconst(e.value): dconst(e.dvalue);
    case VAR: return var(e.ident);
    case COND: return cond(dup(e.test),dup(e.left), dup(e.right));
    case LAMBDA: return lambda(e.ident,dup(e.body));
    case APPL: return appl(dup(e.left),dup(e.right));
    case LETREC: return letrec(dupDecl(e.decls),dup(e.body));
    }
    return null;
  }

  Decl dupDecl(Decl decl) {
    if (decl == null)
      return null;
    return decl(decl.binder,dup(decl.def),dupDecl(decl.next));
  }

  void removeUnusedFunctions(Exp topExp) {
    mark(topExp);
    sweep(topExp);
  }

  void mark(Exp exp) {
    if (exp == null)
      return;
    mark(exp.test);
    mark(exp.left);
    mark(exp.right);
    mark(exp.body);
    if (exp.kind == ExpKind.VAR) {
      Decl decl = lookupDecl(exp.ident,exp);
      if ((decl != null) && !decl.marked) {
        decl.marked = true;
        mark(decl.def);
      }
    }
  }

  void sweep(Exp exp) {
    if (exp == null)
      return;
    sweep(exp.test);
    sweep(exp.left);
    sweep(exp.right);
    sweep(exp.body);
    Decl prev = null, next;
    for (Decl decl = exp.decls; decl != null; decl = next) {
      next = decl.next;
      if (decl.marked) { // keep
        sweep(decl.def);
        decl.marked = false;
        prev = decl;
      }
      else { // remove
        if (prev == null)
          exp.decls = decl.next;
        else
          prev.next = decl.next;
      }
    }
  }

}
