.. _param-omrelp-windowsize:
.. _omrelp.parameter.input.windowsize:

WindowSize
==========

.. index::
   single: omrelp; WindowSize
   single: WindowSize

.. summary-start

Overrides the RELP client window size used for message transmission.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: WindowSize
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
This is an **expert parameter**. It permits to override the RELP window size being used by the client. Changing the window size has both an effect on performance as well as potential message duplication in failure case. A larger window size means more performance, but also potentially more duplicated messages - and vice versa. The default 0 means that librelp's default window size is being used, which is considered a compromise between goals reached. For your information: at the time of this writing, the librelp default window size is 128 messages, but this may change at any time. Note that there is no equivalent server parameter, as the client proposes and manages the window size in RELP protocol.

Input usage
-----------
.. _param-omrelp-input-windowsize:
.. _omrelp.parameter.input.windowsize-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" windowSize="256")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
