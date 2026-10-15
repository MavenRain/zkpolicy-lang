# zkpolicy-lang domain design

> Paste the domain design below the line, verbatim. Do not edit it.

This file is the only source of the core types and the core operations of
zkpolicy-lang. `SPEC.md` sections 5 and 6 cite its sections. Do not add a core
type or a core operation that this file does not give.

When the design changes, paste the new version. Then record the change as an
open item in `SPEC.md` section 9.

---

For type constructors and higher-kinded types, admit products, coproducts, monads (map/bind), algebras and coalgebras (fold/unfold), quotients, paths, loops, HITs, and QIITs as in Cubical TT.  Proof irrelevance is prohibited.

For the base types and operations, restrict to those described in the following:

```agda
{-# OPTIONS --cubical #-}

open import Cubical.Foundations.Prelude
open import Cubical.Foundations.Equiv
open import Cubical.Foundations.Function
open import Cubical.Data.Sigma

-- Minimal span for a DSL of ZK-verifiable agent policies / small-model inference
-- (DAO + edge). Types + operations + path-laws + higher equivalences.

module ZKPolicyDSL where

-- ── Core types ──────────────────────────────────────────────────────────────

-- Visibility split (privacy boundary)
data Visibility : Type where
  public private : Visibility

-- Abstract field indexed by visibility (can be refined to vectors, etc.)
Field : Visibility → Type₁
Field _ = Type

-- Agent / model input (public + private parts)
record Context : Type₁ where
  field
    pub  : Field public
    priv : Field private

-- Decision / inference output (can be a quantized tensor, action, etc.)
Decision : Type₁
Decision = Type

-- Policy / small-model (high-level spec or quantized net)
Policy : Type₁
Policy = Type

-- Execution transcript (log of steps; ZK-friendly)
Transcript : Type₁
Transcript = Type

-- ZK proof of policy adherence / correct inference
-- (dependent on policy, context, and claimed transcript)
ZKProof : Policy → Context → Transcript → Type₁
ZKProof _ _ _ = Type

-- ── Operations ──────────────────────────────────────────────────────────────

-- Denotational evaluation (or forward pass of small net)
eval : Policy → Context → Decision

-- Produce a transcript + proof of adherence / correct inference
prove : (p : Policy) (c : Context) → Σ[ t ∈ Transcript ] ZKProof p c t

-- Sequential / parallel composition of policies (or layers)
compose : Policy → Policy → Policy

-- Privacy boundary restriction (erase / seal private fields)
restrict : Policy → Policy

-- Verification (only needs public part of context)
verify : (p : Policy) (pub : Field public) (t : Transcript)
       → ZKProof p (record { pub = pub ; priv = _ }) t → Bool
  -- (abstract; in practice a circuit)

-- ── Laws (paths) ────────────────────────────────────────────────────────────

-- Associativity of composition
compose-assoc : ∀ p q r → compose (compose p q) r ≡ compose p (compose q r)

-- Evaluation respects composition (sequential case)
eval-compose : ∀ p q c →
  eval (compose p q) c ≡ eval q (record { pub = Context.pub c
                                        ; priv = _ })   -- abstract update
  -- (full version uses an update operation on Context)

-- Soundness: a proof guarantees the claimed decision
soundness : ∀ p c →
  let (t , prf) = prove p c
  in -- path relating the transcript to eval p c
     PathP (λ _ → Decision) (eval p c) (eval p c)   -- placeholder; actual is dependent

-- Privacy independence: for policies that do not depend on private data
-- (or after restrict), changing the private part does not change the decision
privacy : ∀ p (c₁ c₂ : Context) →
  Context.pub c₁ ≡ Context.pub c₂ →
  eval (restrict p) c₁ ≡ eval (restrict p) c₂

-- ── Higher structure / equivalences ─────────────────────────────────────────

-- Two policies are equivalent if they agree on all contexts (observational)
-- (this is a path in the function space; funext gives the equivalence)
PolicyEquiv : Policy → Policy → Type₁
PolicyEquiv p q = ∀ c → eval p c ≡ eval q c

-- Structure identity principle style: equivalent policies are path-equal
-- (via univalence + funext when the equivalence is coherent)
sip-policy : ∀ p q → PolicyEquiv p q ≃ (p ≡ q)
  -- (holds under suitable universe levels / when Policy is a structure)

-- Higher: 2-paths for coherence of composition of proofs
-- (e.g., different ways of composing ZK transcripts are path-equal)
-- or HIT for the free algebra of policies modulo the rewrite rules above.

-- ── DAO / self-referential angle ────────────────────────────────────────────

-- A policy may depend on a governance context that itself contains a Policy
-- (fixed-point / recursive). Can be expressed via a HIT or just
-- Policy depending on a type family that includes Policy.
-- Denotational design: the eval above is the interpretation map.

-- Small-model specialisation: Decision ≃ QuantizedTensor,
-- Policy ≃ architecture + weights, prove ≃ ZK circuit for the forward pass
-- (designed so that the circuit is efficient for the chosen quantization).
```

Treat the laws as a minimal list that can be expanded, but admit no further base types or operations.
