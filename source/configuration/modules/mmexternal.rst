********************************************************
Support module for external message modification modules
********************************************************

===========================  ===========================================================================
**Module Name:**             **mmexternal**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available since:**         8.3.0
===========================  ===========================================================================


Purpose
=======

This module permits to integrate external message modification plugins
into rsyslog.

For details on the interface specification, see rsyslog's source in the
./plugins/external/INTERFACE.md.
 

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

binary
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "yes", "none"

The name of the external message modification plugin to be called. This
can be a full path name.


interface.input
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "msg", "no", "none"

This can either be "msg", "rawmsg" or "fulljson". In case of "fulljson", the
message object is provided as a json object. Otherwise, the respective
property is provided. This setting **must** match the external plugin's
expectations. Check the external plugin documentation for what needs to be used.


output
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

This is a debug aid. If set, this is a filename where the plugins output
is logged. Note that the output is also being processed as usual by rsyslog.
Setting this parameter thus gives insight into the internal processing
that happens between plugin and rsyslog core.


forceSingleInstance
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

This is an expert parameter, just like the equivalent *omprog* parameter.
See the message modification plugin's documentation if it is needed.


Examples
========

Execute external module
-----------------------

The following config file snippet is used to write execute an external
message modification module "mmexternal.py". Note that the path to the
module is specified here. This is necessary if the module is not in the
default search path.

.. code-block:: none

   module (load="mmexternal") # needs to be done only once inside the config

   action(type="mmexternal" binary="/path/to/mmexternal.py")


