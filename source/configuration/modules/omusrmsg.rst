**********************
omusrmsg: notify users
**********************

===========================  ===========================================================================
**Module Name:**             **omusrmsg**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module permits to send log messages to the user terminal. This is a
built-in module so it doesn't need to be loaded.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

Users
^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "yes", "none"

The name of the users to send data to.


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "WallFmt/StdUsrMsgFmt", "no", "none"

Template to user for the message. Default is WallFmt when parameter users is
"*" and StdUsrMsgFmt otherwise.


Examples
========

Write emergency messages to all users
-------------------------------------

The following command writes emergency messages to all users

.. code-block:: none

   action(type="omusrmsg" users="*")

