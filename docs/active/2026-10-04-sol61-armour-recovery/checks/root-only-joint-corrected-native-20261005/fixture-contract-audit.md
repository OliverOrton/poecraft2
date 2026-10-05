# Complete fixture baseline contract audit

Audit only, against implementation fdf043ba96cd81881062a34eb8bb929b81006e62 and
checkout 23dcc5aea8828dc535466613479493bc379d4e7b. No native source edit, build,
checker rerun or original Conquest root occurs in this audit. The corrected
fixture's qualified binary is a8d47954029dffe63ae8bfc501668b0cc26756605ce7ffcd1ee6699f0e7fd74e,
20,595,037 bytes. Its sole selector stops in 385.087 ms with 0/3 cases complete.
LOCAL releases at 18:08:44 UTC with an empty census and clean owned-job drain.

## Captured refusal and its complete source cause

The four-event progress ring captures check_refused with a reason beginning:

> original-root controller violates caller scope: operation 'restart' at node 'bounded_default_restart' is outside the requested

The bounded reason field truncates the suffix. The full reason reconstructed
from solver_compile.cpp:141-145 and solver_policy_assertion_work.cpp:701-705
ends "action scope". The runtime event captures the scope refusal; its enum
CompilationFailure comes from the corresponding source branch, rather than
a serialized assertion object. The retained-root failure printed afterward,
private_compiled_entry_witness_changed, is a consequence of the unissued
certificate and is not the first checker refusal.

The fixture creates a fresh SolveResult proof at test_solver_solve.cpp:17093,
copies its policy/value/start fields, and omits its options. SolveOptions defaults
allow_economic_restart to true (solver_solve_contracts.hpp:205), whereas the
actual work options set it false. The compiler consults result.options, not its
caller's work object. For a bounded policy with economic Restart enabled it
selects a synthetic registry action (solver_compile.cpp:2818-2829), emits the
bounded_default_restart operation (3605-3612) and its edge back to the root
router (4122-4123).

CertificationFailClosed selects offpolicy for router defaults (2867-2877).
It does not suppress that separately emitted Restart node. The permission owner
checks every operation node, including unreachable nodes (116-146), against
the original four-action candidate scope. Restart is absent from that scope;
the refusal is correct. Deleting JSON nodes, allowing Restart, adding a base
price or weakening admission would bypass the actual contract rather than fix
this fixture's mismatched construction options.

## Small full fixture checklist

| Contract | Source audit and remaining gate |
| --- | --- |
| One request/options scope | Copy the normalized work options into the initial proof. Keep Annul, Exalt, Scour and Alchemy only, economic Restart false, automatic candidates false, TargetNeutralZero and all original bounds. The current constructor's final true argument means product_solver_parent, not modifier-identity mode; do not mislabel this as a general full-identity/class-domain fixture. |
| Every emitted operation and payment | The baseline selects Annul at R(G,J), Scour at J and Alchemy at Normal empty. Exalt is permitted for the later treatment. Prices are Annul 1, Exalt 1, Scour 10 and Alchemy 1 under both policies. Each is an ordinary single primitive. No selected Fracture, Imprint or fixed option can inject a recovery operation. After copying options, the compiler's Restart-emission condition is false. All-node permission and complete-price checking remain mandatory. |
| Original start and exact terminal | The start is the supplied Rare G+J item. The goal is Rare family 100/tier 1 with ForbidUnmatched extras; G alone succeeds, G+J does not. The compiler routes the exact goal first, then native policy regions, with default failure. The captured scope event occurs after parsing, original-start identity and success-ingress checks. Those earlier checks pass; exact evaluation has not run. |
| Complete native positive support | Previously reached assertions establish Annul goal mass 1/2 plus one loss, Scour loss-to-Normal with unit mass, and Alchemy Normal-to-original-root with unit mass. Only G and J have positive roll weight in the existing test-owned catalog, so these reachable states have no competing positive modifier identities. Exalt's unit-mass return remains an unexecuted requirement; no law is forced to fit the prediction. |
| Properness and economics | Baseline R --Annul--> G success or J; J --Scour--> N; N --Alchemy--> R. Conditional native laws give V=1+(11+V)/2=13 and almost-sure success. The proposed Exalt loss return gives V=1+(1+V)/2=3 only if its native law passes. The independent checker must still prove finite properness, complete prices and zero off-policy mass. Neither cost is measured. |
| Root-only authority and retention | Keep length-n invalid policy/row placeholders, zero reachability and only the root value finite. The prior placeholder correction passes the first provenance guard by the captured later scope refusal. The existing root checker must issue a matching exact continuation certificate before the retained-root validator can accept it; copied flags or a root scalar are insufficient. The old graph stays owned before treatment capture. |
| Shared resource and lifecycle ownership | The initial fixture emission currently gives the compiler the whole 512 MiB while solver and copied proof remain live. Bound that call by remaining allowance using existing estimated_owned_bytes and solve_result_owned_bytes, and release emission telemetry/proof before queuing the existing checker. Native 32-state/60-second and host 75-second bounds, no allowance reset, one existing complete-candidate slot, missing-support refusal and cap refusal remain unchanged. |

## Exact proposed test-only correction

The semantic correction is proof.options = work.options before initial graph
emission. This carries the actual caller's options to the existing policy compiler;
it neither changes the request nor widens an action permission. The same owner
then emits the complete three-operation baseline with fail-closed defaults and
no synthetic Restart. The later statewise treatment checker already sets
proof.options = scoped from the work options in certify_initial_candidate.

The accompanying fixture accounting correction subtracts the live solver and
external proof bytes from the original shared cap before this initial emission,
using existing census helpers. Its returned telemetry is released after its
node/edge counts are copied; the proof is already released before the checker.
No production compiler, admission, numerical law, expected result or baseline
requirement changes. The exact unapplied delta is retained as
proposed-fixture-options.patch. Existing baseline failure progress output stays.

Future acceptance is the same three-case selector after CI-owned source-matched
compilation and a parent LOCAL grant. Complete baseline checking must precede
all economic assertions; any new refusal stops the run. Missing positive native
support and shared-cap refusal must preserve the checked root artifact and
must not queue a checker. The original 0/3 failures, earlier source-inferred
versus uncaptured-runtime distinction, losing P0 word/Current-tail result and
unused Conquest root slot 2 remain retained. This proposal is not applied.
