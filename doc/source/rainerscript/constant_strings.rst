Rsyslog Parameter String Constants
==================================

String constants provide values that are evaluated at startup and remain unchanged
for the lifetime of the rsyslog process.

Uses
----

String constants are used in comparisons, configuration parameter values, and
function arguments, among other places.

Escaping
--------

In string constants, special characters are escaped by prepending a backslash,
similar to C or PHP.

If in doubt how to escape properly, use the
`RainerScript String Escape Online Tool
<http://www.rsyslog.com/rainerscript-constant-string-escaper/>`_.

Types
-----

Rsyslog provides different types of string constants, inspired by common shell
semantics.

Single quotes
.............

Values are used unaltered, except that escape sequences are processed.

Double quotes
.............

Equivalent to single quotes; a literal dollar sign must be escaped (``\$``).
If ``$`` is not escaped, a syntax error is raised and rsyslog usually refuses
to start.

Backticks
.........

.. versionadded:: 8.33.0
   Initial support for backticks (limited subset).

.. versionchanged:: 8.37.0
   Multiple environment variable expansions and constant text between them.

.. versionchanged:: 8.2508.0
   Support for brace-style variables (``${VAR}``), correct termination of
   unbraced ``$VAR`` at the first non-``[A-Za-z0-9_]`` character, and support
   for variable text **adjacent** to static text (e.g., key/value pairs).

Backticks intentionally implement a **small, safe subset** of shell-like
behavior. Currently, only the following forms are supported:

- ``echo …`` — evaluate simple text with environment variable expansion.
- ``cat <filename>`` — include the contents of a single file.
  If the file is unreadable, the result is an empty string.

Any other construct results in a parse error. There must be **exactly one space**
between ``echo`` or ``cat`` and the following argument.

Environment variable expansion rules
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Both ``$VAR`` and ``${VAR}`` are supported.
- For unbraced ``$VAR``, the variable name ends at the first character
  **not** in ``[A-Za-z0-9_]``; that character is emitted literally.
  This makes expressions like ``$VAR!`` or ``$VAR.suffix`` work as expected.
- For braced ``${VAR}``, expansion ends at the matching ``}``, which naturally
  supports **adjacent** static text (e.g., ``prefix${VAR}suffix``).
- Unknown variables expand to the empty string (not an error).
- Command substitutions and other shell features (e.g., ``$(pwd)``) are
  **not supported** by design.

Examples
========

Simple expansion
----------------

Given ``SOMEPATH=/var/log/custompath``:

.. code-block:: rsyslog

   param = `echo $SOMEPATH/myfile`
   # expands to: /var/log/custompath/myfile

Brace form and adjacent text
----------------------------

Given ``HOSTNAME=myhost``:

.. code-block:: rsyslog

   title = `echo Log-${HOSTNAME}!`
   # expands to: Log-myhost!

Key/value pairs (common with module parameters)
-----------------------------------------------

Given ``KAFKA_PASSWORD=supersecret`` and rsyslog >= 8.2508.0:

.. code-block:: rsyslog

   action(
     type="omkafka"
     broker=["kafka.example.com:9093"]
     confParam=[
       "security.protocol=SASL_SSL",
       "sasl.mechanism=SCRAM-SHA-512",
       "sasl.username=myuser",
       `echo sasl.password=${KAFKA_PASSWORD}`
     ]
   )

.. index::
   single: config.enabled
   single: environment variables; config.enabled
   single: systemd; environment variables
   single: imtcp; conditional loading
   pair: containers; environment-style configuration

Conditional enablement of configuration objects via ``config.enabled``
----------------------------------------------------------------------

You can toggle modules, inputs, actions, etc. based on environment variables by
setting the generic boolean parameter ``config.enabled``. The example below loads
``imtcp`` and starts a TCP listener only if ``LOAD_IMTCP`` evaluates to a
truthy value.

**systemd service (recommended on most distributions)**

Use a drop-in to set the environment variable in rsyslog’s service context:

.. code-block:: bash

   sudo mkdir -p /etc/systemd/system/rsyslog.service.d
   sudo tee /etc/systemd/system/rsyslog.service.d/10-imtcp.conf >/dev/null <<'EOF'
   [Service]
   # Enable
   Environment=LOAD_IMTCP=on
   EOF

   sudo systemctl daemon-reload
   sudo systemctl restart rsyslog

Alternatively, you can point to an ``EnvironmentFile=`` that contains
``LOAD_IMTCP=…``.

.. code-block:: ini

   # /etc/systemd/system/rsyslog.service.d/10-imtcp.conf
   [Service]
   EnvironmentFile=-/etc/default/rsyslog

   # /etc/default/rsyslog
   LOAD_IMTCP=on

**Bash / non-systemd service environment**

If rsyslog is started from a shell wrapper (e.g., SysV init or container entrypoint):

.. code-block:: bash

   # enable (any of 1, true, on, yes will do)
   export LOAD_IMTCP=on

   # or disable
   # export LOAD_IMTCP=off

**RainerScript (rsyslog.conf)**

.. code-block:: rsyslog

   # Load the module conditionally. If LOAD_IMTCP is unset/empty/0/false,
   # the statement is disabled and the module is not loaded.
   module(
     load="imtcp"
     config.enabled=`echo ${LOAD_IMTCP}`
   )

   # Start a TCP input only when enabled. This also works if you prefer
   # to always load the module but gate the input itself.
   input(
     type="imtcp"
     port="514"
     ruleset="in_tcp"
     config.enabled=`echo ${LOAD_IMTCP}`
   )

   ruleset(name="in_tcp") {
     action(type="omfile" file="/var/log/remote/tcp.log")
   }

Accepted truthy value for ``config.enabled`` is ``on``. Any other value,
including no value, is treated as false.

.. note::

   In containerized deployments, this pattern is often the simplest way to pass
   environment-style toggles into a static configuration. For example:

   .. code-block:: bash

      docker run --rm \
        -e LOAD_IMTCP=on \
        -v /path/to/rsyslog.conf:/etc/rsyslog.conf:ro \
        my-rsyslog-image


Including a file’s content
--------------------------

.. code-block:: rsyslog

   token = `cat /etc/rsyslog.d/token.txt`

Compatibility note for older releases
-------------------------------------

On versions **before 8.2508.0**, backticks did **not** support ``${VAR}`` or
adjacent text reliably. If you need to build a ``key=value`` pair, pre-compose
it outside rsyslog and expand the already-complete string, e.g.:

In bash or init script:

.. code-block:: bash

   export KAFKA_PASSWORD='supersecret'
   export SASL_PWDPARAM="sasl.password=${KAFKA_PASSWORD}"

In rsyslog conf:

.. code-block:: rsyslog

   action(
     type="omkafka"
     broker=["kafka.example.com:9093"]
     confParam=[
       "security.protocol=SASL_SSL",
       "sasl.mechanism=SCRAM-SHA-512",
       "sasl.username=myuser",
       `echo $SASL_PWDPARAM`
     ]
   )

Rationale
---------

Backticks are intended to provide just enough functionality to customize
configuration with external data while avoiding the complexity and risks of a
general shell interpreter.
