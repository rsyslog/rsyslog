.. _param-imkafka-confparam:
.. _imkafka.parameter.input.confparam:

confparam
=========

.. index::
   single: imkafka; confparam
   single: confparam

.. summary-start

Passes arbitrary librdkafka configuration parameters to the imkafka consumer.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkafka`.

:Name: confparam
:Scope: input
:Type: array
:Default: input=``none``
:Required?: no
:Introduced: 8.27.0

Description
-----------
Permits to specify Kafka options. Rather than offering a myriad of config
settings to match the Kafka parameters, we provide this setting here as a
vehicle to set any Kafka parameter. This has the big advantage that Kafka
parameters that come up in new releases can immediately be used.

Note that we use librdkafka for the Kafka connection, so the parameters are
actually those that librdkafka supports. As of our understanding, this is a
superset of the native Kafka parameters.

Input usage
-----------
.. _imkafka.parameter.input.confparam-usage:

.. code-block:: rsyslog

   module(load="imkafka")
   input(type="imkafka"
         topic="your-topic"
         confParam=["queued.min.messages=10000",
                    "socket.timeout.ms=60000"])

See also
--------
See also :doc:`../../configuration/modules/imkafka`.
