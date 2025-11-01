.. _param-omhttp-compress-level:
.. _omhttp.parameter.module.compress-level:

compress.level
==============

.. index::
   single: omhttp; compress.level
   single: compress.level

.. summary-start

Sets the zlib compression level used when :ref:`param-omhttp-compress` is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: compress.level
:Scope: module
:Type: integer
:Default: module=-1
:Required?: no
:Introduced: Not specified

Description
-----------
Specify the zlib compression level if :ref:`param-omhttp-compress` is enabled. Check the `zlib manual <https://www.zlib.net/manual.html>`_ for further documentation.

``-1`` is the default value that strikes a balance between best speed and best compression. ``0`` disables compression. ``1`` results in the fastest compression. ``9`` results in the best compression.

Module usage
------------
.. _omhttp.parameter.module.compress-level-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       compress="on"
       compressLevel="9"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
