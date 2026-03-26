.. _yaml_config:

YAML Configuration Format
=========================

rsyslog supports configuration via YAML files as an alternative to
:doc:`RainerScript <../rainerscript/index>`.  The two formats are
equivalent: every YAML configuration key maps directly to a RainerScript
parameter of the same name, so all per-module documentation remains
applicable unchanged.

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
   perctile_stats: [ ... ]
   ratelimits: [ ... ]
   timezones: [ ... ]

All section names are case-sensitive.  Unknown top-level keys are logged
as errors and ignored.

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

.. code-block:: yaml

   templates:
     - name: myFormat
       type: string
       string: "%HOSTNAME% %syslogfacility-text%.%syslogseverity-text% %msg%\n"
     - name: filePerHost
       type: string
       string: "/var/log/hosts/%HOSTNAME%.log"
       options.dyn: "on"

**rulesets**

Ruleset items specify a ``name`` and may carry queue parameters.  Rule
logic (filters, actions, ``if/then``, ``set``, ``foreach``, etc.) is
supplied as a RainerScript text block under the ``script:`` key.  See
`Scripting`_ below.

.. code-block:: yaml

   rulesets:
     - name: main
       script: |
         *.* action(type="omfile" file="/var/log/syslog")

     - name: errors
       script: |
         if $syslogseverity <= 3 then {
           action(type="omfile" file="/var/log/errors.log")
         }
         stop

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
   For simple routing (one filter, one action) the ``script:`` block is
   often just a single ``action()`` line.  More complex conditional logic,
   loops, and variable assignments work exactly as they do in a
   RainerScript config.

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
       script: |
         auth,authpriv.* action(type="omfile" file="/var/log/auth.log")
         *.*;auth,authpriv.none action(type="omfile" file="/var/log/syslog")
         kern.*            action(type="omfile" file="/var/log/kern.log")
         *.emerg           action(type="omusrmsg" users="*")

   include:
     - path: "/etc/rsyslog.d/*.yaml"
       optional: "on"
     - path: "/etc/rsyslog.d/*.conf"
       optional: "on"

Relationship to RainerScript
-----------------------------

YAML configuration is a thin front-end over the same internal machinery
that RainerScript uses.  In particular:

- Parameter names are identical; all per-module parameter documentation
  applies without change.
- Type coercion (integers, sizes, binary flags, UIDs, …) is performed
  by the same ``nvlstGetParams()`` layer after YAML parsing.
- Unknown parameter names produce the same "unused parameter" error
  regardless of which format was used.
- Template, ruleset, and lookup-table names are shared; a YAML-defined
  ruleset can be referenced from a RainerScript ``action()``.

Limitations (current implementation)
--------------------------------------

- Nested YAML mappings inside parameter values are not supported.  Use a
  YAML scalar or sequence.
- The ``script:`` key is the only way to express filter logic.  A future
  version may introduce a structured ``filter:`` shortcut for simple
  priority-based routing.
- ``version:`` is accepted but not enforced; it is reserved for future
  schema evolution.

See Also
--------

- :doc:`RainerScript reference <../rainerscript/index>`
- :doc:`Basic configuration structure <basic_structure>`
- :doc:`Converting legacy config <converting_to_new_format>`
- :doc:`Global configuration parameters <global/index>`
- :doc:`Queue parameters <../rainerscript/queue_parameters>`
