Documentation Style Guide
=========================

This guide defines the standards for writing and maintaining rsyslog
documentation. It ensures a consistent, professional style across all
`.rst` files and helps contributors produce high-quality documentation.

General Principles
------------------

- **Audience:** Primarily system administrators and developers. Content
  should also remain accessible to intermediate users.
- **Tone:** Professional, clear, and concise. Avoid personal references
  or conversational language.
- **Focus:** Prioritize actionable guidance (e.g., how to configure,
  troubleshoot, or understand rsyslog).
- **Language:** Use plain English. Avoid jargon and colloquialisms.
- **Consistency:** Every page should follow the same structure,
  terminology, and heading style.

File Structure and TOC
----------------------

- **Master TOC:** The `index.rst` file is the single entry point. Avoid
  using filesystem order.
- **Preferred Topic Order:**

  1. Introduction / Quick Start
  2. Configuration Basics
  3. Modules & Advanced Topics
  4. Troubleshooting
  5. Reference & API
  6. Community & Support
  7. Legacy Resources

- Use explicit ordering in `.. toctree::` (avoid `:glob:`).

Headings
--------

Use underline-only adornments (no overlines). Assign a character per
level:

- `=` Level 1 (document title)
- `-` Level 2
- `~` Level 3
- `^` Level 4
- `"` Level 5
- `+` Level 6

Keep titles concise (≤ 60 characters).

Text Formatting
---------------

- **Line Length:** Wrap lines at **80 characters**.
- **Code Blocks:** Use `::` or explicit code blocks (e.g.,
  `.. code-block:: bash`).
- **Inline Code:** Use double backticks, e.g., ``rsyslog.conf``.
- **Links:** Use inline links:
  `rsyslog mailing list <http://lists.adiscon.net/mailman/listinfo/rsyslog>`_ .
- **Bold vs. Italic:**
  - **Bold:** For UI elements or emphasized terms.
  - *Italic:* For alternative terms or emphasis.

Cross-Referencing
-----------------

- Use `:ref:` for internal cross-links, e.g.,
  ``See :ref:`troubleshooting`.``.
- Use consistent anchors, e.g.,
  ``.. _troubleshooting:``.

Sections
--------

**Introduction/Overview**
- Describe what the feature/module is and why it is needed.
- Include minimal examples.

**Configuration**

- Use rainerscript syntax for modern examples.
- Comment key parameters.

**Support & Community**

- Recommend modern resources first: AI assistant, GitHub Discussions,
  mailing list.
- Move outdated resources to *Legacy Resources*.


Legacy Content
--------------

- Archive outdated guides and tutorials in `legacy/`.
- Use:

  .. code-block:: rst

     .. note::
        This content is archived and may be outdated.

Licensing & Legal
-----------------

- Keep licensing information in `licensing.rst`.
- Use:
  ``For license details, see :doc:`licensing`.``

Examples and Code
-----------------

- Provide minimal, complete examples.
- Always specify language:

  .. code-block:: bash

     sudo systemctl restart rsyslog

Maintenance
-----------

- Review docs at every release for outdated content.
- Check for deprecated parameters and move them to Legacy.
- Run `make linkcheck` to verify links.
