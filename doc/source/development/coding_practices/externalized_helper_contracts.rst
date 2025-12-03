.. _practice-externalized-helper-contracts:

.. meta::
   :description: Pattern for documenting narrow helper contracts when logic is split from larger routines, including precondition guards and defensive fallbacks.
   :keywords: rsyslog, coding practice, helper function, contract, preconditions, assertions

.. summary-start

Document explicit preconditions and defensive fallbacks for helpers that are split out of larger routines so callers cannot misuse narrowly scoped logic.

.. summary-end

Pattern: document contracts for externalized helpers
====================================================

**Classification:** Pattern

**Context:** Helpers that are carved out of larger routines for readability but still depend on the parent’s invariants (for example, parsers and anonymization helpers).

**Why it matters:** Narrow helpers can look reusable but often rely on prevalidated inputs from their parent routine. If the contract is unstated, future callers may bypass required checks, leading to undefined behavior or latent bugs.

**Steps:**

#. Describe the preconditions that the parent function already established (for example, validated offsets, required separators, or buffer boundaries). State that the helper is not a general-purpose utility.
#. Keep any required state checks close to the helper (assertions or ``rs_assert``) so misuse trips fast during development while still providing a compiler-placating return path when assertions are disabled.
#. Document what postconditions the caller can assume on return so downstream code is safe to execute without rechecking the same invariants.
#. Link to the parent routine or a concrete example so reviewers understand the intended call path and can keep contracts in sync when either side changes.

Assert + safe fallback (defensive design-by-contract)
----------------------------------------------------

Prefer a hybrid guard for narrow helpers instead of inlining logic back into a
larger function:

- **Development safety:** Keep an ``assert()`` or ``rs_assert()`` on the
  "impossible" path so contract violations surface immediately in CI and debug
  builds.
- **Production safety:** Follow the assertion with a minimal, sane return value
  (``0``, ``NULL``, or a specific error code) to avoid undefined behavior when
  assertions are compiled out. Accept the small branch as the cost of clearer,
  testable helpers.
- **Analyzer noise:** If a static analyzer calls the fallback dead code, prefer
  suppressing the warning to losing the protective path; do not re-inline the
  helper solely to quiet the tool.

This approach balances readability and safety without forcing callers to carry
extra checks or move the helper back into its parent.

**References:**

- :ref:`practice-embedded-ipv4-tail-helper` — example of documenting a helper that assumes prevalidated offsets inside ``runtime/mmanon.c``.
