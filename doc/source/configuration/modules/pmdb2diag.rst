**************************************
pmdb2diag: DB2 Diag file parser module
**************************************

===========================  ===========================================================================
**Module Name:**             **pmdb2diag**
**Authors:**                 Jean-Philippe Hilaire <jean-philippe.hilaire@pmu.fr> & Philippe Duveau <philippe.duveau@free.fr>
===========================  ===========================================================================


Purpose
=======

The objective of this module is to extract timestamp, procid and appname form the log
lines without altering it

The parser is acting after an imfile input. This implies that imfile must be configured
with needParse to set to on.

Compile
=======

To successfully compile pmdb2diag module you need to add it via configure.

    ./configure --enable-pmdb2diag ...

Configuration Parameters
========================

**Parser Name:** "db2.diag"

The default value of parameter are defined with escapeLF on in imfile.

timeformat
^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "see strptime manual","%Y-%m-%d-%H.%M.%S."

Format of the timestamp in d2diag log included decimal separator between seconds and second fractions.

timepos
^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "integer", "no", ,"0"

Position of the timestamp in the db2 diag log.

levelpos
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "integer", "no", ,"59"

Position of the severity (level) in the db2 diag log.

pidstarttoprogstartshift
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "integer", "no", ,"49"

Position of the prog related to the pid (form beginning to beginning) in the db2 diag log.

Examples
========

Example 1
^^^^^^^^^

This is the simplest parsing with default values

.. code-block:: none

    module(load="pmdb2diag")
    ruleset(name="ruleDB2" parser="db2.diag") {
        ... do something
    }
    input(type="imfile" file="db2diag.log" ruleset="ruleDB2" tag="db2diag" 
        startmsg.regex="^[0-9]{4}-[0-9]{2}-[0-9]{2}" escapelf="on" needparse="on")


Example 2
^^^^^^^^^

Parsing with custom values

.. code-block:: none

    module(load="pmdb2diag")
    parser(type="pmdb2diag" name="custom.db2.diag" levelpos="57" 
        timeformat=""%y-%m-%d:%H.%M.%S.")
    ruleset(name="ruleDB2" parser="custom.db2.diag") {
        ... do something
    }
    input(type="imfile" file="db2diag.log" ruleset="ruleDB2" tag="db2diag" 
        startmsg.regex="^[0-9]{4}-[0-9]{2}-[0-9]{2}" escapelf="on" needparse="on")

DB2 Log sample
^^^^^^^^^^^^^^

.. code-block:: none

	2015-05-06-16.53.26.989402+120 E1876227378A1702     LEVEL: Info
	PID     : 4390948              TID : 89500          PROC : db2sysc 0
	INSTANCE: db2itst              NODE : 000           DB   : DBTEST
	APPHDL  : 0-14410              APPID: 10.42.2.166.36261.150506120149
	AUTHID  : DBUSR                HOSTNAME: dev-dbm1
	EDUID   : 89500                EDUNAME: db2agent (DBTEST) 0
	FUNCTION: DB2 UDB, relation data serv, sqlrr_dispatch_xa_request, probe:703
	MESSAGE : ZRC=0x80100024=-2146435036=SQLP_NOTA "Transaction was not found"
		  DIA8036C XA error with request type of "". Transaction was not found.
	DATA #1 : String, 27 bytes
	XA Dispatcher received NOTA
	CALLSTCK: (Static functions may not be resolved correctly, as they are resolved to the nearest symbol)
	  [0] 0x090000000A496B70 sqlrr_xrollback__FP14db2UCinterface + 0x11E0
	  [1] 0x090000000A356764 sqljsSyncRollback__FP14db2UCinterface + 0x6E4
	  [2] 0x090000000C1FAAA8 sqljsParseRdbAccessed__FP13sqljsDrdaAsCbP13sqljDDMObjectP14db2UCinterface + 0x529C
	  [3] 0x0000000000000000 ?unknown + 0x0
	  [4] 0x090000000C23D260 @72@sqljsSqlam__FP14db2UCinterfaceP8sqeAgentb + 0x1174
	  [5] 0x090000000C23CE54 @72@sqljsSqlam__FP14db2UCinterfaceP8sqeAgentb + 0xD68
	  [6] 0x090000000D74AB90 @72@sqljsDriveRequests__FP8sqeAgentP14db2UCconHandle + 0xA8
	  [7] 0x090000000D74B6A0 @72@sqljsDrdaAsInnerDriver__FP18SQLCC_INITSTRUCT_Tb + 0x5F8
	  [8] 0x090000000B8F85AC RunEDU__8sqeAgentFv + 0x48C38
	  [9] 0x090000000B876240 RunEDU__8sqeAgentFv + 0x124
	  [10] 0x090000000CD90DFC EDUDriver__9sqzEDUObjFv + 0x130
	  [11] 0x090000000BE01664 sqloEDUEntry + 0x390
	  [12] 0x09000000004F5E10 _pthread_body + 0xF0
	  [13] 0xFFFFFFFFFFFFFFFC ?unknown + 0xFFFFFFFF
