/* The type checker (see front/check.h). SHARED by the tcc kits.

   Checking is bidirectional. A term is checked against an expected type (the
   hint) or its type is inferred. Two types are equal when their normal forms
   are equal (front/eval.c). The checker writes core terms (front/core.h). */
#include "front/check.h"

#include <string.h>

#define CHECK_DEPTH_LIMIT 2000u
#define SPINE_MAX 64u
#define SHOW_MAX 512u
#define NAMES_MAX 64u
#define UNIVERSE_MAX 2u
#define PRINT_MAX 65536u
#define NO_FAMILY UINT32_MAX
#define NO_DEF UINT32_MAX

#define FAIL(c, code, ...) diag_fail((c)->m->diag, (code), (c)->m->def, __VA_ARGS__)

typedef struct Scope Scope;
struct Scope {
  const char *name; /* NULL for the binder of `A -> B` */
  const Value *type;
  uint32_t level;
  const Scope *next;
};

typedef struct {
  Machine *m;
  const DeclList *decls;
  size_t decl_index;
  const Scope *scope; /* the innermost binder first */
  const Env *env;     /* a variable for each binder */
  const Value *face;  /* the faces assumed by enclosing system branches */
  uint32_t level;     /* the number of binders */
  unsigned depth;
  uint32_t eta_count;
  uint32_t self;      /* the family whose fields are checked, or NO_FAMILY */
  uint32_t self_uses; /* the uses of that family in the current field */
  uint32_t sib_uses;  /* the uses of the other sorts of its block there */
  int provisional_universes; /* the first pass of a mutual block only */
} Checker;

typedef enum {
  RULE_TYPE0,
  RULE_TYPE1,
  RULE_TYPE2,
  RULE_EQ,
  RULE_VALUE0,
  RULE_PAIR,
  RULE_INJ,
  RULE_EMPTY,
  RULE_SOME,
  RULE_CONS,
  RULE_PACK,
  RULE_REFL,
  RULE_PART,
  RULE_SIGMA_PART,
  RULE_EITHER,
  RULE_OPTION,
  RULE_PURE,
  RULE_MAP,
  RULE_BIND,
  RULE_FILTER,
  RULE_FOLD,
  RULE_UNFOLD,
  RULE_SYMM,
  RULE_TRANS,
  RULE_CONG,
  RULE_TRANSPORT,
  RULE_NAT2,
  RULE_NAT_IS_SET, /* D4 */
  RULE_FLAG_IF,
  RULE_INTERVAL, /* former C1 */
  RULE_PATHP,    /* former C2 */
  RULE_PAPP,
  RULE_PLAM,
  RULE_FUNEXT,
  RULE_PARTIAL,
  RULE_SYSTEM,
  RULE_FROM_PARTIAL,
  RULE_TRANSP,
  RULE_HCOMP,
  RULE_COMP,
  RULE_FILL,
  RULE_GLUE, /* former C6 */
  RULE_GLUE_ELEM,
  RULE_UNGLUE
} Rule;

typedef struct {
  const char *name;
  uint32_t arity; /* fold: the least number of arguments */
  Rule rule;
  Op op;
  Op result;     /* VALUE0, NAT2: the result type. EMPTY, INJ: the type former. */
  uint32_t part; /* INJ, PART, SIGMA_PART: 0 or 1 */
} Builtin;

static const Builtin BUILTINS[] = {
  {"Nat", 0, RULE_TYPE0, OP_NAT, OP_NAT, 0},
  {"Flag", 0, RULE_TYPE0, OP_FLAG, OP_NAT, 0},
  {"Unit", 0, RULE_TYPE0, OP_UNIT, OP_NAT, 0},
  {"Prod", 2, RULE_TYPE2, OP_PROD, OP_NAT, 0},
  {"Sum", 2, RULE_TYPE2, OP_SUM, OP_NAT, 0},
  {"Option", 1, RULE_TYPE1, OP_OPTION, OP_NAT, 0},
  {"List", 1, RULE_TYPE1, OP_LIST, OP_NAT, 0},
  {"Eq", 3, RULE_EQ, OP_EQ, OP_NAT, 0},
  {"unit", 0, RULE_VALUE0, OP_UNIT_VAL, OP_UNIT, 0},
  {"flagYes", 0, RULE_VALUE0, OP_FLAG_YES, OP_FLAG, 0},
  {"flagNo", 0, RULE_VALUE0, OP_FLAG_NO, OP_FLAG, 0},
  {"pair", 2, RULE_PAIR, OP_PAIR, OP_PROD, 0},
  {"inl", 1, RULE_INJ, OP_INL, OP_SUM, 0},
  {"inr", 1, RULE_INJ, OP_INR, OP_SUM, 1},
  {"none", 0, RULE_EMPTY, OP_NONE, OP_OPTION, 0},
  {"nil", 0, RULE_EMPTY, OP_NIL, OP_LIST, 0},
  {"some", 1, RULE_SOME, OP_SOME, OP_OPTION, 0},
  {"cons", 2, RULE_CONS, OP_CONS, OP_LIST, 0},
  {"pack", 2, RULE_PACK, OP_PACK, OP_NAT, 0},
  {"refl", 0, RULE_REFL, OP_REFL, OP_EQ, 0},
  {"first", 1, RULE_PART, OP_FIRST, OP_PROD, 0},
  {"second", 1, RULE_PART, OP_SECOND, OP_PROD, 1},
  {"witness", 1, RULE_SIGMA_PART, OP_WITNESS, OP_NAT, 0},
  {"payload", 1, RULE_SIGMA_PART, OP_PAYLOAD, OP_NAT, 1},
  {"either", 3, RULE_EITHER, OP_EITHER, OP_SUM, 0},
  {"option", 3, RULE_OPTION, OP_OPTION_ELIM, OP_OPTION, 0},
  {"pure", 1, RULE_PURE, OP_PURE, OP_NAT, 0},
  {"map", 2, RULE_MAP, OP_MAP, OP_NAT, 0},
  {"bind", 2, RULE_BIND, OP_BIND, OP_NAT, 0},
  {"filter", 2, RULE_FILTER, OP_FILTER, OP_NAT, 0},
  {"fold", 2, RULE_FOLD, OP_FOLD_NAT, OP_NAT, 0},
  {"unfold", 3, RULE_UNFOLD, OP_UNFOLD, OP_LIST, 0},
  {"symm", 1, RULE_SYMM, OP_SYMM, OP_EQ, 0},
  {"trans", 2, RULE_TRANS, OP_TRANS, OP_EQ, 0},
  {"cong", 2, RULE_CONG, OP_CONG, OP_EQ, 0},
  {"transport", 3, RULE_TRANSPORT, OP_TRANSPORT, OP_EQ, 0},
  {"natAdd", 2, RULE_NAT2, OP_NAT_ADD, OP_NAT, 0},
  {"natSub", 2, RULE_NAT2, OP_NAT_SUB, OP_NAT, 0},
  {"natMul", 2, RULE_NAT2, OP_NAT_MUL, OP_NAT, 0},
  {"natEq", 2, RULE_NAT2, OP_NAT_EQ, OP_FLAG, 0},
  {"natLe", 2, RULE_NAT2, OP_NAT_LE, OP_FLAG, 0},
  {"natMax", 2, RULE_NAT2, OP_NAT_MAX, OP_NAT, 0},
  {"natIsSet", 4, RULE_NAT_IS_SET, OP_NAT_IS_SET, OP_NAT, 0},
  {"flagIf", 3, RULE_FLAG_IF, OP_FLAG_IF, OP_FLAG, 0},
  /* Former C1 and C2. The parser makes the heads of ~ r, r /\ s, r \/ s,
     p @ r and <i> t; no identifier can spell them. */
  {"I", 0, RULE_INTERVAL, OP_I, OP_SSET, 0},
  {"i0", 0, RULE_INTERVAL, OP_I0, OP_I, 0},
  {"i1", 0, RULE_INTERVAL, OP_I1, OP_I, 0},
  {"~", 1, RULE_INTERVAL, OP_INEG, OP_I, 0},
  {"/\\", 2, RULE_INTERVAL, OP_IMIN, OP_I, 0},
  {"\\/", 2, RULE_INTERVAL, OP_IMAX, OP_I, 0},
  {"PathP", 3, RULE_PATHP, OP_PATHP, OP_NAT, 0},
  {"Path", 3, RULE_PATHP, OP_PATHP, OP_NAT, 1},
  {"@", 2, RULE_PAPP, OP_PAPP, OP_NAT, 0},
  {"<>", 1, RULE_PLAM, OP_PLAM, OP_NAT, 0},
  {"funExt", 1, RULE_FUNEXT, OP_PLAM, OP_NAT, 0},
  /* Faces and systems (former C1). The parser makes the heads of
     [ f |-> u , ... ]; no identifier can spell them. */
  {"Partial", 2, RULE_PARTIAL, OP_PARTIAL, OP_SSET, 0},
  {"|->", 3, RULE_SYSTEM, OP_SYSTEM, OP_NAT, 0},
  {"[]", 0, RULE_SYSTEM, OP_SYSNIL, OP_NAT, 0},
  {"fromPartial", 1, RULE_FROM_PARTIAL, OP_FROMPARTIAL, OP_NAT, 0},
  /* Kan operations (former C3). */
  {"transp", 3, RULE_TRANSP, OP_TRANSP, OP_NAT, 0},
  {"hcomp", 2, RULE_HCOMP, OP_HCOMP, OP_NAT, 0},
  {"comp", 3, RULE_COMP, OP_HCOMP, OP_NAT, 0},
  {"fill", 4, RULE_FILL, OP_HCOMP, OP_NAT, 0},
  /* Glue types (former C6). */
  {"Glue", 2, RULE_GLUE, OP_GLUE, OP_NAT, 0},
  {"glue", 2, RULE_GLUE_ELEM, OP_GLUE_ELEM, OP_NAT, 0},
  {"unglue", 1, RULE_UNGLUE, OP_UNGLUE, OP_NAT, 0}
};

#define BUILTIN_COUNT ((uint32_t)(sizeof BUILTINS / sizeof BUILTINS[0]))

/* Names that no declaration can take (rule R4). */
static const char *const RESERVED[] = {"Text"};

typedef enum {
  HEAD_TERM, /* a local, a definition or another term: plain application */
  HEAD_FAMILY,
  HEAD_CTOR,
  HEAD_PROJ,
  HEAD_BUILTIN
} HeadKind;

typedef struct {
  HeadKind kind;
  uint32_t index; /* FAMILY: the family. CTOR, PROJ: the constructor. */
  uint32_t field; /* PROJ: the field number */
  const Builtin *builtin;
} Head;

/* One application of a core name to the arguments of a spine. */
typedef struct {
  Checker *c;
  const Builtin *b;
  const Term *const *args;
  uint32_t argc;      /* the arguments in the spine */
  const Value *hint;  /* the expected type of the result, or NULL */
  const Value *outer; /* the expected type of the whole spine, or NULL */
  uint32_t used;      /* the arguments that the core name takes */
  const Value *type;  /* the type of the result */
} Call;

static const Core *nul(int failed) {
  (void)failed;
  return NULL;
}

static Core *new_core(Checker *c, CoreKind kind) {
  Core *k = arena_alloc(c->m->arena, sizeof *k);
  if (k == NULL) {
    FAIL(c, "OOM", "out of memory");
    return NULL;
  }
  k->kind = kind;
  return k;
}

static const Core *core_index(Checker *c, CoreKind kind, uint32_t index) {
  Core *k = new_core(c, kind);
  if (k != NULL)
    k->index = index;
  return k;
}

static const Core *core_leaf(Checker *c, CoreKind kind, uint64_t nat) {
  Core *k = new_core(c, kind);
  if (k != NULL)
    k->nat = nat;
  return k;
}

static const Core *core_bind(Checker *c, CoreKind kind, const char *name, const Core *a, const Core *b) {
  Core *k;
  if (b == NULL || (a == NULL && kind != CORE_LAM))
    return NULL;
  k = new_core(c, kind);
  if (k == NULL)
    return NULL;
  k->name = name;
  k->a = a;
  k->b = b;
  return k;
}

static const Core *core_app(Checker *c, const Core *f, const Core *x) {
  Core *k;
  if (f == NULL || x == NULL)
    return NULL;
  k = new_core(c, CORE_APP);
  if (k == NULL)
    return NULL;
  k->a = f;
  k->b = x;
  return k;
}

static const Core *core_op(Checker *c, Op op, uint32_t inst, uint32_t field, const Core *const *args, uint32_t n) {
  const Core **copy = arena_alloc(c->m->arena, ((size_t)n + 1u) * sizeof *copy);
  Core *k;
  uint32_t i;
  if (copy == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  for (i = 0; i < n; i++) {
    if (args[i] == NULL)
      return NULL;
    copy[i] = args[i];
  }
  k = new_core(c, CORE_OP);
  if (k == NULL)
    return NULL;
  k->op = op;
  k->inst = inst;
  k->field = field;
  k->args = copy;
  k->argc = n;
  return k;
}

static const Core *op3(Checker *c, Op op, uint32_t inst, uint32_t n, const Core *a, const Core *b, const Core *x) {
  const Core *args[3];
  args[0] = a;
  args[1] = b;
  args[2] = x;
  return core_op(c, op, inst, 0, args, n);
}

static const Core *op4(Checker *c, Op op, const Core *a, const Core *b, const Core *x, const Core *y) {
  const Core *args[4];
  args[0] = a;
  args[1] = b;
  args[2] = x;
  args[3] = y;
  return core_op(c, op, 0, 0, args, 4);
}

static const Value *tyop(Checker *c, Op op, uint32_t n, const Value *a, const Value *b, const Value *x) {
  return val_make(c->m, op, 0, n, a, b, x);
}

static const Value *nat_type(Checker *c) {
  return tyop(c, OP_NAT, 0, NULL, NULL, NULL);
}

static const Value *flag_type(Checker *c) {
  return tyop(c, OP_FLAG, 0, NULL, NULL, NULL);
}

/* The value of the core term K in the current context. */
static const Value *here(Checker *c, const Core *k) {
  return k == NULL ? NULL : eval_core(c->m, c->env, k);
}

/* V in the surface syntax, for a diagnostic. */
static const char *show(Checker *c, const Value *v) {
  const char *names[NAMES_MAX];
  const Scope *s;
  char *buf = arena_alloc(c->m->arena, SHOW_MAX);
  uint32_t count = c->level < NAMES_MAX ? c->level : NAMES_MAX;
  for (s = c->scope; s != NULL; s = s->next) {
    if (s->level < NAMES_MAX)
      names[s->level] = s->name != NULL ? s->name : "_";
  }
  if (buf == NULL || v == NULL)
    return "?";
  value_print(c->m, names, count, v, buf, SHOW_MAX);
  return buf;
}

static int push(Checker *c, const char *name, const Value *type) {
  Scope *s = arena_alloc(c->m->arena, sizeof *s);
  const Env *e = env_push(c->m, c->env, val_var(c->m, c->level));
  if (type == NULL || s == NULL || e == NULL)
    return c->m->diag->set ? 0 : FAIL(c, "OOM", "out of memory");
  s->name = name;
  s->type = type;
  s->level = c->level;
  s->next = c->scope;
  c->scope = s;
  c->env = e;
  c->level++;
  return 1;
}

static void pop(Checker *c) {
  c->scope = c->scope->next;
  c->env = c->env->next;
  c->level--;
}

/* Face equality in the ambient face context. */
static int same_face(Checker *c, const Value *a, const Value *b) {
  if (c->face != NULL) {
    a = tyop(c, OP_IMIN, 2, c->face, a, NULL);
    b = tyop(c, OP_IMIN, 2, c->face, b, NULL);
  }
  return a != NULL && b != NULL && face_equal(c->m, c->level, a, b);
}

/* 1 when a term of type FOUND can stand where WANT is expected.
   Universes are not cumulative (former F14). A pretype (sort SSet) is in no
   universe (former C1). */
static int conv_on(Checker *c, uint32_t level, const Value *a, const Value *b);

static int expect(Checker *c, const Value *found, const Value *want) {
  if (found == NULL || want == NULL)
    return 0;
  /* Constructor shapes are collected before mutual universe inference.
     Every constructor is checked again with exact levels before success. */
  if (c->provisional_universes && found->kind == VAL_UNIV && want->kind == VAL_UNIV)
    return 1;
  if (conv_on(c, c->level, found, want))
    return 1;
  if (val_is(found, OP_SSET))
    return FAIL(c, "TYPE_PRETYPE", "expected a term of type %s, found a pretype (its sort is SSet)", show(c, want));
  return FAIL(c, "TYPE_MISMATCH", "expected a term of type %s, found a term of type %s", show(c, want), show(c, found));
}

static const Scope *find_local(const Checker *c, const char *name) {
  const Scope *s;
  for (s = c->scope; s != NULL; s = s->next) {
    if (s->name != NULL && strcmp(s->name, name) == 0)
      return s;
  }
  return NULL;
}

static uint32_t find_def(const Machine *m, const char *name) {
  uint32_t i;
  for (i = 0; i < m->def_count; i++) {
    if (strcmp(m->defs[i].name, name) == 0)
      return i;
  }
  return NO_DEF;
}

/* An environment that binds VALS in order: the last one is index 0. */
static const Env *env_of(Checker *c, const Value *const *vals, uint32_t n) {
  const Env *e = NULL;
  uint32_t i;
  for (i = 0; i < n; i++)
    e = env_push(c->m, e, vals[i]);
  return e;
}

static const Core *elab(Checker *c, const Term *t, const Value *hint, const Value **type);
static const Core *check_lam(Checker *c, const Term *t, const Value *hint, const Value **type);
static int annotation_ok(Checker *c, const Term *ann, const Value *dom);
static const Core *apply_rest(Checker *c, const Core *k, const Value *ty, const Term *const *args, uint32_t argc, const Value *hint, const Value **type);

static const Core *check(Checker *c, const Term *t, const Value *want) {
  const Value *type = NULL;
  return want == NULL ? NULL : elab(c, t, want, &type);
}

static const Core *infer(Checker *c, const Term *t, const Value **type) {
  *type = NULL;
  return elab(c, t, NULL, type);
}

/* infer, but a failure leaves no diagnostic. */
static const Core *try_infer(Checker *c, const Term *t, const Value **type) {
  Diag saved = *c->m->diag;
  const Core *k = infer(c, t, type);
  if (k == NULL)
    *c->m->diag = saved;
  return k;
}

/* T as a type; *LEVEL is its universe. With PRETYPE, a pretype (sort SSet: I,
   or a Pi over I) is also a type and *LEVEL is UINT64_MAX (former C1). */
static const Core *check_type_at(Checker *c, const Term *t, uint64_t *level, int pretype) {
  const Value *type = NULL;
  const Core *k = infer(c, t, &type);
  if (k == NULL)
    return NULL;
  if (pretype && val_is(type, OP_SSET)) {
    *level = UINT64_MAX;
    return k;
  }
  if (val_is(type, OP_SSET))
    return nul(FAIL(c, "TYPE_PRETYPE", "expected a type in a universe, found the pretype %s", show(c, here(c, k))));
  if (type == NULL || type->kind != VAL_UNIV)
    return nul(FAIL(c, "TYPE_MISMATCH", "expected a type, found a term of type %s", show(c, type)));
  *level = type->nat;
  return k;
}

static const Core *check_type(Checker *c, const Term *t, uint64_t *level) {
  return check_type_at(c, t, level, 0);
}

static const Core *check_pretype(Checker *c, const Term *t, uint64_t *level) {
  return check_type_at(c, t, level, 1);
}

/* Compares the inferred *TYPE with HINT. */
static const Core *finish(Checker *c, const Core *k, const Value *hint, const Value **type) {
  if (k == NULL || *type == NULL)
    return NULL;
  if (hint != NULL && !expect(c, *type, hint))
    return NULL;
  *type = hint != NULL ? hint : *type;
  return k;
}

/* 1 when T is Option A, List A or Sum E A. *ELEM is A. */
static int carrier_of(const Value *t, Carrier *carrier, const Value **elem) {
  if (val_is(t, OP_OPTION) || val_is(t, OP_LIST)) {
    *carrier = val_is(t, OP_OPTION) ? CARRIER_OPTION : CARRIER_LIST;
    *elem = t->args[0];
    return 1;
  }
  if (!val_is(t, OP_SUM))
    return 0;
  *carrier = CARRIER_SUM;
  *elem = t->args[1];
  return 1;
}

/* SHAPE (Option, List or Sum E) with the element ELEM. */
static const Value *carried(Checker *c, const Value *shape, const Value *elem) {
  if (shape == NULL)
    return NULL;
  return val_is(shape, OP_SUM) ? tyop(c, OP_SUM, 2, shape->args[0], elem, NULL) : tyop(c, shape->op, 1, elem, NULL, NULL);
}

/* Checks T as a function from DOM. *RESULT is WANT, or the result type that
   T infers when WANT is NULL. */
static const Core *fn_into(Checker *c, const Term *t, const Value *dom, const Value *want, const Value **result) {
  const Value *ft = NULL;
  const Core *f;
  if (want != NULL) {
    *result = want;
    return check(c, t, val_arrow(c->m, dom, want));
  }
  f = infer(c, t, &ft);
  if (f == NULL)
    return NULL;
  if (ft == NULL || ft->kind != VAL_PI)
    return nul(FAIL(c, "TYPE_NOT_FUNCTION", "expected a function, found a term of type %s", show(c, ft)));
  *result = closure_apply(c->m, ft, val_var(c->m, c->level));
  /* Compare at two fresh arguments before the result leaves the binder.
     Reusing its level in expect would capture a dependent result. */
  if (!conv_on(c, c->level + 2u, *result,
                   closure_apply(c->m, ft, val_var(c->m, c->level + 1u))))
    return nul(FAIL(c, "TYPE_MISMATCH", "this operation needs a function with a non-dependent result type"));
  return expect(c, ft, val_arrow(c->m, dom, *result)) ? f : NULL;
}

/* The argument type of the function T, from its binder or its type. */
static const Value *lam_domain(Checker *c, const Term *t) {
  uint64_t level = 0;
  const Value *ft = NULL;
  if (t->kind == TERM_LAM && t->left != NULL)
    return here(c, check_type(c, t->left, &level));
  return try_infer(c, t, &ft) != NULL && ft != NULL && ft->kind == VAL_PI ? ft->dom : NULL;
}

/* The carried argument ARG of map, bind or filter. When ARG does not infer
   (`inr 3`), its type comes from the expected type and from the binder of
   the function FN. */
static const Core *carried_arg(Call *k, const Term *arg, const Term *fn, const Value **type) {
  Checker *c = k->c;
  Carrier carrier = CARRIER_OPTION;
  const Value *elem = NULL;
  const Value *dom;
  const Core *x = try_infer(c, arg, type);
  if (x != NULL)
    return x;
  if (!carrier_of(k->hint, &carrier, &elem))
    return infer(c, arg, type);
  dom = lam_domain(c, fn);
  if (dom == NULL)
    return infer(c, arg, type);
  *type = carried(c, k->hint, dom);
  return check(c, arg, *type);
}

static const Core *need_type(Call *k, const char *what) {
  return nul(FAIL(k->c, "TYPE_INFER", "%s needs a declared type %s here", k->b->name, what));
}

/* A value with no arguments: SSet, I, i0 or i1. */
static const Value *op0(Checker *c, Op op) {
  return tyop(c, op, 0, NULL, NULL, NULL);
}

/* Path B x y: PathP over the constant line `fun _ => B` (there is no OP_PATH). */
static const Value *path_type(Checker *c, const Value *b, const Value *x, const Value *y) {
  const Value *line = b == NULL ? NULL : here(c, core_bind(c, CORE_LAM, "_", NULL, quote_value(c->m, c->level + 1u, b)));
  return line == NULL || x == NULL || y == NULL ? NULL : tyop(c, OP_PATHP, 3, line, x, y);
}

/* 1 when the line of a PathP type does not use its interval variable. */
static int line_const(Checker *c, const Value *line) {
  return conv_on(c, c->level + 1u, apply_value(c->m, line, val_var(c->m, c->level)), apply_value(c->m, line, op0(c, OP_I0)));
}

static const Core *not_carrier(Call *k, const Value *type) {
  return nul(FAIL(k->c, "TYPE_MISMATCH", "%s needs a value of type Option A, List A or Sum E A, found a term of type %s", k->b->name, show(k->c, type)));
}

static const Core *infer_eq(Call *k, uint32_t i, const Value **type) {
  const Core *e = infer(k->c, k->args[i], type);
  if (e == NULL)
    return NULL;
  return val_is(*type, OP_EQ) ? e : nul(FAIL(k->c, "TYPE_MISMATCH", "%s needs a proof of type Eq A a b, found a term of type %s", k->b->name, show(k->c, *type)));
}

static const Core *rule_type1(Call *k) {
  uint64_t level = 0;
  const Core *a = check_type(k->c, k->args[0], &level);
  k->type = val_univ(k->c->m, level);
  return op3(k->c, k->b->op, 0, 1, a, NULL, NULL);
}

static const Core *rule_type2(Call *k) {
  uint64_t la = 0;
  uint64_t lb = 0;
  const Core *a = check_type(k->c, k->args[0], &la);
  const Core *b = a == NULL ? NULL : check_type(k->c, k->args[1], &lb);
  k->type = val_univ(k->c->m, la > lb ? la : lb);
  return op3(k->c, k->b->op, 0, 2, a, b, NULL);
}

static const Core *rule_eq(Call *k) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *t = check_type(c, k->args[0], &level);
  const Value *tv = here(c, t);
  const Core *a = check(c, k->args[1], tv);
  const Core *b = a == NULL ? NULL : check(c, k->args[2], tv);
  k->type = val_univ(c->m, 0);
  return op3(c, OP_EQ, 0, 3, t, a, b);
}

static const Core *rule_pair(Call *k) {
  Checker *c = k->c;
  const Value *ta = val_is(k->hint, OP_PROD) ? k->hint->args[0] : NULL;
  const Value *tb = val_is(k->hint, OP_PROD) ? k->hint->args[1] : NULL;
  const Core *a = ta != NULL ? check(c, k->args[0], ta) : infer(c, k->args[0], &ta);
  const Core *b = a == NULL ? NULL : (tb != NULL ? check(c, k->args[1], tb) : infer(c, k->args[1], &tb));
  k->type = tyop(c, OP_PROD, 2, ta, tb, NULL);
  return op3(c, OP_PAIR, 0, 2, a, b, NULL);
}

static const Core *rule_inj(Call *k) {
  if (!val_is(k->hint, OP_SUM))
    return need_type(k, "Sum A B");
  k->type = k->hint;
  return op3(k->c, k->b->op, 0, 1, check(k->c, k->args[0], k->hint->args[k->b->part]), NULL, NULL);
}

static const Core *rule_empty(Call *k) {
  if (!val_is(k->hint, k->b->result))
    return need_type(k, k->b->result == OP_OPTION ? "Option A" : "List A");
  k->type = k->hint;
  return op3(k->c, k->b->op, 0, 0, NULL, NULL, NULL);
}

static const Core *rule_some(Call *k) {
  Checker *c = k->c;
  const Value *a = val_is(k->hint, OP_OPTION) ? k->hint->args[0] : NULL;
  const Core *x = a != NULL ? check(c, k->args[0], a) : infer(c, k->args[0], &a);
  k->type = tyop(c, OP_OPTION, 1, a, NULL, NULL);
  return op3(c, OP_SOME, 0, 1, x, NULL, NULL);
}

static const Core *rule_cons(Call *k) {
  Checker *c = k->c;
  const Value *a = val_is(k->hint, OP_LIST) ? k->hint->args[0] : NULL;
  const Core *x = a != NULL ? check(c, k->args[0], a) : infer(c, k->args[0], &a);
  const Value *list = tyop(c, OP_LIST, 1, a, NULL, NULL);
  const Core *xs = x == NULL ? NULL : check(c, k->args[1], list);
  k->type = list;
  return op3(c, OP_CONS, 0, 2, x, xs, NULL);
}

static const Core *rule_pack(Call *k) {
  Checker *c = k->c;
  const Value *s = k->hint;
  const Core *w;
  const Core *p;
  if (s == NULL || s->kind != VAL_SIGMA)
    return need_type(k, "Sigma (x : A) B");
  w = check(c, k->args[0], s->dom);
  p = w == NULL ? NULL : check(c, k->args[1], closure_apply(c->m, s, here(c, w)));
  k->type = s;
  return op3(c, OP_PACK, 0, 2, w, p, NULL);
}

/* refl against PathP A a b is `<%i> a`: the line is constant and a is b. */
static const Core *refl_path(Call *k) {
  Checker *c = k->c;
  const Value *e = k->hint;
  if (!line_const(c, e->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "refl needs a path type over a constant line, found %s", show(c, e)));
  if (!conv_on(c, c->level, e->args[1], e->args[2]))
    return nul(FAIL(c, "TYPE_ENDPOINT", "refl cannot prove the path: the left end normalizes to %s, the right end normalizes to %s", show(c, e->args[1]), show(c, e->args[2])));
  k->type = e;
  return op3(c, OP_PLAM, 0, 1, core_bind(c, CORE_LAM, "%i", NULL, quote_value(c->m, c->level + 1u, e->args[1])), NULL, NULL);
}

static const Core *rule_refl(Call *k) {
  Checker *c = k->c;
  const Value *e = k->hint;
  if (val_is(e, OP_PATHP))
    return refl_path(k);
  if (!val_is(e, OP_EQ))
    return need_type(k, "Eq A a b");
  if (!conv_on(c, c->level, e->args[1], e->args[2]))
    return nul(FAIL(c, "TYPE_REFL", "refl cannot prove the equality: the left side normalizes to %s, the right side normalizes to %s", show(c, e->args[1]), show(c, e->args[2])));
  k->type = e;
  return op3(c, OP_REFL, 0, 0, NULL, NULL, NULL);
}

static const Core *rule_part(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Core *p = infer(c, k->args[0], &pt);
  if (p == NULL)
    return NULL;
  if (!val_is(pt, OP_PROD))
    return nul(FAIL(c, "TYPE_MISMATCH", "%s needs a pair, found a term of type %s", k->b->name, show(c, pt)));
  k->type = pt->args[k->b->part];
  return op3(c, k->b->op, 0, 1, p, NULL, NULL);
}

static const Core *rule_sigma_part(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Core *s = infer(c, k->args[0], &st);
  const Core *w;
  if (s == NULL)
    return NULL;
  if (st == NULL || st->kind != VAL_SIGMA)
    return nul(FAIL(c, "TYPE_MISMATCH", "%s needs a Sigma value, found a term of type %s", k->b->name, show(c, st)));
  w = op3(c, OP_WITNESS, 0, 1, s, NULL, NULL);
  k->type = k->b->part == 0u ? st->dom : closure_apply(c->m, st, here(c, w));
  return k->b->part == 0u ? w : op3(c, OP_PAYLOAD, 0, 1, s, NULL, NULL);
}

/* either f g s */
static const Core *rule_either(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Value *res = NULL;
  const Core *s = infer(c, k->args[2], &st);
  const Core *f;
  const Core *g;
  if (s == NULL)
    return NULL;
  if (!val_is(st, OP_SUM))
    return nul(FAIL(c, "TYPE_MISMATCH", "either needs a value of type Sum A B, found a term of type %s", show(c, st)));
  f = fn_into(c, k->args[0], st->args[0], k->hint, &res);
  g = f == NULL ? NULL : check(c, k->args[1], val_arrow(c->m, st->args[1], res));
  k->type = res;
  return op3(c, OP_EITHER, 0, 3, f, g, s);
}

/* option z f o */
static const Core *rule_option(Call *k) {
  Checker *c = k->c;
  const Value *ot = NULL;
  const Value *res = k->hint;
  const Core *o = infer(c, k->args[2], &ot);
  const Core *z;
  const Core *f;
  if (o == NULL)
    return NULL;
  if (!val_is(ot, OP_OPTION))
    return nul(FAIL(c, "TYPE_MISMATCH", "option needs a value of type Option A, found a term of type %s", show(c, ot)));
  z = res != NULL ? check(c, k->args[0], res) : infer(c, k->args[0], &res);
  f = z == NULL ? NULL : check(c, k->args[1], val_arrow(c->m, ot->args[0], res));
  k->type = res;
  return op3(c, OP_OPTION_ELIM, 0, 3, z, f, o);
}

static const Core *rule_pure(Call *k) {
  Carrier carrier = CARRIER_OPTION;
  const Value *a = NULL;
  if (!carrier_of(k->hint, &carrier, &a))
    return need_type(k, "Option A, List A or Sum E A");
  k->type = k->hint;
  return op3(k->c, OP_PURE, carrier, 1, check(k->c, k->args[0], a), NULL, NULL);
}

/* map f m */
static const Core *rule_map(Call *k) {
  Checker *c = k->c;
  Carrier carrier = CARRIER_OPTION;
  Carrier hint_carrier = CARRIER_OPTION;
  const Value *mt = NULL;
  const Value *a = NULL;
  const Value *b = NULL;
  const Value *hint_elem = NULL;
  const Core *x = carried_arg(k, k->args[1], k->args[0], &mt);
  const Core *f;
  if (x == NULL)
    return NULL;
  if (!carrier_of(mt, &carrier, &a))
    return not_carrier(k, mt);
  f = fn_into(c, k->args[0], a, carrier_of(k->hint, &hint_carrier, &hint_elem) ? hint_elem : NULL, &b);
  k->type = carried(c, mt, b);
  return op3(c, OP_MAP, carrier, 2, f, x, NULL);
}

/* bind m f */
static const Core *rule_bind(Call *k) {
  Checker *c = k->c;
  Carrier carrier = CARRIER_OPTION;
  Carrier result_carrier = CARRIER_OPTION;
  const Value *mt = NULL;
  const Value *a = NULL;
  const Value *b = NULL;
  const Value *res = NULL;
  const Core *x = carried_arg(k, k->args[0], k->args[1], &mt);
  const Core *f;
  if (x == NULL)
    return NULL;
  if (!carrier_of(mt, &carrier, &a))
    return not_carrier(k, mt);
  f = fn_into(c, k->args[1], a, k->hint, &res);
  if (f == NULL)
    return NULL;
  if (!carrier_of(res, &result_carrier, &b) || result_carrier != carrier)
    return nul(FAIL(c, "TYPE_MISMATCH", "bind needs a function into the carrier of %s, found one into %s", show(c, mt), show(c, res)));
  if (carrier == CARRIER_SUM && !conv_on(c, c->level, res->args[0], mt->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "bind must preserve the Sum error type %s, found %s", show(c, mt->args[0]), show(c, res->args[0])));
  k->type = res;
  return op3(c, OP_BIND, carrier, 2, x, f, NULL);
}

/* filter p m */
static const Core *rule_filter(Call *k) {
  Checker *c = k->c;
  Carrier carrier = CARRIER_OPTION;
  const Value *mt = NULL;
  const Value *a = NULL;
  const Core *x = carried_arg(k, k->args[1], k->args[0], &mt);
  const Core *p;
  if (x == NULL)
    return NULL;
  if (!carrier_of(mt, &carrier, &a) || carrier == CARRIER_SUM)
    return nul(FAIL(c, "TYPE_MISMATCH", "filter needs a value of type Option A or List A, found a term of type %s", show(c, mt)));
  p = check(c, k->args[0], val_arrow(c->m, a, flag_type(c)));
  k->type = mt;
  return op3(c, OP_FILTER, carrier, 2, p, x, NULL);
}

/* fold step z n (Nat) or fold f z xs (List A). */
static const Core *fold_simple(Call *k, const Core *s, const Value *st) {
  Checker *c = k->c;
  const Value *res = k->hint;
  const Core *z = res != NULL ? check(c, k->args[1], res) : infer(c, k->args[1], &res);
  int list = val_is(st, OP_LIST);
  const Value *step = list ? val_arrow(c->m, st->args[0], val_arrow(c->m, res, res)) : val_arrow(c->m, res, res);
  const Core *f = z == NULL ? NULL : check(c, k->args[0], step);
  k->used = 3u;
  k->type = res;
  return op3(c, list ? OP_FOLD_LIST : OP_FOLD_NAT, 0, 3, f, z, s);
}

/* The point term K through the fold cases (D7, t*): a field variable is
   itself (from ENV), and a point constructor of the block of the family F
   applies its case CASES[i] to its fields (the parameters drop). CASES has
   one case for each constructor of the block, in order (D10). */
static const Value *star(Checker *c, const FamilyInfo *f, const Value *const *cases, const Env *env, const Core *k) {
  Machine *m = c->m;
  const FamilyInfo *last = &m->families[f->block_first + f->block_count - 1u];
  uint32_t lo = m->families[f->block_first].first_ctor;
  const Value *acc;
  const Value *arg;
  uint32_t j;
  if (k->kind != CORE_OP || k->op != OP_CTOR || k->inst < lo || k->inst >= last->first_ctor + last->ctor_count)
    return eval_core(m, env, k);
  acc = cases[k->inst - lo];
  for (j = 0; j < m->ctors[k->inst].field_count && acc != NULL; j++) {
    arg = star(c, f, cases, env, k->args[m->families[m->ctors[k->inst].family].param_count + j]);
    acc = arg == NULL ? NULL : apply_value(m, acc, arg);
  }
  return acc;
}

/* Path B x y under the fresh variables up to LEVEL: the constant line
   closes over ENV, an environment of LEVEL entries. */
static const Value *path_at(Checker *c, const Env *env, uint32_t level, const Value *b, const Value *x, const Value *y) {
  const Core *line = b == NULL ? NULL : core_bind(c, CORE_LAM, "_", NULL, quote_value(c->m, level + 1u, b));
  const Value *lv = line == NULL ? NULL : eval_core(c->m, env, line);
  return lv == NULL || x == NULL || y == NULL ? NULL : tyop(c, OP_PATHP, 3, lv, x, y);
}

/* C5: the motive of a fold over an indexed family at the indices of the
   family value T. A uniform fold (MOTIVE NULL) gives RES. */
static const Value *motive_at(Checker *c, const Value *motive, const Value *res, const Value *t) {
  const Value *acc = motive;
  uint32_t i;
  if (motive == NULL)
    return res;
  if (t == NULL)
    return NULL;
  if (!val_is(t, OP_FAMILY)) {
    FAIL(c, "INTERNAL", "a fold over an indexed family at a type outside the family");
    return NULL;
  }
  for (i = 0; i < t->argc && acc != NULL; i++)
    acc = apply_value(c->m, acc, t->args[i]);
  return acc;
}

/* C5: the result of the case of CTOR under the field variables of ENV:
   RES at a uniform fold, else the motive at the result indices of a point
   constructor, or at the indices of the base of a path constructor (its
   line taken at i0 once for each depth). */
static const Value *ctor_result(Checker *c, const FamilyInfo *f, const CtorInfo *ctor, const Env *env, const Value *motive, const Value *res) {
  Machine *m = c->m;
  const Value *acc = motive;
  const Value *v;
  const Value *ix;
  uint32_t i;
  if (motive == NULL)
    return res;
  v = ctor->depth == 0u ? NULL : eval_core(m, env, ctor->type);
  for (i = 0; i < ctor->depth && v != NULL; i++)
    v = val_is(v, OP_PATHP) ? apply_value(m, v->args[0], op0(c, OP_I0)) : NULL;
  if (ctor->depth > 0u)
    return motive_at(c, motive, res, v);
  for (i = 0; i < f->param_count && acc != NULL; i++) {
    ix = eval_core(m, env, ctor->index[i]);
    acc = ix == NULL ? NULL : apply_value(m, acc, ix);
  }
  return acc;
}

/* The type of the fold case of CTOR (D7), a point or a path constructor: a
   Pi over its fields (a FIELD_SELF field has the type RES, a FIELD_SELF_PATH
   field Path RES u* v*) into RES at depth 0, else Path^d RES a* b* with the
   ends through the cases of the earlier point constructors. The field types
   and the ends are evaluated at fresh variables in PENV (the parameters),
   then quoted into the Pi, so a later field may depend on an earlier one.
   With MOTIVE (an indexed family, C5) RES is the motive at the indices of
   each recursive field and of the result, and PENV holds no parameters.
   In a mutual block (D10) MOTIVES has the motive of each sort, and a
   FIELD_SIB field has the motive of its sort at its index args. */
static const Value *case_type(Checker *c, const FamilyInfo *f, const CtorInfo *ctor, const Value *const *cases, const Env *penv, const Value *res, const Value *motive, const Value *const *motives) {
  Machine *m = c->m;
  const Core **doms = arena_alloc(m->arena, ((size_t)ctor->field_count + 1u) * sizeof *doms);
  const Env *env = penv;
  const Env *cenv = c->env;
  const Value *dom = NULL;
  const Value *base;
  const Value *body;
  const Core *k;
  uint32_t n = ctor->field_count;
  uint32_t i;
  if (doms == NULL) {
    FAIL(c, "OOM", "out of memory");
    return NULL;
  }
  for (i = 0; i < n; i++) {
    const FieldInfo *field = &ctor->fields[i];
    switch (field->kind) {
    case FIELD_PLAIN:
      dom = eval_core(m, env, field->type);
      break;
    case FIELD_SELF:
      dom = motive == NULL ? res : motive_at(c, motive, res, eval_core(m, env, field->type));
      break;
    case FIELD_SELF_PATH:
      base = motive == NULL ? res : motive_at(c, motive, res, apply_value(m, eval_core(m, env, field->type->args[0]), op0(c, OP_I0)));
      dom = path_at(c, cenv, c->level + i, base, star(c, f, cases, env, field->type->args[1]), star(c, f, cases, env, field->type->args[2]));
      break;
    case FIELD_SIB:
      /* MOTIVES is NULL outside a mutual block, where no field is FIELD_SIB */
      dom = motives == NULL ? NULL : motive_at(c, motives[field->type->inst - f->block_first], res, eval_core(m, env, field->type));
      break;
    }
    doms[i] = dom == NULL ? NULL : quote_value(m, c->level + i, dom);
    env = env_push(m, env, val_var(m, c->level + i));
    cenv = env_push(m, cenv, val_var(m, c->level + i));
    if (doms[i] == NULL || env == NULL || cenv == NULL)
      return NULL;
  }
  base = ctor_result(c, f, ctor, env, motive, res);
  body = ctor->depth == 2u ? path_at(c, cenv, c->level + n, base, star(c, f, cases, env, ctor->ends[2]), star(c, f, cases, env, ctor->ends[3])) : base;
  body = ctor->depth == 0u ? body : path_at(c, cenv, c->level + n, body, star(c, f, cases, env, ctor->ends[0]), star(c, f, cases, env, ctor->ends[1]));
  k = body == NULL ? NULL : quote_value(m, c->level + n, body);
  for (i = n; i > 0u && k != NULL; i--)
    k = core_bind(c, CORE_PI, ctor->fields[i - 1u].name, doms[i - 1u], k);
  return k == NULL ? NULL : here(c, k);
}

/* The index telescope (COUNT TYPES of the family NAME, or Nat for D1)
   supplies lambda domains, including the unannotated binders written by
   the printer. Infer the final type to retain its universe, and check any
   remaining indices of a non-lambda motive. */
static const Core *check_motive_body(Checker *c, const char *name, uint32_t count, const Core *const *types, const Term *t, uint32_t index, const Env *env) {
  Machine *m = c->m;
  const Value *mt = NULL;
  const Core *mc;
  const Value *ty;
  const Value *pt;
  uint32_t i;
  if (index < count && t->kind == TERM_LAM) {
    pt = eval_core(m, env, types[index]);
    if (pt == NULL || (t->left != NULL && !annotation_ok(c, t->left, pt)) || !push(c, t->name, pt))
      return NULL;
    env = env_push(m, env, c->env->value);
    mc = env == NULL ? NULL : check_motive_body(c, name, count, types, t->right, index + 1u, env);
    pop(c);
    return core_bind(c, CORE_LAM, t->name, NULL, mc);
  }
  mc = infer(c, t, &mt);
  ty = mt;
  if (mc == NULL)
    return NULL;
  for (i = index; i < count && ty != NULL; i++) {
    pt = eval_core(m, env, types[i]);
    ty = ty->kind == VAL_PI && pt != NULL && conv_on(c, c->level + i - index, ty->dom, pt) ? closure_apply(m, ty, val_var(m, c->level + i - index)) : NULL;
    env = env_push(m, env, val_var(m, c->level + i - index));
    ty = env == NULL ? NULL : ty;
  }
  if (ty == NULL || ty->kind != VAL_UNIV)
    return nul(FAIL(c, "TYPE_MISMATCH", "the motive of a fold over %s must take its %u indices and give a type; found a motive of type %s", name, count, show(c, mt)));
  return mc;
}

/* C5: a motive takes the indices of the family and gives a type. */
static const Core *check_motive(Checker *c, const FamilyInfo *f, const Term *t, const Value **motive) {
  const Core *mc = check_motive_body(c, f->name, f->param_count, f->param_types, t, 0u, NULL);
  if (mc == NULL)
    return NULL;
  *motive = here(c, mc);
  return *motive == NULL ? NULL : mc;
}

/* fold case... x over a domain family: one case for each constructor, the
   path constructors included. Over a higher inductive type the result
   type B rides as a hidden last argument (D8: the fold on a formal hcomp
   is an hcomp at B). Over an indexed family the motive M comes first
   (C5, D5): the layout is [M, cases, x], the cases take the fields only
   (D6) and nothing is hidden. */
static const Core *fold_family(Call *k, const Core *s, const Value *st) {
  Checker *c = k->c;
  Machine *m = c->m;
  const FamilyInfo *f = &m->families[st->inst];
  const Env *env = f->indexed ? NULL : env_of(c, st->args, st->argc);
  const Value *res = f->indexed ? NULL : k->hint;
  const Value *motive = NULL;
  const Core **cores = arena_alloc(m->arena, ((size_t)f->ctor_count + 2u) * sizeof *cores);
  const Value **vals = arena_alloc(m->arena, ((size_t)f->ctor_count + 1u) * sizeof *vals);
  const Core *first = NULL;
  const Value *want;
  const CtorInfo *ctor;
  int hit = !f->indexed && f->ctor_count > 0u && m->ctors[f->first_ctor + f->ctor_count - 1u].depth > 0u;
  uint32_t off = f->indexed ? 1u : 0u;
  uint32_t j;
  if (cores == NULL || vals == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  cores[0] = f->indexed ? check_motive(c, f, k->args[0], &motive) : NULL;
  if (f->indexed && cores[0] == NULL)
    return NULL;
  if (!f->indexed && res == NULL && m->ctors[f->first_ctor].field_count == 0u)
    first = infer(c, k->args[0], &res);
  if (!f->indexed && res == NULL)
    return nul(FAIL(c, "TYPE_INFER", "fold over %s needs a declared result type here", f->name));
  for (j = 0; j < f->ctor_count; j++) {
    ctor = &m->ctors[f->first_ctor + j];
    want = case_type(c, f, ctor, vals, env, res, motive, NULL);
    cores[off + j] = j == 0u && first != NULL ? first : want == NULL ? NULL : check(c, k->args[off + j], want);
    vals[j] = cores[off + j] == NULL ? NULL : here(c, cores[off + j]);
    if (vals[j] == NULL)
      return NULL;
  }
  cores[off + f->ctor_count] = s;
  cores[f->ctor_count + 1u] = f->indexed ? s : hit ? quote_value(m, c->level, res) : NULL;
  k->used = off + f->ctor_count + 1u;
  k->type = f->indexed ? motive_at(c, motive, NULL, st) : res;
  return k->type == NULL ? NULL : core_op(c, OP_FOLD_FAMILY, st->inst, 0, cores, f->ctor_count + (f->indexed || hit ? 2u : 1u));
}

/* D10: the number of constructors of the mutual block of F (one family is
   a block of 1). They follow the first constructor of the first sort, in
   declaration order. */
static uint32_t block_ctors(const Machine *m, const FamilyInfo *f) {
  const FamilyInfo *last = &m->families[f->block_first + f->block_count - 1u];
  return last->first_ctor + last->ctor_count - m->families[f->block_first].first_ctor;
}

/* C5 (D10): fold M_1 .. M_n case_1 .. case_K x over a mutual block of n
   sorts and K constructors. Motive j takes the indices of sort j. The case
   of a constructor of sort j gives M_j at its result, and a field at sort
   i (FIELD_SELF or FIELD_SIB) has the type M_i at its index args. The fold
   gives M_j at the indices of x, with x of sort j. */
static const Core *fold_block(Call *k, const Core *s, const Value *st) {
  Checker *c = k->c;
  Machine *m = c->m;
  const FamilyInfo *b = &m->families[m->families[st->inst].block_first];
  uint32_t n = b->block_count;
  uint32_t total = block_ctors(m, b);
  const Core **cores = arena_alloc(m->arena, ((size_t)n + total + 1u) * sizeof *cores);
  const Value **motives = arena_alloc(m->arena, (size_t)n * sizeof *motives);
  const Value **vals = arena_alloc(m->arena, ((size_t)total + 1u) * sizeof *vals);
  const CtorInfo *ctor;
  const Value *want;
  uint32_t j;
  if (cores == NULL || motives == NULL || vals == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  for (j = 0; j < n; j++) {
    cores[j] = check_motive(c, &m->families[b->block_first + j], k->args[j], &motives[j]);
    if (cores[j] == NULL)
      return NULL;
  }
  for (j = 0; j < total; j++) {
    ctor = &m->ctors[b->first_ctor + j];
    want = case_type(c, &m->families[ctor->family], ctor, vals, NULL, NULL, motives[ctor->family - b->block_first], motives);
    cores[n + j] = want == NULL ? NULL : check(c, k->args[n + j], want);
    vals[j] = cores[n + j] == NULL ? NULL : here(c, cores[n + j]);
    if (vals[j] == NULL)
      return NULL;
  }
  cores[n + total] = s;
  k->used = n + total + 1u;
  k->type = motive_at(c, motives[st->inst - b->block_first], NULL, st);
  return k->type == NULL ? NULL : core_op(c, OP_FOLD_FAMILY, st->inst, 0, cores, n + total + 1u);
}

/* D1: M of fold M step base n over Nat, as try_infer: a failure leaves no
   diagnostic, and the call is then a 3-arg fold applied to one more
   argument. */
static const Core *try_nat_motive(Checker *c, const Term *t, const Value **motive) {
  Diag saved = *c->m->diag;
  const Core *types[1];
  const Core *mc;
  types[0] = quote_value(c->m, c->level, nat_type(c));
  mc = types[0] == NULL ? NULL : check_motive_body(c, "Nat", 1u, types, t, 0u, NULL);
  *motive = here(c, mc);
  if (*motive == NULL)
    *c->m->diag = saved;
  return *motive == NULL ? NULL : mc;
}

/* D1: fold M step base n over Nat, with M : Nat -> Type l, base : M 0 and
   step : (k : Nat) -> M k -> M (natAdd k 1). The result is M n. The layout
   is [M, step, base, n] and inst 1 marks the motive. */
static const Core *fold_nat_motive(Call *k, const Core *mc, const Value *motive, const Core *s) {
  Checker *c = k->c;
  Machine *m = c->m;
  const Value *kv = val_var(m, c->level);
  const Env *kenv = env_push(m, c->env, kv);
  const Core *add = op3(c, OP_NAT_ADD, 0, 2, quote_value(m, c->level + 1u, kv), quote_value(m, 0u, val_nat(m, 1u)), NULL);
  const Value *next = add == NULL || kenv == NULL ? NULL : eval_core(m, kenv, add);
  const Value *mk = apply_value(m, motive, kv);
  const Value *mnext = next == NULL ? NULL : apply_value(m, motive, next);
  const Core *dk = mk == NULL ? NULL : quote_value(m, c->level + 1u, mk);
  const Core *rk = mnext == NULL ? NULL : quote_value(m, c->level + 2u, mnext);
  const Core *acc = dk == NULL || rk == NULL ? NULL : core_bind(c, CORE_PI, "acc", dk, rk);
  const Core *nat = quote_value(m, c->level, nat_type(c));
  const Value *want = acc == NULL || nat == NULL ? NULL : here(c, core_bind(c, CORE_PI, "k", nat, acc));
  const Value *zero = apply_value(m, motive, val_nat(m, 0u));
  const Core *cores[4];
  cores[0] = mc;
  cores[2] = zero == NULL ? NULL : check(c, k->args[2], zero);
  cores[1] = cores[2] == NULL || want == NULL ? NULL : check(c, k->args[1], want);
  cores[3] = s;
  k->used = 4u;
  k->type = cores[1] == NULL ? NULL : apply_value(m, motive, here(c, s));
  return k->type == NULL ? NULL : core_op(c, OP_FOLD_NAT, 1u, 0, cores, 4u);
}

/* F6: M of fold M step base xs over List A, as try_nat_motive. The type
   ST of xs can name variables of the context, so the index type
   evaluates in the context. */
static const Core *try_list_motive(Checker *c, const Term *t, const Value *st, const Value **motive) {
  Diag saved = *c->m->diag;
  const Core *types[1];
  const Core *mc;
  types[0] = quote_value(c->m, c->level, st);
  mc = types[0] == NULL ? NULL : check_motive_body(c, "List", 1u, types, t, 0u, c->env);
  *motive = here(c, mc);
  if (*motive == NULL)
    *c->m->diag = saved;
  return *motive == NULL ? NULL : mc;
}

/* F6: fold M step base xs over List A, with M : List A -> Type l,
   base : M nil and step : (a : A) -> (l : List A) -> M l -> M (cons a l).
   The step takes the tail, because the type of the acc names it. The
   result is M xs. The layout is [M, step, base, xs] and inst 1 marks the
   motive. */
static const Core *fold_list_motive(Call *k, const Core *mc, const Value *motive, const Core *s, const Value *st) {
  Checker *c = k->c;
  Machine *m = c->m;
  const Value *av = val_var(m, c->level);
  const Value *lv = val_var(m, c->level + 1u);
  const Env *aenv = env_push(m, c->env, av);
  const Env *lenv = aenv == NULL ? NULL : env_push(m, aenv, lv);
  const Core *cell = op3(c, OP_CONS, 0, 2, quote_value(m, c->level + 2u, av), quote_value(m, c->level + 2u, lv), NULL);
  const Value *cv = cell == NULL || lenv == NULL ? NULL : eval_core(m, lenv, cell);
  const Value *ml = apply_value(m, motive, lv);
  const Value *mcell = cv == NULL ? NULL : apply_value(m, motive, cv);
  const Core *dl = ml == NULL ? NULL : quote_value(m, c->level + 2u, ml);
  const Core *rc = mcell == NULL ? NULL : quote_value(m, c->level + 3u, mcell);
  const Core *acc = dl == NULL || rc == NULL ? NULL : core_bind(c, CORE_PI, "ih", dl, rc);
  const Core *tail = quote_value(m, c->level + 1u, st);
  const Core *rest = acc == NULL || tail == NULL ? NULL : core_bind(c, CORE_PI, "l", tail, acc);
  const Core *elem = quote_value(m, c->level, st->args[0]);
  const Value *want = rest == NULL || elem == NULL ? NULL : here(c, core_bind(c, CORE_PI, "a", elem, rest));
  const Core *empty = op3(c, OP_NIL, 0, 0, NULL, NULL, NULL);
  const Value *nil = empty == NULL ? NULL : here(c, empty);
  const Value *base = nil == NULL ? NULL : apply_value(m, motive, nil);
  const Core *cores[4];
  cores[0] = mc;
  cores[2] = base == NULL ? NULL : check(c, k->args[2], base);
  cores[1] = cores[2] == NULL || want == NULL ? NULL : check(c, k->args[1], want);
  cores[3] = s;
  k->used = 4u;
  k->type = cores[1] == NULL ? NULL : apply_value(m, motive, here(c, s));
  return k->type == NULL ? NULL : core_op(c, OP_FOLD_LIST, 1u, 0, cores, 4u);
}

static int simple_scrutinee(const Value *t) {
  return val_is(t, OP_NAT) || val_is(t, OP_LIST);
}

/* 1 when the family FAM is a higher inductive type (path constructors
   come last). The constructor metadata of FAM must be complete. */
static int hit_family(const Machine *m, uint32_t fam) {
  const FamilyInfo *f = &m->families[fam];
  return f->ctor_count > 0u && m->ctors[f->first_ctor + f->ctor_count - 1u].depth > 0u;
}

/* An indexed fold may return a function. Its scrutinee then precedes the
   remaining applications, including applications exposed by printing.
   A sort of a mutual block folds only with the motives of all its sorts
   (D10), so a prefix over such a sort is not a fold. */
static const Core *fold_indexed_prefix(Call *k) {
  Checker *c = k->c;
  Diag saved = *c->m->diag;
  uint32_t i;
  for (i = 2u; i + 1u < k->argc; i++) {
    const Value *st = NULL;
    const Core *s = try_infer(c, k->args[i], &st);
    const FamilyInfo *f = val_is(st, OP_FAMILY) ? &c->m->families[st->inst] : NULL;
    if (s != NULL && f != NULL && f->indexed && f->block_count == 1u && f->ctor_count + 1u == i && f->first_ctor + f->ctor_count <= c->m->ctor_count) {
      Call trial = *k;
      const Core *body;
      trial.argc = i + 1u;
      trial.hint = NULL;
      body = fold_family(&trial, s, st);
      if (body != NULL)
        body = apply_rest(c, body, trial.type, k->args + i + 1u, k->argc - i - 1u, k->outer, &trial.type);
      if (body != NULL) {
        trial.argc = k->argc;
        trial.used = k->argc;
        *k = trial;
        return body;
      }
      *c->m->diag = saved;
    }
  }
  return NULL;
}

/* fold takes its scrutinee last, so the scrutinee type sets the arity. */
static const Core *rule_fold_family(Call *k);

/* A motive fold consumes four arguments; the remaining spine applies
   its result. Its scrutinee need not be the last argument. The type of
   the fourth argument selects the form: Nat (D1), then List A (F6). A
   fold that fails as a motive fold goes to the family order (block,
   indexed, HIT, kit), because an indexed fold applied to one more Nat
   has the same shape (fold M case x 7, the P4a print). If both fail, the
   diagnostic of the motive form stays. A partial List motive fold has no
   hint (a limit). */
static const Core *rule_fold(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Value *motive = NULL;
  const Core *s = k->argc >= 4u ? try_infer(c, k->args[3], &st) : NULL;
  int list = s != NULL && val_is(st, OP_LIST);
  const Core *mc = s != NULL && val_is(st, OP_NAT) ? try_nat_motive(c, k->args[0], &motive) : list ? try_list_motive(c, k->args[0], st, &motive) : NULL;
  const Call keep = *k;
  const Diag saved = *c->m->diag;
  const Core *core = mc == NULL ? NULL : list ? fold_list_motive(k, mc, motive, s, st) : fold_nat_motive(k, mc, motive, s);
  Diag form;
  if (mc == NULL)
    return rule_fold_family(k);
  if (core != NULL)
    return core;
  form = *c->m->diag;
  *c->m->diag = saved;
  *k = keep;
  core = rule_fold_family(k);
  if (core == NULL)
    *c->m->diag = form;
  return core;
}

static const Core *rule_fold_family(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Core *s = try_infer(c, k->args[k->argc - 1u], &st);
  uint32_t fam = val_is(st, OP_FAMILY) ? st->inst : NO_FAMILY;
  /* The constructor metadata of a family is not complete inside its own
     declaration (the declaration of its mutual block, D10). */
  if (s != NULL && fam != NO_FAMILY && c->m->families[c->m->families[fam].block_first].first_ctor + block_ctors(c->m, &c->m->families[fam]) > c->m->ctor_count)
    return nul(FAIL(c, "DOMAIN_FAMILY", "a constructor of %s cannot fold over %s inside its declaration", c->m->families[fam].name, c->m->families[fam].name));
  /* C5: a fold over a mutual block takes a motive for each sort first (D10). */
  if (s != NULL && fam != NO_FAMILY && c->m->families[fam].block_count > 1u && c->m->families[fam].block_count + block_ctors(c->m, &c->m->families[fam]) + 1u != k->argc)
    return nul(FAIL(c, "TYPE_MISMATCH", "fold over the mutual block of %s takes a motive for each of its %u sorts, one case for each of its %u constructors and the value; found %u arguments", c->m->families[c->m->families[fam].block_first].name, c->m->families[fam].block_count, block_ctors(c->m, &c->m->families[fam]), k->argc));
  if (s != NULL && fam != NO_FAMILY && c->m->families[fam].block_count > 1u)
    return fold_block(k, s, st);
  /* C5: a fold over an indexed family takes an explicit motive first (D5). */
  if (s != NULL && fam != NO_FAMILY && c->m->families[fam].ctor_count + (c->m->families[fam].indexed ? 2u : 1u) == k->argc) {
    if (k->argc > 3u && (c->m->families[fam].indexed || hit_family(c->m, fam))) {
      /* The last argument may instead apply a function returned by an
         earlier fold. The fold over the last family comes first. When
         all interpretations fail, its diagnostic stays. */
      Diag saved = *c->m->diag;
      Diag failed;
      Call trial = *k;
      const Value *simple_type = NULL;
      const Core *simple;
      const Core *body;
      trial.hint = trial.outer;
      body = fold_family(&trial, s, st);
      if (body != NULL)
        body = finish(c, body, k->outer, &trial.type);
      if (body != NULL) {
        *k = trial;
        return body;
      }
      failed = *c->m->diag;
      *c->m->diag = saved;
      body = fold_indexed_prefix(k);
      if (body != NULL)
        return body;
      simple = try_infer(c, k->args[2], &simple_type);
      if (simple != NULL && simple_scrutinee(simple_type)) {
        trial = *k;
        trial.hint = NULL;
        body = fold_simple(&trial, simple, simple_type);
        if (body != NULL) {
          *k = trial;
          return body;
        }
      }
      *c->m->diag = failed;
      return NULL;
    }
    k->hint = k->outer;
    return fold_family(k, s, st);
  }
  if (s != NULL && k->argc == 3u && simple_scrutinee(st)) {
    k->hint = k->outer;
    return fold_simple(k, s, st);
  }
  {
    const Core *body = fold_indexed_prefix(k);
    if (body != NULL)
      return body;
  }
  s = k->argc > 3u ? try_infer(c, k->args[2], &st) : NULL;
  if (s != NULL && simple_scrutinee(st)) {
    k->hint = NULL;
    return fold_simple(k, s, st);
  }
  if (fam != NO_FAMILY && c->m->families[fam].indexed)
    return nul(FAIL(c, "TYPE_MISMATCH", "fold over the indexed family %s takes a motive, one case for each of its %u constructors and the value; found %u arguments", c->m->families[fam].name, c->m->families[fam].ctor_count, k->argc));
  s = infer(c, k->args[k->argc - 1u], &st);
  return s == NULL ? NULL : nul(FAIL(c, "TYPE_MISMATCH", "fold needs a last argument of type Nat, List A or a domain family, and one case for each constructor; found %u arguments and a last argument of type %s", k->argc, show(c, st)));
}

/* unfold g limit seed, with g : S -> Option (Prod A S). */
static const Core *rule_unfold(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Value *a;
  const Core *seed;
  const Core *limit;
  const Core *g;
  if (!val_is(k->hint, OP_LIST))
    return need_type(k, "List A");
  a = k->hint->args[0];
  seed = infer(c, k->args[2], &st);
  limit = seed == NULL ? NULL : check(c, k->args[1], nat_type(c));
  g = limit == NULL ? NULL : check(c, k->args[0], val_arrow(c->m, st, tyop(c, OP_OPTION, 1, tyop(c, OP_PROD, 2, a, st, NULL), NULL, NULL)));
  k->type = k->hint;
  return op3(c, OP_UNFOLD, 0, 3, g, limit, seed);
}

/* symm p for p : PathP L a b is `<%i> p @ ~ %i : PathP (fun i => L (~ i)) b a`,
   built as Core: p and the ends are quoted under the new binder. */
static const Core *symm_path(Call *k, const Core *p, const Value *pt) {
  Checker *c = k->c;
  uint32_t l = c->level + 1u;
  const Core *neg = op3(c, OP_INEG, 0, 1, core_index(c, CORE_VAR, 0), NULL, NULL);
  const Core *args[4];
  const Value *line = here(c, core_bind(c, CORE_LAM, "%i", NULL, core_app(c, quote_value(c->m, l, pt->args[0]), neg)));
  if (line == NULL)
    return NULL;
  args[0] = quote_value(c->m, l, here(c, p));
  args[1] = neg;
  args[2] = quote_value(c->m, l, pt->args[1]);
  args[3] = quote_value(c->m, l, pt->args[2]);
  k->type = tyop(c, OP_PATHP, 3, line, pt->args[2], pt->args[1]);
  return op3(c, OP_PLAM, 0, 1, core_bind(c, CORE_LAM, "%i", NULL, core_op(c, OP_PAPP, 0, 0, args, 4)), NULL, NULL);
}

static const Core *rule_symm(Call *k) {
  const Value *et = NULL;
  const Core *e = try_infer(k->c, k->args[0], &et);
  if (e != NULL && val_is(et, OP_PATHP))
    return symm_path(k, e, et);
  e = e != NULL && val_is(et, OP_EQ) ? e : infer_eq(k, 0, &et);
  if (e == NULL)
    return NULL;
  k->type = tyop(k->c, OP_EQ, 3, et->args[0], et->args[2], et->args[1]);
  return op3(k->c, OP_SYMM, 0, 1, e, NULL, NULL);
}

/* trans p q for p : Path A a b and q : Path A b c is (D9, C7)
   `<%i> hcomp A (~ %i \/ %i) (fun %j => [ ~ %i |-> a , %i |-> q @ %j ]) (p @ %i)
   : Path A a c`, built as Core in the shape of symm_path. */
static const Core *trans_path(Call *k, const Core *p, const Value *pt) {
  Checker *c = k->c;
  uint32_t l = c->level + 1u;
  const Value *qt = NULL;
  const Value *a;
  const Core *q;
  const Core *qa[4];
  const Core *pa[4];
  const Core *cells;
  const Core *side;
  const Core *phi;
  if (!line_const(c, pt->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "trans needs a path over a constant line, found a term of type %s", show(c, pt)));
  q = infer(c, k->args[1], &qt);
  if (q == NULL)
    return NULL;
  if (!val_is(qt, OP_PATHP) || !conv_on(c, c->level, pt->args[0], qt->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "trans needs a second path over the line of the first, %s, found a term of type %s", show(c, pt), show(c, qt)));
  if (!conv_on(c, c->level, pt->args[2], qt->args[1]))
    return nul(FAIL(c, "TYPE_MISMATCH", "trans needs the right side of the first proof, %s, to equal the left side of the second, %s", show(c, pt->args[2]), show(c, qt->args[1])));
  a = apply_value(c->m, pt->args[0], op0(c, OP_I0));
  /* Under %i and %j: %j 0, %i 1. */
  qa[0] = quote_value(c->m, l + 1u, here(c, q));
  qa[1] = core_index(c, CORE_VAR, 0);
  qa[2] = quote_value(c->m, l + 1u, qt->args[1]);
  qa[3] = quote_value(c->m, l + 1u, qt->args[2]);
  cells = op3(c, OP_SYSTEM, 0, 3, core_index(c, CORE_VAR, 1), core_op(c, OP_PAPP, 0, 0, qa, 4), op3(c, OP_SYSNIL, 0, 0, NULL, NULL, NULL));
  side = core_bind(c, CORE_LAM, "%j", NULL, op3(c, OP_SYSTEM, 0, 3, op3(c, OP_INEG, 0, 1, core_index(c, CORE_VAR, 1), NULL, NULL), quote_value(c->m, l + 1u, pt->args[1]), cells));
  /* Under %i: %i 0. */
  pa[0] = quote_value(c->m, l, here(c, p));
  pa[1] = core_index(c, CORE_VAR, 0);
  pa[2] = quote_value(c->m, l, pt->args[1]);
  pa[3] = quote_value(c->m, l, pt->args[2]);
  phi = op3(c, OP_IMAX, 0, 2, op3(c, OP_INEG, 0, 1, core_index(c, CORE_VAR, 0), NULL, NULL), core_index(c, CORE_VAR, 0), NULL);
  k->type = path_type(c, a, pt->args[1], qt->args[2]);
  return op3(c, OP_PLAM, 0, 1, core_bind(c, CORE_LAM, "%i", NULL, op4(c, OP_HCOMP, quote_value(c->m, l, a), phi, side, core_op(c, OP_PAPP, 0, 0, pa, 4))), NULL, NULL);
}

static const Core *rule_trans(Call *k) {
  Checker *c = k->c;
  const Value *t1 = NULL;
  const Value *t2 = NULL;
  const Core *e1 = try_infer(c, k->args[0], &t1);
  const Core *e2;
  if (e1 != NULL && val_is(t1, OP_PATHP))
    return trans_path(k, e1, t1);
  e1 = e1 != NULL && val_is(t1, OP_EQ) ? e1 : infer_eq(k, 0, &t1);
  e2 = e1 == NULL ? NULL : infer_eq(k, 1, &t2);
  if (e2 == NULL)
    return NULL;
  if (!conv_on(c, c->level, t1->args[0], t2->args[0]) || !conv_on(c, c->level, t1->args[2], t2->args[1]))
    return nul(FAIL(c, "TYPE_MISMATCH", "trans needs the right side of the first proof, %s, to equal the left side of the second, %s", show(c, t1->args[2]), show(c, t2->args[1])));
  k->type = tyop(c, OP_EQ, 3, t1->args[0], t1->args[1], t2->args[2]);
  return op3(c, OP_TRANS, 0, 2, e1, e2, NULL);
}

/* cong f p for p : Path A a b is `<%i> f (p @ %i) : Path B (f a) (f b)`, built
   as Core (a Term desugar fails on a lambda head such as `fun k => k x`). */
static const Core *cong_path(Call *k, const Core *p, const Value *pt) {
  Checker *c = k->c;
  uint32_t l = c->level + 1u;
  const Value *want = val_is(k->hint, OP_PATHP) ? apply_value(c->m, k->hint->args[0], op0(c, OP_I0)) : NULL;
  const Value *bt = NULL;
  const Value *fv;
  const Core *f;
  const Core *args[4];
  if (!line_const(c, pt->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "cong needs a path over a constant line, found a term of type %s", show(c, pt)));
  f = fn_into(c, k->args[0], apply_value(c->m, pt->args[0], op0(c, OP_I0)), want, &bt);
  if (f == NULL)
    return NULL;
  fv = here(c, f);
  args[0] = quote_value(c->m, l, here(c, p));
  args[1] = core_index(c, CORE_VAR, 0);
  args[2] = quote_value(c->m, l, pt->args[1]);
  args[3] = quote_value(c->m, l, pt->args[2]);
  k->type = path_type(c, bt, apply_value(c->m, fv, pt->args[1]), apply_value(c->m, fv, pt->args[2]));
  return op3(c, OP_PLAM, 0, 1, core_bind(c, CORE_LAM, "%i", NULL, core_app(c, quote_value(c->m, l, fv), core_op(c, OP_PAPP, 0, 0, args, 4))), NULL, NULL);
}

/* cong f e */
static const Core *rule_cong(Call *k) {
  Checker *c = k->c;
  const Value *et = NULL;
  const Value *bt = NULL;
  const Value *fv;
  const Value *hint_type = val_is(k->hint, OP_EQ) ? k->hint->args[0] : NULL;
  const Core *e = try_infer(c, k->args[1], &et);
  const Core *f;
  if (e != NULL && val_is(et, OP_PATHP))
    return cong_path(k, e, et);
  e = e != NULL && val_is(et, OP_EQ) ? e : infer_eq(k, 1, &et);
  f = e == NULL ? NULL : fn_into(c, k->args[0], et->args[0], hint_type, &bt);
  if (f == NULL)
    return NULL;
  fv = here(c, f);
  k->type = tyop(c, OP_EQ, 3, bt, apply_value(c->m, fv, et->args[1]), apply_value(c->m, fv, et->args[2]));
  return op3(c, OP_CONG, 0, 2, f, e, NULL);
}

/* transport P p u for p : Path A a b is `transp (fun %i => P (p @ %i)) i0 u`
   (former C3), built as Core in the shape of cong_path. */
static const Core *transport_path(Call *k, const Core *e, const Value *et) {
  Checker *c = k->c;
  uint32_t l = c->level + 1u;
  const Value *pt = NULL;
  const Value *cod;
  const Value *pv;
  const Value *at;
  const Core *p;
  const Core *u;
  const Core *args[4];
  if (!line_const(c, et->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "transport needs a path over a constant line, found a term of type %s", show(c, et)));
  at = apply_value(c->m, et->args[0], op0(c, OP_I0));
  p = infer(c, k->args[0], &pt);
  if (p == NULL)
    return NULL;
  cod = pt != NULL && pt->kind == VAL_PI ? closure_apply(c->m, pt, val_var(c->m, c->level)) : NULL;
  if (cod == NULL || cod->kind != VAL_UNIV || !conv_on(c, c->level, pt->dom, at))
    return nul(FAIL(c, "TYPE_MISMATCH", "transport needs a family of types over %s, found a term of type %s", show(c, at), show(c, pt)));
  pv = here(c, p);
  u = check(c, k->args[2], apply_value(c->m, pv, et->args[1]));
  if (u == NULL)
    return NULL;
  args[0] = quote_value(c->m, l, here(c, e));
  args[1] = core_index(c, CORE_VAR, 0);
  args[2] = quote_value(c->m, l, et->args[1]);
  args[3] = quote_value(c->m, l, et->args[2]);
  k->type = apply_value(c->m, pv, et->args[2]);
  return op3(c, OP_TRANSP, 0, 3, core_bind(c, CORE_LAM, "%i", NULL, core_app(c, quote_value(c->m, l, pv), core_op(c, OP_PAPP, 0, 0, args, 4))),
             op3(c, OP_I0, 0, 0, NULL, NULL, NULL), u);
}

/* transport P e u */
static const Core *rule_transport(Call *k) {
  Checker *c = k->c;
  const Value *et = NULL;
  const Value *pt = NULL;
  const Value *cod;
  const Value *pv;
  const Core *e = try_infer(c, k->args[1], &et);
  const Core *p;
  const Core *u;
  if (e != NULL && val_is(et, OP_PATHP))
    return transport_path(k, e, et);
  e = e != NULL && val_is(et, OP_EQ) ? e : infer_eq(k, 1, &et);
  p = e == NULL ? NULL : infer(c, k->args[0], &pt);
  if (p == NULL)
    return NULL;
  cod = pt != NULL && pt->kind == VAL_PI ? closure_apply(c->m, pt, val_var(c->m, c->level)) : NULL;
  if (cod == NULL || cod->kind != VAL_UNIV || !conv_on(c, c->level, pt->dom, et->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "transport needs a family of types over %s, found a term of type %s", show(c, et->args[0]), show(c, pt)));
  pv = here(c, p);
  u = check(c, k->args[2], apply_value(c->m, pv, et->args[1]));
  k->type = apply_value(c->m, pv, et->args[2]);
  return op3(c, OP_TRANSPORT, 0, 3, p, e, u);
}

static const Core *rule_nat2(Call *k) {
  Checker *c = k->c;
  const Core *x = check(c, k->args[0], nat_type(c));
  const Core *y = x == NULL ? NULL : check(c, k->args[1], nat_type(c));
  k->type = tyop(c, k->b->result, 0, NULL, NULL, NULL);
  return op3(c, k->b->op, 0, 2, x, y, NULL);
}

/* natIsSet x y p q (D4, USER flag 5 = (a)): isSet Nat as a trusted prim.
   x and y are in Nat, p and q are in Path Nat x y, and the result is in
   Path (Path Nat x y) p q. It has no reduction rule. */
static const Core *rule_nat_is_set(Call *k) {
  Checker *c = k->c;
  const Core *args[4];
  const Value *xy;
  args[0] = check(c, k->args[0], nat_type(c));
  args[1] = args[0] == NULL ? NULL : check(c, k->args[1], nat_type(c));
  xy = args[1] == NULL ? NULL : path_type(c, nat_type(c), here(c, args[0]), here(c, args[1]));
  args[2] = xy == NULL ? NULL : check(c, k->args[2], xy);
  args[3] = args[2] == NULL ? NULL : check(c, k->args[3], xy);
  k->type = args[3] == NULL ? NULL : path_type(c, xy, here(c, args[2]), here(c, args[3]));
  return k->type == NULL ? NULL : core_op(c, OP_NAT_IS_SET, 0, 0, args, 4);
}

/* flagIf b yes no */
static const Core *rule_flag_if(Call *k) {
  Checker *c = k->c;
  const Value *res = k->hint;
  const Core *b = check(c, k->args[0], flag_type(c));
  const Core *yes = b == NULL ? NULL : (res != NULL ? check(c, k->args[1], res) : infer(c, k->args[1], &res));
  const Core *no = yes == NULL ? NULL : check(c, k->args[2], res);
  k->type = res;
  return op3(c, OP_FLAG_IF, 0, 3, b, yes, no);
}

/* I, i0, i1, ~ r, r /\ s and r \/ s (former C1): each argument is in I. */
static const Core *rule_interval(Call *k) {
  Checker *c = k->c;
  const Value *iv = op0(c, OP_I);
  const Core *a = k->b->arity > 0u ? check(c, k->args[0], iv) : NULL;
  const Core *b = k->b->arity > 1u && a != NULL ? check(c, k->args[1], iv) : NULL;
  k->type = op0(c, k->b->result);
  return op3(c, k->b->op, 0, k->b->arity, a, b, NULL);
}

/* Path A: the constant line `fun _ => A` (there is no OP_PATH). */
static const Core *const_line(Checker *c, const Term *t, uint64_t *level) {
  const Core *a = check_type(c, t, level);
  return a == NULL ? NULL : core_bind(c, CORE_LAM, "_", NULL, quote_value(c->m, c->level + 1u, here(c, a)));
}

/* PathP A: A is a line of type I -> Type n. */
static const Core *pathp_line(Checker *c, const Term *t, uint64_t *level) {
  const Value *lt = NULL;
  const Core *a = infer(c, t, &lt);
  const Value *cod = NULL;
  if (a == NULL)
    return NULL;
  if (lt != NULL && lt->kind == VAL_PI && conv_on(c, c->level, lt->dom, op0(c, OP_I)))
    cod = closure_apply(c->m, lt, val_var(c->m, c->level));
  if (cod == NULL || cod->kind != VAL_UNIV)
    return nul(FAIL(c, "TYPE_MISMATCH", "PathP needs a line of type I -> Type n, found a term of type %s", show(c, lt)));
  *level = cod->nat;
  return a;
}

/* PathP A a b (part 0) and Path A a b (part 1), former C2. */
static const Core *rule_pathp(Call *k) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *line = k->b->part == 1u ? const_line(c, k->args[0], &level) : pathp_line(c, k->args[0], &level);
  const Value *lv = here(c, line);
  const Core *a = lv == NULL ? NULL : check(c, k->args[1], apply_value(c->m, lv, op0(c, OP_I0)));
  const Core *b = a == NULL ? NULL : check(c, k->args[2], apply_value(c->m, lv, op0(c, OP_I1)));
  k->type = val_univ(c->m, level);
  return op3(c, OP_PATHP, 0, 3, line, a, b);
}

/* p @ r: the ends of the type of p ride in the core, as values carry no types. */
static const Core *rule_papp(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Core *p = infer(c, k->args[0], &pt);
  const Core *r = p == NULL ? NULL : check(c, k->args[1], op0(c, OP_I));
  const Core *args[4];
  if (r == NULL)
    return NULL;
  if (!val_is(pt, OP_PATHP))
    return nul(FAIL(c, "TYPE_MISMATCH", "@ needs a path of type PathP A a b, found a term of type %s", show(c, pt)));
  args[0] = p;
  args[1] = r;
  args[2] = quote_value(c->m, c->level, pt->args[1]);
  args[3] = quote_value(c->m, c->level, pt->args[2]);
  k->type = apply_value(c->m, pt->args[0], here(c, r));
  return core_op(c, OP_PAPP, 0, 0, args, 4);
}

/* <i> t against PathP A a b: t checks against A i under i : I, then the ends
   of the path must be a and b. */
static const Core *rule_plam(Call *k) {
  Checker *c = k->c;
  const Value *e = k->hint;
  const Term *t = k->args[0];
  const Core *body;
  const Core *lam;
  const Value *f;
  const Value *at0;
  const Value *at1;
  if (!val_is(e, OP_PATHP))
    return need_type(k, "PathP A a b");
  if (t->kind != TERM_LAM)
    return nul(FAIL(c, "TYPE_MISMATCH", "a path abstraction needs a binder"));
  if (!push(c, t->name, op0(c, OP_I)))
    return NULL;
  body = check(c, t->right, apply_value(c->m, e->args[0], c->env->value));
  pop(c);
  lam = core_bind(c, CORE_LAM, t->name, NULL, body);
  f = here(c, lam);
  if (f == NULL)
    return NULL;
  at0 = apply_value(c->m, f, op0(c, OP_I0));
  at1 = apply_value(c->m, f, op0(c, OP_I1));
  if (!conv_on(c, c->level, at0, e->args[1]))
    return nul(FAIL(c, "TYPE_ENDPOINT", "the path at i0 normalizes to %s, the type needs %s", show(c, at0), show(c, e->args[1])));
  if (!conv_on(c, c->level, at1, e->args[2]))
    return nul(FAIL(c, "TYPE_ENDPOINT", "the path at i1 normalizes to %s, the type needs %s", show(c, at1), show(c, e->args[2])));
  k->type = e;
  return op3(c, OP_PLAM, 0, 1, lam, NULL, NULL);
}

/* Check h against the pointwise path type before building <i> fun x => h x @ i.
   This also gives lambda arguments the binder and endpoint hints they need. */
static const Core *rule_funext(Call *k) {
  Checker *c = k->c;
  const Value *e = k->hint;
  const Value *ft;
  const Value *point;
  const Value *ht;
  const Core *point_core;
  const Core *h;
  const Core *args[4];
  uint32_t l = c->level;
  if (!val_is(k->hint, OP_PATHP))
    return need_type(k, "Path ((x : A) -> B x) f g");
  ft = apply_value(c->m, e->args[0], op0(c, OP_I0));
  if (ft == NULL || ft->kind != VAL_PI || !line_const(c, e->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "funExt needs a path over a constant function type"));
  if (!push(c, "%x", ft->dom))
    return NULL;
  point = path_type(c, closure_apply(c->m, ft, c->env->value),
                    apply_value(c->m, e->args[1], c->env->value),
                    apply_value(c->m, e->args[2], c->env->value));
  point_core = quote_value(c->m, c->level, point);
  pop(c);
  ht = point_core == NULL ? NULL : val_closure(c->m, VAL_PI, "%x", ft->dom, c->env, point_core);
  h = ht == NULL ? NULL : check(c, k->args[0], ht);
  if (h == NULL)
    return NULL;
  args[0] = core_app(c, quote_value(c->m, l + 2u, here(c, h)), core_index(c, CORE_VAR, 0));
  args[1] = core_index(c, CORE_VAR, 1);
  args[2] = core_app(c, quote_value(c->m, l + 2u, e->args[1]), core_index(c, CORE_VAR, 0));
  args[3] = core_app(c, quote_value(c->m, l + 2u, e->args[2]), core_index(c, CORE_VAR, 0));
  k->type = e;
  return op3(c, OP_PLAM, 0, 1,
             core_bind(c, CORE_LAM, "%i", NULL,
                       core_bind(c, CORE_LAM, "%x", NULL, core_op(c, OP_PAPP, 0, 0, args, 4))),
             NULL, NULL);
}

/* Partial phi A (former C1): phi is in I and A is a type. A pretype, as I. */
static const Core *rule_partial(Call *k) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *phi = check(c, k->args[0], op0(c, OP_I));
  const Core *a = phi == NULL ? NULL : check_type(c, k->args[1], &level);
  k->type = op0(c, OP_SSET);
  return op3(c, OP_PARTIAL, 0, 2, phi, a, NULL);
}

#define CELLS_MAX FACE_MEETS_MAX

/* 1 when T is a cell `f |-> u` applied to the other cells. */
static int is_cell(const Term *t) {
  return t->kind == TERM_APP && t->left->kind == TERM_APP && t->left->left->kind == TERM_APP
         && t->left->left->left->kind == TERM_VAR && strcmp(t->left->left->left->name, "|->") == 0;
}

/* The checker environment with the variable of each literal set to i1
   (level * 2) or i0 (level * 2 + 1). */
static const Env *face_env_at(Checker *c, uint32_t depth, const uint32_t *lits, uint32_t n) {
  const Env *env = NULL;
  const Value *v;
  uint32_t level;
  uint32_t i;
  for (level = 0; level < depth; level++) {
    v = val_var(c->m, level);
    for (i = 0; i < n; i++)
      if (lits[i] / 2u == level)
        v = op0(c, lits[i] % 2u == 0 ? OP_I1 : OP_I0);
    env = env_push(c->m, env, v);
    if (env == NULL)
      return NULL;
  }
  return env;
}

static const Env *face_env(Checker *c, const uint32_t *lits, uint32_t n) {
  return face_env_at(c, c->level, lits, n);
}

/* Conversion in a branch is required on each meet of its enclosing faces.
   Keep the original Core variables, restricting only the comparison. */
static int conv_on(Checker *c, uint32_t level, const Value *a, const Value *b) {
  FaceMeets fm;
  const Core *ka;
  const Core *kb;
  const Env *env;
  uint32_t q;
  if (a == NULL || b == NULL)
    return 0;
  if (conv_values(c->m, level, a, b))
    return 1;
  if (c->face == NULL || !face_meets(c->m, level, c->face, &fm))
    return 0;
  ka = quote_value(c->m, level, a);
  kb = quote_value(c->m, level, b);
  if (ka == NULL || kb == NULL)
    return 0;
  for (q = 0; q < fm.count; q++) {
    env = face_env_at(c, level, fm.lit[q], fm.len[q]);
    if ((env == NULL && level > 0) || !conv_values(c->m, level, eval_core(c->m, env, ka), eval_core(c->m, env, kb)))
      return 0;
  }
  return 1;
}

static const Core *check_on(Checker *c, const Term *t, const Value *want, const Core *face) {
  FaceMeets fm;
  const Value *saved = c->face;
  const Value *fv = here(c, face);
  const Core *out;
  c->face = saved == NULL ? fv : tyop(c, OP_IMIN, 2, saved, fv, NULL);
  if (c->face == NULL) {
    c->face = saved;
    return NULL;
  }
  /* Empty cells still undergo the ordinary branch checks. */
  if (c->face != NULL && face_meets(c->m, c->level, c->face, &fm) && fm.count == 0)
    c->face = saved;
  out = check(c, t, want);
  c->face = saved;
  return out;
}

/* The branches UA and UB agree on each meet of the faces FA /\ FB. A
   neutral atom in a meet stays, so the test is stricter, and sound. */
static int cells_agree(Checker *c, const Core *fa, const Core *ua, const Core *fb, const Core *ub) {
  FaceMeets fm;
  const Value *meet = here(c, op3(c, OP_IMIN, 0, 2, fa, fb, NULL));
  const Env *env;
  const Value *x;
  const Value *y;
  uint32_t q;
  if (meet == NULL)
    return 0;
  if (c->face != NULL)
    meet = tyop(c, OP_IMIN, 2, meet, c->face, NULL);
  if (!face_meets(c->m, c->level, meet, &fm))
    return FAIL(c, "TYPE_SYSTEM", "the overlap %s of two faces is not in the face bounds", show(c, meet));
  for (q = 0; q < fm.count; q++) {
    env = face_env(c, fm.lit[q], fm.len[q]);
    x = env == NULL && c->level > 0 ? NULL : eval_core(c->m, env, ua);
    y = x == NULL ? NULL : eval_core(c->m, env, ub);
    if (x == NULL || y == NULL)
      return c->m->diag->set ? 0 : FAIL(c, "OOM", "out of memory");
    if (!conv_values(c->m, c->level, x, y))
      return FAIL(c, "TYPE_SYSTEM", "two branches disagree where the faces meet at %s: %s against %s", show(c, meet), show(c, x), show(c, y));
  }
  return 1;
}

/* [ f1 |-> u1 , ... ] against Partial psi A (former C1): each face is in I,
   each branch is in A under its face, the faces join to psi, and two
   branches agree where their faces meet. */
static const Core *rule_system(Call *k) {
  Checker *c = k->c;
  const Value *e = k->hint;
  const Term *face[CELLS_MAX];
  const Term *branch[CELLS_MAX];
  const Core *fk[CELLS_MAX];
  const Core *uk[CELLS_MAX];
  const Term *rest = k->b->arity == 3u ? k->args[2] : NULL;
  const Value *join;
  const Value *fv;
  const Core *acc;
  uint32_t n = 0;
  uint32_t i;
  uint32_t j;
  if (!val_is(e, OP_PARTIAL))
    return need_type(k, "Partial phi A");
  if (rest != NULL) {
    face[0] = k->args[0];
    branch[0] = k->args[1];
    n = 1;
  }
  for (; rest != NULL && is_cell(rest) && n < CELLS_MAX; rest = rest->right) {
    face[n] = rest->left->left->right;
    branch[n] = rest->left->right;
    n++;
  }
  if (rest != NULL && (rest->kind != TERM_VAR || strcmp(rest->name, "[]") != 0))
    return nul(FAIL(c, "TYPE_SYSTEM", "a system needs at most %u cells", (unsigned)CELLS_MAX));
  join = op0(c, OP_I0);
  for (i = 0; i < n; i++) {
    fk[i] = check(c, face[i], op0(c, OP_I));
    uk[i] = fk[i] == NULL ? NULL : check_on(c, branch[i], e->args[1], fk[i]);
    fv = uk[i] == NULL ? NULL : here(c, fk[i]);
    if (fv == NULL)
      return NULL;
    join = i == 0 ? fv : tyop(c, OP_IMAX, 2, join, fv, NULL);
  }
  if (!same_face(c, join, e->args[0]))
    return nul(FAIL(c, "TYPE_SYSTEM", "the system covers %s, the type needs %s", show(c, join), show(c, e->args[0])));
  for (i = 0; i < n; i++)
    for (j = i + 1u; j < n; j++)
      if (!cells_agree(c, fk[i], uk[i], fk[j], uk[j]))
        return NULL;
  acc = op3(c, OP_SYSNIL, 0, 0, NULL, NULL, NULL);
  for (i = n; i > 0; i--)
    acc = op3(c, OP_SYSTEM, 0, 3, fk[i - 1u], uk[i - 1u], acc);
  k->type = e;
  return acc;
}

/* fromPartial p (former C1): p : Partial phi A and phi holds. */
static const Core *rule_from_partial(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Core *p = infer(c, k->args[0], &pt);
  if (p == NULL)
    return NULL;
  if (!val_is(pt, OP_PARTIAL))
    return nul(FAIL(c, "TYPE_MISMATCH", "fromPartial needs a partial element of type Partial phi A, found a term of type %s", show(c, pt)));
  if (c->face == NULL ? !face_holds(c->m, c->level, pt->args[0])
                      : !same_face(c, pt->args[0], op0(c, OP_I1)))
    return nul(FAIL(c, "TYPE_SYSTEM", "fromPartial needs the face %s to hold", show(c, pt->args[0])));
  k->type = pt->args[1];
  return op3(c, OP_FROMPARTIAL, 0, 1, p, NULL, NULL);
}

/* The line A of transp A r u (former C3): an unannotated `fun i => T` binds
   i : I, so the printed form checks again. Any other term goes to pathp_line. */
static const Core *line_of(Checker *c, const Term *t, uint64_t *level) {
  const Core *body;
  if (t->kind != TERM_LAM || t->left != NULL)
    return pathp_line(c, t, level);
  if (!push(c, t->name, op0(c, OP_I)))
    return NULL;
  body = check_type_at(c, t->right, level, 0);
  pop(c);
  return core_bind(c, CORE_LAM, t->name, NULL, body);
}

/* The line LINE is constant on each meet of R, so transp A r u = u where r
   holds. A neutral atom in a meet stays, so the test is stricter, and sound. */
static int line_const_on(Checker *c, const Core *line, const Value *r) {
  FaceMeets fm;
  const Env *env;
  const Value *lv;
  uint32_t q;
  if (c->face != NULL)
    r = tyop(c, OP_IMIN, 2, r, c->face, NULL);
  if (r == NULL)
    return 0;
  if (!face_meets(c->m, c->level, r, &fm)) {
    lv = here(c, line);
    return lv != NULL && line_const(c, lv) ? 1 : FAIL(c, "TYPE_LINE", "transp needs a line that is constant where r holds, found the line %s", show(c, lv));
  }
  for (q = 0; q < fm.count; q++) {
    env = face_env(c, fm.lit[q], fm.len[q]);
    lv = env == NULL && c->level > 0 ? NULL : eval_core(c->m, env, line);
    if (lv == NULL)
      return c->m->diag->set ? 0 : FAIL(c, "OOM", "out of memory");
    if (!line_const(c, lv))
      return FAIL(c, "TYPE_LINE", "transp needs a line that is constant where r holds, found the line %s", show(c, here(c, line)));
  }
  return 1;
}

/* transp A r u (former C3): A : I -> Type n, r : I and u : A i0, of type
   A i1. A is constant where r holds (D2). */
static const Core *rule_transp(Call *k) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *line = line_of(c, k->args[0], &level);
  const Value *lv = here(c, line);
  const Core *r = lv == NULL ? NULL : check(c, k->args[1], op0(c, OP_I));
  const Core *u;
  if (r == NULL || !line_const_on(c, line, here(c, r)))
    return NULL;
  u = check(c, k->args[2], apply_value(c->m, lv, op0(c, OP_I0)));
  k->type = apply_value(c->m, lv, op0(c, OP_I1));
  return op3(c, OP_TRANSP, 0, 3, line, r, u);
}

/* The side `fun j => [ f1 |-> u1 , ... ]` of hcomp, comp and fill (C2),
   against T, or against LINE j when LINE is given, with j : I. Pass 1 checks each face at I in the outer context, so a face
   cannot name j: the faces go to FK and their join to *PHI. Pass 2 checks
   the branches and overlaps using these same faces under j. A
   neutral side is a known limit. */
static const Core *side_of(Checker *c, const Term *t, const Value *ty, const Value *line, const Core **phi, const Core **fk, uint32_t *n) {
  const Term *s = t->kind == TERM_LAM ? t->right : NULL;
  const Value *fv[CELLS_MAX];
  const Term *branch[CELLS_MAX];
  const Core *inner[CELLS_MAX];
  const Core *uk[CELLS_MAX];
  const Core *body;
  uint32_t i;
  uint32_t q;
  *n = 0;
  *phi = op3(c, OP_I0, 0, 0, NULL, NULL, NULL);
  if (s == NULL || (t->left != NULL && (t->left->kind != TERM_VAR || strcmp(t->left->name, "I") != 0))
      || (!is_cell(s) && (s->kind != TERM_VAR || strcmp(s->name, "[]") != 0)))
    return nul(FAIL(c, "TYPE_SYSTEM", "hcomp needs fun j => [ ... ]"));
  for (; is_cell(s) && *n < CELLS_MAX; s = s->right) {
    fk[*n] = check(c, s->left->left->right, op0(c, OP_I));
    if (fk[*n] == NULL)
      return NULL;
    fv[*n] = here(c, fk[*n]);
    if (fv[*n] == NULL)
      return NULL;
    branch[*n] = s->left->right;
    *phi = *n == 0 ? fk[0] : op3(c, OP_IMAX, 0, 2, *phi, fk[*n], NULL);
    (*n)++;
  }
  if (s->kind != TERM_VAR || strcmp(s->name, "[]") != 0)
    return nul(FAIL(c, "TYPE_SYSTEM", "a system needs at most %u cells", (unsigned)CELLS_MAX));
  if (!push(c, t->name, op0(c, OP_I)))
    return NULL;
  for (i = 0; i < *n; i++) {
    inner[i] = quote_value(c->m, c->level, fv[i]);
    uk[i] = inner[i] == NULL ? NULL : check_on(c, branch[i], line == NULL ? ty : apply_value(c->m, line, val_var(c->m, c->level - 1u)), inner[i]);
    if (uk[i] == NULL) {
      pop(c);
      return NULL;
    }
  }
  for (i = 0; i < *n; i++)
    for (q = i + 1u; q < *n; q++)
      if (!cells_agree(c, inner[i], uk[i], inner[q], uk[q])) {
        pop(c);
        return NULL;
      }
  body = op3(c, OP_SYSNIL, 0, 0, NULL, NULL, NULL);
  for (i = *n; i > 0; i--)
    body = op3(c, OP_SYSTEM, 0, 3, inner[i - 1u], uk[i - 1u], body);
  pop(c);
  return core_bind(c, CORE_LAM, t->name, NULL, body);
}

/* The base condition of hcomp, comp and fill (D6, C2): where its face
   holds, each branch of SIDE at j = i0 is U0. */
static int side_base(Checker *c, const Core *side, const Core **fk, uint32_t n, const Core *u0) {
  const Core *i0 = op3(c, OP_I0, 0, 0, NULL, NULL, NULL);
  const Core *i1 = op3(c, OP_I1, 0, 0, NULL, NULL, NULL);
  const Core *s = side->b;
  uint32_t q;
  for (q = 0; q < n && s != NULL && s->kind == CORE_OP && s->op == OP_SYSTEM; q++, s = s->args[2])
    if (!cells_agree(c, fk[q], core_app(c, core_bind(c, CORE_LAM, "j", NULL, s->args[1]), i0), i1, u0))
      return 0;
  return 1;
}

/* Only types in a universe carry Kan composition. Pi pretypes propagate
   SSet from either the domain or the codomain. */
static int hcomp_type(Checker *c, const Value *a) {
  const Value *point;
  if (a == NULL)
    return 0;
  if (val_is(a, OP_I) || val_is(a, OP_PARTIAL))
    return FAIL(c, "TYPE_PRETYPE", "hcomp needs a type in a universe, found the pretype %s", show(c, a));
  if (a->kind != VAL_PI)
    return 1;
  if (!hcomp_type(c, a->dom))
    return 0;
  point = val_var(c->m, c->m->fresh_level);
  return point != NULL && hcomp_type(c, closure_apply(c->m, a, point));
}

/* hcomp side u0 (D6, C2): A is the hint, else the type of u0; phi is the
   join of the faces of side; where its face holds, each branch at j = i0
   is u0. The type is A. */
static const Core *rule_hcomp(Call *k) {
  Checker *c = k->c;
  const Value *a = k->hint;
  const Core *fk[CELLS_MAX];
  const Core *phi = NULL;
  const Core *side;
  const Core *u0 = NULL;
  uint32_t n = 0;
  if (a == NULL) {
    u0 = try_infer(c, k->args[1], &a);
    if (u0 == NULL)
      return need_type(k, "A");
  }
  if (!hcomp_type(c, a))
    return NULL;
  side = side_of(c, k->args[0], a, NULL, &phi, fk, &n);
  if (side == NULL)
    return NULL;
  if (u0 == NULL)
    u0 = check(c, k->args[1], a);
  if (u0 == NULL || !side_base(c, side, fk, n, u0))
    return NULL;
  k->type = a;
  return op4(c, OP_HCOMP, quote_value(c->m, c->level, a), phi, side, u0);
}

/* The type Sigma (T : Type 0) (Equiv T A) of a Glue branch (former C6),
   from the domain definition Equiv. */
static const Value *equiv_sigma(Checker *c, const Value *av) {
  uint32_t d = find_def(c->m, "Equiv");
  const Core *body;
  if (d == NO_DEF) {
    FAIL(c, "NAME_UNKNOWN", "Glue needs the domain definition Equiv, which is not declared");
    return NULL;
  }
  body = core_app(c, core_app(c, core_index(c, CORE_GLOBAL, d), core_index(c, CORE_VAR, 0)), quote_value(c->m, c->level + 1u, av));
  return here(c, core_bind(c, CORE_SIGMA, "T", core_leaf(c, CORE_UNIV, 0), body));
}

/* 1 when a failure inside a Glue branch keeps its own code. */
static int glue_keeps(const char *code) {
  return code == NULL || strcmp(code, "OOM") == 0 || strcmp(code, "INTERNAL") == 0 || strcmp(code, "EVAL_FUEL") == 0
         || strcmp(code, "EVAL_DEPTH") == 0;
}

/* A failed Glue branch refails as CODE over the diagnostic state SAVED,
   with the inner code and message kept in the text. */
static const Core *glue_refail(Checker *c, const Diag *saved, const char *code) {
  Diag inner = *c->m->diag;
  if (glue_keeps(inner.code))
    return NULL;
  *c->m->diag = *saved;
  return nul(FAIL(c, code, "each branch of Glue needs pack T e with e : Equiv T A (%s: %s)", inner.code, inner.msg));
}

/* The system [ f |-> t , ... ] of Glue and glue (former C6), with no
   binder: each face at I, their join to *PHI, each branch against WANT
   under its face, then the overlaps. The faces go to FK and FV, the
   branches to UK. A branch that fails refails as WRAP, when WRAP is not
   NULL. */
static const Core *glue_system(Checker *c, const Term *t, const char *who, const Value *want, const char *wrap, const Core **phi, const Core **fk, const Value **fv, const Core **uk, uint32_t *n) {
  const Term *s = t;
  const Term *branch[CELLS_MAX];
  const Core *body;
  Diag saved = *c->m->diag;
  uint32_t i;
  uint32_t q;
  *n = 0;
  *phi = op3(c, OP_I0, 0, 0, NULL, NULL, NULL);
  if (!is_cell(s) && (s->kind != TERM_VAR || strcmp(s->name, "[]") != 0))
    return nul(FAIL(c, "TYPE_SYSTEM", "%s needs [ ... ]", who));
  for (; is_cell(s) && *n < CELLS_MAX; s = s->right) {
    fk[*n] = check(c, s->left->left->right, op0(c, OP_I));
    if (fk[*n] == NULL)
      return NULL;
    fv[*n] = here(c, fk[*n]);
    if (fv[*n] == NULL)
      return NULL;
    branch[*n] = s->left->right;
    *phi = *n == 0 ? fk[0] : op3(c, OP_IMAX, 0, 2, *phi, fk[*n], NULL);
    (*n)++;
  }
  if (s->kind != TERM_VAR || strcmp(s->name, "[]") != 0)
    return nul(FAIL(c, "TYPE_SYSTEM", "a system needs at most %u cells", (unsigned)CELLS_MAX));
  for (i = 0; i < *n; i++) {
    uk[i] = check_on(c, branch[i], want, fk[i]);
    if (uk[i] == NULL)
      return wrap == NULL ? NULL : glue_refail(c, &saved, wrap);
  }
  for (i = 0; i < *n; i++)
    for (q = i + 1u; q < *n; q++)
      if (!cells_agree(c, fk[i], uk[i], fk[q], uk[q]))
        return NULL;
  body = op3(c, OP_SYSNIL, 0, 0, NULL, NULL, NULL);
  for (i = *n; i > 0; i--)
    body = op3(c, OP_SYSTEM, 0, 3, fk[i - 1u], uk[i - 1u], body);
  return body;
}

/* Glue A [ f |-> pack T e , ... ] (former C6): A : Type 0 and each branch
   is pack T e with e : Equiv T A. The type is Type 0. A branch that fails
   is GLUE_EQUIV. */
static const Core *rule_glue(Call *k) {
  Checker *c = k->c;
  const Core *fk[CELLS_MAX];
  const Value *fv[CELLS_MAX];
  const Core *uk[CELLS_MAX];
  const Core *phi = NULL;
  const Core *sys;
  const Core *a = check(c, k->args[0], val_univ(c->m, 0));
  const Value *want = a == NULL ? NULL : equiv_sigma(c, here(c, a));
  uint32_t n = 0;
  if (want == NULL)
    return NULL;
  sys = glue_system(c, k->args[1], "Glue", want, "GLUE_EQUIV", &phi, fk, fv, uk, &n);
  if (sys == NULL)
    return NULL;
  k->type = val_univ(c->m, 0);
  return op3(c, OP_GLUE, 0, 3, a, phi, sys);
}

/* glue [ f |-> t , ... ] a against Glue A sys (former C6), in check mode:
   the faces join to the face of the type (GLUE_COVER), each branch has the
   type under its face (there Glue A sys is T), a : A, and on each face
   witness e t is a (GLUE_BOUNDARY). */
static const Core *rule_glue_elem(Call *k) {
  Checker *c = k->c;
  const Value *g = k->hint;
  const Core *fk[CELLS_MAX];
  const Value *fv[CELLS_MAX];
  const Core *uk[CELLS_MAX];
  const Core *phi = NULL;
  const Core *sys;
  const Core *a;
  const Core *ga;
  const Core *gphi;
  const Core *gsys;
  const Core *gt;
  const Value *cover;
  const Value *want_cover;
  const Value *face;
  const Core *ung;
  const Env *env;
  const Value *uv;
  const Value *av;
  FaceMeets fm;
  uint32_t n = 0;
  uint32_t i;
  uint32_t q;
  if (!val_is(g, OP_GLUE))
    return need_type(k, "Glue A sys");
  sys = glue_system(c, k->args[0], "glue", g, NULL, &phi, fk, fv, uk, &n);
  if (sys == NULL)
    return NULL;
  cover = here(c, phi);
  want_cover = g->args[1];
  if (c->face != NULL) {
    cover = tyop(c, OP_IMIN, 2, c->face, cover, NULL);
    want_cover = tyop(c, OP_IMIN, 2, c->face, want_cover, NULL);
  }
  if (cover == NULL || want_cover == NULL)
    return NULL;
  if (!face_equal(c->m, c->level, cover, want_cover))
    return nul(FAIL(c, "GLUE_COVER", "the faces of glue join to %s, but the type needs %s", show(c, here(c, phi)), show(c, g->args[1])));
  a = check(c, k->args[1], g->args[0]);
  ga = quote_value(c->m, c->level, g->args[0]);
  gphi = quote_value(c->m, c->level, g->args[1]);
  gsys = quote_value(c->m, c->level, g->args[2]);
  gt = quote_value(c->m, c->level, g);
  if (a == NULL || ga == NULL || gphi == NULL || gsys == NULL || gt == NULL)
    return NULL;
  for (i = 0; i < n; i++) {
    face = c->face == NULL ? fv[i] : tyop(c, OP_IMIN, 2, c->face, fv[i], NULL);
    ung = op4(c, OP_UNGLUE, ga, gphi, gsys, uk[i]);
    if (face == NULL || ung == NULL)
      return NULL;
    /* A branch has the Glue type only on its face. Restrict its Core
       before evaluating unglue, since the branch may itself be a glue. */
    if (!face_meets(c->m, c->level, face, &fm))
      return nul(FAIL(c, "TYPE_SYSTEM", "the Glue boundary face %s is not in the face bounds", show(c, face)));
    for (q = 0; q < fm.count; q++) {
      env = face_env(c, fm.lit[q], fm.len[q]);
      uv = env == NULL && c->level > 0 ? NULL : eval_core(c->m, env, ung);
      av = uv == NULL ? NULL : eval_core(c->m, env, a);
      if (uv == NULL || av == NULL)
        return c->m->diag->set ? NULL : nul(FAIL(c, "OOM", "out of memory"));
      if (!conv_values(c->m, c->level, uv, av))
        return c->m->diag->set ? NULL : nul(FAIL(c, "GLUE_BOUNDARY", "on the face %s, witness e t is %s but the base is %s", show(c, face), show(c, uv), show(c, av)));
    }
  }
  k->type = g;
  return op3(c, OP_GLUE_ELEM, 0, 3, gt, sys, a);
}

/* unglue g (former C6): g : Glue A sys, of type A. */
static const Core *rule_unglue(Call *k) {
  Checker *c = k->c;
  const Value *gt = NULL;
  const Core *g = infer(c, k->args[0], &gt);
  if (g == NULL)
    return NULL;
  if (!val_is(gt, OP_GLUE))
    return nul(FAIL(c, "UNGLUE_TYPE", "unglue needs a term of a type Glue A sys, found a term of type %s", show(c, gt)));
  k->type = gt->args[0];
  return op4(c, OP_UNGLUE, quote_value(c->m, c->level, gt->args[0]), quote_value(c->m, c->level, gt->args[1]), quote_value(c->m, c->level, gt->args[2]), g);
}

/* The Core of comp LINE side u0 (D8, C6) from the values of LINE and SIDE,
   the face PHI and the Core U0. A constant line (HOMOG, checked once here)
   gives hcomp (LINE i1) phi side u0. A varying line gives the CCHM form
   hcomp (LINE i1) phi (fun j => SYSMAP (fun b => transp (fun i =>
   LINE (i \/ j)) j b) (side j)) (transp LINE i0 u0). */
static const Core *comp_core(Checker *c, int homog, const Value *line, const Core *phi, const Value *side, const Core *u0) {
  uint32_t l = c->level;
  const Core *a1;
  const Core *at;
  const Core *f;
  const Core *s;
  if (line == NULL || side == NULL)
    return NULL;
  a1 = quote_value(c->m, l, apply_value(c->m, line, op0(c, OP_I1)));
  if (homog)
    return op4(c, OP_HCOMP, a1, phi, quote_value(c->m, l, side), u0);
  at = op3(c, OP_IMAX, 0, 2, core_index(c, CORE_VAR, 0), core_index(c, CORE_VAR, 2), NULL);
  f = core_bind(c, CORE_LAM, "b", NULL, op3(c, OP_TRANSP, 0, 3, core_bind(c, CORE_LAM, "i", NULL, core_app(c, quote_value(c->m, l + 3u, line), at)), core_index(c, CORE_VAR, 1), core_index(c, CORE_VAR, 0)));
  s = core_bind(c, CORE_LAM, "j", NULL, op3(c, OP_SYSMAP, 0, 2, f, core_app(c, quote_value(c->m, l + 1u, side), core_index(c, CORE_VAR, 0)), NULL));
  return op4(c, OP_HCOMP, a1, phi, s, op3(c, OP_TRANSP, 0, 3, quote_value(c->m, l, line), op3(c, OP_I0, 0, 0, NULL, NULL, NULL), u0));
}

/* comp A side u0 and fill A side u0 r (D8, C6): A : I -> Type n, each
   branch of side at j has type A j, and u0 : A i0 meets the base condition.
   comp has type A i1. fill has type A r and is the comp
   comp (fun j => A (r /\ j)) (fun j => [ ~ r |-> u0 , side (r /\ j) ]) u0
   over the face phi \/ ~ r. */
static const Core *rule_comp(Call *k, int fill) {
  Checker *c = k->c;
  uint64_t level = 0;
  uint32_t l = c->level + 1u;
  uint32_t n = 0;
  const Core *fk[CELLS_MAX];
  const Core *phi = NULL;
  const Core *line = line_of(c, k->args[0], &level);
  const Value *lv = here(c, line);
  const Core *side = lv == NULL ? NULL : side_of(c, k->args[1], NULL, lv, &phi, fk, &n);
  const Core *u0 = side == NULL ? NULL : check(c, k->args[2], apply_value(c->m, lv, op0(c, OP_I0)));
  const Core *r;
  const Core *rj;
  const Value *rv;
  const Value *sv;
  int homog;
  if (u0 == NULL || !side_base(c, side, fk, n, u0))
    return NULL;
  homog = line_const(c, lv);
  sv = here(c, side);
  if (!fill) {
    k->type = apply_value(c->m, lv, op0(c, OP_I1));
    return comp_core(c, homog, lv, phi, sv, u0);
  }
  r = check(c, k->args[3], op0(c, OP_I));
  rv = here(c, r);
  if (rv == NULL || sv == NULL)
    return NULL;
  rj = op3(c, OP_IMIN, 0, 2, quote_value(c->m, l, rv), core_index(c, CORE_VAR, 0), NULL);
  k->type = apply_value(c->m, lv, rv);
  return comp_core(c, homog, here(c, core_bind(c, CORE_LAM, "j", NULL, core_app(c, quote_value(c->m, l, lv), rj))),
                   op3(c, OP_IMAX, 0, 2, phi, op3(c, OP_INEG, 0, 1, r, NULL, NULL), NULL),
                   here(c, core_bind(c, CORE_LAM, "j", NULL, op3(c, OP_SYSTEM, 0, 3, op3(c, OP_INEG, 0, 1, quote_value(c->m, l, rv), NULL, NULL), quote_value(c->m, l, here(c, u0)), core_app(c, quote_value(c->m, l, sv), rj)))),
                   u0);
}

static const Core *rule(Call *k) {
  switch (k->b->rule) {
  case RULE_TYPE0:
    k->type = val_univ(k->c->m, 0);
    return op3(k->c, k->b->op, 0, 0, NULL, NULL, NULL);
  case RULE_VALUE0:
    k->type = tyop(k->c, k->b->result, 0, NULL, NULL, NULL);
    return op3(k->c, k->b->op, 0, 0, NULL, NULL, NULL);
  case RULE_TYPE1:
    return rule_type1(k);
  case RULE_TYPE2:
    return rule_type2(k);
  case RULE_EQ:
    return rule_eq(k);
  case RULE_PAIR:
    return rule_pair(k);
  case RULE_INJ:
    return rule_inj(k);
  case RULE_EMPTY:
    return rule_empty(k);
  case RULE_SOME:
    return rule_some(k);
  case RULE_CONS:
    return rule_cons(k);
  case RULE_PACK:
    return rule_pack(k);
  case RULE_REFL:
    return rule_refl(k);
  case RULE_PART:
    return rule_part(k);
  case RULE_SIGMA_PART:
    return rule_sigma_part(k);
  case RULE_EITHER:
    return rule_either(k);
  case RULE_OPTION:
    return rule_option(k);
  case RULE_PURE:
    return rule_pure(k);
  case RULE_MAP:
    return rule_map(k);
  case RULE_BIND:
    return rule_bind(k);
  case RULE_FILTER:
    return rule_filter(k);
  case RULE_FOLD:
    return rule_fold(k);
  case RULE_UNFOLD:
    return rule_unfold(k);
  case RULE_SYMM:
    return rule_symm(k);
  case RULE_TRANS:
    return rule_trans(k);
  case RULE_CONG:
    return rule_cong(k);
  case RULE_TRANSPORT:
    return rule_transport(k);
  case RULE_NAT2:
    return rule_nat2(k);
  case RULE_NAT_IS_SET:
    return rule_nat_is_set(k);
  case RULE_FLAG_IF:
    return rule_flag_if(k);
  case RULE_INTERVAL:
    return rule_interval(k);
  case RULE_PATHP:
    return rule_pathp(k);
  case RULE_PAPP:
    return rule_papp(k);
  case RULE_PLAM:
    return rule_plam(k);
  case RULE_PARTIAL:
    return rule_partial(k);
  case RULE_SYSTEM:
    return rule_system(k);
  case RULE_FROM_PARTIAL:
    return rule_from_partial(k);
  case RULE_TRANSP:
    return rule_transp(k);
  case RULE_HCOMP:
    return rule_hcomp(k);
  case RULE_COMP:
    return rule_comp(k, 0);
  case RULE_FILL:
    return rule_comp(k, 1);
  case RULE_GLUE:
    return rule_glue(k);
  case RULE_GLUE_ELEM:
    return rule_glue_elem(k);
  case RULE_UNGLUE:
    return rule_unglue(k);
  case RULE_FUNEXT:
    return rule_funext(k);
  }
  return NULL;
}

/* Checks the family parameters ARGS. VALS gets their values. */
static int check_params(Checker *c, const FamilyInfo *f, const Term *const *args, const Core **cores, const Value **vals) {
  uint32_t i;
  for (i = 0; i < f->param_count; i++) {
    cores[i] = check(c, args[i], eval_core(c->m, env_of(c, vals, i), f->param_types[i]));
    vals[i] = here(c, cores[i]);
    if (vals[i] == NULL)
      return 0;
  }
  return 1;
}

static const Core *family_app(Call *k, uint32_t fam) {
  Checker *c = k->c;
  const FamilyInfo *f = &c->m->families[fam];
  const Core **cores = arena_alloc(c->m->arena, ((size_t)f->param_count + 1u) * sizeof *cores);
  const Value **vals = arena_alloc(c->m->arena, ((size_t)f->param_count + 1u) * sizeof *vals);
  if (cores == NULL || vals == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  if (!check_params(c, f, k->args, cores, vals))
    return NULL;
  k->type = val_univ(c->m, f->universe);
  return core_op(c, OP_FAMILY, fam, 0, cores, f->param_count);
}

/* The environment of the type of field J of CI (its result type at J =
   field_count) at the values VALS, laid out [params, fields]: the fields of
   a uniform family see the parameters, those of an indexed family only the
   earlier fields (D3). */
static const Env *field_env(Checker *c, const CtorInfo *ci, const Value *const *vals, uint32_t j) {
  const FamilyInfo *f = &c->m->families[ci->family];
  return f->indexed ? env_of(c, vals + f->param_count, j) : env_of(c, vals, f->param_count + j);
}

/* The result indices of a constructor of an indexed family at its field
   values (D3): VALS[0 .. p) and their quotes CORES[0 .. p). */
static int index_values(Checker *c, const CtorInfo *ci, const Value **vals, const Core **cores) {
  const FamilyInfo *f = &c->m->families[ci->family];
  const Env *env = field_env(c, ci, vals, ci->field_count);
  uint32_t i;
  if (ci->index == NULL)
    return FAIL(c, "INTERNAL", "a constructor of the indexed family %s has no result indices", f->name);
  for (i = 0; i < f->param_count; i++) {
    vals[i] = eval_core(c->m, env, ci->index[i]);
    cores[i] = vals[i] == NULL ? NULL : quote_value(c->m, c->level, vals[i]);
    if (cores[i] == NULL)
      return 0;
  }
  return 1;
}

/* A path constructor at depth d (former C4, D4) elaborates to
   <r_1> .. <r_d> PCTOR[params, fields, r_1 .. r_d]. Its type is the declared
   Path type at VALS (the params, then the fields). */
static const Core *pctor_app(Call *k, uint32_t ctor, const Value *const *vals, uint32_t n) {
  Checker *c = k->c;
  const CtorInfo *ci = &c->m->ctors[ctor];
  uint32_t d = ci->depth;
  const Core **args = arena_alloc(c->m->arena, ((size_t)n + d) * sizeof *args);
  const Core *body;
  uint32_t i;
  if (args == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  for (i = 0; i < n; i++)
    args[i] = quote_value(c->m, c->level + d, vals[i]);
  for (i = 0; i < d; i++)
    args[n + i] = core_index(c, CORE_VAR, d - 1u - i);
  k->type = eval_core(c->m, field_env(c, ci, vals, ci->field_count), ci->type);
  body = core_op(c, OP_PCTOR, ctor, 0, args, n + d);
  for (i = 0; i < d; i++)
    body = op3(c, OP_PLAM, 0, 1, core_bind(c, CORE_LAM, "%i", NULL, body), NULL, NULL);
  return body;
}

/* A constructor of a uniform family takes the family parameters (the
   indices), then its fields. A constructor of an indexed family takes its
   fields only: its result indices lead the value, computed from the fields
   (D3). The field types are a telescope. */
static const Core *ctor_app(Call *k, uint32_t ctor) {
  Checker *c = k->c;
  const CtorInfo *ci = &c->m->ctors[ctor];
  const FamilyInfo *f = &c->m->families[ci->family];
  uint32_t p = f->param_count;
  uint32_t given = f->indexed ? 0u : p;
  uint32_t n = p + ci->field_count;
  const Core **cores = arena_alloc(c->m->arena, ((size_t)n + 1u) * sizeof *cores);
  const Value **vals = arena_alloc(c->m->arena, ((size_t)n + 1u) * sizeof *vals);
  uint32_t j;
  if (cores == NULL || vals == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  if (!f->indexed && !check_params(c, f, k->args, cores, vals))
    return NULL;
  for (j = 0; j < ci->field_count; j++) {
    cores[p + j] = check(c, k->args[given + j], eval_core(c->m, field_env(c, ci, vals, j), ci->fields[j].type));
    vals[p + j] = cores[p + j] == NULL ? NULL : here(c, cores[p + j]);
    if (vals[p + j] == NULL)
      return NULL;
  }
  if (f->indexed && !index_values(c, ci, vals, cores))
    return NULL;
  if (ci->depth > 0u)
    return pctor_app(k, ctor, vals, n);
  k->type = val_op(c->m, VAL_OP, OP_FAMILY, ci->family, 0, vals, p);
  return core_op(c, OP_CTOR, ctor, 0, cores, n);
}

/* A field of a one-constructor family is a projection. */
static const Core *proj_app(Call *k, uint32_t ctor, uint32_t field) {
  Checker *c = k->c;
  const CtorInfo *ci = &c->m->ctors[ctor];
  const Value *xt = NULL;
  const Core *x = infer(c, k->args[0], &xt);
  const Env *env;
  uint32_t i;
  if (x == NULL)
    return NULL;
  if (!val_is(xt, OP_FAMILY) || xt->inst != ci->family)
    return nul(FAIL(c, "TYPE_MISMATCH", "%s needs a value of the family %s, found a term of type %s", ci->fields[field].name, c->m->families[ci->family].name, show(c, xt)));
  /* The field types are a telescope: the earlier fields are the earlier
     projections of X. */
  env = c->m->families[ci->family].indexed ? NULL : env_of(c, xt->args, xt->argc);
  for (i = 0; i < field; i++)
    env = env_push(c->m, env, here(c, core_op(c, OP_PROJ, ctor, i, &x, 1)));
  k->type = eval_core(c->m, env, ci->fields[field].type);
  return core_op(c, OP_PROJ, ctor, field, &x, 1);
}

static int find_known(Checker *c, const char *name, Head *h) {
  const Machine *m = c->m;
  uint32_t i;
  uint32_t j;
  for (i = 0; i < m->family_count; i++) {
    if (strcmp(m->families[i].name, name) == 0) {
      h->kind = HEAD_FAMILY;
      h->index = i;
      c->self_uses += i == c->self;
      c->sib_uses += c->self != NO_FAMILY && i != c->self && i >= m->families[c->self].block_first && i < m->families[c->self].block_first + m->families[c->self].block_count;
      return 1;
    }
  }
  for (i = 0; i < m->ctor_count; i++) {
    if (strcmp(m->ctors[i].name, name) == 0) {
      h->kind = HEAD_CTOR;
      h->index = i;
      return 1;
    }
  }
  for (i = 0; i < m->ctor_count; i++) {
    for (j = 0; m->families[m->ctors[i].family].ctor_count == 1u && j < m->ctors[i].field_count; j++) {
      if (strcmp(m->ctors[i].fields[j].name, name) == 0) {
        h->kind = HEAD_PROJ;
        h->index = i;
        h->field = j;
        return 1;
      }
    }
  }
  for (i = 0; i < BUILTIN_COUNT; i++) {
    if (strcmp(BUILTINS[i].name, name) == 0) {
      h->kind = HEAD_BUILTIN;
      h->builtin = &BUILTINS[i];
      return 1;
    }
  }
  return 0;
}

/* An unknown name. When it names this declaration or a later one, the use
   is recursion (rule R3). */
static int unknown_name(Checker *c, const char *name) {
  size_t i;
  for (i = c->decl_index; i < c->decls->count; i++) {
    if (strcmp(c->decls->items[i].name, name) == 0)
      return FAIL(c, "REFUSE_REC", "%s is this definition or a later one; a definition can use only earlier ones (rule R3)", name);
  }
  return FAIL(c, "NAME_UNKNOWN", "unknown name %s", name);
}

/* Name resolution order: locals (the innermost first), definitions,
   families, constructors, projections, core names. */
static int resolve_head(Checker *c, const Term *head, Head *h) {
  memset(h, 0, sizeof *h);
  h->kind = HEAD_TERM;
  if (head->kind != TERM_VAR || find_local(c, head->name) != NULL || find_def(c->m, head->name) != NO_DEF)
    return 1;
  return find_known(c, head->name, h) ? 1 : unknown_name(c, head->name);
}

static uint32_t known_arity(const Checker *c, const Head *h) {
  const CtorInfo *ci = h->kind == HEAD_CTOR ? &c->m->ctors[h->index] : NULL;
  switch (h->kind) {
  case HEAD_TERM:
    return 0u;
  case HEAD_FAMILY:
    return c->m->families[h->index].param_count;
  case HEAD_CTOR:
    return ci == NULL ? 0u : (c->m->families[ci->family].indexed ? 0u : c->m->families[ci->family].param_count) + ci->field_count;
  case HEAD_PROJ:
    return 1u;
  case HEAD_BUILTIN:
    return h->builtin->arity;
  }
  return 0u;
}

static const char *known_name(const Checker *c, const Head *h) {
  switch (h->kind) {
  case HEAD_TERM:
    return "?";
  case HEAD_FAMILY:
    return c->m->families[h->index].name;
  case HEAD_CTOR:
    return c->m->ctors[h->index].name;
  case HEAD_PROJ:
    return c->m->ctors[h->index].fields[h->field].name;
  case HEAD_BUILTIN:
    return h->builtin->name;
  }
  return "?";
}

static const Core *known_core(Call *k, const Head *h) {
  switch (h->kind) {
  case HEAD_TERM:
    return nul(FAIL(k->c, "INTERNAL", "a plain term reached the core name rules"));
  case HEAD_FAMILY:
    return family_app(k, h->index);
  case HEAD_CTOR:
    return ctor_app(k, h->index);
  case HEAD_PROJ:
    return proj_app(k, h->index, h->field);
  case HEAD_BUILTIN:
    return rule(k);
  }
  return NULL;
}

/* An under-applied core name checked against a function type: WHOLE becomes
   `fun x => WHOLE x`, with a name that the lexer cannot make. */
static const Core *eta(Checker *c, const Term *whole, const Value *hint, const Value **type) {
  char *name = arena_alloc(c->m->arena, 24);
  Term *x = arena_alloc(c->m->arena, sizeof *x);
  Term *app = arena_alloc(c->m->arena, sizeof *app);
  Term *lam = arena_alloc(c->m->arena, sizeof *lam);
  if (name == NULL || x == NULL || app == NULL || lam == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  c->eta_count++;
  snprintf(name, 24, "%%eta%u", c->eta_count);
  x->kind = TERM_VAR;
  x->name = name;
  app->kind = TERM_APP;
  app->left = whole;
  app->right = x;
  lam->kind = TERM_LAM;
  lam->name = name;
  lam->right = app;
  return check_lam(c, lam, hint, type);
}

/* Applies K of type TY to ARGS, one by one. */
static const Core *apply_rest(Checker *c, const Core *k, const Value *ty, const Term *const *args, uint32_t argc, const Value *hint, const Value **type) {
  uint32_t i;
  for (i = 0; i < argc && k != NULL; i++) {
    const Core *a;
    if (ty == NULL || ty->kind != VAL_PI)
      return nul(FAIL(c, "TYPE_NOT_FUNCTION", "a term of type %s is applied to an argument", show(c, ty)));
    a = check(c, args[i], ty->dom);
    ty = closure_apply(c->m, ty, here(c, a));
    k = core_app(c, k, a);
  }
  *type = ty;
  return finish(c, k, hint, type);
}

static const Core *var_core(Checker *c, const char *name, const Value **type) {
  const Scope *s = find_local(c, name);
  uint32_t d = find_def(c->m, name);
  if (s != NULL) {
    *type = s->type;
    return core_index(c, CORE_VAR, c->level - 1u - s->level);
  }
  if (d != NO_DEF) {
    *type = c->m->defs[d].type;
    return core_index(c, CORE_GLOBAL, d);
  }
  return nul(FAIL(c, "NAME_UNKNOWN", "unknown name %s", name));
}

static const Core *apply_term(Checker *c, const Term *head, const Term *const *args, uint32_t argc, const Value *hint, const Value **type) {
  const Value *ty = NULL;
  const Core *k = head->kind == TERM_VAR ? var_core(c, head->name, &ty) : infer(c, head, &ty);
  return apply_rest(c, k, ty, args, argc, hint, type);
}

static const Core *apply_known(Checker *c, const Term *whole, const Head *h, const Term *const *args, uint32_t argc, const Value *hint, const Value **type) {
  uint32_t arity = known_arity(c, h);
  Call k;
  const Core *core;
  const Value *motive;
  if (h->kind == HEAD_BUILTIN && h->builtin->rule == RULE_FOLD && argc > 0u && argc < 4u && hint != NULL && hint->kind == VAL_PI && try_nat_motive(c, args[0], &motive) != NULL)
    return eta(c, whole, hint, type);
  if (argc < arity && hint != NULL && hint->kind == VAL_PI)
    return eta(c, whole, hint, type);
  if (argc < arity)
    return nul(FAIL(c, "TYPE_ARITY", "%s needs %u arguments, found %u", known_name(c, h), arity, argc));
  memset(&k, 0, sizeof k);
  k.c = c;
  k.b = h->builtin;
  k.args = args;
  k.argc = argc;
  k.hint = argc == arity ? hint : NULL;
  k.outer = hint;
  k.used = arity;
  core = known_core(&k, h);
  return apply_rest(c, core, k.type, args + k.used, argc - k.used, hint, type);
}

static const Core *elab_spine(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Term **args;
  const Term *head = t;
  uint32_t argc = 0;
  uint32_t i;
  Head h;
  while (head->kind == TERM_APP) {
    argc++;
    head = head->left;
  }
  if (argc > SPINE_MAX)
    return nul(FAIL(c, "TYPE_ARITY", "an application has more than %u arguments", SPINE_MAX));
  args = arena_alloc(c->m->arena, ((size_t)argc + 1u) * sizeof *args);
  if (args == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  for (head = t, i = argc; head->kind == TERM_APP; head = head->left)
    args[--i] = head->right;
  if (!resolve_head(c, head, &h))
    return NULL;
  return h.kind == HEAD_TERM ? apply_term(c, head, args, argc, hint, type) : apply_known(c, t, &h, args, argc, hint, type);
}

static int annotation_ok(Checker *c, const Term *ann, const Value *dom) {
  uint64_t level = 0;
  const Value *v = here(c, check_pretype(c, ann, &level));
  if (v == NULL)
    return 0;
  return conv_on(c, c->level, v, dom) ? 1 : FAIL(c, "TYPE_MISMATCH", "the binder type %s differs from the expected type %s", show(c, v), show(c, dom));
}

static const Core *check_lam(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Core *body;
  if (hint->kind != VAL_PI)
    return nul(FAIL(c, "TYPE_MISMATCH", "a function is not a term of type %s", show(c, hint)));
  if (t->left != NULL && !annotation_ok(c, t->left, hint->dom))
    return NULL;
  if (!push(c, t->name, hint->dom))
    return NULL;
  body = check(c, t->right, closure_apply(c->m, hint, c->env->value));
  pop(c);
  *type = hint;
  return core_bind(c, CORE_LAM, t->name, NULL, body);
}

static const Core *infer_lam(Checker *c, const Term *t, const Value **type) {
  uint64_t level = 0;
  const Value *dom;
  const Value *bt = NULL;
  const Core *body;
  const Core *quoted;
  if (t->left == NULL)
    return nul(FAIL(c, "TYPE_INFER", "fun %s => ... needs a binder type here", t->name));
  dom = here(c, check_pretype(c, t->left, &level));
  if (!push(c, t->name, dom))
    return NULL;
  body = infer(c, t->right, &bt);
  quoted = body == NULL ? NULL : quote_value(c->m, c->level, bt);
  pop(c);
  *type = val_closure(c->m, VAL_PI, t->name, dom, c->env, quoted);
  return core_bind(c, CORE_LAM, t->name, NULL, body);
}

/* (x : A) -> B and Sigma (x : A) B. */
static const Core *elab_binder(Checker *c, const Term *t, CoreKind kind, const Value *hint, const Value **type) {
  uint64_t la = 0;
  uint64_t lb = 0;
  int pretype = kind == CORE_PI; /* a Pi over I is a pretype; a Sigma refuses one (former C1) */
  const Core *a = check_type_at(c, t->left, &la, pretype);
  const Core *b;
  if (a == NULL || !push(c, t->name, here(c, a)))
    return NULL;
  b = check_type_at(c, t->right, &lb, pretype);
  pop(c);
  *type = la == UINT64_MAX || lb == UINT64_MAX ? op0(c, OP_SSET) : val_univ(c->m, la > lb ? la : lb);
  return finish(c, core_bind(c, kind, t->name, a, b), hint, type);
}

static const Core *elab_inner(Checker *c, const Term *t, const Value *hint, const Value **type) {
  switch (t->kind) {
  case TERM_VAR:
  case TERM_APP:
    return elab_spine(c, t, hint, type);
  case TERM_NAT:
    *type = nat_type(c);
    return finish(c, core_leaf(c, CORE_NAT, t->nat), hint, type);
  case TERM_LAM:
    return hint != NULL ? check_lam(c, t, hint, type) : infer_lam(c, t, type);
  case TERM_PI:
    return elab_binder(c, t, CORE_PI, hint, type);
  case TERM_SIGMA:
    return elab_binder(c, t, CORE_SIGMA, hint, type);
  case TERM_UNIV:
    if (t->nat >= UNIVERSE_MAX)
      return nul(FAIL(c, "TYPE_UNIVERSE", "Type %llu is above the largest universe Type %u", (unsigned long long)t->nat, UNIVERSE_MAX - 1u));
    *type = val_univ(c->m, t->nat + 1u);
    return finish(c, core_leaf(c, CORE_UNIV, t->nat), hint, type);
  }
  return NULL;
}

static const Core *elab(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Core *k;
  if (c->depth >= CHECK_DEPTH_LIMIT)
    return nul(FAIL(c, "CHECK_DEPTH", "the term is nested deeper than %u levels", CHECK_DEPTH_LIMIT));
  c->depth++;
  if (hint != NULL && c->face != NULL) {
    hint = value_under_face(c->m, c->level, c->face, hint);
    if (hint == NULL) {
      c->depth--;
      return NULL;
    }
  }
  k = c->m->diag->set ? NULL : elab_inner(c, t, hint, type);
  if (k != NULL && c->face != NULL) {
    *type = value_under_face(c->m, c->level, c->face, *type);
    if (*type == NULL)
      k = NULL;
  }
  c->depth--;
  return k;
}

/* NULL when NAME is free, else the reason it is taken (rule R4). */
static const char *name_owner(const Checker *c, const char *name) {
  const Machine *m = c->m;
  uint32_t i;
  uint32_t j;
  for (i = 0; i < sizeof RESERVED / sizeof RESERVED[0]; i++) {
    if (strcmp(RESERVED[i], name) == 0)
      return "a reserved name";
  }
  for (i = 0; i < BUILTIN_COUNT; i++) {
    if (strcmp(BUILTINS[i].name, name) == 0)
      return "a core name";
  }
  for (i = 0; i < m->family_count; i++) {
    if (strcmp(m->families[i].name, name) == 0)
      return "a domain family";
  }
  for (i = 0; i < m->ctor_count; i++) {
    for (j = 0; j < m->ctors[i].field_count; j++) {
      if (strcmp(m->ctors[i].fields[j].name, name) == 0)
        return "a domain field";
    }
    if (strcmp(m->ctors[i].name, name) == 0)
      return "a domain constructor";
  }
  i = find_def(m, name);
  if (i == NO_DEF)
    return NULL;
  return m->defs[i].origin == ORIGIN_DOMAIN ? "a domain definition" : "already defined";
}

static int name_free(Checker *c, const char *name) {
  const char *why = name_owner(c, name);
  return why == NULL ? 1 : FAIL(c, "REFUSE_NAME", "%s is %s (rule R4)", name, why);
}

#define NO_FIELD UINT32_MAX

/* The constructor being checked: its family and the fields in scope (only a
   path constructor pushes its fields). */
typedef struct {
  uint32_t fam;
  const FieldInfo *fields;
  uint32_t count;
} CtorScope;

/* The field of S that K names under DEPTH extra binders, or NO_FIELD. With
   COUNT fields in scope, index i < COUNT is field COUNT - 1 - i. */
static uint32_t scope_field(const CtorScope *s, const Core *k, uint32_t depth) {
  return k->kind == CORE_VAR && k->index >= depth && k->index - depth < s->count ? s->count - 1u - (k->index - depth) : NO_FIELD;
}

/* 1 when ARGS are the family parameters in order, under DEPTH extra binders:
   parameter j of n is index DEPTH + COUNT + n - 1 - j. */
static int scope_params(const Checker *c, const CtorScope *s, const Core *const *args, uint32_t depth) {
  uint32_t n = c->m->families[s->fam].param_count;
  uint32_t j;
  for (j = 0; j < n; j++) {
    if (args[j]->kind != CORE_VAR || args[j]->index != depth + s->count + (n - 1u - j))
      return 0;
  }
  return 1;
}

/* 1 when K mentions a variable with an index in [LO, HI) after the SHIFT
   binders inside K are discounted. */
static int mentions_var(const Core *k, uint32_t shift, uint32_t lo, uint32_t hi) {
  uint32_t j;
  int any = 0;
  if (k == NULL)
    return 0;
  switch (k->kind) {
  case CORE_VAR:
    return k->index >= shift + lo && k->index < shift + hi;
  case CORE_GLOBAL:
  case CORE_NAT:
  case CORE_TRAP:
  case CORE_UNIV:
    return 0;
  case CORE_LAM:
  case CORE_PI:
  case CORE_SIGMA:
    return mentions_var(k->a, shift, lo, hi) || mentions_var(k->b, shift + 1u, lo, hi);
  case CORE_APP:
    return mentions_var(k->a, shift, lo, hi) || mentions_var(k->b, shift, lo, hi);
  case CORE_OP:
    for (j = 0; j < k->argc; j++)
      any = any || mentions_var(k->args[j], shift, lo, hi);
    return any;
  }
  return 0;
}

/* 1 when K, under DEPTH extra binders, mentions a recursive field of S
   (FIELD_SELF or FIELD_SELF_PATH). Field i is index COUNT - 1 - i. */
static int mentions_self_field(const CtorScope *s, const Core *k, uint32_t depth) {
  uint32_t i;
  int any = 0;
  for (i = 0; i < s->count; i++)
    any = any || (s->fields[i].kind != FIELD_PLAIN && mentions_var(k, depth, s->count - 1u - i, s->count - i));
  return any;
}

/* 1 when K is the sort FAM applied to param_count arguments. */
static int family_head(const Checker *c, uint32_t fam, const Core *k) {
  return k->kind == CORE_OP && k->op == OP_FAMILY && k->inst == fam && k->argc == c->m->families[fam].param_count;
}

/* 1 when K is the family of S applied to param_count arguments. */
static int self_head(const Checker *c, const CtorScope *s, const Core *k) {
  return family_head(c, s->fam, k);
}

/* 1 when FAM is a sort of the mutual block of S, not the family of S. */
static int sib_sort(const Checker *c, const CtorScope *s, uint32_t fam) {
  const FamilyInfo *f = &c->m->families[s->fam];
  return fam != s->fam && fam >= f->block_first && fam < f->block_first + f->block_count;
}

/* 1 when ARGS are leading arguments of the sort FAM (the family of S or a
   sort of its block) under DEPTH extra binders: the parameters in order (a
   uniform family; a uniform sort of a block has none), or index terms over
   the plain fields, with no binder variable and no recursive field (an
   indexed family, D4). */
static int leading_args_ok(const Checker *c, const CtorScope *s, uint32_t fam, const Core *const *args, uint32_t depth) {
  const FamilyInfo *f = &c->m->families[fam];
  uint32_t j;
  int ok = 1;
  if (!f->indexed)
    return fam == s->fam ? scope_params(c, s, args, depth) : f->param_count == 0u;
  for (j = 0; j < f->param_count; j++)
    ok = ok && !mentions_var(args[j], 0u, 0u, depth) && !mentions_self_field(s, args[j], depth);
  return ok;
}

/* 1 when K, under DEPTH extra binders, is the family of S applied to its
   parameters, or to index terms over the plain fields in an indexed family
   (the type of a FIELD_SELF field). */
static int uniform_family(const Checker *c, const CtorScope *s, const Core *k, uint32_t depth) {
  return self_head(c, s, k) && leading_args_ok(c, s, s->fam, k->args, depth);
}

/* 1 when K, under DEPTH extra binders, is another sort of the block of S
   applied to index terms over the plain fields (the type of a FIELD_SIB
   field, D9). */
static int sib_family(const Checker *c, const CtorScope *s, const Core *k, uint32_t depth) {
  return k->kind == CORE_OP && k->op == OP_FAMILY && sib_sort(c, s, k->inst) && family_head(c, k->inst, k) && leading_args_ok(c, s, k->inst, k->args, depth);
}

/* 1 when K, under DEPTH extra binders, is a point term of the sort FAM (the
   family of S or a sort of its block, D9): a field of the constructor at
   FAM (FIELD_SELF or FIELD_SIB), or an earlier point constructor of FAM
   applied to its leading arguments, then for each of its fields a point
   term (a FIELD_SELF or FIELD_SIB field) or a field of the constructor. The
   check is syntactic. */
static int point_term_at(const Checker *c, const CtorScope *s, uint32_t fam, const Core *k, uint32_t depth) {
  const Machine *m = c->m;
  const FamilyInfo *f = &m->families[fam];
  uint32_t field = scope_field(s, k, depth);
  const CtorInfo *ci;
  uint32_t j;
  if (field != NO_FIELD)
    return fam == s->fam ? s->fields[field].kind == FIELD_SELF : s->fields[field].kind == FIELD_SIB && s->fields[field].type->inst == fam;
  if (k->kind != CORE_OP || k->op != OP_CTOR || k->inst < f->first_ctor || k->inst >= f->first_ctor + f->ctor_count || k->inst >= m->ctor_count)
    return 0;
  ci = &m->ctors[k->inst];
  if (ci->depth != 0u || k->argc != f->param_count + ci->field_count || !leading_args_ok(c, s, fam, k->args, depth))
    return 0;
  for (j = 0; j < ci->field_count; j++) {
    const Core *arg = k->args[f->param_count + j];
    int rec = ci->fields[j].kind == FIELD_SELF || ci->fields[j].kind == FIELD_SIB;
    if (rec ? !point_term_at(c, s, ci->fields[j].kind == FIELD_SELF ? fam : ci->fields[j].type->inst, arg, depth) : scope_field(s, arg, depth) == NO_FIELD)
      return 0;
  }
  return 1;
}

/* A point term of the family of S. */
static int point_term(const Checker *c, const CtorScope *s, const Core *k, uint32_t depth) {
  return point_term_at(c, s, s->fam, k, depth);
}

/* The line body of K when K is Path A a b, whose line is `fun _ => A` (see
   const_line), else NULL. Inside the line, indices shift by 1. */
static const Core *path_body(const Core *k) {
  return k->kind == CORE_OP && k->op == OP_PATHP && k->argc == 3u && k->args[0]->kind == CORE_LAM ? k->args[0]->b : NULL;
}

/* The family of S under the Path layers of T, their number in DEPTH; NULL
   (and depth 0) when T is not Path^d over the family. */
static const Core *path_base(const Checker *c, const CtorScope *s, const Core *t, uint32_t *depth) {
  const Core *k = t;
  *depth = 0;
  while (k != NULL && !uniform_family(c, s, k, *depth)) {
    k = path_body(k);
    (*depth)++;
  }
  *depth = k == NULL ? 0u : *depth;
  return k;
}

/* K from under a constant line binder, in the current context. */
static const Core *unshift(Checker *c, const Core *k, uint32_t depth) {
  const Env *e = c->env;
  const Value *v;
  uint32_t d;
  for (d = 0; d < depth; d++)
    e = env_push(c->m, e, val_var(c->m, c->level + d));
  v = eval_core(c->m, e, k);
  return quote_value(c->m, c->level, v);
}

/* A field type that mentions another sort of the block and not the
   family of S (D9) is that sort applied to index terms over the plain
   fields (FIELD_SIB). A path type over another sort is a K1 limit. */
static int sib_field(Checker *c, const CtorScope *s, const Core *t, const Field *field, FieldInfo *out) {
  if (sib_family(c, s, t, 0u)) {
    out->kind = FIELD_SIB;
    return 1;
  }
  if (t->kind == CORE_OP && t->op == OP_PATHP)
    return FAIL(c, "DOMAIN_QIIT", "the field %s has a path type over another sort of the mutual block; only a path over %s is available (K1 limit)", field->name, c->m->families[s->fam].name);
  return FAIL(c, "DOMAIN_FAMILY", "the field %s must have the type S i1 .. in, with S another sort of the mutual block of %s and index terms over the plain fields", field->name, c->m->families[s->fam].name);
}

/* A field type that mentions the family is the family applied to its
   parameters (FIELD_SELF) or, in a path constructor only, Path X u v with X
   that type and point terms u, v (FIELD_SELF_PATH). In an indexed family
   the parameters are index terms over the plain fields. A plain field type
   cannot mention a recursive field (K3): a fold case gets it folded. */
static int check_field(Checker *c, const CtorScope *s, int path_ctor, const Field *field, FieldInfo *out) {
  uint64_t level = 0;
  const char *fam = c->m->families[s->fam].name;
  int indexed = c->m->families[s->fam].indexed;
  int ok_line;
  const Core *t;
  const Core *line;
  if (!name_free(c, field->name))
    return 0;
  c->self_uses = 0;
  c->sib_uses = 0;
  t = check_type(c, field->type, &level);
  if (t == NULL)
    return 0;
  if (level > c->m->families[s->fam].universe)
    c->m->families[s->fam].universe = level;
  out->name = field->name;
  out->type = t;
  out->kind = uniform_family(c, s, t, 0u) ? FIELD_SELF : FIELD_PLAIN;
  line = path_body(t);
  if (out->kind == FIELD_SELF)
    return 1;
  ok_line = line != NULL && uniform_family(c, s, line, 1u);
  if (!self_head(c, s, t) && !(line != NULL && self_head(c, s, line)) && mentions_self_field(s, t, 0u))
    return FAIL(c, "DOMAIN_INDEX", "the type of the field %s mentions a recursive field; only the plain fields are in scope (K3)", field->name);
  if (c->self_uses == 0u && c->sib_uses > 0u)
    return sib_field(c, s, t, field, out);
  if (c->self_uses == 0u)
    return 1;
  if (!ok_line && indexed && (self_head(c, s, t) || (line != NULL && self_head(c, s, line))))
    return FAIL(c, "DOMAIN_INDEX", "an index of the recursive field %s mentions a recursive field; only the plain fields are in scope", field->name);
  if (!ok_line)
    return FAIL(c, "DOMAIN_FAMILY", "the recursive field %s must have the type %s p1 .. pn with the family parameters", field->name, fam);
  if (!path_ctor)
    return FAIL(c, "DOMAIN_HIT", "the field %s has a path type over %s, so its constructor must be a path constructor", field->name, fam);
  if (!point_term(c, s, t->args[1], 0u) || !point_term(c, s, t->args[2], 0u))
    return FAIL(c, "DOMAIN_HIT", "an end of a path constructor must be a point term (the field %s)", field->name);
  out->kind = FIELD_SELF_PATH;
  return 1;
}

/* The result indices of a path constructor of an indexed family (D3): the
   arguments of BASE, the family under DEPTH line binders, brought into the
   context of the fields. */
static int index_from_base(Checker *c, const CtorScope *s, const Core *base, uint32_t depth, CtorInfo *ci) {
  uint32_t n = c->m->families[s->fam].param_count;
  const Core **index = arena_alloc(c->m->arena, ((size_t)n + 1u) * sizeof *index);
  uint32_t i;
  if (index == NULL)
    return FAIL(c, "OOM", "out of memory");
  for (i = 0; i < n; i++) {
    index[i] = unshift(c, base->args[i], depth);
    if (index[i] == NULL)
      return FAIL(c, "OOM", "out of memory");
  }
  ci->index = index;
  return 1;
}

/* The type of a path constructor, in the context of the parameters and all
   of its fields: Path X a b (depth 1) or Path (Path X x y) p q (depth 2),
   with X the family applied to its parameters, point terms a, b, x, y and
   fields p, q. A wrong end type is a TYPE_MISMATCH from check_type. */
static int check_pctor(Checker *c, const CtorScope *s, const Ctor *ctor, CtorInfo *ci) {
  uint64_t level = 0;
  const char *fam = c->m->families[s->fam].name;
  const Core *t = check_type(c, ctor->type, &level);
  const Core *inner = t == NULL ? NULL : path_body(t);
  const Core **ends = arena_alloc(c->m->arena, 4u * sizeof *ends);
  uint32_t depth = 0;
  const Core *base = t == NULL ? NULL : path_base(c, s, t, &depth);
  if (t == NULL)
    return 0;
  if (ends == NULL)
    return FAIL(c, "OOM", "out of memory");
  if (depth == 0u)
    return FAIL(c, "DOMAIN_HIT", "the path constructor %s must have the type Path %s a b or Path (Path %s x y) p q", ctor->name, fam, fam);
  if (depth > 2u)
    return FAIL(c, "DOMAIN_HIT", "the path constructor %s has dimension %u; only 1 and 2 are available", ctor->name, depth);
  ends[0] = t->args[1];
  ends[1] = t->args[2];
  ends[2] = depth == 2u ? inner->args[1] : NULL;
  ends[3] = depth == 2u ? inner->args[2] : NULL;
  if (depth == 1u && !(point_term(c, s, ends[0], 0u) && point_term(c, s, ends[1], 0u)))
    return FAIL(c, "DOMAIN_HIT", "an end of a path constructor must be a point term (the constructor %s)", ctor->name);
  if (depth == 2u && !(scope_field(s, ends[0], 0u) != NO_FIELD && scope_field(s, ends[1], 0u) != NO_FIELD))
    return FAIL(c, "DOMAIN_HIT", "the ends p, q of the square %s must be fields of the constructor", ctor->name);
  if (depth == 2u && !(point_term(c, s, ends[2], 1u) && point_term(c, s, ends[3], 1u)))
    return FAIL(c, "DOMAIN_HIT", "an end of a path constructor must be a point term (the constructor %s)", ctor->name);
  ends[2] = depth == 2u ? unshift(c, ends[2], 1u) : NULL;
  ends[3] = depth == 2u ? unshift(c, ends[3], 1u) : NULL;
  if (depth == 2u && (ends[2] == NULL || ends[3] == NULL))
    return FAIL(c, "OOM", "out of memory");
  if (c->m->families[s->fam].indexed && !index_from_base(c, s, base, depth, ci))
    return 0;
  ci->depth = depth;
  ci->type = t;
  ci->ends = ends;
  return 1;
}

/* The result of a point constructor of an indexed family (D1): the family
   applied to index terms over the plain fields, in the context of the
   fields. The head test in check_ctor admits only the family itself. */
static int check_index(Checker *c, const CtorScope *s, const Ctor *ctor, CtorInfo *ci) {
  uint64_t level = 0;
  const char *fam = c->m->families[s->fam].name;
  const Core *t = check_type(c, ctor->type, &level);
  if (t == NULL)
    return 0;
  if (!self_head(c, s, t))
    return FAIL(c, "DOMAIN_INDEX", "the result of the point constructor %s must be %s i1 .. in", ctor->name, fam);
  if (!leading_args_ok(c, s, s->fam, t->args, 0u))
    return FAIL(c, "DOMAIN_INDEX", "an index of the point constructor %s mentions a recursive field; only the plain fields are in scope", ctor->name);
  ci->index = t->args;
  return 1;
}

/* 1 when the head of the application spine T is the name NAME. */
static int names_family(const Term *t, const char *name) {
  const Term *k = t;
  while (k->kind == TERM_APP)
    k = k->left;
  return k->kind == TERM_VAR && strcmp(k->name, name) == 0;
}

/* Point constructors come first. The fields form a telescope: each one is
   in scope in the later fields and in the type. A result annotation headed
   by the family names the result indices of a point constructor (D1); any
   other annotation is a path type. */
static int check_ctor(Checker *c, uint32_t fam, const Ctor *ctor) {
  Machine *m = c->m;
  FieldInfo *fields = arena_alloc(m->arena, (ctor->field_count + 1u) * sizeof *fields);
  CtorInfo *ci = &m->ctors[m->ctor_count];
  int index_ctor = ctor->type != NULL && names_family(ctor->type, m->families[fam].name);
  int path_ctor = ctor->type != NULL && !index_ctor;
  int first = m->ctor_count == m->families[fam].first_ctor;
  CtorScope s;
  size_t j;
  if (fields == NULL)
    return FAIL(c, "OOM", "out of memory");
  if (!name_free(c, ctor->name))
    return 0;
  if (m->families[fam].indexed && !path_ctor && !index_ctor)
    return FAIL(c, "DOMAIN_INDEX", "the point constructor %s must state its result %s i1 .. in, as every point constructor of an indexed family", ctor->name, m->families[fam].name);
  if (path_ctor && first)
    return FAIL(c, "DOMAIN_HIT", "the first constructor of %s must be a point constructor, not the path constructor %s", m->families[fam].name, ctor->name);
  if (!path_ctor && !first && m->ctors[m->ctor_count - 1u].depth > 0u)
    return FAIL(c, "DOMAIN_HIT", "the point constructor %s must come before the path constructors of %s", ctor->name, m->families[fam].name);
  s.fam = fam;
  s.fields = fields;
  s.count = 0;
  for (j = 0; j < ctor->field_count; j++) {
    size_t earlier;
    const char *name = ctor->fields[j].name;
    if (strcmp(name, ctor->name) == 0)
      return FAIL(c, "REFUSE_NAME", "%s is the constructor name (rule R4)", name);
    for (earlier = 0; earlier < j; earlier++)
      if (strcmp(name, ctor->fields[earlier].name) == 0)
        return FAIL(c, "REFUSE_NAME", "%s is already a field of this constructor (rule R4)", name);
    if (!check_field(c, &s, path_ctor, &ctor->fields[j], &fields[j]))
      return 0;
    if (!push(c, name, here(c, fields[j].type)))
      return 0;
    s.count++;
  }
  ci->name = ctor->name;
  ci->family = fam;
  ci->fields = fields;
  ci->field_count = (uint32_t)ctor->field_count;
  ci->depth = 0;
  ci->type = NULL;
  ci->ends = NULL;
  ci->index = NULL;
  if (path_ctor && !check_pctor(c, &s, ctor, ci))
    return 0;
  if (index_ctor && !check_index(c, &s, ctor, ci))
    return 0;
  for (; s.count > 0u; s.count--)
    pop(c);
  m->ctor_count++;
  return 1;
}

/* 1 when a constructor of D states a result headed by the name of D: the
   header binders are then the index telescope of an indexed family (D1). */
static int indexed_decl(const Decl *d) {
  size_t i;
  int any = 0;
  for (i = 0; i < d->ctor_count; i++)
    any = any || (d->ctors[i].type != NULL && names_family(d->ctors[i].type, d->name));
  return any;
}

/* The header of the sort D as the family FAM, one of the COUNT sorts SORTS
   of its block. The parameters of a uniform family stay in scope in the
   fields; the index telescope of an indexed family does not (each
   constructor binds its own indices as fields). In a mutual block a header
   cannot name a sort of the block, and a uniform sort takes no parameters
   (K1 limit). */
static int check_header(Checker *c, const Decl *d, uint32_t fam, const Decl *sorts, size_t count) {
  Machine *m = c->m;
  FamilyInfo *f = &m->families[fam];
  const Core **params = arena_alloc(m->arena, (d->param_count + 1u) * sizeof *params);
  int indexed = indexed_decl(d);
  size_t i;
  size_t j;
  if (params == NULL)
    return FAIL(c, "OOM", "out of memory");
  if (count > 1u && d->param_count > 0u && !indexed)
    return FAIL(c, "DOMAIN_QIIT", "the sort %s of a mutual block has parameters, so it must be indexed; a uniform sort of a block takes no parameters (K1 limit)", d->name);
  for (i = 0; i < d->param_count; i++) {
    uint64_t level = 0;
    for (j = 0; j < count; j++) {
      if (count > 1u && names_family(d->params[i].type, sorts[j].name))
        return FAIL(c, "DOMAIN_QIIT", "the index %s of the sort %s has the type %s, a sort of the same mutual block; a sort indexed by a sort of its block is not available (K1 limit)", d->params[i].name, d->name, sorts[j].name);
    }
    params[i] = check_type(c, d->params[i].type, &level);
    if (!push(c, d->params[i].name, here(c, params[i])))
      return 0;
  }
  for (i = indexed ? d->param_count : 0u; i > 0u; i--)
    pop(c);
  f->name = d->name;
  f->param_types = params;
  f->param_count = (uint32_t)d->param_count;
  f->ctor_count = (uint32_t)d->ctor_count;
  f->indexed = indexed;
  return 1;
}

/* Infer the universes of a mutual block after collecting its constructors.
   A later sort can raise the universe of an earlier sibling field. Probe
   field telescopes until no level grows; a failed probe leaves its name
   unbound and is retried after the other sorts have supplied their levels.
   The final constructor pass checks every field at the inferred levels. */
static int infer_block_universes(Checker *c, const Decl *sorts, size_t count, uint32_t first) {
  Machine *m = c->m;
  Diag saved = *m->diag;
  int any = 0;
  int changed;
  size_t j;
  do {
    changed = 0;
    for (j = 0; j < count; j++) {
      FamilyInfo *f = &m->families[first + j];
      size_t i;
      for (i = 0; i < sorts[j].ctor_count; i++) {
        const Ctor *ctor = &sorts[j].ctors[i];
        Checker trial = *c;
        size_t k;
        trial.self = first + (uint32_t)j;
        for (k = 0; k < ctor->field_count; k++) {
          Checker before = trial;
          uint32_t depth = m->depth;
          uint64_t level = 0;
          const Core *t = check_type(&trial, ctor->fields[k].type, &level);
          const Value *type = t == NULL ? NULL : here(&trial, t);
          if (type == NULL || !push(&trial, ctor->fields[k].name, type)) {
            trial = before;
          } else if (level > f->universe) {
            f->universe = level;
            changed = 1;
            any = 1;
          }
          m->depth = depth;
          *m->diag = saved;
        }
      }
    }
  } while (changed);
  return any;
}

/* A family is in scope in its own fields, so a field can be recursive. In a
   mutual block (D9) all the sort headers bind before a constructor is
   checked: a constructor can use every sort of the block, and the
   constructors of its own sort and of the earlier sorts. A sort becomes
   visible after the headers of its block. */
static int check_family(Checker *c, const Decl *d) {
  Machine *m = c->m;
  uint32_t first = m->family_count;
  uint32_t first_ctor = m->ctor_count;
  uint32_t ctor = m->ctor_count;
  int changed;
  size_t count = d->block_count > 0u ? d->block_count : 1u;
  const Decl *sorts = d->block_count > 0u ? d->block : d;
  size_t i;
  size_t j;
  for (j = 0; j < count; j++) {
    for (i = 0; i < j; i++) {
      if (strcmp(sorts[i].name, sorts[j].name) == 0)
        return FAIL(c, "REFUSE_NAME", "%s is already a sort of this mutual block (rule R4)", sorts[j].name);
    }
    if (j > 0u && !name_free(c, sorts[j].name))
      return 0;
    if (!check_header(c, &sorts[j], first + (uint32_t)j, sorts, count))
      return 0;
    m->families[first + j].first_ctor = ctor;
    m->families[first + j].block_first = first;
    m->families[first + j].block_count = (uint32_t)count;
    ctor += (uint32_t)sorts[j].ctor_count;
  }
  m->family_count = first + (uint32_t)count;
  c->provisional_universes = count > 1u;
  do {
    changed = c->provisional_universes;
    m->ctor_count = first_ctor;
    for (j = 0; j < count; j++) {
      uint64_t level = m->families[first + j].universe;
      c->self = first + (uint32_t)j;
      for (i = 0; i < sorts[j].ctor_count; i++) {
        if (!check_ctor(c, c->self, &sorts[j].ctors[i]))
          return 0;
      }
      changed |= level != m->families[first + j].universe;
    }
    /* A field type computed from an earlier constructor may reveal a new
       level only now. Propagate it and recheck the block at those levels. */
    c->provisional_universes = 0;
    if (count > 1u)
      changed |= infer_block_universes(c, sorts, count, first);
  } while (count > 1u && changed);
  return 1;
}

static int check_def(Checker *c, const Decl *d) {
  Machine *m = c->m;
  uint64_t level = 0;
  const Core *t = check_pretype(c, d->type, &level);
  const Value *tv = here(c, t);
  const Core *body = check(c, d->body, tv);
  DefInfo *info = &m->defs[m->def_count];
  if (body == NULL)
    return 0;
  info->name = d->name;
  info->origin = d->origin;
  info->body = body;
  info->type = tv;
  info->value = NULL;
  m->def_count++;
  return 1;
}

static int check_decl(Checker *c, const Decl *d) {
  if (d->kind == DECL_FAMILY && d->origin == ORIGIN_PROGRAM)
    return FAIL(c, "REFUSE_DATA", "a program cannot declare a family (rule R1); only domain/domain.lang can");
  if (d->kind == DECL_AXIOM)
    return FAIL(c, "REFUSE_AXIOM", "an axiom has no proof (rule R2)");
  if (d->is_rec)
    return FAIL(c, "REFUSE_REC", "def rec is refused (rule R3); use fold or unfold");
  if (!name_free(c, d->name))
    return 0;
  return d->kind == DECL_FAMILY ? check_family(c, d) : check_def(c, d);
}

static int check_program_inner(Arena *arena, const DeclList *decls, Machine *m, Diag *diag) {
  Checker c;
  size_t ctors = 0;
  size_t sorts = 0;
  size_t i;
  memset(m, 0, sizeof *m);
  m->arena = arena;
  m->diag = diag;
  m->fuel = EVAL_FUEL_STEPS;
  m->symbolic_nat_iota = 1;
  for (i = 0; i < decls->count; i++) {
    const Decl *d = &decls->items[i];
    size_t j;
    sorts += d->block_count > 0u ? d->block_count : 1u;
    ctors += d->block_count > 0u ? 0u : d->ctor_count;
    for (j = 0; j < d->block_count; j++)
      ctors += d->block[j].ctor_count;
  }
  m->defs = arena_alloc(arena, (decls->count + 1u) * sizeof *m->defs);
  m->families = arena_alloc(arena, (sorts + 1u) * sizeof *m->families);
  m->ctors = arena_alloc(arena, (ctors + 1u) * sizeof *m->ctors);
  if (m->defs == NULL || m->families == NULL || m->ctors == NULL)
    return diag_fail(diag, "OOM", NULL, "out of memory");
  m->def_cap = (uint32_t)decls->count;
  m->family_cap = (uint32_t)sorts;
  m->ctor_cap = (uint32_t)ctors;
  memset(&c, 0, sizeof c);
  c.m = m;
  c.decls = decls;
  for (i = 0; i < decls->count; i++) {
    c.decl_index = i;
    c.scope = NULL;
    c.env = NULL;
    c.level = 0;
    c.depth = 0;
    c.self = NO_FAMILY;
    m->def = decls->items[i].name;
    if (!check_decl(&c, &decls->items[i]))
      return 0;
  }
  m->def = NULL;
  return 1;
}

int check_program(Arena *arena, const DeclList *decls, Machine *m, Diag *diag) {
  int ok = check_program_inner(arena, decls, m, diag);
  uint32_t i;
  m->symbolic_nat_iota = 0;
  /* Runtime values must be evaluated from the checked core, without
     reusing symbolic reductions cached during checking. */
  for (i = 0; i < m->def_count; i++)
    m->defs[i].value = NULL;
  return ok;
}

static int entry_of_inner(Machine *m, const Value *type, Entry *out) {
  const Value *t = type;
  memset(out, 0, sizeof *out);
  while (t != NULL && t->kind == VAL_PI && out->param_count < ENTRY_PARAMS_MAX) {
    if (!val_is(t->dom, OP_NAT) && !val_is(t->dom, OP_FLAG))
      return 0;
    out->param_flag[out->param_count] = val_is(t->dom, OP_FLAG);
    t = closure_apply(m, t, val_var(m, out->param_count));
    out->param_count++;
  }
  out->result_flag = val_is(t, OP_FLAG);
  return val_is(t, OP_NAT) || val_is(t, OP_FLAG);
}

int entry_of(Machine *m, const Value *type, Entry *out) {
  int saved = m->symbolic_nat_iota;
  int entry;
  m->symbolic_nat_iota = 1;
  entry = entry_of_inner(m, type, out);
  m->symbolic_nat_iota = saved;
  return entry;
}

static int parse_u64(const char *s, uint64_t *out) {
  uint64_t n = 0;
  size_t i;
  if (s[0] == '\0')
    return 0;
  for (i = 0; s[i] != '\0'; i++) {
    unsigned d = (unsigned)(unsigned char)s[i] - (unsigned)'0';
    if (d > 9u || n > (UINT64_MAX - d) / 10u)
      return 0;
    n = n * 10u + d;
  }
  *out = n;
  return 1;
}

/* An entry argument: a Nat in decimal, or a Flag as 0 or 1. */
static const Value *entry_arg(Machine *m, const char *text, int is_flag) {
  uint64_t n = 0;
  if (!parse_u64(text, &n) || (is_flag && n > 1u)) {
    diag_fail(m->diag, "EVAL_ARGS", m->def, "'%s' is not a %s", text, is_flag ? "Flag (0 or 1)" : "Nat (0 to 2^64-1)");
    return NULL;
  }
  return is_flag ? val_make(m, n == 1u ? OP_FLAG_YES : OP_FLAG_NO, 0, 0, NULL, NULL, NULL) : val_nat(m, n);
}

static int print_result(Machine *m, const char *name, const Value *v, FILE *out, FILE *err) {
  switch (v->kind) {
  case VAL_NAT:
    fprintf(out, "%llu\n", (unsigned long long)v->nat);
    return 0;
  case VAL_TRAP:
    fputs("trap\n", out);
    fprintf(err, "langc: EVAL_OVERFLOW: %s: a Nat operation overflowed past 2^64-1\n", name);
    return 0;
  case VAL_OP:
    if (val_is(v, OP_FLAG_YES) || val_is(v, OP_FLAG_NO)) {
      fprintf(out, "%d\n", val_is(v, OP_FLAG_YES));
      return 0;
    }
    break;
  case VAL_UNIV:
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
    break;
  }
  diag_fail(m->diag, "EVAL_ENTRY", name, "the entry did not compute a number");
  return 1;
}

int eval_command(Machine *m, const char *name, char *const *args, int arg_count, FILE *out, FILE *err) {
  uint32_t d = find_def(m, name);
  Entry entry;
  const Value *v;
  char *buf;
  uint32_t i;
  m->def = name;
  if (d == NO_DEF) {
    diag_fail(m->diag, "EVAL_ENTRY", name, "no definition has this name");
    return 1;
  }
  v = def_value(m, d);
  if (v == NULL)
    return 1;
  if (!entry_of(m, m->defs[d].type, &entry) && arg_count != 0) {
    diag_fail(m->diag, "EVAL_ARGS", name, "the definition is not an entry, so it takes no arguments");
    return 2;
  }
  if (!entry_of(m, m->defs[d].type, &entry)) {
    buf = arena_alloc(m->arena, PRINT_MAX);
    if (buf == NULL)
      return diag_fail(m->diag, "OOM", name, "out of memory") + 1;
    if (!value_print(m, NULL, 0, v, buf, PRINT_MAX)) {
      diag_fail(m->diag, "EVAL_PRINT", name, "the normal form exceeds the printer limits");
      return 1;
    }
    fprintf(out, "%s\n", buf);
    return 0;
  }
  if ((uint32_t)arg_count != entry.param_count) {
    diag_fail(m->diag, "EVAL_ARGS", name, "the entry takes %u arguments, found %d", entry.param_count, arg_count);
    return 2;
  }
  for (i = 0; i < entry.param_count && v != NULL; i++) {
    const Value *arg = entry_arg(m, args[i], entry.param_flag[i]);
    v = arg == NULL ? NULL : apply_value(m, v, arg);
  }
  if (v == NULL)
    return strcmp(m->diag->code != NULL ? m->diag->code : "", "EVAL_ARGS") == 0 ? 2 : 1;
  return print_result(m, name, v, out, err);
}
