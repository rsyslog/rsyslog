*******
mmcount
*******

===========================  ===========================================================================
**Module Name:**             **mmcount**
**Author:**                  Bala.FA <barumuga@redhat.com>
**Available since:**         7.5.0
===========================  ===========================================================================


**Status:**\ Non project-supported module - contact author or rsyslog
mailing list for questions


Purpose
=======

Message modification plugin which counts messages.
        
This module provides the capability to count log messages by severity
or json property of given app-name. The count value is added into the
log message as json property named 'mmcount'.
 

Examples
========

Example usage of the module in the configuration file.

.. code-block:: none

   module(load="mmcount")

   # count each severity of appname gluster
   action(type="mmcount" appname="gluster")

   # count each value of gf_code of appname gluster
   action(type="mmcount" appname="glusterd" key="!gf_code")

   # count value 9999 of gf_code of appname gluster
   action(type="mmcount" appname="glusterfsd" key="!gf_code" value="9999")

   # send email for every 50th mmcount
   if $app-name == 'glusterfsd' and $!mmcount <> 0 and $!mmcount % 50 == 0 then {
      $ActionMailSMTPServer smtp.example.com
      $ActionMailFrom rsyslog@example.com
      $ActionMailTo glusteradmin@example.com
      $template mailSubject,"50th message of gf_code=9999 on %hostname%"
      $template mailBody,"RSYSLOG Alert\r\nmsg='%msg%'"
      $ActionMailSubject mailSubject
      $ActionExecOnlyOnceEveryInterval 30
      :ommail:;RSYSLOG_SyslogProtocol23Format
   }


