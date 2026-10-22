/* The zkml writer (P9a rule R4, P9b rules R1, R2 and S1 to S5). The model
   NAME is a program definition of type List (List Nat): the weights. The
   writer copies NAME, the table (lookup or softmax) and the def closure of the model
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
   MAX_COLS. Only the model columns are entry parameters. */
static int read_rows(const Value *v, uint32_t *rows, uint32_t *cols, uint32_t max_cols) {
  uint32_t r = 0;
  uint32_t n = 0;
  for (; val_is(v, OP_CONS); v = v->args[1], r++) {
    const Value *x = v->args[0];
    uint32_t k = 0;
    for (; val_is(x, OP_CONS) && x->args[0]->kind == VAL_NAT && k < max_cols; x = x->args[1]) k++;
    if (!val_is(x, OP_NIL) || k == 0 || (r > 0 && k != n)) return 0;
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

/* Prints the input I, as xI (xI_ for the model xI). */
static void print_input(FILE *out, const char *model, uint32_t i) {
  char name[32];
  snprintf(name, sizeof name, "x%u", i);
  fprintf(out, "%s%s", name, strcmp(name, model) == 0 ? "_" : "");
}

/* Prints `cons xF (cons xF+1 (... nil))` for the N inputs from F. */
static void print_row(FILE *out, const char *model, uint32_t f, uint32_t n) {
  for (uint32_t i = 0; i + 1 < n; i++) {
    fputs("cons ", out);
    print_input(out, model, f + i);
    fputs(" (", out);
  }
  fputs("cons ", out);
  print_input(out, model, f + n - 1);
  fputs(" nil", out);
  for (uint32_t i = 0; i + 1 < n; i++) fputc(')', out);
}

/* Prints the input of N columns: for H = 0 the row of the N inputs, else
   the list of H rows of N inputs (x0 to xN-1 in row 0, then the next N). */
static void print_inputs(FILE *out, const char *model, uint32_t n, uint32_t h) {
  if (h == 0) {
    print_row(out, model, 0, n);
    return;
  }
  for (uint32_t r = 0; r < h; r++) {
    fputs("cons (", out);
    print_row(out, model, r * n, n);
    fputs(r + 1 < h ? ") (" : ") nil", out);
  }
  for (uint32_t r = 0; r + 1 < h; r++) fputc(')', out);
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

/* Prints `bbRun NAMES NAMET NAMECirc (NAMERd NAME) (INPUTS)` for the input
   of N columns and H rows (print_inputs). */
static void print_run(FILE *out, const char *name, uint32_t n, uint32_t h) {
  fprintf(out, "bbRun %sS %sT %sCirc (%sRd %s) (", name, name, name, name, name);
  print_inputs(out, name, n, h);
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
    case ZKML_OP_SOFTMAX:
      fprintf(out, "the dense layer, then the softmax with the exp table at the scale %u (circSeq of opMatMul and opSoftmax)",
              ZKML_SOFTMAX_K);
      break;
    case ZKML_OP_LAYERNORM:
      fputs("the dense layer, then the layer normalization with the gamma-beta matrix and the inverse square root table "
            "(circSeq of opMatMul and opLayerNorm)", out);
      break;
    case ZKML_OP_ATTENTION:
      fprintf(out, "the attention on %u rows with the key matrix, the value matrix and the exp table at the scale %u (opAttention)",
              ZKML_ATTENTION_H, ZKML_SOFTMAX_K);
      break;
  }
  fprintf(out, " from %sS to %sT.\n", name, name);
}

/* The table name of MODE (the lookup table, the exp table or the inverse
   square root table): NULL in the modes with no table. */
static const char *table_of(ZkmlMode mode) {
  switch (mode.op) {
    case ZKML_OP_DENSE:
    case ZKML_OP_RESIDUAL:
    case ZKML_OP_RESIDUAL_LOOP:
      return NULL;
    case ZKML_OP_LOOKUP:
    case ZKML_OP_SOFTMAX:
    case ZKML_OP_LAYERNORM:
    case ZKML_OP_ATTENTION:
      return mode.table;
  }
  return NULL;
}

/* The key matrix name (VALUE 0) or the value matrix name (VALUE 1) of MODE:
   NULL in the modes other than attention. */
static const char *key_of(ZkmlMode mode, int value) {
  switch (mode.op) {
    case ZKML_OP_DENSE:
    case ZKML_OP_RESIDUAL:
    case ZKML_OP_RESIDUAL_LOOP:
    case ZKML_OP_LOOKUP:
    case ZKML_OP_SOFTMAX:
    case ZKML_OP_LAYERNORM:
      return NULL;
    case ZKML_OP_ATTENTION:
      return value ? mode.wv : mode.wk;
  }
  return NULL;
}

/* The row count of the input of MODE: 0 for an input List Nat, else
   ZKML_ATTENTION_H for an input List (List Nat) of that many rows. */
static uint32_t input_rows(ZkmlMode mode) {
  switch (mode.op) {
    case ZKML_OP_DENSE:
    case ZKML_OP_RESIDUAL:
    case ZKML_OP_RESIDUAL_LOOP:
    case ZKML_OP_LOOKUP:
    case ZKML_OP_SOFTMAX:
    case ZKML_OP_LAYERNORM:
      return 0;
    case ZKML_OP_ATTENTION:
      return ZKML_ATTENTION_H;
  }
  return 0;
}

/* The gamma-beta matrix name of MODE: NULL in the modes other than layer
   normalization. */
static const char *gb_of(ZkmlMode mode) {
  switch (mode.op) {
    case ZKML_OP_DENSE:
    case ZKML_OP_RESIDUAL:
    case ZKML_OP_RESIDUAL_LOOP:
    case ZKML_OP_LOOKUP:
    case ZKML_OP_SOFTMAX:
    case ZKML_OP_ATTENTION:
      return NULL;
    case ZKML_OP_LAYERNORM:
      return mode.gb;
  }
  return NULL;
}

/* Prints the comment on the weights of NAMERd that bbRun reads in MODE. */
static void print_reads(FILE *out, const char *name, ZkmlMode mode) {
  switch (mode.op) {
    case ZKML_OP_DENSE:
    case ZKML_OP_RESIDUAL:
    case ZKML_OP_RESIDUAL_LOOP:
      fprintf(out, "-- bbRun reads the matrix 0 of %sRd %s, and that matrix is %s.\n", name, name, name);
      return;
    case ZKML_OP_LOOKUP:
    case ZKML_OP_SOFTMAX:
      fprintf(out, "-- bbRun reads the matrix 0 and the table 1 of %sRd %s: the matrix is %s, the table is %s.\n", name, name, name,
              mode.table);
      return;
    case ZKML_OP_LAYERNORM:
      fprintf(out, "-- bbRun reads the matrix 0, the gamma-beta matrix 1 and the table 2 of %sRd %s: the matrix is %s, the gamma-beta "
                   "matrix is %s, the table is %s.\n", name, name, name, mode.gb, mode.table);
      return;
    case ZKML_OP_ATTENTION:
      fprintf(out, "-- bbRun reads the query matrix 0, the key matrix 1, the value matrix 2 and the table 3 of %sRd %s: the query "
                   "matrix is %s, the key matrix is %s, the value matrix is %s, the table is %s.\n", name, name, name, mode.wk,
              mode.wv, mode.table);
      return;
  }
}

/* The count of the weights names after the model in NAMERd. */
#define WEIGHTS_NAMES 4u

/* 1 when one of the COUNT names NAMES (NULL for no name) is B. */
static int has_name(const char *const *names, size_t count, const char *b) {
  int found = 0;
  for (size_t i = 0; i < count; i++) found |= names[i] != NULL && strcmp(names[i], b) == 0;
  return found;
}

/* The weights binder of NAMERd: the first of w, w_, w__ and w___ that is
   not one of the WEIGHTS_NAMES names NAMES. The names are at most three
   different names, so the last binder is free when the others are not. */
static const char *weights_binder(const char *const *names) {
  static const char *const binders[] = {"w", "w_", "w__", "w___"};
  size_t i = 0;
  while (i < 3 && has_name(names, WEIGHTS_NAMES, binders[i])) i++;
  return binders[i];
}

/* The form of the open NAMESat proof at the PIN. */
typedef enum SatForm { SAT_REFL, SAT_DEC_ENC } SatForm;

/* The NAMESat form for the circuit MODE. For the dense, lookup, softmax and
   layer normalization circuits, refl proves the open path (softmax: at the
   scale ZKML_SOFTMAX_K only;  layer normalization: at every table size to
   256 rows, P9b s6c kickoff F1). For circResidual the zipWithT bbAdd fold is stuck on an open input,
   so refl fails. The proof is then the inverse of the domain lemma
   stageDecEnc at NAMET (P9b s6a). For the attention, refl fails at an open
   input, and the stageDecEnc proof checks for an exp table of at most
   ZKML_ATTENTION_ROWS_MAX rows (P9c kickoff F2 and F7). */
static SatForm sat_form(ZkmlMode mode) {
  switch (mode.op) {
    case ZKML_OP_DENSE:
    case ZKML_OP_LOOKUP:
    case ZKML_OP_SOFTMAX:
    case ZKML_OP_LAYERNORM:
      return SAT_REFL;
    case ZKML_OP_RESIDUAL:
    case ZKML_OP_RESIDUAL_LOOP:
    case ZKML_OP_ATTENTION:
      return SAT_DEC_ENC;
  }
  return SAT_REFL;
}

/* Prints NAMECirc for the circuit MODE. The dense layer is Op [N] [M]. The
   residual circuit is circResidual of Op [N] [N] (M = N). The residual loop
   is circResidualLoop of Op [N] [N] with K steps (M = N). The lookup circuit
   is circSeq of the dense layer and opLookup on [M] with the table 1. The
   softmax circuit is circSeq of the dense layer and opSoftmax on [M] with the
   exp table 1 and the scale ZKML_SOFTMAX_K. The layer normalization circuit
   is circSeq of the dense layer and opLayerNorm on [M] with the gamma-beta
   matrix 1 and the inverse square root table 2. The attention circuit is
   opAttention on [ZKML_ATTENTION_H, N] (M = N) with the query matrix 0, the
   key matrix 1, the value matrix 2, the exp table 3 and the scale
   ZKML_SOFTMAX_K. */
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
    case ZKML_OP_SOFTMAX:
      fprintf(out, "def %sCirc : Circuit %sS %sT := circSeq %sS %sT %sT (circOp %sS %sT (opMatMul %u %u 0)) (circOp %sT %sT (opSoftmax %sT 1 %u))\n",
              name, name, name, name, name, name, name, name, n, m, name, name, name, ZKML_SOFTMAX_K);
      return;
    case ZKML_OP_LAYERNORM:
      fprintf(out, "def %sCirc : Circuit %sS %sT := circSeq %sS %sT %sT (circOp %sS %sT (opMatMul %u %u 0)) (circOp %sT %sT (opLayerNorm %sT 1 2))\n",
              name, name, name, name, name, name, name, name, n, m, name, name, name);
      return;
    case ZKML_OP_ATTENTION:
      fprintf(out, "def %sCirc : Circuit %sS %sT := circOp %sS %sT (opAttention %u %u 0 1 2 3 %u)\n", name, name, name, name, name,
              ZKML_ATTENTION_H, n, ZKML_SOFTMAX_K);
      return;
  }
}

/* Prints the generated defs for a model of M rows and N columns, with the
   circuit MODE. The weights reader gives the model as the matrix 0, in the
   lookup and softmax modes the table as the list 1, and in the layer
   normalization mode the gamma-beta matrix as the list 1 and the table as
   the list 2, and in the attention mode the key matrix as the list 1, the
   value matrix as the list 2 and the table as the list 3. The attention
   input is a list of H rows of N entries (shape [H, N]). */
static void print_generated(FILE *out, const char *name, uint32_t m, uint32_t n, ZkmlMode mode) {
  const char *table = table_of(mode);
  const char *gb = gb_of(mode);
  const char *const names[WEIGHTS_NAMES] = {gb, key_of(mode, 0), key_of(mode, 1), table};
  uint32_t h = input_rows(mode);
  const char *row = h == 0 ? "List Nat" : "List (List Nat)";
  print_circuit_text(out, name, mode);
  print_reads(out, name, mode);
  fprintf(out, "-- %sWitE evaluates the trace of two stages: the input and the bbRun output.\n", name);
  switch (sat_form(mode)) {
    case SAT_REFL:
      fprintf(out, "-- %sSat: the two-stage witness %sWit satisfies %sCirc at every input, by refl at the PIN.\n", name, name, name);
      break;
    case SAT_DEC_ENC:
      fprintf(out, "-- %sSat: the two-stage witness %sWit satisfies %sCirc at every input, by the inverse of stageDecEnc at the PIN.\n", name,
              name, name);
      break;
  }
  fprintf(out, "-- %sEnc is the base-1000 code of %s: an eval code, not injective above 999.\n", name, h == 0 ? "a row" : "the rows");
  if (h == 0) {
    fprintf(out, "def %sS : Shape := shape (cons %u nil)\n", name, n);
    fprintf(out, "def %sT : Shape := shape (cons %u nil)\n", name, m);
  } else {
    fprintf(out, "def %sS : Shape := shape (cons %u (cons %u nil))\n", name, h, n);
    fprintf(out, "def %sT : Shape := shape (cons %u (cons %u nil))\n", name, h, m);
  }
  const char *weights = weights_binder(names);
  fprintf(out, "def %sRd : List (List Nat) -> BbWeights := fun (%s : List (List Nat)) => cons %s ", name, weights, weights);
  size_t listed_names = 0;
  for (size_t i = 0; i < WEIGHTS_NAMES; i++) {
    if (names[i] != NULL) fprintf(out, "(cons %s ", names[i]);
    listed_names += names[i] != NULL;
  }
  fputs("nil", out);
  for (size_t i = 0; i < listed_names; i++) fputc(')', out);
  fputc('\n', out);
  print_circuit(out, name, m, n, mode);
  if (h == 0) {
    fprintf(out, "def %sEnc : List Nat -> Nat := fun (ys : List Nat) => fold (fun (v : Nat) (acc : Nat) => natAdd (natMul acc 1000) v) 0 ys\n",
            name);
  } else {
    fprintf(out, "def %sEnc : List (List Nat) -> Nat := fun (ys : List (List Nat)) => fold (fun (r : List Nat) (acc : Nat) => fold (fun "
                 "(v : Nat) (a2 : Nat) => natAdd (natMul a2 1000) v) acc r) 0 ys\n", name);
  }
  const char *input = strcmp(name, "x") == 0 ? "x_" : "x";
  uint32_t params = h == 0 ? n : h * n;
  fprintf(out, "def %sWit : %s -> Prod (List (List Nat)) (List (Stage Nat)) := fun (%s : %s) => pair %s (cons (stageEnc Nat %sS %s) (cons (stageEnc Nat %sT (bbRun %sS %sT %sCirc (%sRd %s) %s)) nil))\n",
          name, row, input, row, name, name, input, name, name, name, name, name, name, input);
  print_entry_head(out, name, "RunE", params);
  fprintf(out, "%sEnc (", name);
  print_run(out, name, n, h);
  fputs(")\n", out);
  print_entry_head(out, name, "WitE", params);
  fprintf(out, "%sEnc (instEval (bbInst (List (List Nat)) %sRd) %sS %sT %sCirc (%sWit (", name, name, name, name, name, name);
  print_inputs(out, name, n, h);
  fputs(")))\n", out);
  fprintf(out, "def %sSat : (x : %s) -> bbInstSat (List (List Nat)) %sRd %sS %sT %sCirc x (%sWit x) := fun (%s : %s) => ", name, row,
          name, name, name, name, name, input, row);
  /* The dim binder is i_ for the model i, as the input binder is x_ for the model x. */
  const char *dim = strcmp(name, "i") == 0 ? "i_" : "i";
  switch (sat_form(mode)) {
    case SAT_REFL:
      fputs("refl\n", out);
      break;
    case SAT_DEC_ENC:
      fprintf(out, "<%s> stageDecEnc Nat 0 %sT (bbRun %sS %sT %sCirc (%sRd %s) %s) @ ~ %s\n", dim, name, name, name, name, name, name, input,
              dim);
      break;
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

/* The BabyBear prime bbP of zkml-lang. A table or gamma-beta entry at or
   above bbP can overflow natMul in the host Wasm (P9b s6c kickoff F7). */
#define ZKML_BB_P 2013265921u

/* 1 when each entry of the closed rows V is below ZKML_BB_P. */
static int below_p(const Value *v) {
  for (; val_is(v, OP_CONS); v = v->args[1]) {
    for (const Value *x = v->args[0]; val_is(x, OP_CONS); x = x->args[1]) {
      if (x->args[0]->nat >= ZKML_BB_P) return 0;
    }
  }
  return 1;
}

/* Resolves the weights WHAT named WEIGHTS (the table or the gamma-beta
   matrix) on the path of the model (ZKML_NAME, then ZKML_MODEL for the type,
   the rows and an entry at or above bbP). Gives the declaration in *DI and
   the shape in *ROWS and *COLS. Returns 1, or 0 after a diagnostic. */
static int check_weights(Machine *m, const DeclList *decls, const char *weights, const char *what, uint32_t max_cols, size_t *di,
                         uint32_t *rows, uint32_t *cols, Diag *diag) {
  size_t i = find_program_decl(decls, weights);
  uint32_t dd = find_program_def(m, weights);
  if (i == decls->count || dd == m->def_count) return diag_fail(diag, "ZKML_NAME", weights, "no program definition has this name");
  if (!is_weights_type(m->defs[dd].type)) return diag_fail(diag, "ZKML_MODEL", weights, "the type of the %s is not List (List Nat)", what);
  m->def = weights;
  const Value *v = def_value(m, dd);
  if (v == NULL) return 0;
  if (!read_rows(v, rows, cols, max_cols))
    return diag_fail(diag, "ZKML_MODEL", weights, "the value is not m >= 1 rows of equal length n, 1 <= n <= %u", max_cols);
  if (!below_p(v)) return diag_fail(diag, "ZKML_MODEL", weights, "the %s has an entry at or above bbP = %u", what, ZKML_BB_P);
  *di = i;
  return 1;
}

/* Supported attention table sizes at the PIN for widths 1 to 8. The Sat
   proof capacity depends on both the width and the table size. Each bound
   is checked with distinct dense query, key and value matrices. */
static uint32_t attention_rows_max(uint32_t n) {
  static const uint32_t limits[] = {0, ZKML_ATTENTION_ROWS_MAX, ZKML_ATTENTION_ROWS_MAX, 5, 4, 3, 2, 1, 1};
  return n < sizeof limits / sizeof limits[0] ? limits[n] : 0;
}

/* Resolves the table of MODE (lookup, softmax, layer normalization or attention) with
   check_weights, then checks that the rows have two entries. Gives the
   declaration of the table in *TI (NI when the mode has no table). Returns
   1, or 0 after a diagnostic. */
static int check_table(Machine *m, const DeclList *decls, ZkmlMode mode, uint32_t n, size_t ni, size_t *ti, Diag *diag) {
  const char *table = table_of(mode);
  uint32_t rows = 0;
  uint32_t cols = 0;
  *ti = ni;
  if (table == NULL) return 1;
  if (!check_weights(m, decls, table, "table", ENTRY_PARAMS_MAX, ti, &rows, &cols, diag)) return 0;
  if (cols != 2) return diag_fail(diag, "ZKML_MODEL", table, "the table needs rows of two entries");
  uint32_t max_rows = attention_rows_max(n);
  return input_rows(mode) == 0 || rows <= max_rows
             ? 1
             : diag_fail(diag, "ZKML_MODEL", table, "the attention table has %u rows, above %u for n = %u: the Sat proof stops at the arena limit at the PIN",
                         rows, max_rows, n);
}

/* Resolves the key matrix and the value matrix of MODE (attention) with
   check_weights, then checks that each has N rows of N entries. Gives the
   declarations in *KI and *VI (NI when the mode has no such matrix).
   Returns 1, or 0 after a diagnostic. */
static int check_keys(Machine *m, const DeclList *decls, ZkmlMode mode, uint32_t n, size_t ni, size_t *ki, size_t *vi, Diag *diag) {
  size_t *const at[] = {ki, vi};
  static const char *const what[] = {"key matrix", "value matrix"};
  int ok = 1;
  for (int i = 0; i < 2 && ok; i++) {
    const char *key = key_of(mode, i);
    uint32_t rows = 0;
    uint32_t cols = 0;
    *at[i] = ni;
    if (key == NULL) continue;
    ok = check_weights(m, decls, key, what[i], n, at[i], &rows, &cols, diag) &&
         (rows == n && cols == n ? 1 : diag_fail(diag, "ZKML_MODEL", key, "the %s needs n = %u rows of n entries", what[i], n));
  }
  return ok;
}

/* Resolves the gamma-beta matrix of MODE (layer normalization) with
   check_weights, then checks that it has two rows (gamma, beta) of OUT_ROWS
   entries: one entry for each output of the dense layer. Gives the
   declaration of the matrix in *GI (NI when the mode has no matrix).
   Returns 1, or 0 after a diagnostic. */
static int check_matrix(Machine *m, const DeclList *decls, ZkmlMode mode, uint32_t out_rows, size_t ni, size_t *gi, Diag *diag) {
  const char *gb = gb_of(mode);
  uint32_t rows = 0;
  uint32_t cols = 0;
  *gi = ni;
  if (gb == NULL) return 1;
  if (!check_weights(m, decls, gb, "gamma-beta matrix", out_rows, gi, &rows, &cols, diag)) return 0;
  return rows == 2 && cols == out_rows
             ? 1
             : diag_fail(diag, "ZKML_MODEL", gb, "the gamma-beta matrix needs two rows of m = %u entries", out_rows);
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
  size_t gi = decls->count;
  uint32_t di = find_program_def(m, name);
  if (ni == decls->count || di == m->def_count) return diag_fail(diag, "ZKML_NAME", name, "no program definition has this name") + 1;
  if (!is_weights_type(m->defs[di].type)) return diag_fail(diag, "ZKML_MODEL", name, "the type of the model is not List (List Nat)") + 1;
  m->def = name;
  const Value *v = def_value(m, di);
  if (v == NULL) return 1;
  uint32_t rows = 0;
  uint32_t cols = 0;
  if (!read_rows(v, &rows, &cols, ENTRY_PARAMS_MAX))
    return diag_fail(diag, "ZKML_MODEL", name, "the value is not m >= 1 rows of equal length n, 1 <= n <= %u", ENTRY_PARAMS_MAX) + 1;
  if ((mode.op == ZKML_OP_RESIDUAL || mode.op == ZKML_OP_RESIDUAL_LOOP) && rows != cols)
    return diag_fail(diag, "ZKML_MODEL", name, "the residual circuit needs m = n") + 1;
  if (mode.op == ZKML_OP_ATTENTION && (rows != cols || ZKML_ATTENTION_H * cols > ENTRY_PARAMS_MAX))
    return diag_fail(diag, "ZKML_MODEL", name, "the attention circuit needs m = n and %u n <= %u", ZKML_ATTENTION_H, ENTRY_PARAMS_MAX) + 1;
  size_t ki = decls->count;
  size_t vi = decls->count;
  if (!check_matrix(m, decls, mode, rows, ni, &gi, diag) || !check_keys(m, decls, mode, cols, ni, &ki, &vi, diag) ||
      !check_table(m, decls, mode, cols, ni, &ti, diag))
    return 1;
  m->def = name;
  if (!check_generated_names(m, decls, name, diag)) return 1;
  Walk w = {decls, arena_alloc(m->arena, decls->count + 1), arena_alloc(m->arena, (decls->count + 1) * sizeof(size_t)), 0, 0, NULL};
  unsigned char *copy = arena_alloc(m->arena, decls->count + 1);
  unsigned char *skip = arena_alloc(m->arena, decls->count + 1);
  if (w.mark == NULL || w.todo == NULL || copy == NULL || skip == NULL) return diag_fail(diag, "OOM", name, "out of memory") + 1;
  memset(copy, 0, decls->count + 1);
  memset(skip, 0, decls->count + 1);
  /* COPY is a union of marks, so a definition in two closures is copied once. */
  if (!copy_closure(&w, ni, copy) || (gi != ni && !copy_closure(&w, gi, copy)) || (ki != ni && !copy_closure(&w, ki, copy)) ||
      (vi != ni && !copy_closure(&w, vi, copy)) || (ti != ni && !copy_closure(&w, ti, copy)))
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
  ZkmlMode dense = {ZKML_OP_DENSE, 0, NULL, NULL, NULL, NULL};
  return zkml_command_op(m, decls, text, len, name, dense, out_path, diag);
}
