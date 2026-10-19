/* The zkml writer (P9a rule R4, P9b): zkml-lang source for a model of a
   checked program. */
#ifndef LANG_ZKML_H
#define LANG_ZKML_H

#include "front/check.h"

/* The circuit of the model: the dense layer, or x plus the dense layer. */
typedef enum ZkmlOp { ZKML_OP_DENSE, ZKML_OP_RESIDUAL } ZkmlOp;

/* Writes the zkml-lang source for the model NAME of the program TEXT (LEN
   bytes) to OUT_PATH, with the circuit OP. DECLS and M are the checked
   program. Returns 0, or 1 after a diagnostic. */
int zkml_command_op(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, ZkmlOp op,
                    const char *out_path, Diag *diag);

/* zkml_command_op with the dense circuit. */
int zkml_command(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, const char *out_path, Diag *diag);

#endif
