.. _param-imuxsock-ignoretimestamp:
.. _imuxsock.parameter.input.ignoretimestamp:

IgnoreTimestamp
===============

.. index::
   single: imuxsock; IgnoreTimestamp
   single: IgnoreTimestamp

.. summary-start

Ignores timestamps included in messages from this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: IgnoreTimestamp
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Ignore timestamps included in messages received from the input being
defined.

Input usage
-----------
.. _param-imuxsock-input-ignoretimestamp:
.. _imuxsock.parameter.input.ignoretimestamp-usage:

.. code-block:: rsyslog

   input(type="imuxsock" ignoreTimestamp="off")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.inputunixlistensocketignoremsgtimestamp:

- $InputUnixListenSocketIgnoreMsgTimestamp — maps to IgnoreTimestamp (status: legacy)

.. index::
   single: imuxsock; $InputUnixListenSocketIgnoreMsgTimestamp
   single: $InputUnixListenSocketIgnoreMsgTimestamp

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
