/* The zkml writer (P9a rule R4, P9b): zkml-lang source for a model of a
   checked program. */
#ifndef LANG_ZKML_H
#define LANG_ZKML_H

#include "front/check.h"

/* The circuit of the model: the dense layer, x plus the dense layer, K
   residual steps of the dense layer (circResidualLoop, P9b s3), or the dense
   layer then a table lookup on each entry (circSeq of opMatMul and opLookup,
   P9b s4), or the dense layer then a softmax with an exp table (circSeq of
   opMatMul and opSoftmax at the scale ZKML_SOFTMAX_K, P9b s6b), or the dense
   layer then a layer normalization with a gamma-beta matrix and an inverse
   square root table (circSeq of opMatMul and opLayerNorm, P9b s6c), or the
   attention on ZKML_ATTENTION_H rows with the model as the query matrix, a
   key matrix, a value matrix and an exp table (opAttention, P9c s2). */
typedef enum ZkmlOp {
  ZKML_OP_DENSE,
  ZKML_OP_RESIDUAL,
  ZKML_OP_RESIDUAL_LOOP,
  ZKML_OP_LOOKUP,
  ZKML_OP_SOFTMAX,
  ZKML_OP_LAYERNORM,
  ZKML_OP_ATTENTION
} ZkmlOp;

/* The softmax scale K: the output entries are floor(e 2^K / sum e). The PIN
   check of the refl NAMESat stops at the arena limit for K above 3 (P9b s6b
   kickoff F1), so K is fixed. The attention uses the same K. */
#define ZKML_SOFTMAX_K 3u

/* The row count H of the attention input (P9c rule R23). */
#define ZKML_ATTENTION_H 2u

/* The largest attention exp table at widths 1 and 2. Larger widths have
   smaller caps in attention_rows_max: the PIN check of the stageDecEnc
   NAMESat stops at the arena limit (P9c kickoff F7, rule R25). */
#define ZKML_ATTENTION_ROWS_MAX 8u

/* The largest step count K of the residual loop. */
#define ZKML_LOOP_MAX 16u

/* The circuit OP, the step count STEPS, the table name TABLE and the
   gamma-beta matrix name GB. Only ZKML_OP_RESIDUAL_LOOP reads STEPS (1 to
   ZKML_LOOP_MAX); the other modes have STEPS 0. Only ZKML_OP_LOOKUP,
   ZKML_OP_SOFTMAX, ZKML_OP_LAYERNORM and ZKML_OP_ATTENTION read TABLE (a
   program definition name: the table at index 1 of the weights, index 2
   for layer normalization, or index 3 for attention); the other modes
   have TABLE NULL. Only ZKML_OP_LAYERNORM
   reads GB (a program definition name other than TABLE: the matrix at index
   1 of the weights, gamma in row 0 and beta in row 1); the other modes have
   GB NULL. Only ZKML_OP_ATTENTION reads WK and WV (program definition names:
   the key matrix at index 1 and the value matrix at index 2 of the weights,
   the table at index 3); the other modes have WK and WV NULL. */
typedef struct ZkmlMode {
  ZkmlOp op;
  unsigned steps;
  const char *table;
  const char *gb;
  const char *wk;
  const char *wv;
} ZkmlMode;

/* Writes the zkml-lang source for the model NAME of the program TEXT (LEN
   bytes) to OUT_PATH, with the circuit MODE. DECLS and M are the checked
   program. Returns 0, or 1 after a diagnostic. */
int zkml_command_op(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, ZkmlMode mode,
                    const char *out_path, Diag *diag);

/* zkml_command_op with the dense circuit. */
int zkml_command(Machine *m, const DeclList *decls, const char *text, size_t len, const char *name, const char *out_path, Diag *diag);

#endif
