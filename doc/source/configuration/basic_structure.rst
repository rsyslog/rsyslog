Basic Structure
===============

This page introduces the core concepts and structure of rsyslog configuration.
Rsyslog is best thought of as a **highly extensible logging and event
processing framework**. While the general message flow is fixed, almost every
aspect of its processing pipeline can be customized by configuring rsyslog
objects.

Message Flow Overview
---------------------

At a high level, rsyslog processes messages in three main stages:

1. **Inputs**: Messages are received from input modules (e.g., `imuxsock` for
   system logs or `imtcp` for TCP-based logging).

2. **Rulesets**: Each message is passed through one or more rulesets. A
   ruleset contains a sequence of rules, each with a filter and associated
   actions.

3. **Actions (Outputs)**: When a rule matches, its actions are executed.
   Actions determine what to do with the message: write to a file, forward to a
   remote server, insert into a database, or other processing.

.. mermaid::

   flowchart LR
       A[Input Modules] --> B[Rulesets]
       B --> C[Rules: Filter and Action List]
       C --> D[Actions and Outputs]
       D --> E[File, DB, Remote,<br>Custom Destinations]

Processing Principles
---------------------

Key rules to understand the rsyslog processing model:

- **Rulesets as Entry Points**:
  
  Inputs submit messages to a ruleset.  
  If no ruleset is explicitly bound, the default ruleset
  (`RSYSLOG_DefaultRuleset`) is used.  
  Additional, custom rulesets can be defined.

- **Rules within Rulesets**:

  A ruleset contains zero or more rules (though zero rules makes no sense).  
  Rules are evaluated **in order, top to bottom**, within the ruleset.

- **Filter and Action Lists**:

  A rule consists of a **filter** (evaluated as true/false) and an
  **action list**.  
  When a filter matches, the action list is executed.  
  If a filter does not match, rsyslog continues with the next rule.

- **Full Evaluation**:

  All rules are evaluated unless message processing is explicitly stopped
  by a ``stop`` command.  ``stop`` immediately halts processing for that message.

- **Action Lists**:

  An action list can contain one or multiple actions.  
  Actions are executed sequentially.  
  Actions have no filters inside the same list (filters apply only at the
  rule level).

.. mermaid::

   flowchart TD
       Start([Message Entered]) --> CheckFilter1{Filter 1 Match?}
       CheckFilter1 -- Yes --> Action1[Execute Action 1]
       CheckFilter1 -- No --> CheckFilter2{Filter 2 Match?}
       CheckFilter2 -- Yes --> Action2[Execute Action 2]
       CheckFilter2 -- No --> End([Processing Ends])
       Action1 --> CheckFilter2
       Action2 --> End

Configuration File
------------------

By default, rsyslog loads its configuration from ``/etc/rsyslog.conf``.
This file may include other configuration snippets (commonly under
``/etc/rsyslog.d/``).

To specify a different configuration file, use:

.. code-block:: bash

   rsyslogd -f /path/to/your-config.conf

Supported Configuration Formats
-------------------------------

Rsyslog historically supported three configuration syntaxes:

1. **RainerScript (modern style)** – **recommended and actively maintained**  

   This is the current, fully supported configuration format. It is
   clean, structured, and best suited for new and complex configurations.

2. **sysklogd style (legacy)** – **deprecated for new configs**  

   This format is widely known and still functional, but **hard to grasp for
   new users**. It remains an option for experienced admins who know it
   well or need to maintain older configurations. Example:

   .. code-block:: none

      mail.info    /var/log/mail.log
      mail.err     @server.example.net

3. **Legacy rsyslog style (dollar-prefix)** – **deprecated**  

   This format, with directives starting with `$` (e.g.,
   `$ActionFileDefaultTemplate`), is fully supported for backward
   compatibility but **not recommended for any new configuration**.

Why Prefer RainerScript?
~~~~~~~~~~~~~~~~~~~~~~~~

RainerScript is easier to read and maintain, avoids side effects with
include files, and supports modern features such as structured filters,
templates, and complex control flow.

**For new configurations, always use RainerScript.**  
Legacy formats exist only for compatibility with older setups and
distributions.

Example RainerScript rule:

.. code-block:: rsyslog

   if $syslogfacility-text == 'mail' and $syslogseverity-text == 'err' then {
       action(type="omfile" file="/var/log/mail-errors.log")
   }

Comments
--------

Rsyslog supports:

- **# Comments** — start with `#` and extend to the end of the line.
- **C-style Comments** — start with `/*` and end with `*/`.  
  These can span multiple lines but cannot be nested.

Processing Order
----------------

- Directives are processed **in order from top to bottom** of the
  configuration.
- Once a message is stopped via ``stop`` subsequent statements
  will not be evaluated for that message.

Flow Control
~~~~~~~~~~~~

- Control structures (if/else, etc.) are available in RainerScript.
- Filters (e.g., `prifilt()`) provide conditional matching for messages.

See :doc:`../rainerscript/control_structures` and :doc:`filters` for details.

Data Manipulation
~~~~~~~~~~~~~~~~~

Data can be modified using the `set`, `unset`, and `reset` statements.
For details, refer to :doc:`../rainerscript/variable_property_types`.

Inputs
------

- Each input requires a dedicated input module.
- Inputs are defined using the `input()` object after loading the module.

Example:

.. code-block:: rsyslog

   module(load="imtcp")                   # Load TCP input module
   input(type="imtcp" port="514")         # Listen on TCP port 514

See :doc:`modules/index` for the full list of input modules.

Outputs (Actions)
-----------------

- Actions are responsible for output, such as writing to files, databases,
  or forwarding to other systems.
- Actions are configured with the `action()` object.

Example:

.. code-block:: rsyslog

   action(type="omfile" file="/var/log/messages")   # Write to local file

Rulesets and Rules
------------------

- A **ruleset** acts like a "program" for message processing.
- A ruleset can be bound to specific inputs or used as the default.

Example:

.. code-block:: rsyslog

   ruleset(name="fileLogging") {
       if prifilt("*.info") then {
           action(type="omfile" file="/var/log/info.log")
       }
   }

.. mermaid::

   graph TD
       Input1[Input: imtcp] --> Ruleset1
       Input2[Input: imudp] --> Ruleset2
       Ruleset1 --> Action1[omfile]
       Ruleset2 --> Action2[omfwd]
       Ruleset2 --> Action3[omelasticsearch]

For details, see :doc:`../concepts/multi_ruleset`.

