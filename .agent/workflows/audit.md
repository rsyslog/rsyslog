---
description: Perform a comprehensive, multi-pass AI-driven review of current code changes.
---

// turbo-all

1.  **Gather Changes**: Run `git diff` to identify all uncommitted changes.
2.  **Specialized Persona Audits**:
    -   **Pass 1: The Memory Guardian (Ownership & Leaks)**:
        - Persona: Senior C Security Auditor.
        - Canned Prompt: `ai/rsyslog_memory_auditor/base_prompt.txt`
        - Focus: `RS_RET` paths, `es_str2cstr()` NULL checks, `strdup/free` balance, `CHKmalloc` usage.
    -   **Pass 2: The Concurrency Architect (Races & Lock Logic)**:
        - Persona: Systems Architect.
        - Canned Prompt: `ai/rsyslog_bug_finder/base_prompt.txt`
        - Focus: Data races, lock balance, structured path analysis, counterexamples for non-OK findings.
    -   **Pass 3: The Project Standards Guard (Build & Distribution)**:
        - Persona: rsyslog Maintainer.
        - Focus: 
            - `tests/Makefile.am`: "Define at Top, Distribute Unconditionally, Register Conditionally" pattern.
            - `EXTRA_DIST` completeness.
            - RainerScript syntax correctness (rsyslog Assistant mode from `ai/support_gpt/base_prompt-gpt4.txt`).
    -   **Pass 4: The Structural & Formatting Auditor**:
        - Persona: Documentation & Quality Engineer.
        - Focus: 
            - Check for duplicate section numbering (e.g., two "### 6.").
            - Check for redundant or duplicate guidance lines (e.g., identical `- Focus:` bullets).
            - Verify all file links and internal cross-references are valid.
3.  **Adversarial Pass (Simulation)**:
    - Persona: Adversarial Tester.
    - Final Pass: Assume a different set of constraints (e.g., "Review this as if you are a strict static analysis tool like Clang Tidy or Coverity") to find any lingering edge cases.
4.  **Consolidated Report**:
    - **CRITICAL**: Functional bugs, leaks, or failing distribution rules (VPATH failures).
    - **ADVISORY**: Pattern improvements, macro suggestions (`CHKmalloc`), or style nits.
    - **CLEAN**: Verification of correctly implemented rsyslog patterns.
