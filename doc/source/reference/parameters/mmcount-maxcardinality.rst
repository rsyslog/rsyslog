.. SPDX-License-Identifier: Apache-2.0

.. _param-mmcount-maxcardinality:
.. _mmcount.parameter.input.maxcardinality:

.. meta::
   :description: Reference for the mmcount maxCardinality action parameter.
   :keywords: rsyslog, mmcount, maxCardinality, counter, cardinality limit, hash flooding

maxCardinality
==============

.. index::
   single: mmcount; maxCardinality
   single: maxCardinality

.. summary-start

Limits how many distinct values of :ref:`param-mmcount-key` are counted.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmcount`.

:Name: maxCardinality
:Scope: input
:Type: positive integer
:Default: 10000
:Required?: no
:Introduced: 8.2612.0

Description
-----------
Only used when :ref:`param-mmcount-key` is set and
:ref:`param-mmcount-value` is not, that is, when mmcount keeps a separate
counter for every observed value. Each distinct value occupies memory for the
lifetime of the action, and the values usually come from message content.
``maxCardinality`` caps the number of counters an action keeps.

Once the limit is reached, values that already have a counter continue to be
counted normally. A message carrying a value that has no counter yet is left
unchanged: no counter is created and no ``mmcount`` property is added. The
first time this happens, mmcount logs a warning naming the action's appname
and key. The warning is emitted only once per action instance.

Choose a limit that covers the expected set of values, for example the number
of distinct error codes or hosts being tracked, with headroom. Raising it
increases the memory an untrusted sender can make the action consume.

Input usage
-----------
.. _param-mmcount-maxcardinality-usage:
.. _mmcount.parameter.input.maxcardinality-usage:

.. code-block:: rsyslog

   action(type="mmcount" appName="glusterd" key="!gf_code" maxCardinality="500")

See also
--------
See also :doc:`../../configuration/modules/mmcount`.
