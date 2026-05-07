.. _param-imudp-listenportfilename:
.. _imudp.parameter.input.listenportfilename:

ListenPortFileName
==================

.. index::
   single: imudp; ListenPortFileName
   single: ListenPortFileName

.. summary-start

Writes the UDP listener's bound port number into the given file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: ListenPortFileName
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Specifies a file name into which the port number this input listens on is
written after the UDP socket is bound. It is primarily useful with ``port="0"``,
where the operating system assigns an available port and rsyslog writes the
assigned port to the file.

This parameter can be used only when the ``port`` array contains one value and
that value binds exactly one UDP socket. Wildcard addresses can resolve to
multiple sockets, for example separate IPv4 and IPv6 sockets, and are rejected
with ``listenPortFileName`` because one file would be ambiguous. Set
``address`` to a single local address, or configure separate ``imudp`` inputs
with separate ``listenPortFileName`` values.

When used with a nonzero single port, rsyslog writes the actual bound port
number to the file after the bind succeeds.

Input usage
-----------
.. _param-imudp-input-listenportfilename:
.. _imudp.parameter.input.listenportfilename-usage:

.. code-block:: rsyslog

   input(type="imudp" port="0" listenPortFileName="/tmp/imudp.port")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
