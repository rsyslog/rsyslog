.. _param-immark-markmessagetext:
.. _immark.parameter.module.markmessagetext:

markMessageText
================

.. index::
   single: immark; markMessageText
   single: markMessageText

.. summary-start

Overrides the text that immark injects for each mark message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/immark`.

:Name: markMessageText
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module="-- MARK --"
:Required?: no
:Introduced: 8.2012.0

Description
-----------
Provide a custom string that immark uses when it emits the periodic mark
message. If unset, immark falls back to the historical ``-- MARK --``
text. The string may include whitespace and other characters supported
by :doc:`../../rainerscript/constant_strings`.

Module usage
------------
.. _immark.parameter.module.markmessagetext-usage:

.. code-block:: rsyslog

   module(load="immark" markMessageText="-- CUSTOM MARK --")

See also
--------

.. seealso::

   * :ref:`param-immark-interval`
   * :doc:`../../configuration/modules/immark`
