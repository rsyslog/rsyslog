.. _yaml_config:

YAML Configuration Format
=========================

.. meta::
   :description: rsyslog YAML configuration format reference: schema, activation, ruleset scripting, templates, and relationship to RainerScript.
   :keywords: yaml, configuration, rsyslog, rainerscript, yamlconf, yaml config

.. summary-start

rsyslog supports YAML as an alternative configuration syntax for users comfortable with YAML. Every YAML key maps directly to the equivalent RainerScript parameter.

.. summary-end

rsyslog supports configuration via YAML files as an alternative to
RainerScript.  It is one of the :doc:`supported configuration formats
<conf_formats>` and is a good choice if you are more comfortable with
YAML syntax than with RainerScript.

The two formats are equivalent: every YAML configuration key maps
directly to a RainerScript parameter of the same name, so all per-module
documentation remains applicable unchanged.  You can also mix formats:
a YAML main config may include RainerScript ``.conf`` fragments and vice
versa.

Activation
----------

rsyslog automatically selects the YAML loader whenever the configuration
file name ends in ``.yaml`` or ``.yml``.  No special flag or directive is
required:

.. code-block:: shell

   rsyslogd -f /etc/rsyslog.yaml

The same detection applies to files pulled in via ``include()`` (from a
RainerScript config) or the ``include:`` section of a YAML config.  It is
therefore valid to mix formats:  a YAML main config may include ``.conf``
RainerScript fragments and vice versa.

.. note::
   YAML configuration support requires rsyslog to have been compiled with
   libyaml (``yaml-0.1``).  If libyaml is absent at build time the daemon
   will print an error and refuse to load a ``.yaml`` / ``.yml`` file.
   Install the ``libyaml-dev`` (Debian/Ubuntu) or ``libyaml-devel``
   (RHEL/Fedora) package before compiling.

Schema Overview
---------------

A YAML config file is a single YAML mapping (dictionary) whose top-level
keys correspond to rsyslog configuration object types:

.. code-block:: yaml

   version: 2          # optional, informational only

   global:   { ... }
   modules:  [ ... ]
   inputs:   [ ... ]
   templates: [ ... ]
   rulesets: [ ... ]
   mainqueue: { ... }
   include:  [ ... ]
   parsers:  [ ... ]
   lookup_tables: [ ... ]
   dyn_stats: [ ... ]
   perctile_stats: [ ... ]   # note: spelt "perctile" — matches rsyslog internal naming
   ratelimits: [ ... ]
   timezones: [ ... ]

All section names are case-sensitive.  Unknown top-level keys are logged
as errors and ignored.  Each top-level key **must appear at most once**;
duplicate keys are undefined behaviour in the YAML specification and
unsupported by rsyslog — a warning is logged if a duplicate is detected.
To specify multiple items of the same type use a sequence under one key,
or use ``include:`` for file-level composition.

Singleton Sections
------------------

``global`` and ``mainqueue`` are singleton sections: their value is a
mapping of parameter names to values.

.. code-block:: yaml

   global:
     workdirectory: "/var/spool/rsyslog"
     maxmessagesize: 8192
     defaultnetstreamdriver: "gtls"
     privdrop.user.name: "syslog"
     privdrop.group.name: "syslog"

   mainqueue:
     type: linkedlist
     size: 100000
     dequeuebatchsize: 1024

Parameter names are identical to their RainerScript counterparts (see
:doc:`global configuration parameters <global/index>` and
:doc:`queue parameters <../rainerscript/queue_parameters>`).

Sequence Sections
-----------------

``modules``, ``inputs``, ``templates``, ``rulesets``, ``parsers``,
``lookup_tables``, ``dyn_stats``, ``perctile_stats``, ``ratelimits``, and
``timezones`` are sequences: each list element is a mapping that describes
one object of that type.

**modules**

Each item must contain a ``load`` key (the module name).  Additional keys
supply module-level parameters:

.. code-block:: yaml

   modules:
     - load: imuxsock
     - load: imklog
     - load: imtcp
       threads: 2
     - load: imudp

**inputs**

Each item must contain a ``type`` key.  All other keys are input instance
parameters:

.. code-block:: yaml

   inputs:
     - type: imtcp
       port: "514"
       ruleset: main
     - type: imudp
       port: "514"
       ruleset: main
     - type: imfile
       file: "/var/log/app/*.log"
       tag: "applog"
       ruleset: main

**templates**

Four template types are supported:

*String template* — the most common type; a format string with ``%PROPERTY%``
placeholders:

.. code-block:: yaml

   templates:
     - name: myFormat
       type: string
       string: "%HOSTNAME% %syslogfacility-text%.%syslogseverity-text% %msg%\n"
     - name: filePerHost     # use with omfile dynafile: filePerHost
       type: string
       string: "/var/log/hosts/%HOSTNAME%.log"

*Subtree template* — serialises a JSON sub-tree of the message object:

.. code-block:: yaml

   templates:
     - name: jsonApp
       type: subtree
       subtree: "$!app"

*List template* — assembles output from an explicit sequence of ``property``
and ``constant`` items.  Each element maps to a ``property()`` or
``constant()`` call; all parameters accepted by those RainerScript
functions can be used:

.. code-block:: yaml

   templates:
     - name: listFmt
       type: list
       elements:
         - property:
             name: timestamp
             dateformat: rfc3339
         - constant:
             value: " "
         - property:
             name: hostname
         - constant:
             value: " "
         - property:
             name: msg
             droplastlf: "on"
         - constant:
             value: "\n"

*Plugin template* — delegates formatting to an external string-generator
plugin (the ``type="plugin"`` form from RainerScript):

.. code-block:: yaml

   templates:
     - name: myPlugin
       type: plugin
       plugin: strgen_addprimary

**rulesets**

Ruleset items specify a ``name`` and may carry queue parameters.  Rule
logic is expressed in one of three ways:

1. **Structured shortcut** (``filter:`` + ``actions:``) — for simple
   priority- or property-based routing without scripting.  See
   `Structured Filter Shortcut`_ below.
2. **YAML-native statements** (``statements:`` key) — for conditional
   routing, ``if/then/else``, ``call``, ``call_indirect``, ``foreach``,
   ``stop``, ``unset``, variable assignment — without writing any
   RainerScript syntax.  See `YAML-Native Statements`_ below.
   **Recommended for most configs.**
3. **Inline RainerScript** (``script:`` key) — escape hatch for complex
   metaprogramming or advanced RainerScript not covered by ``statements:``.
   See `Scripting`_ below.

``filter:`` + ``actions:`` example:

.. code-block:: yaml

   rulesets:
     - name: main
       filter: "*.*"
       actions:
         - type: omfile
           file: "/var/log/syslog"

     - name: errors
       filter: "*.err"
       actions:
         - type: omfile
           file: "/var/log/errors.log"
         - type: omfwd
           target: "logserver.example.com"
           port: "514"
           protocol: "tcp"

     # Property filter — starts with ':'
     - name: tag_filter
       filter: ":syslogtag, startswith, \"myapp\""
       actions:
         - type: omfile
           file: "/var/log/myapp.log"

     # No filter — unconditional routing
     - name: archive
       actions:
         - type: omfile
           file: "/var/log/archive.log"

``script:`` example (complex logic):

.. code-block:: yaml

   rulesets:
     - name: errors
       script: |
         if $syslogseverity <= 3 then {
           action(type="omfile" file="/var/log/errors.log")
         }
         stop

**parsers**

Custom parser chains (loaded via ``lmregdups``, ``pmrfc3164``, etc.).
Each item must contain a ``name`` key and the parser-specific parameters:

.. code-block:: yaml

   parsers:
     - name: pmrfc3164.default
       force.tagEndingByColon: "on"

**lookup_tables**

In-memory lookup tables loaded from JSON files.  The ``name`` and
``filename`` keys are required.

.. code-block:: yaml

   lookup_tables:
     - name: hostmap
       filename: "/etc/rsyslog.d/hostmap.json"
       reloadOnHUP: "on"

**timezones**

Named timezone definitions for use in template date formatting:

.. code-block:: yaml

   timezones:
     - id: "CET"
       offset: "+01:00"

**dyn_stats, perctile_stats, ratelimits**

These advanced sections follow the same key=value mapping pattern.
Refer to the respective :doc:`module documentation <../index>` for
available parameters.  Example:

.. code-block:: yaml

   dyn_stats:
     - name: "msg_rate"
       resetsIntervals: 60

   ratelimits:
     - name: "input_rl"
       interval: 5
       burst: 1000


Arrays
------

Parameters that accept a list of values are expressed as YAML sequences:

.. code-block:: yaml

   global:
     environment:
       - "TZ=UTC"
       - "LC_ALL=C"

Including Other Files
---------------------

The ``include:`` section accepts a sequence of items.  Each item must have
a ``path`` key (which may contain shell globs) and an optional ``optional``
flag:

.. code-block:: yaml

   include:
     - path: "/etc/rsyslog.d/*.yaml"
       optional: "on"
     - path: "/etc/rsyslog.d/*.conf"
       optional: "on"
     - path: "/etc/rsyslog.d/tls.conf"

If ``optional`` is ``on`` (or ``yes`` or ``1``) a missing file or empty
glob result is not an error.  Files with a ``.yaml`` or ``.yml`` extension
are loaded by the YAML loader; all other files are loaded by the
RainerScript parser.

.. note::
   **Include ordering with mixed file types**: within one ``include:``
   list, ``.yaml`` files are always loaded before ``.conf`` files,
   regardless of their order in the list.  This is an architectural
   constraint — YAML sub-files are processed immediately and recursively,
   while ``.conf`` files are deferred to the RainerScript flex-buffer
   stack.  If strict ordering between ``.yaml`` and ``.conf`` fragments
   matters, use separate ``include:`` sections or consolidate to one
   file type.  Multiple ``.conf`` entries within the same list ARE
   processed in document order.

.. _yaml_filter_shortcut:

Structured Filter Shortcut
--------------------------

For common routing patterns (match by severity or message property, then
write to one or more outputs) you do not need any RainerScript.  Use
``filter:`` and ``actions:`` directly on a ruleset:

``filter:``
   Optional string.  Two forms are recognised:

   * **Priority filter** — standard syslog selector syntax
     (e.g. ``*.info``, ``kern.warn``, ``auth,authpriv.*``).
   * **Property filter** — starts with ``:``
     (e.g. ``:msg, contains, "error"``).

   If ``filter:`` is absent, all messages match (unconditional routing).

``actions:``
   YAML sequence.  Each item maps directly to a ``action()`` call; the
   ``type`` key names the output module and all remaining keys are
   module-specific parameters.  Multiple actions are chained in order.

.. code-block:: yaml

   rulesets:
     # Route *.info to syslog; *.err also to a dedicated error file
     - name: main
       filter: "*.info"
       actions:
         - type: omfile
           file: "/var/log/syslog"

     - name: errors
       filter: "*.err"
       actions:
         - type: omfile
           file: "/var/log/errors.log"
         - type: omfwd
           target: "logserver.example.com"
           port: "514"
           protocol: "tcp"

     # Property filter
     - name: myapp
       filter: ":syslogtag, startswith, \"myapp\""
       actions:
         - type: omfile
           file: "/var/log/myapp.log"

.. note::
   ``filter:`` and ``actions:`` are mutually exclusive with ``script:`` and
   ``statements:``.
   If you need conditionals, loops, ``set``, ``call``, or ``stop``,
   use ``statements:`` (recommended) or ``script:`` instead.

.. _yaml_statements:

YAML-Native Statements
----------------------

The ``statements:`` key is the recommended way to express conditional routing
and control flow without writing raw RainerScript.  Each item in the sequence
is a YAML mapping that represents one statement.  Only the *filter expression*
inside ``if:`` remains as a RainerScript expression string — all structural
elements and action parameters are expressed as YAML.

``statements:`` is mutually exclusive with ``script:``, ``filter:``, and
``actions:``.

**If / action (single action shorthand)**

.. code-block:: yaml

   rulesets:
     - name: main
       statements:
         - if: '$msg contains "error"'
           action:
             type: omfile
             file: "/var/log/errors.log"
           else:              # optional
             - stop: true

**If / then / else (multiple statements per branch)**

.. code-block:: yaml

   rulesets:
     - name: main
       statements:
         - if: '$syslogseverity <= 3'
           then:
             - type: omfile
               file: "/var/log/critical.log"
             - type: omfwd
               target: "logserver.example.com"
               port: "514"
               protocol: "tcp"
           else:
             - type: omfile
               file: "/var/log/messages"

**Control flow: stop, continue, call**

.. code-block:: yaml

         - stop: true        # stop processing this message
         - continue: true    # continue (no-op, rarely needed)
         - call: other_rs    # call another ruleset by name

**Variable assignment**

.. code-block:: yaml

         - set:
             var: "$.nbr"
             expr: 'field($msg, 58, 2)'

**Supported statement types**

+----------------------+-------------------------------------------------------------+
| Type                 | YAML form                                                   |
+======================+=============================================================+
| Action               | ``{type: module, param: val, ...}`` — unconditional         |
+----------------------+-------------------------------------------------------------+
| If/action            | ``{if: expr, action: {type: ..., ...}, else: [...]}``       |
+----------------------+-------------------------------------------------------------+
| If/then              | ``{if: expr, then: [...], else: [...]}``                    |
+----------------------+-------------------------------------------------------------+
| Stop                 | ``{stop: true}``                                            |
+----------------------+-------------------------------------------------------------+
| Continue             | ``{continue: true}``                                        |
+----------------------+-------------------------------------------------------------+
| Call                 | ``{call: rulesetname}``                                     |
+----------------------+-------------------------------------------------------------+
| Set                  | ``{set: {var: "$.x", expr: "rainerscript-expression"}}``    |
+----------------------+-------------------------------------------------------------+
| Unset                | ``{unset: "$.x"}``                                          |
+----------------------+-------------------------------------------------------------+
| call_indirect        | ``{call_indirect: "$.varname"}``                            |
+----------------------+-------------------------------------------------------------+
| foreach              | ``{foreach: {var: "$.i", in: "$!arr", do: [...]}}``         |
+----------------------+-------------------------------------------------------------+
| reload_lookup_table  | ``{reload_lookup_table: {table: name, stub_value: val}}``   |
+----------------------+-------------------------------------------------------------+

``foreach`` iterates over a JSON array.  The ``do:`` value is a sequence of
statement items using the same syntax as ``statements:``.  Example:

.. code-block:: yaml

   statements:
     - foreach:
         var: "$.item"
         in: "$!items"
         do:
           - type: omfile
             file: /var/log/items.log
             template: outfmt

.. _yaml_scripting:

Scripting
---------

RainerScript filter expressions and statement types (``if/then/else``,
``set``, ``unset``, ``foreach``, ``stop``, ``call``, legacy PRI-filters,
property-filters, ``action()``) are all available inside a ``script:``
block.  The value is an ordinary YAML scalar (use a
`YAML block scalar <https://yaml.org/spec/1.2.2/#81-block-scalar-styles>`_
for multi-line content):

.. code-block:: yaml

   rulesets:
     - name: main
       script: |
         # Route by severity
         if $syslogseverity <= 3 then {
           action(type="omfile" file="/var/log/critical.log")
           action(type="omfwd"  target="logserver.example.com" port="514"
                                protocol="tcp")
         }

         # Discard noisy debug messages
         if $syslogseverity == 7 then stop

         # Default sink
         action(type="omfile" file="/var/log/messages")

The script content is passed verbatim to the RainerScript lexer/parser,
so anything that is valid RainerScript in a ``ruleset() {}`` body is valid
here.  Inline actions defined in the ``script:`` block do **not** need to
be listed separately under an ``inputs:`` or ``actions:`` section.

.. tip::
   For simple routing (one filter, one or more actions, no branching)
   use the ``filter:`` + ``actions:`` shortcut described in
   `Structured Filter Shortcut`_.
   For conditional routing, variable assignments, ``call``, ``foreach``,
   ``stop``, and ``reload_lookup_table`` use the ``statements:`` block
   described in `YAML-Native Statements`_.  **Recommended for most configs.**
   Reserve ``script:`` only for advanced RainerScript that cannot be
   expressed through ``statements:``, such as complex nested legacy
   priority/property filters or inline ``call_direct`` patterns.

Complete Example
----------------

The following is a near-complete rsyslog YAML configuration equivalent to
a typical ``/etc/rsyslog.conf``:

.. code-block:: yaml

   version: 2

   global:
     workdirectory: "/var/spool/rsyslog"
     maxmessagesize: 8192
     privdrop.user.name: "syslog"
     privdrop.group.name: "adm"

   modules:
     - load: imuxsock
     - load: imklog
     - load: imtcp
       threads: 2

   inputs:
     - type: imtcp
       port: "514"
       ruleset: main
     - type: imuxsock

   templates:
     - name: fileFormat
       type: string
       string: "%TIMESTAMP% %HOSTNAME% %syslogtag%%msg%\n"

   mainqueue:
     type: linkedlist
     size: 100000

   rulesets:
     - name: main
       filter: "auth,authpriv.*"
       actions:
         - type: omfile
           file: "/var/log/auth.log"

     - name: main_all
       filter: "*.*;auth,authpriv.none"
       actions:
         - type: omfile
           file: "/var/log/syslog"

     - name: kern
       filter: "kern.*"
       actions:
         - type: omfile
           file: "/var/log/kern.log"

     - name: emerg
       filter: "*.emerg"
       actions:
         - type: omusrmsg
           users: "*"

   include:
     - path: "/etc/rsyslog.d/*.yaml"
       optional: "on"
     - path: "/etc/rsyslog.d/*.conf"
       optional: "on"

Relationship to RainerScript
-----------------------------

YAML configuration is a thin translation front-end over the same internal
machinery that RainerScript uses.  The YAML parser (``runtime/yamlconf.c``)
converts each YAML block into ``cnfobj`` + ``nvlst`` structures — the identical
intermediate representation that the RainerScript lex/bison grammar produces —
and hands them to the shared ``cnfDoObj()`` dispatcher.  There is no independent
YAML runtime; the shared back-end handles all validation, module initialisation,
and execution.

This approach deliberately minimises the change surface: the YAML-specific code
is confined to one file with no runtime presence after configuration loading.
It may be refactored in the future, but only when concrete requirements justify
the additional maintenance surface.

In practical terms:

- Parameter names are identical; all per-module parameter documentation
  applies without change.
- Type coercion (integers, sizes, binary flags, UIDs, …) is performed
  by the same ``nvlstGetParams()`` layer after YAML parsing.
- Unknown parameter names produce the same "unused parameter" error
  regardless of which format was used.
- Template, ruleset, and lookup-table names are shared; a YAML-defined
  ruleset can be referenced from a RainerScript ``action()``.

For a detailed description of the pipeline see
:doc:`yaml_config_architecture <../development/yaml_config_architecture>`.

Limitations (current implementation)
--------------------------------------

- Nested YAML mappings inside parameter values are not supported.  Use a
  YAML scalar or sequence.
- ``filter:`` + ``actions:`` support one flat filter level.  For nested
  ``if/then/else`` chains use ``statements:`` (recommended) or ``script:``.
- ``version:`` is accepted but not enforced; it is reserved for future
  schema evolution.
- Within a single ``include:`` list, ``.yaml`` files are always processed
  before ``.conf`` files regardless of document order (see
  `Including Other Files`_).

See Also
--------

- :doc:`Configuration formats overview <conf_formats>`
- :doc:`RainerScript reference <../rainerscript/index>`
- :doc:`Basic configuration structure <basic_structure>`
- :doc:`Converting legacy config <converting_to_new_format>`
- :doc:`Global configuration parameters <global/index>`
- :doc:`Queue parameters <../rainerscript/queue_parameters>`
