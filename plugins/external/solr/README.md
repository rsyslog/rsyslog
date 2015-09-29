What?
=====
A plugin to push messages to Solr.

How?
====
* make sure you have omprog loaded and at least one input defined:

```
module(load="omprog")
module(load="imfile")
input(
  type="imfile"
  File="/opt/logs/example.log"
  Tag="tag1"
)
```

* define a JSON template in rsyslog, like

```
template(name="json_lines" type="list" option.json="on") {
  constant(value="{")
  constant(value="\"timestamp\":\"")
  property(name="timereported" dateFormat="rfc3339")
  constant(value="\",\"message\":\"")
  property(name="msg")
  constant(value="\",\"host\":\"")
  property(name="hostname")
  constant(value="\",\"severity\":\"")
  property(name="syslogseverity-text")
  constant(value="\",\"facility\":\"")
  property(name="syslogfacility-text")
  constant(value="\",\"syslog-tag\":\"")
  property(name="syslogtag")
  constant(value="\"}\n")
}
```


* use omprog to call the python plugin, like

```
action(
  type="omprog"
  binary="/opt/rsyslog/rsyslog_solr.py"
  template="json_lines"
)
```

* use queue parameters for buffering and to control the number of external processes spawned. Here's an example of a main queue using 4 threads (which will spawn 4 Python processes) working on a disk-assisted queue of up to 100K messages in memory and 5GB of disk space

```
main_queue(
  queue.workerthreads="4"  # threads to work on the queue (and external processes to spawn)
  queue.workerthreadminimummessages="1"  # keep all 4 processes up pretty much all the time (at least 1 message in the queue)
  queue.filename="solr_action"     # write to disk if needed (disk-assisted)
  queue.maxfilesize="100m"        # max size per single queue file
  queue.spoolDirectory="/opt/rsyslog/queues" # where to write queue files
  queue.maxdiskspace="5g"         # up to 5g to be written on disk
  queue.highwatermark="100000"   # start writing to disk when this many messages are stored in memory
  queue.lowwatermark="50000"    # when writing to disk reduced the in-memory queue to this much, stop spilling to disk
  queue.dequeueBatchSize="500"  # max number of messages to process at once in the queue
  
  queue.size="10000000"          # absolute max queue size (should never be reached)
  queue.saveonshutdown="on"    # write queue contents to disk on shutdown
)
```

* change the Python script to point to your Solr collection (also make sure it's executable!):

```
# host and port
solrServer = "localhost"
solrPort = 8983

# how many documents to send to Solr in a single update
maxAtOnce = 5000

# path to the update handler. "gettingstarted" is a collection name. Use aliases for time-based collections
solrUpdatePath = "/solr/gettingstarted/update"
```

* restart rsyslog and have fun

Improvements?
=============
Improvements to this doc and the plugin itself are very welcome. Just
send us a patch!
