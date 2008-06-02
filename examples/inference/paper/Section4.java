import java.util.Map;

public abstract class Section4 extends Section3 {

  boolean isGeneric(Type var, Exp ctx) {
    for (; ctx != null; ctx = ctx.parent) {
      if ((ctx.kind == ExpKind.LAMBDA) && occursIn(var,ctx.binderType))
        return false;
    }
    return true;
  }

  boolean containsClone(Type orig, Type maybeClone) {
    for (Type clone : orig.clones) {
      if (occursIn(clone,maybeClone) || containsClone(clone,maybeClone))
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

  void instantiate(Type type1, Type type2, Exp context)
  {
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
    var.clones.add(newVar); // added
    return newVar;
  }

}
