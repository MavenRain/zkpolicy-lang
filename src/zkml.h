/* The zkml writer (P9a rule R4, P9b): zkml-lang source for a model of a
   checked program. */
#ifndef LANG_ZKML_H
#define LANG_ZKML_H

#include "front/check.h"

/* The circuit of the model: the dense layer, x plus the dense layer, or K
   residual steps of the dense layer (circResidualLoop, P9b s3). */
typedef enum ZkmlOp { ZKML_OP_DENSE, ZKML_OP_RESIDUAL, ZKML_OP_RESIDUAL_LOOP } ZkmlOp;

/* The largest step count K of the residual loop. */
#define ZKML_LOOP_MAX 16u

/* The circuit OP and the step count STEPS. Only ZKML_OP_RESIDUAL_LOOP reads
   STEPS (1 to ZKML_LOOP_MAX). The other modes have STEPS 0. */
typedef struct ZkmlMode {
  ZkmlOp op;
  unsigned steps;
} ZkmlMode;

/* Writes the zkml-lang source for the model NAME of the program TEXT (LEN
   bytes) to OUT_PATH, with the circuit MODE. DECLS and M are the checked
   program. Returns 0, or 1 after a diagnostic. */
int zkml_command_op(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, ZkmlMode mode,
                    const char *out_path, Diag *diag);

/* zkml_command_op with the dense circuit. */
int zkml_command(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, const char *out_path, Diag *diag);

#endif
