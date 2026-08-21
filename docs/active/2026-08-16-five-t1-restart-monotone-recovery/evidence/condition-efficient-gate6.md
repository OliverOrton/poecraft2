# Condition-Efficient Compilation Gate 6

**Status: complete; solver policy and bounded-result truth are unchanged.**

The three frozen graphs were solved, compiled, and independently exact-
evaluated after the shared decision-DAG consolidation. Their strategy bytes
are identical to the pre-consolidation Gate 2 outputs.

| Control | Transition hash | Policy hash | Strategy SHA-256 | Exact cost | Success | Off-policy |
| --- | --- | --- | --- | ---: | ---: | ---: |
| Witness A | `284ff325a96fe0d7` | `16a9d1bf4edd9e29` | `c84585816f381d6765f8cd6563f007d97d9cc72bacc48857e38f52970e77bb5c` | 624,800.9519118543 | 1 | 0 |
| priced Witness B | `f1de3453ca5b0e87` | `9525d51169f024a7` | `59925dd142e7329e4ea8fc69a348cd30a15699910b340eea0fd6581791a3efb1` | 16,226,566.773294946 | 1 | 0 |
| exact four-T1 | `1c5594f87917f760` | `2c96f9faf0479667` | `dc0a495c328878ab544af13ae9d193ba1547067fca239eae5407835c4432d639` | 3,745.73093400839 | 1 | 0 |

All transition and policy hashes exactly match Gate 0, proving the compiler
work did not change solver selection.

Witness B remains a bounded executable policy, not an optimum. It selects
Chaos, Exalt, and Dense Fossil with one-socket resonators. Exact expected
visits remain 367,893.75912151358 Chaos, 14,688.024081601376 Exalts, and
1,266,462.0414789424 Dense Fossils/resonators. Product defaults remain safe
Restart; the paired certification defaults remain fail-closed off-policy.

The known proof limitations are deliberately unchanged:

- direct certification remains `cost_mismatch`, comparing stored
  `37279651.842345364` with exact `16226566.773294946`;
- strict lift remains `coarse_mapping_failure` because carrier 5983 maps
  outside the solved coarse graph;
- the global lower bound remains zero; and
- Witness B still does not select Fracture.

The exact four-T1 control still selects Fracture, with
3.99695198371769 expected applied uses and 767 reachable strict states
represented by three shared executable regions. Its graph retains the safe
product Restart default and exact terminal/accounting result.
