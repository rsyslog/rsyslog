omoracle: Oracle Database Output Module
=======================================

**Module Name:    omoracle**

**Author:**\ Luis Fernando Muñoz Mejías
<Luis.Fernando.Munoz.Mejias@cern.ch> - this module is currently
orphaned, the original author does no longer support it.

**Available since:**: 4.3.0, **does not work with recent rsyslog 
versions (v7 and up).  Use** :doc:`omlibdbi <omlibdbi>` **instead.**
An upgrade to the new interfaces is needed. If you would like
to contribute, please send us a patch or open a github pull request.

**Status:**: contributed module, not maintained by rsyslog core authors

**Description**:

This module provides native support for logging to Oracle databases. It
offers superior performance over the more generic
`omlibdbi <omlibdbi.html>`_ module. It also includes a number of
enhancements, most importantly prepared statements and batching, what
provides a big performance improvement.

Note that this module is maintained by its original author. If you need
assistance with it, it is suggested to post questions to the `rsyslog
mailing list <http://lists.adiscon.net/mailman/listinfo/rsyslog>`_.

From the header comments of this module:

::


        This is an output module feeding directly to an Oracle
        database. It uses Oracle Call Interface, a proprietary module
        provided by Oracle.

        Selector lines to be used are of this form:

        :omoracle:;TemplateName

        The module gets its configuration via rsyslog $... directives,
        namely:

        $OmoracleDBUser: user name to log in on the database.

        $OmoracleDBPassword: password to log in on the database.

        $OmoracleDB: connection string (an Oracle easy connect or a db
        name as specified by tnsnames.ora)

        $OmoracleBatchSize: Number of elements to send to the DB on each
        transaction.

        $OmoracleStatement: Statement to be prepared and executed in
        batches. Please note that Oracle's prepared statements have their
        placeholders as ':identifier', and this module uses the colon to
        guess how many placeholders there will be.

        All these directives are mandatory. The dbstring can be an Oracle
        easystring or a DB name, as present in the tnsnames.ora file.

        The form of the template is just a list of strings you want
        inserted to the DB, for instance:

        $template TestStmt,"%hostname%%msg%"

        Will provide the arguments to a statement like

        $OmoracleStatement \
            insert into foo(hostname,message)values(:host,:message)

        Also note that identifiers to placeholders are arbitrary. You
        need to define the properties on the template in the correct order
        you want them passed to the statement!

Some additional documentation contributed by Ronny Egner:

::

    REQUIREMENTS:
    --------------

    - Oracle Instantclient 10g (NOT 11g) Base + Devel
      (if you´re on 64-bit linux you should choose the 64-bit libs!) 
    - JDK 1.6 (not necessary for oracle plugin but "make" did not finished successfully without it)

    - "oracle-instantclient-config" script 
      (seems to shipped with instantclient 10g Release 1 but i was unable to find it for 10g Release 2 so here it is)

      
    ======================  /usr/local/bin/oracle-instantclient-config =====================
    #!/bin/sh
    #
    # Oracle InstantClient SDK config file
    # Jean-Christophe Duberga - Bordeaux 2 University
    #

    # just adapt it to your environment
    incdirs="-I/usr/include/oracle/10.2.0.4/client64"
    libdirs="-L/usr/lib/oracle/10.2.0.4/client64/lib"

    usage="\
    Usage: oracle-instantclient-config [--prefix[=DIR]] [--exec-prefix[=DIR]] [--version] [--cflags] [--libs] [--static-libs]"

    if test $# -eq 0; then
          echo "${usage}" 1>&2
          exit 1
    fi

    while test $# -gt 0; do
      case "$1" in
      -*=*) optarg=`echo "$1" | sed 's/[-_a-zA-Z0-9]*=//'` ;;
      *) optarg= ;;
      esac

      case $1 in
        --prefix=*)
          prefix=$optarg
          if test $exec_prefix_set = no ; then
            exec_prefix=$optarg
          fi
          ;;
        --prefix)
          echo $prefix
          ;;
        --exec-prefix=*)
          exec_prefix=$optarg
          exec_prefix_set=yes
          ;;
        --exec-prefix)
          echo ${exec_prefix}
          ;;
        --version)
          echo ${version}
          ;;
        --cflags)
          echo ${incdirs}
          ;;
        --libs)
          echo $libdirs -lclntsh -lnnz10 -locci -lociei -locijdbc10
          ;;
        --static-libs)
          echo "No static libs" 1>&2
          exit 1
          ;;
        *)
          echo "${usage}" 1>&2
          exit 1
          ;;
      esac
      shift
    done

    ===============   END ==============




    COMPILING RSYSLOGD
    -------------------


    ./configure --enable-oracle




    RUNNING
    -------

    - make sure rsyslogd is able to locate the oracle libs (either via LD_LIBRARY_PATH or /etc/ld.so.conf)
    - set TNS_ADMIN to point to your tnsnames.ora
    - create a tnsnames.ora and test you are able to connect to the database

    - create user in oracle as shown in the following example:
            create user syslog identified by syslog default tablespace users quota unlimited on users;
            grant create session to syslog;
            create role syslog_role;
            grant syslog_role to syslog;
            grant create table to syslog_role;
            grant create sequence to syslog_role;
            
    - create tables as needed

    - configure rsyslog as shown in the following example
            $ModLoad omoracle

            $OmoracleDBUser syslog
            $OmoracleDBPassword syslog
            $OmoracleDB syslog
            $OmoracleBatchSize 1
            $OmoracleBatchItemSize 4096

            $OmoracleStatementTemplate OmoracleStatement
            $template OmoracleStatement,"insert into foo(hostname,message) values (:host,:message)"
            $template TestStmt,"%hostname%%msg%"
            *.*                     :omoracle:;TestStmt
        (you guess it: username = password = database = "syslog".... see $rsyslogd_source/plugins/omoracle/omoracle.c for me info)

