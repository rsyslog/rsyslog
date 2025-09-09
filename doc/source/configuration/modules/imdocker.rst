***************************************
imdocker: Docker Input Module
***************************************

===========================  ===========================================================================
**Module Name:**             **imdocker**
**Author:**                  Nelson Yen
**Available since:**         8.41.0
===========================  ===========================================================================


Purpose
=======

The imdocker input plug-in provides the ability to receive container logs from Docker (engine)
via the Docker Rest API.

Other features include:

- filter containers through the plugin options
- handle long log lines (greater than 16kb) and obtain
- container metadata, such as container id, name, image, image id, labels, etc.

**Note**: Multiple docker instances are not supported at the time of this writing.


Configuration Parameters
========================

The configuration parameters for this module are designed for tailoring
the behavior of imdocker.

.. note::

   Parameter names are case-insensitive; CamelCase is recommended for readability.

.. note::

   This module supports module parameters, only.



Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imdocker-dockerapiunixsockaddr`
     - .. include:: ../../reference/parameters/imdocker-dockerapiunixsockaddr.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdocker-apiversionstr`
     - .. include:: ../../reference/parameters/imdocker-apiversionstr.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdocker-pollinginterval`
     - .. include:: ../../reference/parameters/imdocker-pollinginterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdocker-listcontainersoptions`
     - .. include:: ../../reference/parameters/imdocker-listcontainersoptions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdocker-getcontainerlogoptions`
     - .. include:: ../../reference/parameters/imdocker-getcontainerlogoptions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdocker-retrievenewlogsfromstart`
     - .. include:: ../../reference/parameters/imdocker-retrievenewlogsfromstart.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdocker-defaultfacility`
     - .. include:: ../../reference/parameters/imdocker-defaultfacility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdocker-defaultseverity`
     - .. include:: ../../reference/parameters/imdocker-defaultseverity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdocker-escapelf`
     - .. include:: ../../reference/parameters/imdocker-escapelf.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/imdocker-dockerapiunixsockaddr
   ../../reference/parameters/imdocker-apiversionstr
   ../../reference/parameters/imdocker-pollinginterval
   ../../reference/parameters/imdocker-listcontainersoptions
   ../../reference/parameters/imdocker-getcontainerlogoptions
   ../../reference/parameters/imdocker-retrievenewlogsfromstart
   ../../reference/parameters/imdocker-defaultfacility
   ../../reference/parameters/imdocker-defaultseverity
   ../../reference/parameters/imdocker-escapelf

Metadata
========
The imdocker module supports message metadata. It supports the following
data items:

- **Id** - the container id associated with the message.

- **Names** - the first container associated with the message.

- **Image** - the image name and tag of the container associated with the message.

- **ImageID** - the image id of the container associated with the message.

- **Labels** - all the labels of the container associated with the message in json format.

**Note**: At the time of this writing, metadata is always enabled.


Statistic Counter
=================

This plugin maintains `statistics <http://www.rsyslog.com/rsyslog-statistic-counter/>`. The statistic is named "imdocker".

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted to main queue after reading from journal for processing
   since startup. All records may not be submitted due to rate-limiting.

-  **ratelimit.discarded** - number of messages discarded due to rate-limiting within configured
   rate-limiting interval.

-  **curl.errors** - total number of curl errors.


Caveats/Known Bugs
==================

-  At the moment, this plugin only supports a single instance of docker on a host.


Configuration Examples
======================

Load module, with only defaults
--------------------------------

This activates the module with all the default options:

.. code-block:: none

   module(load="imdocker")


Load module, with container filtering
-------------------------------------

This activates the module with container filtering on a label:

.. code-block:: none

  module(load="imdocker"
    DockerApiUnixSockAddr="/var/run/docker.sock"
    ApiVersionStr="v1.27"
    PollingInterval="60"
    ListContainersOptions="filters={\"label\":[\"log_opt_enabled\"]}"
    GetContainerLogOptions="timestamps=0&follow=1&stdout=1&stderr=0&tail=1"
  )


Example template to get container metadata
------------------------------------------

An example of how to create a template with container metadata

.. code-block:: none

  template (name="ImdockerFormat" type="string"
  	string="program:%programname% tag:%syslogtag% id:%$!metadata!Id% name:%$!metadata!Names% image:%$!metadata!Image% imageid:%$!metadata!ImageID% labels:%$!metadata!Labels% msg: %msg%\n"
  )

