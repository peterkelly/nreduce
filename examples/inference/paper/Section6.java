import java.util.Map;

interface TypeFunction {
  public Type computeType(Type[] args);
}

public abstract class Section6 extends Section5
{
  Type prune(Type type) {
    if (type.isvar && (type.function != null)) {
      assert(type.instance == null);
      for (int i = 0; i < type.args.length; i++)
        type.args[i] = prune(type.args[i]);
      Type computed = type.function.computeType(type.args);
      if (computed == null) 
        return type;
      type.function = null;
      type.args = null;
      type.instance = prune(computed);
      return type.instance;
    }

    // original prune logic
    if (!type.isvar || (type.instance == null))
      return type;
    type.instance = prune(type.instance);
    return type.instance;
  }

  void instantiate(Type type1, Type type2, Exp context) {
    type1.function = null;
    type1.args = null;
    removeClone(type1,type2);
    if (containsClone(type1,type2))
      throw new RuntimeException("Recursion detected: Type is undecidable");
    type1.instance = type2;
    for (Type clone : type1.clones) // Propagate update
      unify(clone,freshType(type2,context),context);
    type1.clones.clear();
  }

  Type freshVar(Type var, Map<Type,Type> copymap, Exp context) {
    if (copymap.containsKey(var))
      return copymap.get(var);

    Type newVar = typeVar();
    copymap.put(var,newVar);
    var.clones.add(newVar);
    // added
    if (var.function != null) {
      newVar.function = var.function;
      newVar.args = new Type[var.args.length];
      for (int i = 0; i < newVar.args.length; i++)
        newVar.args[i] = fresh(var.args[i],context,copymap);
    }
    return newVar;
  }

  class Arith implements TypeFunction {
    public Type computeType(Type[] args) {
      if (args[0].isvar || args[1].isvar)
        return null; // not yet known
      String name0 = args[0].ident, name1 = args[1].ident;
      if (name0.equals("double") && name1.equals("double"))
        return DoubleType;
      else if (name0.equals("double") && name1.equals("int"))
        return DoubleType;
      else if (name0.equals("int") && name1.equals("double"))
        return DoubleType;
      else if (name0.equals("int") && name1.equals("int"))
        return IntType;
      else
        throw new RuntimeException("Invalid type argument");
    }
  }

  boolean dependsOn(Type fun, Type on) {
    fun = prune(fun);
    on = prune(on);
    if (fun == on)
      return true;
    if (fun.isvar && (fun.function != null)) {
      for (Type arg : fun.args)
        if (dependsOn(arg,on))
          return true;
    }
    return false;
  }

  void unify(Type type1, Type type2, Exp context) {
    type1 = prune(type1);
    type2 = prune(type2);

    if (type1 == type2)
      return;
    else if (type1.isvar && type2.isvar &&
             (type1.function != null) && (type2.function == null))
      unify(type2,type1,context);
    else if (type1.isvar && type2.isvar && dependsOn(type1,type2))
      instantiate(type1,type2,context);
    else if (type1.isvar && type2.isvar && dependsOn(type2,type1))
      instantiate(type2,type1,context);
    // previous unify code
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

}
