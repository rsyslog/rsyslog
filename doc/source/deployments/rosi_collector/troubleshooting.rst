.. _rosi-collector-troubleshooting:

Troubleshooting
===============

.. index::
   pair: ROSI Collector; troubleshooting
   single: debugging
   single: logs

This guide covers common issues with ROSI Collector and how to resolve them.

.. figure:: /_static/docker-logs.png
   :alt: Docker logs output
   :align: center
   
   Monitor script showing container status

Diagnostic Commands
-------------------

Start troubleshooting with these commands:

.. code-block:: bash

   # Check container status
   docker compose ps
   
   # View recent logs
   docker compose logs --tail=100
   
   # Follow logs in real-time
   docker compose logs -f
   
   # Check specific service
   docker compose logs grafana
   docker compose logs loki
   docker compose logs rsyslog

Logs Not Appearing in Grafana
-----------------------------

**Symptom**: Clients are configured but no logs appear in Grafana.

**Check 1: rsyslog is receiving logs**

.. code-block:: bash

   docker compose logs rsyslog | tail -20

Look for incoming message indicators or errors.

**Check 2: Network connectivity**

From a client:

.. code-block:: bash

   telnet COLLECTOR_IP 10514
   # Should connect. Type some text and press Enter.
   # Ctrl+] then 'quit' to exit

If connection fails, check firewalls on both client and collector.

**Check 3: rsyslog is sending to Loki**

.. code-block:: bash

   docker compose logs rsyslog | grep -i loki

Look for omhttp connection messages or errors.

**Check 4: Loki is healthy**

.. code-block:: bash

   curl http://localhost:3100/ready
   # Should return: ready
   
   curl http://localhost:3100/metrics | grep loki_ingester

**Check 5: Client rsyslog queue**

On the client:

.. code-block:: bash

   ls -la /var/spool/rsyslog/
   # Growing files indicate delivery problems

Container Won't Start
---------------------

**Symptom**: ``docker compose up`` fails or containers restart repeatedly.

**Check 1: View logs**

.. code-block:: bash

   docker compose logs <service-name>

Look for error messages indicating the cause.

**Check 2: Port conflicts**

.. code-block:: bash

   sudo netstat -tlnp | grep -E "80|443|3000|3100|9090|10514"

If ports are in use, stop conflicting services or change ports in
``docker-compose.yml``.

**Check 3: Disk space**

.. code-block:: bash

   df -h
   docker system df

Remove old containers and images if disk is full:

.. code-block:: bash

   docker system prune -a

**Check 4: Permissions**

Ensure volumes have correct ownership:

.. code-block:: bash

   docker compose down
   sudo chown -R 472:472 ./grafana-data  # Grafana user
   sudo chown -R 10001:10001 ./loki-data  # Loki user
   docker compose up -d

Loki Storage Issues
-------------------

**Symptom**: Loki crashes or queries fail.

**Check 1: Available disk space**

Loki needs space for chunks and index:

.. code-block:: bash

   df -h
   du -sh /var/lib/docker/volumes/*loki*

**Check 2: Retention settings**

If disk is filling up, reduce retention in ``loki-config.yml``:

.. code-block:: yaml

   limits_config:
     retention_period: 168h  # 7 days instead of 30

Restart Loki after changes:

.. code-block:: bash

   docker compose restart loki

**Check 3: Compaction running**

.. code-block:: bash

   docker compose logs loki | grep -i compact

Compaction reclaims space from deleted logs.

Prometheus Scrape Failures
--------------------------

**Symptom**: Client metrics don't appear; targets show as "DOWN".

**Check 1: Targets status in Prometheus**

.. code-block:: bash

   curl http://localhost:9090/api/v1/targets | jq .

Or visit ``http://YOUR_DOMAIN:9090/targets`` in a browser.

**Check 2: Network connectivity to clients**

From the collector:

.. code-block:: bash

   curl http://CLIENT_IP:9100/metrics | head

If this fails:

- Check client firewall allows port 9100 from collector IP
- Verify node_exporter is running on client
- Check for network routing issues

**Check 3: Targets file syntax**

.. code-block:: bash

   cat prometheus-targets/nodes.yml

YAML syntax must be valid. Each target needs proper indentation.

TLS Certificate Problems
------------------------

**Symptom**: Browser shows certificate errors; HTTPS doesn't work.

**Check 1: Traefik logs**

.. code-block:: bash

   docker compose logs traefik | grep -i acme

Look for Let's Encrypt errors.

**Check 2: DNS resolution**

.. code-block:: bash

   dig +short YOUR_DOMAIN

Must resolve to your server's public IP.

**Check 3: Ports 80/443 accessible**

Let's Encrypt needs to reach port 80 for verification:

.. code-block:: bash

   # From external host
   curl http://YOUR_DOMAIN/.well-known/acme-challenge/test

**Check 4: Rate limits**

Let's Encrypt has rate limits. If exceeded, wait an hour and retry.

For testing, use Let's Encrypt staging:

In ``docker-compose.yml``:

.. code-block:: yaml

   traefik:
     command:
       - "--certificatesresolvers.letsencrypt.acme.caserver=https://acme-staging-v02.api.letsencrypt.org/directory"

Performance Issues
------------------

**Symptom**: Queries are slow; Grafana times out.

**Loki Query Optimization**

- Use labels to filter before full-text search
- Reduce time range for initial exploration
- Add ``| line_format`` at end to reduce parsing

Instead of:

.. code-block:: text

   {job="syslog"} |= "error"

Use:

.. code-block:: text

   {job="syslog", host="specific-host"} |= "error"

**Memory Tuning**

Loki benefits from more memory. In ``docker-compose.yml``:

.. code-block:: yaml

   loki:
     deploy:
       resources:
         limits:
           memory: 4G

**Prometheus Cardinality**

High-cardinality labels slow queries. Check:

.. code-block:: bash

   curl http://localhost:9090/api/v1/status/tsdb | jq .data.seriesCountByMetricName

High Alert Volume
-----------------

**Symptom**: Too many alerts firing.

**Adjust thresholds** in ``grafana/provisioning/alerting/default.yml``

**Add silences** for known issues:

1. Go to Grafana → Alerting → Silences
2. Create silence matching the alert labels
3. Set duration

Client Queue Backup
-------------------

**Symptom**: Client's rsyslog queue files growing.

**Check 1: Collector reachable**

.. code-block:: bash

   telnet COLLECTOR_IP 10514

**Check 2: rsyslog status**

.. code-block:: bash

   sudo systemctl status rsyslog
   journalctl -u rsyslog -n 50

**Check 3: Queue configuration**

Increase queue size in client config:

.. code-block:: none

   $ActionQueueMaxDiskSpace 2g
   $ActionQueueLowWaterMark 2000
   $ActionQueueHighWaterMark 8000

Getting Help
------------

If these steps don't resolve your issue:

1. Gather logs: ``docker compose logs > collector-logs.txt``
2. Check rsyslog GitHub issues
3. Ask on the rsyslog mailing list with log excerpts

See Also
--------

- :doc:`../../troubleshooting/index` - General rsyslog troubleshooting
- `Loki Troubleshooting <https://grafana.com/docs/loki/latest/operations/troubleshooting/>`_
- `Prometheus Troubleshooting <https://prometheus.io/docs/prometheus/latest/troubleshooting/>`_
