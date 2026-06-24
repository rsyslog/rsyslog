.. _param-imuxsock-socket:
.. _imuxsock.parameter.input.socket:

Socket
======

.. index::
   single: imuxsock; Socket
   single: Socket

.. summary-start

Adds an additional UNIX socket for this input to listen on.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: Socket
:Scope: input
:Type: string
:Default: input=none
:Required?: yes
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Adds an additional unix socket. Formerly specified with the ``-a`` option.
This parameter is required for every ``input(type="imuxsock" ...)`` object,
because input objects configure additional sockets only. The system log socket
is configured on the module object with ``SysSock.*`` parameters.

Input usage
-----------
.. _param-imuxsock-input-socket:
.. _imuxsock.parameter.input.socket-usage:

.. code-block:: rsyslog

   input(type="imuxsock" socket="/path/to/socket")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.
.. _imuxsock.parameter.legacy.addunixlistensocket:

- $AddUnixListenSocket — maps to Socket (status: legacy)

.. index::
   single: imuxsock; $AddUnixListenSocket
   single: $AddUnixListenSocket

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
