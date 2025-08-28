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
used properly escapes file paths*. This is done by using the *securepath*
parameter in the template's property statements, or the *secpath-drop*
or *secpath-replace* property options with the property replacer.

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
