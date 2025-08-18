.. _param-imptcp-compression-mode:
.. _imptcp.parameter.input.compression-mode:

Compression.mode
================

.. index::
   single: imptcp; Compression.mode
   single: Compression.mode

.. summary-start

Selects decompression mode matching compression used by omfwd.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Compression.mode
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
This is the counterpart to the compression modes set in
:doc:`omfwd <../../configuration/modules/omfwd>`.
Please see its documentation for details.

Input usage
-----------
.. _param-imptcp-input-compression-mode:
.. _imptcp.parameter.input.compression-mode-usage:

.. code-block:: rsyslog

   input(type="imptcp" compression.mode="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
