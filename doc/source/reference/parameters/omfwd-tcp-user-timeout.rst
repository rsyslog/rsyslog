.. index:: ! omfwd; tcp_user_timeout

.. _param-omfwd-tcp-user-timeout:

tcp_user_timeout
================

.. summary-start

**Default:** 0

**Type:** integer

**Description:**

Sets the TCP_USER_TIMEOUT socket option, in milliseconds, for TCP-based omfwd
connections. A value of ``0`` leaves the operating system default unchanged.
This option is independent of TCP keepalive and may not be available on all
platforms.

.. versionadded:: 8.2608.0

.. summary-end

Examples
--------

RainerScript:

.. code-block:: rsyslog

   action(type="omfwd"
          target="central.example.net"
          port="6514"
          protocol="tcp"
          tcp_user_timeout="10000")

YAML:

.. code-block:: yaml

   actions:
     - type: omfwd
       target: central.example.net
       port: 6514
       protocol: tcp
       tcp_user_timeout: 10000
