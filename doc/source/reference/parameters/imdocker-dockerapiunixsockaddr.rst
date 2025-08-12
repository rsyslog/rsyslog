.. _param-imdocker-dockerapiunixsockaddr:
.. _imdocker.parameter.module.dockerapiunixsockaddr:

.. index::
   single: imdocker; DockerApiUnixSockAddr
   single: DockerApiUnixSockAddr

.. summary-start

Specifies the Docker unix socket address to use.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: DockerApiUnixSockAddr
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=/var/run/docker.sock
:Required?: no
:Introduced: 8.41.0

Description
-----------
Specifies the Docker unix socket address to use.

.. _param-imdocker-module-dockerapiunixsockaddr:
.. _imdocker.parameter.module.dockerapiunixsockaddr-usage:

Module usage
------------
.. code-block:: rsyslog

   module(load="imdocker" DockerApiUnixSockAddr="...")

See also
--------
See also :doc:`../../configuration/modules/imdocker`.
