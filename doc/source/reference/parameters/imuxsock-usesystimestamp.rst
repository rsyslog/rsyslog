.. _param-imuxsock-usesystimestamp:
.. _imuxsock.parameter.input.usesystimestamp:

UseSysTimeStamp
===============

.. index::
   single: imuxsock; UseSysTimeStamp
   single: UseSysTimeStamp

.. summary-start

Takes message timestamps from the system instead of the message itself.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: UseSysTimeStamp
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: 5.9.1

Description
-----------
This parameter instructs ``imuxsock`` to obtain message time from
the system (via control messages) instead of using time recorded inside
the message. This may be most useful in combination with systemd. Due to
the usefulness of this functionality, we decided to enable it by default.
As such, the behavior is slightly different than previous versions.
However, we do not see how this could negatively affect existing environments.

.. versionadded:: 5.9.1

Input usage
-----------
.. _param-imuxsock-input-usesystimestamp:
.. _imuxsock.parameter.input.usesystimestamp-usage:

.. code-block:: rsyslog

   input(type="imuxsock" useSysTimeStamp="off")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.inputunixlistensocketusesystimestamp:

- $InputUnixListenSocketUseSysTimeStamp — maps to UseSysTimeStamp (status: legacy)

.. index::
   single: imuxsock; $InputUnixListenSocketUseSysTimeStamp
   single: $InputUnixListenSocketUseSysTimeStamp

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
