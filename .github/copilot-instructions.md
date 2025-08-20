# Copilot: Repository Operating Rules

**Mandatory first step (for any task):**
1. Open and read:
   - `AGENTS.md`
   - `DEVELOPING.md`
   - `MODULE_AUTHOR_CHECKLIST.md`
   - `doc/ai/module_map.yaml`
2. Reply with a short readiness summary:
   - 3–5 bullets of key constraints you will follow
   - Which paths/modules the task touches
3. If any file is missing or unreadable, **stop and ask**.

**Guardrails**
- Do **not** paraphrase long passages; quote exact lines and include file paths (and line ranges if available).
- In plans/PRs, **cite** the source file(s) for every rule you apply.
- **Refuse** work that conflicts with these files or bypasses them.

**Style**
- Keep diffs minimal and well-scoped. Add tests/docs when behavior changes.
- Prefer existing conventions stated in the files above. Do not invent new ones.

