Common Configuration Mistakes and Misunderstandings
====================================================

This section documents syntax patterns that are often misunderstood,
invalid, or misused in rsyslog configurations — especially when transitioning
from legacy syntax to modern RainerScript.

It is intended to help both human users and automated tools avoid common traps
when writing or validating rsyslog configuration files.

.. list-table::
   :header-rows: 1
   :widths: 20 20 35 25

   * - Incorrect Syntax
     - Legacy Meaning
     - Why It's Invalid or Misleading
     - Correct Form (RainerScript)

   * - ``& stop``
     - Shorthand: same selector + stop message processing
     - Only valid in legacy syntax. ``&`` means "same selector". Not allowed in RainerScript.
     - Use:

       .. code-block:: rainerscript

          if <condition> then {
              action(...)
              stop
          }

   * - ``stop()``
     - Mistaken as a function call
     - Not a function; `stop` is a **bare statement** in RainerScript
     - Just use ``stop``

   * - ``& ~``
     - Discard message with same selector
     - Legacy shorthand only. ``~`` means discard. ``&`` invalid in RainerScript.
     - Use:

       .. code-block:: rainerscript

          if <condition> then {
              stop
          }

   * - ``action(type="stop")``
     - Assumes "stop" is an action module
     - There is no such module. ``stop`` is a statement, not an action.
     - Use bare ``stop`` as a control flow statement

   * - ``if (...) then stop();``
     - Combines multiple errors: function call and syntax mismatch
     - Parentheses are invalid, and ``stop`` is not a function
     - Use ``if (...) then stop`` or wrap in a block as needed

.. note::

   These mistakes are common in AI-generated configs or user translations
   from legacy examples. Always validate your configuration with:

   .. code-block:: bash

      rsyslogd -N1

