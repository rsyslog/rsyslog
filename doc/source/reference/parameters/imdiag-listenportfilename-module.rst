.. _param-imdiag-listenportfilename-module:
.. _imdiag.parameter.module.listenportfilename:

.. meta::
   :description: Reference for the imdiag listenPortFileName module-scope parameter.
   :keywords: rsyslog, imdiag, listenportfilename, testbench, port file, startup

ListenPortFileName (module scope)
==================================

.. index::
   single: imdiag; ListenPortFileName (module)

.. summary-start

Path of the file to which imdiag writes its chosen TCP listen port number,
set at module load time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: ListenPortFileName
:Scope: module
:Type: string (file path)
:Default: none
:Required?: no (but recommended for testbench use)
:Introduced: 8.x

Description
-----------
When set at module scope, ``ListenPortFileName`` specifies the file that
imdiag will create and populate with its chosen listen port number after the
TCP listener is ready. The testbench reads this file to discover the port
without needing a fixed port number.

This is the module-scope version of the parameter. The same name can also
be set at input scope via ``input(type="imdiag" listenPortFileName=...)``.

Module usage
------------

.. code-block:: rsyslog

   module(load="imdiag" listenPortFileName="/tmp/imdiag.port")

YAML usage
----------

.. code-block:: yaml

   testbench_modules:
     - load: "../plugins/imdiag/.libs/imdiag"
       listenportfilename: "/tmp/imdiag.port"

See also
--------
See also :doc:`../../configuration/modules/imdiag` and
:ref:`param-imdiag-listenportfilename`.
