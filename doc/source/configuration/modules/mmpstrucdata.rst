RFC5424 structured data parsing module (mmpstrucdata)
=====================================================

**Module Name:** mmpstrucdata

**Author:** Rainer Gerhards <rgerhards@adiscon.com>

**Available since**: 7.5.4

**Description**:

The mmpstrucdata parses the structured data of `RFC5424 <https://datatracker.ietf.org/doc/html/rfc5424>`_ into the message json variable tree. The data parsed, if available, is stored under "jsonRoot!rfc5424-sd!...". Please note that only RFC5424 messages will be processed.

The difference of RFC5424 is in the message layout: the SYSLOG-MSG part only contains the structured-data part instead of the normal message part. Further down you can find a example of a structured-data part.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

Module Parameters
-----------------

This module has no module parameters.

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmpstrucdata-jsonroot`
     - .. include:: ../../reference/parameters/mmpstrucdata-jsonroot.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmpstrucdata-sd_name.lowercase`
     - .. include:: ../../reference/parameters/mmpstrucdata-sd_name.lowercase.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

**See Also**

-  `Howto anonymize messages that go to specific
   files <http://www.rsyslog.com/howto-anonymize-messages-that-go-to-specific-files/>`_

**Caveats/Known Bugs:**

-  this module is currently experimental; feedback is appreciated
-  property names are treated case-insensitive in rsyslog. As such,
   RFC5424 names are treated case-insensitive as well. If such names
   only differ in case (what is not recommended anyways), problems will
   occur.
-  structured data with duplicate SD-IDs and SD-PARAMS is not properly
   processed

**Samples:**

Below you can find a structured data part of a random message which has three parameters.

::

  [exampleSDID@32473 iut="3" eventSource="Application"eventID="1011"]


In this snippet, we parse the message and emit all json variable to a
file with the message anonymized. Note that once mmpstrucdata has run,
access to the original message is no longer possible (except if stored
in user variables before anonymization).

::

  module(load="mmpstrucdata") action(type="mmpstrucdata")
  template(name="jsondump" type="string" string="%msg%: %$!%\\n")
  action(type="omfile" file="/path/to/log" template="jsondump")


**A more practical one:**

Take this example message (inspired by RFC5424 sample;)):

``<34>1 2003-10-11T22:14:15.003Z mymachine.example.com su - ID47 [exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"][id@2 test="test"] BOM'su root' failed for lonvick on /dev/pts/8``

We apply this configuration:

::

  module(load="mmpstrucdata") action(type="mmpstrucdata")
  template(name="sample2" type="string" string="ALL: %$!%\\nSD:
  %$!RFC5424-SD%\\nIUT:%$!rfc5424-sd!exampleSDID@32473!iut%\\nRAWMSG:
  %rawmsg%\\n\\n") action(type="omfile" file="/path/to/log"
  template="sample2")



This will output:

``ALL: { "rfc5424-sd": { "examplesdid@32473": { "iut": "3", "eventsource": "Application", "eventid": "1011" }, "id@2": { "test": "test" } } } SD: { "examplesdid@32473": { "iut": "3", "eventsource": "Application", "eventid": "1011" }, "id@2": { "test": "test" } } IUT:3 RAWMSG: <34>1 2003-10-11T22:14:15.003Z mymachine.example.com su - ID47 [exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"][id@2 test="test"] BOM'su root' failed for lonvick on /dev/pts/8``

As you can seem, you can address each of the individual items. Note that
the case of the RFC5424 parameter names has been converted to lower
case.

.. toctree::
   :hidden:

   ../../reference/parameters/mmpstrucdata-jsonroot
   ../../reference/parameters/mmpstrucdata-sd_name.lowercase

