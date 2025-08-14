.. _param-imfile-escapelf:
.. _imfile.parameter.module.escapelf:

escapeLF
========

.. index::
   single: imfile; escapeLF
   single: escapeLF

.. summary-start

This is only meaningful if multi-line messages are to be processed.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: escapeLF
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This is only meaningful if multi-line messages are to be processed.
LF characters embedded into syslog messages cause a lot of trouble,
as most tools and even the legacy syslog TCP protocol do not expect
these. If set to "on", this option avoid this trouble by properly
escaping LF characters to the 4-byte sequence "#012". This is
consistent with other rsyslog control character escaping. By default,
escaping is turned on. If you turn it off, make sure you test very
carefully with all associated tools. Please note that if you intend
to use plain TCP syslog with embedded LF characters, you need to
enable octet-counted framing. For more details, see
`Rainer Gerhards' blog posting on imfile LF escaping <https://rainer.gerhards.net/2013/09/imfile-multi-line-messages.html>`_.

Input usage
-----------
.. _param-imfile-input-escapelf:
.. _imfile.parameter.input.escapelf:
.. code-block:: rsyslog

   input(type="imfile" escapeLF="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
