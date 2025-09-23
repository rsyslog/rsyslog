***********************************
Fields Extraction Module (mmfields)
***********************************

===========================  ===========================================================================
**Module Name:**             **mmfields**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available since:**         7.5.1
===========================  ===========================================================================


Purpose
=======

The mmfield module permits to extract fields. It is an alternate to
using the property replacer field extraction capabilities. In contrast
to the property replacer, all fields are extracted as once and stored
inside the structured data part (more precisely: they become Lumberjack
[JSON] properties).

Using this module is of special advantage if a field-based log format is
to be processed, like for example CEF **and** either a large number
of fields is needed or a specific field is used multiple times inside
filters. In these scenarios, mmfields potentially offers better
performance than the property replacer of the RainerScript field
extraction method. The reason is that mmfields extracts all fields as
one big sweep, whereas the other methods extract fields individually,
which requires multiple passes through the same data. On the other hand,
adding field content to the rsyslog property dictionary also has some
overhead, so for high-performance use cases it is suggested to do some
performance testing before finally deciding which method to use. This is
most important if only a smaller subset of the fields is actually
needed.

In any case, mmfields provides a very handy and easy to use way to parse
structured data into a it's individual data items. Again, a primary use
case was support for CEF (Common Event Format), which is made extremely
easy to do with this module.

This module is implemented via the action interface. Thus it can be
conditionally used depending on some prerequisites.

 
Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive. camelCase is recommended for readability.


Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmfields-separator`
     - .. include:: ../../reference/parameters/mmfields-separator.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmfields-jsonroot`
     - .. include:: ../../reference/parameters/mmfields-jsonroot.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/mmfields-separator
   ../../reference/parameters/mmfields-jsonroot


Examples
========

Parsing messages and writing them to file
-----------------------------------------

This is a very simple use case where each message is parsed. The default
separator character of comma is being used.

.. code-block:: none

   module(load="mmfields")
   template(name="ftpl"
            type="string"
            string="%$!%\\n")
   action(type="mmfields")
   action(type="omfile"
          file="/path/to/logfile"
          template="ftpl")


Writing into a specific json path
---------------------------------

The following sample is similar to the previous one, but this time the
colon is used as separator and data is written into the "$!mmfields"
json path.

.. code-block:: none

   module(load="mmfields")
   template(name="ftpl"
            type="string"
            string="%$!%\\n")
   action(type="mmfields"
          separator=":"
          jsonRoot="!mmfields")
   action(type="omfile"
          file="/path/to/logfile"
          template="ftpl")

