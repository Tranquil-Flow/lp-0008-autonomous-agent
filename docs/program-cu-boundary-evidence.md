# Program/CU boundary evidence

This evidence closes the module-level program skill boundary without overclaiming live program submission.

- Evidence dir: `/Users/evinova-self/lp0008-phase0/program_cu_boundary_20260626_202930`
- `program.query` returns simulated mode with explicit unavailable-live-query note.
- `program.call` fails closed with `submitted=false` and `live_program_call_not_available`.
- `program.deploy` fails closed for existing binaries and reports `binary_not_found` for missing binaries.
- CU/proof documentation remains honest: current rc3 wallet output does not expose stable CU cost, and module-level program tx proof is not claimed.
