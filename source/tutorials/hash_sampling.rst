Hash-based Sampling
===================


Rsyslog supports various sampling mechanisms. These can be used on client systems to save servers from getting overwhelmed. Here we introduce a new sampling mechanism "Hash-based sampling".

Let's consider the following setup of systems running services and generating logs.
There are three services A, B and C.
These services work together to create a request processing pipeline.
User request lands up at system A, which processes the request, generates logs and forwards it to service B.
B processes the request it received from A, generates logs and forwards it to C and so on and so forth.

::

    +-+-+-+-+-+-+      +-+      +-+     +-+      +-+-++-+-+
    |UserRequest|  ->  |A|   -> |B|  -> |C| ->  | Database |
    +-+-+-+-+-+-+      +-+      +-+     +-+      +-+-++-+-+


Consider three sample user requests
    ::

    {"request_id": "reqid1", "data": "payload_1_original"}
    {"request_id": "reqid2", "data": "payload_2_original"}
    {"request_id": "reqid3", "data": "payload_3_original"}

Service A generated logs
    ::

    {"request_id": "reqid1", "datetime": "<time>", "Processing_state" : "A", "Exception" : "none"}
    {"request_id": "reqid2", "datetime": "<time>", "Processing_state" : "A_prime", "Exception" : "Unknown flow"}
    {"request_id": "reqid3", "datetime": "<time>", "Processing_state" : "A_double_prime", "Exception" : "Parsing Failed"}

Service B generated logs
    ::

    {"request_id": "reqid1", "datetime": "<time>", "Processing_state" : "B", "Exception" : "none"}
    {"request_id": "reqid2", "datetime": "<time>", "Processing_state" : "A_Failed_at_1st_Stage", "Exception" : "none"}
    {"request_id": "reqid3", "datetime": "<time>", "Processing_state" : "B_Failed", "Exception" : "Field not found"}

Service C generated logs
    ::

    {"request_id": "reqid1", "datetime": "<time>", "Processing_state" : "C", "Exception" : "none"}
    {"request_id": "reqid2", "datetime": "<time>", "Processing_state" : "NO_OP", "Exception" : "none"}
    {"request_id": "reqid3", "datetime": "<time>", "Processing_state" : "C_Failed", "Exception" : "NullPointer"}


We can sample logs based on request_id thereby getting **all-or-none** logs associated with a request_id. This is a kind of transactional guarantee on logs for a request ID. This helps to create an end to end picture of pipeline even on sampled data.

Using hash64mod for sampling
    ::

     if (hash64mod($!msg!request_id, 100) <= 30) then {
      //send out
     }

With hash64mod sampling, we get sampled logs like
    ::

    {"request_id": "reqid3", "data": "payload_3_original"}
    {"request_id": "reqid3", "datetime": "<time>", "Processing_state" : "A_double_prime", "Exception" : "Parsing Failed"}
    {"request_id": "reqid3", "datetime": "<time>", "Processing_state" : "B_Failed", "Exception" : "Field not found"}
    {"request_id": "reqid3", "datetime": "<time>", "Processing_state" : "C_Failed", "Exception" : "NullPointer"}

We will get all-or-none logs associated to hash-string (request ID in this case or combination of field_1 & field_2 & field_3) giving a full view of a request's life cycle in the cluster across various systems. The only constraint in hash-based sampling is, it will work only if we have the same unique identifier (single field or combination of fields) in logs across services.

Same as hash64mod, we can use hash64 to add hash as tags that can be used later to filter logs.

Using hash64 for filtering later
    ::

     set $.hash = hash64($!msg!field_1 & $!msg!field_2 & $!msg!field_3)
     set $!tag= $syslogtag & $.hash;
     //send out

Hash-based sampling is helpful on a single system too. Say a single process generates 4 log lines for each transaction it commits. We can get all-or-none guarantees around these 4 logs generated for each transaction. Thereby giving a complete picture of a transaction (while sampling).

**Note**
  * Hash-based sampling can be used only when logs have **same unique identifier** across services.
  * Default hash implementation is djb2 hash and xxhash can be enabled using compile-time flag (hash implementation can change release to release, check changelog).
  * For parsing log to generate json to act on individual fields use mmnormalize.
