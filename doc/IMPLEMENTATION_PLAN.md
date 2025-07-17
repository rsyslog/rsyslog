# Rsyslog Documentation: Implementation & Quality Improvement Plan

**Version: 1.0**

**Related Document:** `STRATEGY.md`

## 1. Introduction

This document is the practical playbook for executing the official Rsyslog Documentation Strategy. It breaks down the high-level strategy into concrete phases and actionable tasks. Its purpose is to guide the development team through the setup, migration, automation, and long-term improvement of the documentation.

---

## 2. Phase 1: Foundational Setup

This phase focuses on creating the new structure and environment.

### Task 2.1: Establish the New Directory Structure

The first step is to create the new, hierarchical directory structure within `doc/source/`. This provides a clean foundation for all subsequent work.

**Action:** Create the following directories and move the existing `.rst` files into their most logical new home. For files that are difficult to categorize, place them in a temporary `_needs_triage` directory.

```

doc/
├── examples/             \# For validated, standalone rsyslog.conf examples
└── source/
├── \_static/              \# For CSS, images, etc.
├── \_templates/           \# For custom HTML templates
├── \_needs\_triage/        \# Temporary home for uncategorized old files
├── 01\_getting\_started/
├── 02\_concepts/
├── 03\_configuration/
├── 04\_modules/
├── 05\_how-to\_guides/
├── 06\_reference/
├── 07\_tutorials/
├── 08\_deployment\_guides/
│   ├── container\_deployment/
│   └── aws\_deployment/
└── man/                    \# For the RST source of man pages

````

### Task 2.2: Configure `conf.py` for the New System

To power our new features, we must update the Sphinx configuration file.

**Action:** Edit `doc/source/conf.py` and ensure the following extensions are added to the `extensions` list.

```python
# doc/source/conf.py

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',  # For linking to external docs
    'sphinx.ext.githubpages',
    'sphinx_mermaid',          # For Mermaid diagrams
    'sphinx_sitemap',          # For generating sitemap.xml
    # 'breathe',               # Add this once Doxygen is set up
]

# Add this configuration for intersphinx
intersphinx_mapping = {
    'agent': ('URL_TO_WINDOWS_AGENT_DOCS', None),
}

# Add this for sitemap generation
html_baseurl = '[https://www.rsyslog.com/doc/](https://www.rsyslog.com/doc/)'

# Add this for the "Edit on GitHub" link
html_context = {
    "display_github": True,
    "github_user": "rsyslog",
    "github_repo": "rsyslog",
    "github_version": "master",
    "conf_py_path": "/doc/source/",
}
````

-----

## 3\. Phase 2: Content Migration and Enhancement

This phase involves moving existing content into the new system and refactoring it to meet our quality standards.

### Task 3.1: Migrating Man Pages to RST

**Goal:** Convert a legacy man page into a Sphinx-manageable RST file.

**Steps:**

1.  **Create RST File:** For `rsyslogd.8`, create a new file at `doc/source/man/rsyslogd.rst`.
2.  **Copy Content:** Copy the raw text content from the old man page into the new RST file.
3.  **Add Metadata:** Add the required metadata fields at the top of the RST file. This is what the man page builder uses to create the file correctly.
    ```rst
    ..
      :manpage_url: [https://www.rsyslog.com/doc/man/rsyslogd.8.html](https://www.rsyslog.com/doc/man/rsyslogd.8.html)

    rsyslogd
    ########

    :program:`rsyslogd` - reliable and extended syslogd

    ..
      This is the rst source for the rsyslogd man page.
      It is generated as part of the rsyslog build process.

    .. program:: rsyslogd

    .. sectionauthor:: Rainer Gerhards <rgerhards@adiscon.com>

    ..
      (rest of man page content follows, formatted in RST)
    ```
4.  **Update Makefile:** Ensure the `Makefile` contains a target to build the man pages.
    ```makefile
    # In your main Makefile
    man:
        @$(SPHINXBUILD) -b man $(ALLSPHINXOPTS) $(BUILDDIR)/man
    ```
5.  **Build and Validate:** Run `make man` and check the output in the `build/man` directory.

### Task 3.2: Building the Validated Example Library

**Goal:** Create a repository of tested, reliable configuration examples.

**Steps:**

1.  **Identify Candidates:** Review the existing rsyslog testbench to identify self-contained, high-value configuration examples.
2.  **Extract and Sanitize:** Copy these configurations into new, clean `.conf` files within the `doc/examples/` directory (e.g., `doc/examples/basic_forwarding.conf`). Remove any testbench-specific scaffolding.
3.  **Create Validation Script:** Create a CI script that iterates through all files in `doc/examples/` and runs `rsyslogd -N1` against them to check for syntax validity.
4.  **Refactor Existing Docs:** Search the documentation for inline `.. code-block::` sections that contain full configurations. Replace them with the `.. literalinclude::` directive pointing to the corresponding validated file.
    ```rst
    .. literalinclude:: ../examples/basic_forwarding.conf
       :language: rsyslog-conf
    ```

-----

## 4\. Phase 3: Automation and CI Integration

This phase involves the core engineering work to automate our quality gates.

### Task 4.1: Implement the "Drift Detector" Linter

**Goal:** Create a CI script that automatically detects technical inaccuracies in the documentation.

**Specification:**

  * **Language:** Python (recommended, for ease of integration with Sphinx).
  * **Input:**
    1.  An "authoritative spec" file (`spec.json`) generated by the code analysis agent, containing a list of all valid parameters and modules.
    2.  The set of all `.rst` files in the `doc/source/` directory.
  * **Process:**
    1.  The script reads `spec.json` into memory.
    2.  It iterates through each `.rst` file.
    3.  It uses regular expressions to find all potential rsyslog parameter names (e.g., `\$[a-zA-Z0-9_]+`, `[a-zA-Z]+\s*\(.*?\)`).
    4.  For each term found, it checks if the term exists in the `spec.json`.
    5.  If an unknown term is found, the script prints an error message indicating the term and the file/line number where it was found, then exits with a non-zero status code, failing the CI build.

### Task 4.2: Implement the Code Analysis Agent

**Goal:** Use Doxygen and Breathe to generate reference documentation directly from C code comments.

**Steps:**

1.  **Define Comment Standard:** Establish a mandatory, standardized Doxygen comment block format for all developers to use. (See **Appendix B**).
2.  **Configure Doxygen:** Create a `Doxyfile` configuration. Key settings:
      * `GENERATE_XML = YES`
      * `GENERATE_HTML = NO`
      * `GENERATE_LATEX = NO`
      * Set `INPUT` to the relevant source code directories.
3.  **Integrate with Breathe:**
      * Add `breathe` to the `extensions` list in `conf.py`.
      * Configure the path to the Doxygen XML output: `breathe_projects = { "rsyslog": "path/to/doxygen/xml/" }`
4.  **Create RST Stubs:** In the `06_reference/` directory, create stub `.rst` files that use Breathe directives to pull in the documentation.
    ```rst
    // In doc/source/06_reference/imfile.rst

    Module: imfile
    ==============

    .. doxygenmodule:: imfile
       :members:
       :undoc-members:
    ```
5.  **Generate `spec.json`:** The Doxygen XML output is machine-readable. Create a simple script that parses this XML and generates the `spec.json` file needed by the Drift Detector linter.

### Task 4.3: Create the Full Documentation CI Pipeline

**Goal:** A complete GitHub Actions workflow that automates all checks.

**Action:** Create a file at `.github/workflows/docs.yml`.

```yaml
name: 'Documentation CI'

on:
  pull_request:
    paths:
      - 'doc/**'
      - '.github/workflows/docs.yml'

jobs:
  build_and_test_docs:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout repository
        uses: actions/checkout@v4

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.x'

      - name: Install dependencies
        run: |
          python -m pip install -r doc/requirements.txt
          # Add other dependencies like doxygen, etc.

      - name: Generate Authoritative Spec
        run: |
          # Command to run doxygen and the spec generator script
          make doc-spec

      - name: Validate Examples
        run: |
          # Command to run the example validation script
          python doc/ci/validate_examples.py

      - name: Run Drift Detector Linter
        run: |
          # Command to run the drift detector
          python doc/ci/drift_detector.py

      - name: Build Sphinx Docs
        run: |
          make -C doc html
```

-----

## 5\. Phase 4: Iterative Quality Improvement

This phase is a continuous, long-term process.

### Task 5.1: The RST Quality Rubric

Use this checklist when refactoring an old documentation file to meet the new standard.

  * [ ] **Headings:** Are the heading levels consistent with the style guide (See **Appendix A**)?
  * [ ] **Semantic Markup:** Have plain backticks (`` ` ``) been replaced with specific semantic roles where appropriate (e.g., `:program:`, `:file:`, `:option:`)?
  * [ ] **Cross-References:** Are all mentions of other modules, concepts, or sections linked using the `:ref:` or `:doc:` role?
  * [ ] **Examples:** Are all configuration examples either short, illustrative snippets or replaced with a `.. literalinclude::` directive pointing to a validated example?
  * [ ] **Clarity:** Is the language clear, concise, and unambiguous?

### Task 5.2: Prioritization Strategy

It's impossible to refactor everything at once. Prioritize the work in this order:

1.  **High-Traffic Pages:** Start with the Getting Started guide, core configuration files, and the most popular modules.
2.  **Community Pain Points:** Address pages that are frequently mentioned in GitHub issues or community forums as being confusing or incorrect.
3.  **Core Concepts:** Ensure the files in `02_concepts/` are crystal clear, as they form the foundation of user understanding.

-----

## 6\. Appendices

### Appendix A: Recommended RST Style Guide

  * **Page Title (H1):** `################` (over and under)
  * **Chapter (H2):** `****************` (over and under)
  * **Section (H3):** `================` (underline only)
  * **Subsection (H4):** `----------------` (underline only)
  * **Line Length:** Wrap all prose at 88 characters to improve readability in code editors.

### Appendix B: Sample Standardized Code Comment (Doxygen)

All new functions and configuration parameters in the C code should use this format.

```c
/**
 * @brief A brief, one-sentence description of the parameter.
 *
 * A more detailed, multi-sentence description can follow here.
 * It should explain what the parameter does, its purpose, and any
 * important side effects or interactions.
 *
 * @param[in] name The name of the parameter. (Example for a function)
 *
 * @default "default_value"
 * @module-type output
 * @supported-since 8.2404.0
 * @see another_function()
 *
 * @return A description of the return value.
 */
```
