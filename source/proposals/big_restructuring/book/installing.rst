Installing and configuring Rsyslog
==================================

General procedures to install and configure.


Installing from packages
------------------------

How to install using apt-get, yum, etc.


Installing from sources
-----------------------

How to compile the sources into your system.

Testing configuration blocks


    .. code-block:: bash

        #### MODULES ####

        # Load (i)nput and (o)utput (m)odules
        module(load="imuxsock")
        module(load="imklog")
        module(load="imudp")
        module(load="imtcp")
        module(load="imrelp")
        module(load="omrelp")
        module(load="impstats" interval="3600" severity="7" log.syslog="off" log.file="/var/log/rsyslog-stats.log")

        # Module parameters
        input(type="imrelp" port="1514" ruleset="remote")
        input(type="imtcp" port="514" ruleset="remote")
        input(type="imudp" port="514" ruleset="remote")

        #### GLOBAL DIRECTIVES ####

        # Use default timestamp format
        $ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat

        # Spool files
        $WorkDirectory /var/spool/rsyslog

        # Filter duplicate messages
        $RepeatedMsgReduction on

        #### RULES ####

        #...cut out standard log rules for brevity...#

        ruleset(name="remote"){

                   action(Name="storage"
                         Type="omrelp"
                         Target="10.1.1.100"
                         Port="514"
                         Action.ExecOnlyWhenPreviousIsSuspended="on"
                         queue.FileName="storage-buffer"
                         queue.SaveOnShutdown="on"
                         queue.Type="LinkedList"
                         Action.ResumeInterval="30"
                         Action.ResumeRetryCount="-1"
                         Timeout="5")

                   action(Name="analysis"
                         Type="omrelp"
                         Target="10.1.1.101"
                         Port="514"
                         Action.ExecOnlyWhenPreviousIsSuspended="on"
                         queue.FileName="analysis-buffer"
                         queue.SaveOnShutdown="on"
                         queue.Type="LinkedList"
                         Action.ResumeInterval="30"
                         Action.ResumeRetryCount="-1"
                         Timeout="5")

                    action(Name="indexer"
                         Type="omfwd"
                         Target="10.1.1.102"
                         Protocol="tcp"
                         Port="514"
                         Action.ExecOnlyWhenPreviousIsSuspended="on"
                         queue.FileName="indexer-buffer"
                         queue.SaveOnShutdown="on"
                         queue.Type="LinkedList"
                         Action.ResumeInterval="30"
                         Action.ResumeRetryCount="-1"
                         Timeout="5")
        }

        #### INCLUDES ####

        # Includes config files (Do these last)
        $IncludeConfig /etc/rsyslog.d/*.conf


.. note::

   You'll learn exactly how to load each file/format in the next section.

.. option:: dest_dir

   Destination directory.

.. option:: -m <module>, --module <module>

   Run a module as a script.

.. envvar:: nome_envvar

Descrevendo um programa.

.. program:: rm

.. option:: -r

   Work recursively.

.. program:: svn

.. option:: -r revision

   Specify the revision to work upon.

-------------------------------------------------

.. describe:: PAPER

   You can set this variable to select a paper size.

-------------------------------------------------

  todo::

    Este item é do TO DO.

-------------------------------------------------

  todolist::

    none

-------------------------------------------------

FIM
