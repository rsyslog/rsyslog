.. _param-omhttp-errorfile:
.. _omhttp.parameter.module.errorfile:

errorfile
=========

.. index::
   single: omhttp; errorfile
   single: errorfile

.. summary-start

Specifies a file where omhttp records HTTP requests that return error responses.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: errorfile
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
Here you can set the name of a file where all errors will be written to. Any request that returns a 4XX or 5XX HTTP code is recorded in the error file. Each line is JSON formatted with ``"request"`` and ``"response"`` fields, example pretty-printed below.

.. code-block:: text

    {
        "request": {
            "url": "https://example.com:443/path",
            "postdata": "mypayload"
        },
        "response" : {
            "status": 400,
            "message": "error string"
        }
    }

It is intended that a full replay of failed data is possible by processing this file.

Module usage
------------
.. _omhttp.parameter.module.errorfile-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       errorFile="/var/log/rsyslog/omhttp.error"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
