.. _param-imptcp-addtlframedelimiter:
.. _imptcp.parameter.input.addtlframedelimiter:

AddtlFrameDelimiter
===================

.. index::
   single: imptcp; AddtlFrameDelimiter
   single: AddtlFrameDelimiter

.. summary-start

Defines an additional ASCII frame delimiter for non-standard senders.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: AddtlFrameDelimiter
:Scope: input
:Type: integer
:Default: input=-1
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This directive permits to specify an additional frame delimiter for
plain tcp syslog. The industry-standard specifies using the LF
character as frame delimiter. Some vendors, notable Juniper in their
NetScreen products, use an invalid frame delimiter, in Juniper's case
the NUL character. This directive permits to specify the ASCII value
of the delimiter in question. Please note that this does not
guarantee that all wrong implementations can be cured with this
directive. It is not even a sure fix with all versions of NetScreen,
as I suggest the NUL character is the effect of a (common) coding
error and thus will probably go away at some time in the future. But
for the time being, the value 0 can probably be used to make rsyslog
handle NetScreen's invalid syslog/tcp framing. For additional
information, see this `forum
thread <http://kb.monitorware.com/problem-with-netscreen-log-t1652.html>`_.
**If this doesn't work for you, please do not blame the rsyslog team.
Instead file a bug report with Juniper!**

Note that a similar, but worse, issue exists with Cisco's IOS
implementation. They do not use any framing at all. This is confirmed
from Cisco's side, but there seems to be very limited interest in
fixing this issue. This directive **can not** fix the Cisco bug. That
would require much more code changes, which I was unable to do so
far. Full details can be found at the `Cisco tcp syslog
anomaly <http://www.rsyslog.com/Article321.phtml>`_ page.

Input usage
-----------
.. _param-imptcp-input-addtlframedelimiter:
.. _imptcp.parameter.input.addtlframedelimiter-usage:

.. code-block:: rsyslog

   input(type="imptcp" addtlFrameDelimiter="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imptcp.parameter.legacy.inputptcpserveraddtlframedelimiter:

- $InputPTCPServerAddtlFrameDelimiter — maps to AddtlFrameDelimiter (status: legacy)

.. index::
   single: imptcp; $InputPTCPServerAddtlFrameDelimiter
   single: $InputPTCPServerAddtlFrameDelimiter

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
