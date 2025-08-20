---
applyTo: "plugins/**"
---

# Copilot: Module development (plugins/**)

**Before editing:**
- Read `AGENTS.md`, `MODULE_AUTHOR_CHECKLIST.md`, and `doc/ai/module_map.yaml`.
- Confirm module placement and naming align with `module_map.yaml`.

**When changing code:**
- Preserve public behavior unless the issue/task says otherwise.
- Update module docs under `doc/source/configuration/modules/` if parameters or semantics change.
- Include clear commit messages and minimal, focused diffs.
- Add/adjust tests where applicable; keep build green.

**Do not:**
- Introduce new config styles if an established one exists.
- Skip docs or checklist steps referenced above.
