.. _param-omfile-dynafile:
.. _omfile.parameter.module.dynafile:

dynaFile
========

.. index::
   single: omfile; dynaFile
   single: dynaFile

.. summary-start

For each message, the file name is generated based on the given
template.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: dynaFile
:Scope: action
:Type: string
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

For each message, the file name is generated based on the given
template. Then, this file is opened. As with the *file* property,
data is appended if the file already exists. If the file does not
exist, a new file is created. The template given in "templateName"
is just a regular :doc:`rsyslog template <../../configuration/templates>`, so
you have full control over how to format the file name.

To avoid path traversal attacks, *you must make sure that the template
properly escapes every message-derived path component*. In string
templates, use the *secpath-replace* property replacer option. In list
templates, use ``securePath="replace"`` on the corresponding
``property()`` statements. Apply this to all fields that can come from
the message or sender, such as ``HOSTNAME``, ``programname``,
``APP-NAME``, or parsed variables. Keep fixed directory separators in
constant text.

By default, omfile also performs lexical containment checks for dynafile
paths. The rendered path must remain below the fixed leading directory
prefix that was configured in the dynafile template. This blocks remote
path traversal through components such as ``..`` or unexpected leading
slashes in message-derived fields, but it is not a filesystem sandbox. It
does not protect against symlinks, bind mounts, time-of-check/time-of-use
races, or malicious local filesystem state. For untrusted inputs,
``secpath-replace`` or ``securePath="replace"`` remains the strongest
recommended pattern.

Backward-compatibility change
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Opaque legacy templates (subtree and plugin/string-generator templates) do
not provide a fixed prefix that omfile can inspect. They remain supported, but
their fallback guard rejects output paths that are absolute or lexically escape
through ``..``. This deliberately changes the historical behavior of affected
legacy configurations. To preserve such a configuration temporarily, set
:ref:`param-omfile-dynafile-dangerouspermitpathescape` to ``on`` on the
affected action (or module). That opt-in disables the guard for that action;
use it only with trusted path data.

.. code-block:: rsyslog

   template(name="DynFile" type="string"
            string="/var/log/hosts/%HOSTNAME:::secpath-replace%/%programname:::secpath-replace%.log")

   template(name="DynFileList" type="list") {
     constant(value="/var/log/hosts/")
     property(name="hostname" securePath="replace")
     constant(value="/")
     property(name="programname" securePath="replace")
     constant(value=".log")
   }

When ``global(compatibility.defaults.secure="strict")`` is active,
dynafile templates default fields without explicit secure path handling
to ``secpath-replace``.  The ``warn`` setting keeps the historical
default and emits a warning for dynafile templates that need an explicit
``securepath`` or ``secpath-*`` option.  The ``backward-compatible``
setting keeps the historical default silently.

If the same template is used both for ``dynaFile`` and for non-dynafile
output, rsyslog emits a warning.  In strict mode the secure dynafile
default still applies to that shared template.  Use separate templates
when non-dynafile output must preserve literal slashes.

Either file or dynaFile can be used, but not both. If both are given,
dynaFile will be used.

A cache of recent files is kept. Note
that this cache can consume quite some memory (especially if large
buffer sizes are used). Files are kept open as long as they stay
inside the cache.
Files are removed from the cache when a HUP signal is sent, the
*closeTimeout* occurs, or the cache runs out of space, in which case
the least recently used entry is evicted.

Action usage
------------

.. _param-omfile-action-dynafile:
.. _omfile.parameter.action.dynafile:
.. code-block:: rsyslog

   action(type="omfile" dynaFile="...")

See also
--------

See also :doc:`../../configuration/modules/omfile`.
