/* Core terms, values and the evaluator. SHARED by the tcc kits.

   The checker (front/check.c) writes core terms. Evaluation is normalization
   by evaluation: each value is a normal form. A neutral value (VAL_VAR,
   VAL_APP, VAL_STUCK) is stuck on a variable. A trap (a Nat overflow) is a
   value too. It goes up through each strict position (a Nat operation, the
   scrutinee of an eliminator, the function of an application) and nowhere
   else. So the evaluator and the residual program agree: a trap that no
   strict position reads does not stop the program. */
#ifndef LANG_FRONT_CORE_H
#define LANG_FRONT_CORE_H

#include "front/ast.h"
#include "front/base.h"

#define EVAL_DEPTH_LIMIT 2000u
#define EVAL_FUEL_STEPS 20000000u

/* The core names: type formers, constructors and operations. */
typedef enum {
  OP_NAT,
  OP_FLAG,
  OP_UNIT,
  OP_PROD,
  OP_SUM,
  OP_OPTION,
  OP_LIST,
  OP_EQ,
  OP_FAMILY,
  OP_UNIT_VAL,
  OP_FLAG_YES,
  OP_FLAG_NO,
  OP_PAIR,
  OP_INL,
  OP_INR,
  OP_NONE,
  OP_SOME,
  OP_NIL,
  OP_CONS,
  OP_PACK,
  OP_REFL,
  OP_CTOR,
  OP_FIRST,
  OP_SECOND,
  OP_EITHER,
  OP_OPTION_ELIM,
  OP_PURE,
  OP_MAP,
  OP_BIND,
  OP_FOLD_NAT,
  OP_FOLD_LIST,
  OP_FOLD_FAMILY,
  OP_FOLD_PROP,
  OP_UNFOLD,
  OP_FILTER,
  OP_WITNESS,
  OP_PAYLOAD,
  OP_SYMM,
  OP_TRANS,
  OP_TRANSPORT,
  OP_CONG,
  OP_NAT_ADD,
  OP_NAT_SUB,
  OP_NAT_MUL,
  OP_NAT_EQ,
  OP_NAT_LE,
  OP_NAT_MAX,
  OP_NAT_IS_SET, /* D4: a trusted proof; it never computes */
  OP_FLAG_IF,
  OP_PROJ,
  OP_SSET,  /* the sort of pretypes (former C1); no surface name */
  OP_I,     /* the interval */
  OP_I0,
  OP_I1,
  OP_INEG,  /* ~ r */
  OP_IMIN,  /* r /\ s */
  OP_IMAX,  /* r \/ s */
  OP_PATHP, /* PathP A a b (former C2) */
  OP_PLAM,  /* <i> t: the argument is a function over I */
  OP_PAPP,  /* p @ r, then the endpoints a0 and a1 of the type of p */
  OP_PARTIAL,    /* Partial phi A (former C1): a pretype */
  OP_SYSTEM,     /* [ f |-> u , ... ]: a face, a branch and the other cells */
  OP_SYSNIL,     /* [] */
  OP_FROMPARTIAL, /* fromPartial p, when the face of the type of p holds */
  OP_TRANSP,      /* transp A r u (former C3): [line, r, u] */
  OP_TPART,       /* internal: a part of a type, field = the part */
  OP_TCOD,        /* internal: the codomain of a Pi or Sigma at x: [T, x] */
  OP_HCOMP,       /* hcomp A phi side u0 (former C3): [A, phi, side, u0] */
  OP_SYSMAP,      /* internal: f mapped over the branches of a system: [f, sys] */
  OP_GLUE,        /* Glue A [ f |-> pack T e , ... ] (former C6): [A, phi, sys] */
  OP_GLUE_ELEM,   /* glue [ f |-> t , ... ] a at G = Glue A sys (former C6): [G, sys, a] */
  OP_UNGLUE,      /* unglue g at Glue A sys (former C6): [A, phi, sys, g] */
  OP_PCTOR        /* a path constructor (former C4): [params, fields, r_1 .. r_d] */
} Op;

/* The instance of pure, map, bind and filter (formers F5 and F8). */
typedef enum {
  CARRIER_OPTION,
  CARRIER_LIST,
  CARRIER_SUM
} Carrier;

typedef enum {
  CORE_VAR,    /* index: de Bruijn index */
  CORE_GLOBAL, /* index: definition number */
  CORE_NAT,    /* nat */
  CORE_TRAP,   /* an overflow; only quote writes it */
  CORE_UNIV,   /* Type nat */
  CORE_LAM,    /* fun name => b */
  CORE_PI,     /* (name : a) -> b */
  CORE_SIGMA,  /* Sigma (name : a) b */
  CORE_APP,    /* a b */
  CORE_OP      /* a core name with all of its arguments */
} CoreKind;

typedef struct Core Core;
struct Core {
  CoreKind kind;
  uint32_t index;
  uint64_t nat;
  Op op;
  /* OP_FAMILY, OP_FOLD_FAMILY, OP_FOLD_PROP: the family. OP_CTOR, OP_PCTOR,
     OP_PROJ: the constructor. OP_PURE, OP_MAP, OP_BIND, OP_FILTER: the Carrier. */
  uint32_t inst;
  uint32_t field; /* OP_PROJ: the field number */
  const char *name;
  const Core *a;
  const Core *b;
  const Core *const *args;
  uint32_t argc;
};

typedef enum {
  VAL_NAT,
  VAL_TRAP,
  VAL_UNIV,
  VAL_LAM,
  VAL_PI,
  VAL_SIGMA,
  VAL_OP,   /* a canonical value or a type */
  VAL_VAR,  /* neutral: a variable at a de Bruijn level */
  VAL_APP,  /* neutral: a neutral function applied to a value */
  VAL_STUCK /* neutral: an operation with a neutral scrutinee */
} ValKind;

typedef struct Value Value;
typedef struct Env Env;

/* The head is de Bruijn index 0. */
struct Env {
  const Value *value;
  const Env *next;
};

struct Value {
  ValKind kind;
  uint64_t nat; /* NAT: the number. UNIV: the level. VAR: the level. */
  Op op;        /* OP, STUCK */
  uint32_t inst;
  uint32_t field;
  const Value *const *args;
  uint32_t argc;
  const char *name; /* LAM, PI, SIGMA: the binder */
  const Value *dom; /* PI, SIGMA: the domain. APP: the function. */
  const Value *arg; /* APP: the argument. A PI with no body: the codomain. */
  const Env *env;   /* LAM, PI, SIGMA: the closure */
  const Core *body;
};

/* FIELD_SELF: the field type is the family applied to its own parameters.
   FIELD_SELF_PATH: Path X u v, with X that type and point terms u, v; only in
   a path constructor. FIELD_SIB: another sort of the mutual block applied
   to index terms over the plain fields (former C5, D9). */
typedef enum {
  FIELD_PLAIN,
  FIELD_SELF,
  FIELD_SELF_PATH,
  FIELD_SIB
} FieldKind;

typedef struct {
  const char *name;
  /* in the context of the family parameters and the earlier fields (the
     fields form a telescope); in an indexed family, of the earlier fields
     only (the header binders are the index telescope, not in scope) */
  const Core *type;
  FieldKind kind;
} FieldInfo;

/* A path constructor has depth 1 (Path X a b) or 2 (Path (Path X x y) p q).
   TYPE and ENDS are in the context of the parameters and all of the fields.
   ENDS has 2 * depth entries: a, b at depth 1, or p, q at depth 2; then x, y
   at depth 2. In TYPE, x and y sit under the line binder; ENDS stores them
   unshifted. */
typedef struct {
  const char *name;
  uint32_t family;
  const FieldInfo *fields;
  uint32_t field_count;
  uint32_t depth;          /* 0 for a point constructor */
  const Core *type;        /* NULL for a point constructor */
  const Core *const *ends; /* NULL for a point constructor */
  /* an indexed family (former C5, D3): the param_count result indices, in
     the context of the fields; NULL in a uniform family */
  const Core *const *index;
} CtorInfo;

/* A domain family. Its constructors are ctors[first_ctor ...]. A family
   with one constructor has one projection for each field. */
typedef struct {
  const char *name;
  const Core *const *param_types;
  uint32_t param_count;
  uint32_t first_ctor;
  uint32_t ctor_count;
  uint64_t universe; /* the largest universe of a constructor field */
  /* 1 when the header binders are the result index telescope: every point
     constructor states its result (former C5, D1) */
  int indexed;
  /* the mutual block (former C5, D9): the sorts block_first to
     block_first + block_count - 1; a single family is a block of 1 */
  uint32_t block_first;
  uint32_t block_count;
} FamilyInfo;

typedef struct {
  const char *name;
  Origin origin;
  const Core *body;
  const Value *type;
  const Value *value; /* computed when first used */
} DefInfo;

/* The checked program and the evaluation state. No global state: every
   function gets this context. */
typedef struct {
  Arena *arena;
  Diag *diag;
  const char *def; /* the current definition, for diagnostics */
  DefInfo *defs;
  uint32_t def_count;
  uint32_t def_cap;
  FamilyInfo *families;
  uint32_t family_count;
  uint32_t family_cap;
  CtorInfo *ctors;
  uint32_t ctor_count;
  uint32_t ctor_cap;
  unsigned depth;
  uint32_t fresh_level; /* above every variable level allocated by this machine */
  uint64_t fuel;
  int overflowed;
  int symbolic_nat_iota; /* checking/conversion only: runtime must keep the count's overflow */
} Machine;

/* Each function returns NULL (or 0) after a diagnostic. A NULL input gives a
   NULL result, so a caller can test once at the end. */
const Value *val_nat(Machine *m, uint64_t n);
const Value *val_var(Machine *m, uint32_t level);
const Value *val_univ(Machine *m, uint64_t level);
const Value *val_op(Machine *m, ValKind kind, Op op, uint32_t inst, uint32_t field, const Value *const *args, uint32_t argc);
const Value *val_make(Machine *m, Op op, uint32_t inst, uint32_t argc, const Value *a, const Value *b, const Value *c);
/* A non-dependent function type. */
const Value *val_arrow(Machine *m, const Value *dom, const Value *cod);
const Value *val_closure(Machine *m, ValKind kind, const char *name, const Value *dom, const Env *env, const Core *body);
int val_is(const Value *v, Op op);
const Env *env_push(Machine *m, const Env *env, const Value *value);
const Value *def_value(Machine *m, uint32_t index);
const Value *eval_core(Machine *m, const Env *env, const Core *core);
const Value *apply_value(Machine *m, const Value *fn, const Value *arg);
/* The body of a LAM, PI or SIGMA value at ARG. */
const Value *closure_apply(Machine *m, const Value *binder, const Value *arg);
/* 1 when A and B have the same normal form. LEVEL is the next free level. */
int conv_values(Machine *m, uint32_t level, const Value *a, const Value *b);
/* Faces (former C1). The face normal form is the interval normal form
   without the meets that hold a literal and its negation. A literal is
   level * 2 for (i = 1) and level * 2 + 1 for (i = 0). LEVEL is the next
   free level. */
#define FACE_MEETS_MAX 16u
#define FACE_LITS_MAX 8u
typedef struct {
  uint32_t lit[FACE_MEETS_MAX][FACE_LITS_MAX];
  uint32_t len[FACE_MEETS_MAX];
  uint32_t count;
} FaceMeets;
/* 1 when the face PHI holds: its normal form holds the empty meet. */
int face_holds(Machine *m, uint32_t level, const Value *phi);
/* 1 when the faces A and B have the same normal form. */
int face_equal(Machine *m, uint32_t level, const Value *a, const Value *b);
/* The meets of the normal form of PHI, with the variable literals only (a
   neutral atom drops out). 0 when PHI is not a face in the bounds. */
int face_meets(Machine *m, uint32_t level, const Value *phi, FaceMeets *out);
/* Conversion after restricting to every meet of PHI. Neutral face atoms
   stay unconstrained, so conversion is conservative in those contexts. */
int conv_under_face(Machine *m, uint32_t level, const Value *phi, const Value *a, const Value *b);
/* Reduce V at the variable endpoints common to every meet of PHI. */
const Value *value_under_face(Machine *m, uint32_t level, const Value *phi, const Value *v);
const Core *quote_value(Machine *m, uint32_t level, const Value *v);
/* Writes V in the surface syntax. NAMES gives the names of the levels below
   NAME_COUNT. Returns 1 for a complete rendering, 0 on truncation or error. */
int value_print(Machine *m, const char *const *names, uint32_t name_count, const Value *v, char *buf, size_t cap);
const char *op_name(const Machine *m, Op op, uint32_t inst, uint32_t field);

#endif
