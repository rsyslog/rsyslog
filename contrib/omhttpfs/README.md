OmHTTPFS
===
Author: sskaje ([sskaje@gmail.com](mailto:sskaje@gmail.com), [http://sskaje.me/](http://sskaje.me/))

OmHTTPFS is an Rsyslog plugin writing data to HDS via *Hadoop HDFS over HTTP*.


## Hadoop HDFS over HTTP

Official site: [Hadoop HDFS over HTTP](http://hadoop.apache.org/docs/current/hadoop-hdfs-httpfs/index.html)

HTTPFS is not well documented. I tried to read its source and write an [intro with examples](http://sskaje.me/2014/08/doc-for-httpfs/), until I found [Administering the file system by using HttpFS REST APIs](http://www-01.ibm.com/support/knowledgecenter/SSPT3X_2.1.2/com.ibm.swg.im.infosphere.biginsights.admin.doc/doc/admin_fileupload_rest_apis.html) by IBM.


## OmHDFS for Rsyslog
Rsyslog provides a plugin named omHDFS which requires lots of work compiling and configuring, and it's not that usable.
Here is what I tried before writing this omhttpfs: [Build omhdfs for Rsyslog](http://sskaje.me/2014/08/build-omhdfs-rsyslog/).


## Rsyslog config
Legacy config **NOT** supported.


Example:

```
module(load="omhttpfs")
template(name="hdfs_tmp_file" type="string" string="/tmp/%$YEAR%/test.log")
template(name="hdfs_tmp_filecontent" type="string" string="%$YEAR%-%$MONTH%-%$DAY% %MSG% ==\n")
local4.*    action(type="omhttpfs" host="10.1.1.161" port="14000" https="off" file="hdfs_tmp_file" isDynFile="on")
local5.*    action(type="omhttpfs" host="10.1.1.161" port="14000" https="off" file="hdfs_tmp_file" isDynFile="on" template="hdfs_tmp_filecontent")
```

Tested with CDH 5.2.0 + Rsyslog 8.6.0 on CentOS 7

\# EOF
