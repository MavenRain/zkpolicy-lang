/* The zkml writer (P9a rule R4, P9b rules R1, R2 and S1 to S5). The model
   NAME is a program definition of type List (List Nat): the weights. The
   writer copies NAME, the lookup table and the def closure of the model
   entries (the entries that refer to NAME), then generates the shapes, the
   circuit (see ZkmlOp), the weights reader, the encoder and the circuit
   entries. The writer is
   trusted base for the text that it copies and generates; `make zk-check`
   checks the text with zkml-lang at the PIN. */
#include <stdio.h>
#include <string.h>

#include "zkml.h"
#include "zkml-names.h"

/* The generated names are NAME with these suffixes. */
static const char *const SUFFIXES[] = {"S", "T", "Rd", "Circ", "Enc", "Wit", "RunE", "WitE", "Sat"};
#define SUFFIX_COUNT (sizeof SUFFIXES / sizeof SUFFIXES[0])

/* A binder in scope. It shadows a definition of the same name. */
typedef struct Scope {
  const char *name;
  const struct Scope *next;
} Scope;

/* A dependency closure: mark[i] is 1 when declaration i is in it.
   Domain dependencies validate imports and are not copied. */
typedef struct {
  const DeclList *decls;
  unsigned char *mark;
  size_t *todo;
  size_t todo_count;
  int quotient;
  const char *unsupported;
} Walk;

static int listed(const char *const *names, const char *name) {
  for (; *names != NULL; names++) {
    if (strcmp(*names, name) == 0) return 1;
  }
  return 0;
}

static int owns_name(const Decl *d, const char *name) {
  if (strcmp(d->name, name) == 0) return 1;
  for (size_t i = 0; i < d->ctor_count; i++) {
    const Ctor *c = &d->ctors[i];
    if (strcmp(c->name, name) == 0) return 1;
    for (size_t j = 0; j < c->field_count; j++) {
      if (strcmp(c->fields[j].name, name) == 0) return 1;
    }
  }
  return 0;
}

static size_t find_decl(const DeclList *decls, const char *name) {
  for (size_t i = 0; i < decls->count; i++) {
    const Decl *d = &decls->items[i];
    if (owns_name(d, name)) return i;
    for (size_t j = 0; j < d->block_count; j++) {
      if (owns_name(&d->block[j], name)) return i;
    }
  }
  return decls->count;
}

static size_t find_program_decl(const DeclList *decls, const char *name) {
  for (size_t i = 0; i < decls->count; i++) {
    if (decls->items[i].origin == ORIGIN_PROGRAM && strcmp(decls->items[i].name, name) == 0) return i;
  }
  return decls->count;
}

static int is_decl_name(const DeclList *decls, const char *name) {
  return find_decl(decls, name) != decls->count || listed(ZKML_DOMAIN_NAMES, name);
}

static uint32_t find_program_def(const Machine *m, const char *name) {
  for (uint32_t i = 0; i < m->def_count; i++) {
    if (m->defs[i].origin == ORIGIN_PROGRAM && strcmp(m->defs[i].name, name) == 0) return i;
  }
  return m->def_count;
}

/* The domain family Quot, its constructors and the domain definitions on it. */
static int is_quotient_name(const char *name) {
  return strncmp(name, "quot", 4) == 0 || strstr(name, "Quot") != NULL;
}

static int is_bound(const Scope *s, const char *name) {
  for (; s != NULL; s = s->next) {
    if (s->name != NULL && strcmp(s->name, name) == 0) return 1;
  }
  return 0;
}

static void note_name(Walk *w, const char *name, const Scope *s) {
  if (name == NULL || is_bound(s, name)) return;
  if (strcmp(name, "Glue") == 0 || strcmp(name, "glue") == 0 || strcmp(name, "unglue") == 0) w->unsupported = name;
  size_t i = find_decl(w->decls, name);
  if (i == w->decls->count || w->decls->items[i].origin == ORIGIN_DOMAIN) {
    w->quotient |= is_quotient_name(name);
    if (i == w->decls->count) return;
    if (!listed(ZKML_IMPORT_NAMES, name)) w->unsupported = name;
  }
  if (w->mark[i]) return;
  w->mark[i] = 1;
  w->todo[w->todo_count++] = i;
}

static void walk_term(Walk *w, const Term *t, const Scope *s) {
  if (t == NULL) return;
  switch (t->kind) {
    case TERM_VAR:
      note_name(w, t->name, s);
      return;
    case TERM_NAT:
    case TERM_UNIV:
      return;
    case TERM_APP:
      walk_term(w, t->left, s);
      walk_term(w, t->right, s);
      return;
    case TERM_LAM:
    case TERM_PI:
    case TERM_SIGMA: {
      Scope inner = {t->name, s};
      walk_term(w, t->left, s);
      walk_term(w, t->right, &inner);
      return;
    }
  }
}

static void walk_fields(Walk *w, const Field *fields, size_t count, const Term *type, const Scope *s) {
  if (count == 0) {
    walk_term(w, type, s);
    return;
  }
  walk_term(w, fields->type, s);
  Scope inner = {fields->name, s};
  walk_fields(w, fields + 1, count - 1, type, &inner);
}

static void walk_sort(Walk *w, const Decl *d, size_t param, const Scope *s) {
  if (param < d->param_count) {
    const Field *f = &d->params[param];
    walk_term(w, f->type, s);
    Scope inner = {f->name, s};
    walk_sort(w, d, param + 1, &inner);
    return;
  }
  for (size_t i = 0; i < d->ctor_count; i++) {
    const Ctor *c = &d->ctors[i];
    walk_fields(w, c->fields, c->field_count, c->type, s);
  }
}

/* Follow domain dependencies too, to check imports. Only program
   declarations are copied to the output. */
static void close_over(Walk *w, size_t start) {
  memset(w->mark, 0, w->decls->count);
  w->todo_count = 0;
  w->quotient = 0;
  w->unsupported = NULL;
  w->mark[start] = 1;
  w->todo[w->todo_count++] = start;
  while (w->todo_count > 0) {
    const Decl *d = &w->decls->items[w->todo[--w->todo_count]];
    walk_term(w, d->type, NULL);
    walk_term(w, d->body, NULL);
    if (d->block_count == 0) walk_sort(w, d, 0, NULL);
    for (size_t i = 0; i < d->block_count; i++) walk_sort(w, &d->block[i], 0, NULL);
  }
}

static int is_weights_type(const Value *t) {
  return val_is(t, OP_LIST) && t->argc == 1 && val_is(t->args[0], OP_LIST) && t->args[0]->argc == 1 &&
         val_is(t->args[0]->args[0], OP_NAT);
}

/* Reads a closed list of ROWS >= 1 rows of equal length COLS, 1 <= COLS <=
   ENTRY_PARAMS_MAX. */
static int read_rows(const Value *v, uint32_t *rows, uint32_t *cols) {
  uint32_t r = 0;
  uint32_t n = 0;
  for (; val_is(v, OP_CONS); v = v->args[1], r++) {
    const Value *x = v->args[0];
    uint32_t k = 0;
    for (; val_is(x, OP_CONS) && x->args[0]->kind == VAL_NAT && k <= ENTRY_PARAMS_MAX; x = x->args[1]) k++;
    if (!val_is(x, OP_NIL) || k == 0 || k > ENTRY_PARAMS_MAX || (r > 0 && k != n)) return 0;
    n = k;
  }
  *rows = r;
  *cols = n;
  return val_is(v, OP_NIL) && r > 0;
}

/* The byte offset of the start of LINE (from 1) in TEXT. */
static size_t line_offset(const char *text, size_t len, int line) {
  size_t at = 0;
  for (int l = 1; l < line && at < len; at++) {
    if (text[at] == '\n') l++;
  }
  return at;
}

/* Prints the lines of TEXT from START to END, without blank lines and
   full-line comments. */
static void print_lines(FILE *out, const char *text, size_t start, size_t end) {
  while (start < end) {
    const char *nl = memchr(text + start, '\n', end - start);
    size_t stop = nl != NULL ? (size_t)(nl - text) : end;
    size_t first = start;
    while (first < stop && (text[first] == ' ' || text[first] == '\t' || text[first] == '\r')) first++;
    int skip = first == stop || (stop - first >= 2 && text[first] == '-' && text[first + 1] == '-');
    if (!skip) {
      fwrite(text + start, 1, stop - start, out);
      fputc('\n', out);
    }
    start = stop + 1;
  }
}

/* Prints the program declaration I: from its `def` token to the start of the
   next program declaration, or to the end of TEXT. */
static void print_decl(FILE *out, const DeclList *decls, const char *text, size_t len, size_t i) {
  const Decl *d = &decls->items[i];
  size_t start = line_offset(text, len, d->line) + (size_t)(d->col - 1);
  size_t end = len;
  for (size_t j = i + 1; j < decls->count && end == len; j++) {
    const Decl *e = &decls->items[j];
    if (e->origin == ORIGIN_PROGRAM) end = line_offset(text, len, e->line) + (size_t)(e->col - 1);
  }
  print_lines(out, text, start, end < start ? start : end);
}

/* Prints `cons x0 (cons x1 (... nil))` for N inputs. */
static void print_input(FILE *out, const char *model, uint32_t i) {
  char name[32];
  snprintf(name, sizeof name, "x%u", i);
  fprintf(out, "%s%s", name, strcmp(name, model) == 0 ? "_" : "");
}

static void print_inputs(FILE *out, const char *model, uint32_t n) {
  for (uint32_t i = 0; i + 1 < n; i++) {
    fputs("cons ", out);
    print_input(out, model, i);
    fputs(" (", out);
  }
  fputs("cons ", out);
  print_input(out, model, n - 1);
  fputs(" nil", out);
  for (uint32_t i = 0; i + 1 < n; i++) fputc(')', out);
}

/* Prints the type and the binders of a circuit entry with N inputs. */
static void print_entry_head(FILE *out, const char *name, const char *suffix, uint32_t n) {
  fprintf(out, "def %s%s : ", name, suffix);
  for (uint32_t i = 0; i < n; i++) fputs("Nat -> ", out);
  fputs("Nat := fun", out);
  for (uint32_t i = 0; i < n; i++) {
    fputc(' ', out);
    print_input(out, name, i);
  }
  fputs(" => ", out);
}

/* Prints `bbRun NAMES NAMET NAMECirc (NAMERd NAME) (INPUTS)` for N inputs. */
static void print_run(FILE *out, const char *name, uint32_t n) {
  fprintf(out, "bbRun %sS %sT %sCirc (%sRd %s) (", name, name, name, name, name);
  print_inputs(out, name, n);
  fputc(')', out);
}

/* Prints the first generated comment line: the text for the circuit MODE. */
static void print_circuit_text(FILE *out, const char *name, ZkmlMode mode) {
  fprintf(out, "-- Generated. %sCirc is ", name);
  switch (mode.op) {
    case ZKML_OP_DENSE:
      fputs("the dense layer (opMatMul)", out);
      break;
    case ZKML_OP_RESIDUAL:
      fputs("x plus the dense layer (circResidual of opMatMul)", out);
      break;
    case ZKML_OP_RESIDUAL_LOOP:
      fprintf(out, "%u residual steps of the dense layer (circResidualLoop of opMatMul)", mode.steps);
      break;
    case ZKML_OP_LOOKUP:
      fputs("the dense layer, then the table lookup on each entry (circSeq of opMatMul and opLookup)", out);
      break;
  }
  fprintf(out, " from %sS to %sT.\n", name, name);
}

/* The lookup table name of MODE: NULL in the modes with no table. */
static const char *table_of(ZkmlMode mode) {
  switch (mode.op) {
    case ZKML_OP_DENSE:
    case ZKML_OP_RESIDUAL:
    case ZKML_OP_RESIDUAL_LOOP:
      return NULL;
    case ZKML_OP_LOOKUP:
      return mode.table;
  }
  return NULL;
}

/* 1 when the open NAMESat proof by refl checks at the PIN for the circuit
   MODE: the dense and lookup circuits. For circResidual the zipWithT bbAdd
   fold is stuck on an open input, so refl does not prove the open path and
   the writer generates no NAMESat (P9b s5). */
static int sat_by_refl(ZkmlMode mode) {
  switch (mode.op) {
    case ZKML_OP_DENSE:
    case ZKML_OP_LOOKUP:
      return 1;
    case ZKML_OP_RESIDUAL:
    case ZKML_OP_RESIDUAL_LOOP:
      return 0;
  }
  return 0;
}

/* Prints NAMECirc for the circuit MODE. The dense layer is Op [N] [M]. The
   residual circuit is circResidual of Op [N] [N] (M = N). The residual loop
   is circResidualLoop of Op [N] [N] with K steps (M = N). The lookup circuit
   is circSeq of the dense layer and opLookup on [M] with the table 1. */
static void print_circuit(FILE *out, const char *name, uint32_t m, uint32_t n, ZkmlMode mode) {
  switch (mode.op) {
    case ZKML_OP_DENSE:
      fprintf(out, "def %sCirc : Circuit %sS %sT := circOp %sS %sT (opMatMul %u %u 0)\n", name, name, name, name, name, n, m);
      return;
    case ZKML_OP_RESIDUAL:
      fprintf(out, "def %sCirc : Circuit %sS %sT := circResidual %sS (circOp %sS %sS (opMatMul %u %u 0))\n", name, name, name,
              name, name, name, n, n);
      return;
    case ZKML_OP_RESIDUAL_LOOP:
      fprintf(out, "def %sCirc : Circuit %sS %sT := circResidualLoop %sS (circOp %sS %sS (opMatMul %u %u 0)) %u\n", name, name,
              name, name, name, name, n, n, mode.steps);
      return;
    case ZKML_OP_LOOKUP:
      fprintf(out, "def %sCirc : Circuit %sS %sT := circSeq %sS %sT %sT (circOp %sS %sT (opMatMul %u %u 0)) (circOp %sT %sT (opLookup %sT 1))\n",
              name, name, name, name, name, name, name, name, n, m, name, name, name);
      return;
  }
}

/* Prints the generated defs for a model of M rows and N columns, with the
   circuit MODE. The weights reader gives the model as the matrix 0, and in
   the lookup mode the table as the list 1. */
static void print_generated(FILE *out, const char *name, uint32_t m, uint32_t n, ZkmlMode mode) {
  const char *table = table_of(mode);
  print_circuit_text(out, name, mode);
  if (table != NULL) {
    fprintf(out, "-- bbRun reads the matrix 0 and the table 1 of %sRd %s: the matrix is %s, the table is %s.\n", name, name, name, table);
  } else {
    fprintf(out, "-- bbRun reads the matrix 0 of %sRd %s, and that matrix is %s.\n", name, name, name);
  }
  fprintf(out, "-- %sWitE evaluates the trace of two stages: the input and the bbRun output.\n", name);
  if (sat_by_refl(mode)) {
    fprintf(out, "-- %sSat: the two-stage witness %sWit satisfies %sCirc at every input, by refl at the PIN.\n", name, name, name);
  } else {
    fprintf(out, "-- %sSat is not generated: at the PIN, refl does not prove the open path for circResidual (the zipWithT bbAdd fold is stuck on an open input).\n",
            name);
  }
  fprintf(out, "-- %sEnc is the base-1000 code of a row: an eval code, not injective above 999.\n", name);
  fprintf(out, "def %sS : Shape := shape (cons %u nil)\n", name, n);
  fprintf(out, "def %sT : Shape := shape (cons %u nil)\n", name, m);
  if (table != NULL) {
    const char *weights = strcmp(table, "w") == 0 ? "w_" : "w";
    fprintf(out, "def %sRd : List (List Nat) -> BbWeights := fun (%s : List (List Nat)) => cons %s (cons %s nil)\n",
            name, weights, weights, table);
  } else {
    fprintf(out, "def %sRd : List (List Nat) -> BbWeights := fun (w : List (List Nat)) => cons w nil\n", name);
  }
  print_circuit(out, name, m, n, mode);
  fprintf(out, "def %sEnc : List Nat -> Nat := fun (ys : List Nat) => fold (fun (v : Nat) (acc : Nat) => natAdd (natMul acc 1000) v) 0 ys\n",
          name);
  const char *input = strcmp(name, "x") == 0 ? "x_" : "x";
  fprintf(out, "def %sWit : List Nat -> Prod (List (List Nat)) (List (Stage Nat)) := fun (%s : List Nat) => pair %s (cons (stageEnc Nat %sS %s) (cons (stageEnc Nat %sT (bbRun %sS %sT %sCirc (%sRd %s) %s)) nil))\n",
          name, input, name, name, input, name, name, name, name, name, name, input);
  print_entry_head(out, name, "RunE", n);
  fprintf(out, "%sEnc (", name);
  print_run(out, name, n);
  fputs(")\n", out);
  print_entry_head(out, name, "WitE", n);
  fprintf(out, "%sEnc (instEval (bbInst (List (List Nat)) %sRd) %sS %sT %sCirc (%sWit (", name, name, name, name, name, name);
  print_inputs(out, name, n);
  fputs(")))\n", out);
  if (sat_by_refl(mode)) {
    fprintf(out, "def %sSat : (x : List Nat) -> bbInstSat (List (List Nat)) %sRd %sS %sT %sCirc x (%sWit x) := fun (x : List Nat) => refl\n",
            name, name, name, name, name, name);
  }
}

/* Refuses a generated name that is already a declaration name. */
static int check_generated_names(Machine *m, const DeclList *decls, const char *name, Diag *diag) {
  size_t len = strlen(name);
  char *buf = arena_alloc(m->arena, len + 8);
  if (buf == NULL) return diag_fail(diag, "OOM", name, "out of memory");
  for (size_t i = 0; i < SUFFIX_COUNT; i++) {
    snprintf(buf, len + 8, "%s%s", name, SUFFIXES[i]);
    if (is_decl_name(decls, buf)) return diag_fail(diag, "ZKML_MODEL", name, "the generated name '%s' is already a declaration", buf);
  }
  return 1;
}

/* Resolves the lookup table of MODE on the path of the model NAME (ZKML_NAME,
   then ZKML_MODEL for the type and the rows), then checks that the rows have
   two entries. Gives the declaration of the table in *TI (NI when the mode
   has no table). Returns 1, or 0 after a diagnostic. */
static int check_table(Machine *m, const DeclList *decls, ZkmlMode mode, size_t ni, size_t *ti, Diag *diag) {
  const char *table = table_of(mode);
  *ti = ni;
  if (table == NULL) return 1;
  size_t di = find_program_decl(decls, table);
  uint32_t dd = find_program_def(m, table);
  if (di == decls->count || dd == m->def_count) return diag_fail(diag, "ZKML_NAME", table, "no program definition has this name");
  if (!is_weights_type(m->defs[dd].type)) return diag_fail(diag, "ZKML_MODEL", table, "the type of the lookup table is not List (List Nat)");
  m->def = table;
  const Value *v = def_value(m, dd);
  if (v == NULL) return 0;
  uint32_t rows = 0;
  uint32_t cols = 0;
  if (!read_rows(v, &rows, &cols))
    return diag_fail(diag, "ZKML_MODEL", table, "the value is not m >= 1 rows of equal length n, 1 <= n <= %u", ENTRY_PARAMS_MAX);
  if (cols != 2) return diag_fail(diag, "ZKML_MODEL", table, "the lookup table needs rows of two entries");
  *ti = di;
  return 1;
}

/* Closes over the declaration START and marks the program declarations of
   the closure in COPY. Returns 0 when the closure uses an unsupported name. */
static int copy_closure(Walk *w, size_t start, unsigned char *copy) {
  close_over(w, start);
  if (w->unsupported != NULL) return 0;
  for (size_t i = 0; i < w->decls->count; i++) {
    if (w->decls->items[i].origin == ORIGIN_PROGRAM) copy[i] |= w->mark[i];
  }
  return 1;
}

int zkml_command_op(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, ZkmlMode mode,
                    const char *out_path, Diag *diag) {
  size_t ni = find_program_decl(decls, name);
  size_t ti = decls->count;
  uint32_t di = find_program_def(m, name);
  if (ni == decls->count || di == m->def_count) return diag_fail(diag, "ZKML_NAME", name, "no program definition has this name") + 1;
  if (!is_weights_type(m->defs[di].type)) return diag_fail(diag, "ZKML_MODEL", name, "the type of the model is not List (List Nat)") + 1;
  m->def = name;
  const Value *v = def_value(m, di);
  if (v == NULL) return 1;
  uint32_t rows = 0;
  uint32_t cols = 0;
  if (!read_rows(v, &rows, &cols))
    return diag_fail(diag, "ZKML_MODEL", name, "the value is not m >= 1 rows of equal length n, 1 <= n <= %u", ENTRY_PARAMS_MAX) + 1;
  if ((mode.op == ZKML_OP_RESIDUAL || mode.op == ZKML_OP_RESIDUAL_LOOP) && rows != cols)
    return diag_fail(diag, "ZKML_MODEL", name, "the residual circuit needs m = n") + 1;
  if (!check_table(m, decls, mode, ni, &ti, diag)) return 1;
  m->def = name;
  if (!check_generated_names(m, decls, name, diag)) return 1;
  Walk w = {decls, arena_alloc(m->arena, decls->count + 1), arena_alloc(m->arena, (decls->count + 1) * sizeof(size_t)), 0, 0, NULL};
  unsigned char *copy = arena_alloc(m->arena, decls->count + 1);
  unsigned char *skip = arena_alloc(m->arena, decls->count + 1);
  if (w.mark == NULL || w.todo == NULL || copy == NULL || skip == NULL) return diag_fail(diag, "OOM", name, "out of memory") + 1;
  memset(copy, 0, decls->count + 1);
  memset(skip, 0, decls->count + 1);
  if (!copy_closure(&w, ni, copy) || (ti != ni && !copy_closure(&w, ti, copy)))
    return diag_fail(diag, "ZKML_MODEL", name, "name '%s' is not supported at the zkml PIN", w.unsupported) + 1;
  int skipped = 0;
  for (uint32_t d = 0; d < m->def_count; d++) {
    Entry entry;
    size_t e = m->defs[d].origin == ORIGIN_PROGRAM ? find_program_decl(decls, m->defs[d].name) : decls->count;
    if (e == decls->count || !entry_of(m, m->defs[d].type, &entry)) continue;
    close_over(&w, e);
    if (!w.mark[ni]) continue;
    if (!w.quotient && w.unsupported != NULL)
      return diag_fail(diag, "ZKML_MODEL", name, "entry '%s' uses name '%s', unsupported at the zkml PIN",
                       decls->items[e].name, w.unsupported) + 1;
    skip[e] = (unsigned char)w.quotient;
    skipped |= w.quotient;
    for (size_t i = 0; i < decls->count && !w.quotient; i++) {
      if (decls->items[i].origin == ORIGIN_PROGRAM) copy[i] |= w.mark[i];
    }
  }
  for (size_t i = 0; i < decls->count; i++) {
    if (copy[i] && listed(ZKML_DOMAIN_NAMES, decls->items[i].name))
      return diag_fail(diag, "ZKML_MODEL", name, "copied name '%s' is a domain name at the zkml PIN", decls->items[i].name) + 1;
    uint32_t d = copy[i] ? find_program_def(m, decls->items[i].name) : m->def_count;
    if (d < m->def_count && m->defs[d].has_prop_fold)
      return diag_fail(diag, "ZKML_MODEL", name, "definition '%s' uses a proposition-valued HIT fold, unsupported at the zkml PIN",
                       decls->items[i].name) + 1;
  }
  FILE *out = fopen(out_path, "wb");
  if (out == NULL) return diag_fail(diag, "IO", name, "cannot write '%s'", out_path) + 1;
  fprintf(out, "-- Written by langc zkml (model %s) for zkml-lang at the PIN.\n", name);
  if (skipped) {
    fputs("-- Not copied (a quotient entry: the quotient view stays zp side in P9a):", out);
    for (size_t i = 0; i < decls->count; i++) {
      if (skip[i]) fprintf(out, " %s", decls->items[i].name);
    }
    fputc('\n', out);
  }
  for (size_t i = 0; i < decls->count; i++) {
    if (copy[i]) print_decl(out, decls, text, len, i);
  }
  print_generated(out, name, rows, cols, mode);
  return fclose(out) == 0 ? 0 : diag_fail(diag, "IO", name, "cannot write '%s'", out_path) + 1;
}

int zkml_command(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, const char *out_path, Diag *diag) {
  ZkmlMode dense = {ZKML_OP_DENSE, 0, NULL};
  return zkml_command_op(m, decls, text, len, name, dense, out_path, diag);
}
