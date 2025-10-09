.. _param-imkafka-parsehostname:
.. _imkafka.parameter.input.parsehostname:

parsehostname
=============

.. index::
   single: imkafka; parsehostname
   single: parsehostname

.. summary-start

Controls whether imkafka parses the hostname from each received message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkafka`.

:Name: parsehostname
:Scope: input
:Type: boolean
:Default: input=``off``
:Required?: no
:Introduced: 8.38.0

Description
-----------
If this parameter is set to on, imkafka will parse the hostname in log if it
exists. The result can be retrieved from $hostname. If it's off, for
compatibility reasons, the local hostname is used, same as the previous
version.

Input usage
-----------
.. _imkafka.parameter.input.parsehostname-usage:

.. code-block:: rsyslog

   module(load="imkafka")
   input(type="imkafka"
         topic="your-topic"
         parseHostname="on")

See also
--------
See also :doc:`../../configuration/modules/imkafka`.
