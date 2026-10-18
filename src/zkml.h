/* The zkml writer (P9a, rule R4): zkml-lang source for a model of a checked
   program. */
#ifndef LANG_ZKML_H
#define LANG_ZKML_H

#include "front/check.h"

/* Writes the zkml-lang source for the model NAME of the program TEXT (LEN
   bytes) to OUT_PATH. DECLS and M are the checked program. Returns 0, or 1
   after a diagnostic. */
int zkml_command(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, const char *out_path, Diag *diag);

#endif
