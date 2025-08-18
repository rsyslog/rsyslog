.. _param-imptcp-socketbacklog:
.. _imptcp.parameter.input.socketbacklog:

SocketBacklog
=============

.. index::
   single: imptcp; SocketBacklog
   single: SocketBacklog

.. summary-start

Sets the listen() backlog for pending TCP connections.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: SocketBacklog
:Scope: input
:Type: integer
:Default: input=64
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the backlog parameter passed to the ``listen()`` system call. This parameter
defines the maximum length of the queue for pending connections, which includes
partially established connections (those in the SYN-ACK handshake phase) and fully
established connections waiting to be accepted by the application.

For more details, refer to the ``listen(2)`` man page.

By default, the value is set to 64 to accommodate modern workloads. It can
be adjusted to suit specific requirements, such as:

- **High rates of concurrent connection attempts**: Increasing this value helps handle bursts of incoming connections without dropping them.
- **Test environments with connection flooding**: Larger values are recommended to prevent SYN queue overflow.
- **Servers with low traffic**: Lower values may be used to reduce memory usage.

The effective backlog size is influenced by system-wide kernel settings, particularly ``net.core.somaxconn`` and ``net.ipv4.tcp_max_syn_backlog``. The smaller value between this parameter and the kernel limits is used as the actual backlog.

Input usage
-----------
.. _param-imptcp-input-socketbacklog:
.. _imptcp.parameter.input.socketbacklog-usage:

.. code-block:: rsyslog

   input(type="imptcp" socketBacklog="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
