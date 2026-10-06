.. SPDX-License-Identifier: Apache-2.0

.. _param-imrelp-maxdatasize:
.. _imrelp.parameter.input.maxdatasize:

.. meta::
   :description: Configure the maximum RELP message size accepted by imrelp.
   :keywords: imrelp, maxDataSize, RELP message size, RELP framing

maxDataSize
===========

.. index::
   single: imrelp; maxDataSize
   single: maxDataSize

.. summary-start

Sets the max message size the RELP listener accepts before oversize handling.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: maxDataSize
:Scope: input
:Type: size_nbr
:Default: input=:doc:`global(maxMessageSize) <../../rainerscript/global>`
:Required?: no
:Introduced: Not documented

Description
-----------
Sets the max message size (in bytes) that can be received. Messages that are too
long are handled as specified in parameter :ref:`param-imrelp-oversizemode`. Note
that maxDataSize cannot be smaller than the global parameter
:doc:`global(maxMessageSize) <../../rainerscript/global>`. The largest accepted
value is ``999999999`` bytes. Larger values are rejected during configuration
validation because the supported librelp framing parser accepts at most nine
decimal length digits.

Input usage
-----------
.. _param-imrelp-input-maxdatasize-usage:
.. _imrelp.parameter.input.maxdatasize-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" maxDataSize="10k")

Notes
-----
The effective value must remain at or below ``999999999``. Because imrelp raises
``maxDataSize`` to ``global(maxMessageSize)`` when the global value is larger,
configuration validation also fails if the global value exceeds this limit.

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
