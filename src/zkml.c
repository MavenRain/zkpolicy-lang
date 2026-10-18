/* The zkml writer (P9a, rule R4). The model NAME is a program definition of
   type List (List Nat): the weights. The writer copies NAME and the def
   closure of the model entries (the entries that refer to NAME), then
   generates the circuit, the encoder and the circuit entries. The writer is
   trusted base for the text that it copies and generates; `make zk-check`
   checks the text with zkml-lang at the PIN. */
#include <stdio.h>
#include <string.h>

#include "zkml.h"
#include "zkml-names.h"

/* The generated names are NAME with these suffixes. */
static const char *const SUFFIXES[] = {"S", "Circ", "Enc", "RunE", "WitE"};
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

static void print_generated(FILE *out, const char *name, uint32_t n) {
  fprintf(out, "-- Generated. At the PIN only circId and circSeq exist, so %sCirc is the identity\n", name);
  fputs("-- and does not compute the denotation. The operator circuits come in P9b.\n", out);
  fprintf(out, "-- %sEnc is the base-1000 code of a row: an eval code, not injective above 999.\n", name);
  fprintf(out, "def %sS : Shape := shape (cons %u nil)\n", name, n);
  fprintf(out, "def %sCirc : Circuit %sS %sS := circSeq %sS %sS %sS (circId %sS) (circId %sS)\n", name, name, name,
          name, name, name, name, name);
  fprintf(out, "def %sEnc : List Nat -> Nat := fun (ys : List Nat) => fold (fun (v : Nat) (acc : Nat) => natAdd (natMul acc 1000) v) 0 ys\n",
          name);
  print_entry_head(out, name, "RunE", n);
  fprintf(out, "%sEnc (bbRun %sS %sS %sCirc (", name, name, name, name);
  print_inputs(out, name, n);
  fputs("))\n", out);
  print_entry_head(out, name, "WitE", n);
  fprintf(out, "%sEnc (bbInstEval (List (List Nat)) %sS %sS %sCirc (pair %s (cons (stageEnc Nat %sS (", name, name, name,
          name, name, name);
  print_inputs(out, name, n);
  fputs(")) nil)))\n", out);
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

int zkml_command(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, const char *out_path, Diag *diag) {
  size_t ni = find_program_decl(decls, name);
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
  if (!check_generated_names(m, decls, name, diag)) return 1;
  Walk w = {decls, arena_alloc(m->arena, decls->count + 1), arena_alloc(m->arena, (decls->count + 1) * sizeof(size_t)), 0, 0, NULL};
  unsigned char *copy = arena_alloc(m->arena, decls->count + 1);
  unsigned char *skip = arena_alloc(m->arena, decls->count + 1);
  if (w.mark == NULL || w.todo == NULL || copy == NULL || skip == NULL) return diag_fail(diag, "OOM", name, "out of memory") + 1;
  memset(copy, 0, decls->count + 1);
  memset(skip, 0, decls->count + 1);
  close_over(&w, ni);
  if (w.unsupported != NULL)
    return diag_fail(diag, "ZKML_MODEL", name, "name '%s' is not supported at the zkml PIN", w.unsupported) + 1;
  for (size_t i = 0; i < decls->count; i++) {
    if (decls->items[i].origin == ORIGIN_PROGRAM) copy[i] |= w.mark[i];
  }
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
  print_generated(out, name, cols);
  return fclose(out) == 0 ? 0 : diag_fail(diag, "IO", name, "cannot write '%s'", out_path) + 1;
}
