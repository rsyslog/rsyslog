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
- container metadata, such as container id, name, image id, labels, etc.

**Note**: Multiple docker instances are not supported at the time of this writing.


Configuration Parameters
========================

The configuration parameters for this module are designed for tailoring
the behavior of imdocker.

.. note::

   Parameter names are case-insensitive.

.. note::

   This module supports module parameters, only.



Module Parameters
-----------------


DockerApiUnixSockAddr
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "/var/run/docker.sock", "no", "none"

Specifies the Docker unix socket address to use.

ApiVersionStr
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "v1.27", "no", "none"

Specifies the version of Docker API to use. Must be in the format specified by the
Docker api, e.g. similar to the default above (v1.27, v1.28, etc).


PollingInterval
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "60", "no", "none"

Specifies the polling interval in seconds, imdocker will poll for new containers by
calling the 'List containers' API from the Docker engine.


ListContainersOptions
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "", "no", "none"

Specifies the HTTP query component of the a 'List Containers' HTTP API request.
See Docker API for more information about available options.
**Note**: It is not necessary to prepend the string with '?'.


GetContainerLogOptions
^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "timestamp=0&follow=1&stdout=1&stderr=1&tail=1", "no", "none"

Specifies the HTTP query component of the a 'Get container logs' HTTP API request.
See Docker API for more information about available options.
**Note**: It is not necessary to prepend the string with '?'.


RetrieveNewLogsFromStart
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "1", "no", "none"

This option specifies the whether imdocker will process newly found container logs from the beginning.
The exception is for containers found on start-up. The container logs for containers
that were active at imdocker start-up are controlled via 'GetContainerLogOptions', the
'tail' in particular.


DefaultFacility
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer or string (preferred)", "user", "no", "``$InputFileFacility``"

The syslog facility to be assigned to log messages received. Specified as numbers.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog


DefaultSeverity
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer or string (preferred)", "notice", "no", "``$InputFileSeverity``"

The syslog severity to be assigned to log messages received. Specified as numbers (e.g. 6
for ``info``). Textual form is suggested. Default is ``notice``.

.. seealso::

   https://en.wikipedia.org/wiki/Syslog


escapeLF
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

This is only meaningful if multi-line messages are to be processed.
LF characters embedded into syslog messages cause a lot of trouble,
as most tools and even the legacy syslog TCP protocol do not expect
these. If set to "on", this option avoid this trouble by properly
escaping LF characters to the 4-byte sequence "#012". This is
consistent with other rsyslog control character escaping. By default,
escaping is turned on. If you turn it off, make sure you test very
carefully with all associated tools. Please note that if you intend
to use plain TCP syslog with embedded LF characters, you need to
enable octet-counted framing.
For more details, see Rainer's blog posting on imfile LF escaping.


Metadata
========
The imdocker module supports message metadata. It supports the following
data items:

- **Id** - the container id associated with the message.

- **Names** - the first container associated with the message.

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
  	string="program:%programname% tag:%syslogtag% id:%$!metadata!Id% name:%$!metadata!Names% imageid:%$!metadata!ImageID% labels:%$!metadata!Labels% msg: %msg%\n"
  )

