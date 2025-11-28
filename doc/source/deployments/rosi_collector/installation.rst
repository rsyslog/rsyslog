.. _rosi-collector-installation:

Installation
============

.. index::
   pair: ROSI Collector; installation
   single: Docker Compose
   single: TLS

This guide walks through deploying ROSI Collector on a server. By the end,
you'll have a working log collection stack with Grafana dashboards.

Prerequisites
-------------

**Server Requirements**

- Linux server (Ubuntu 22.04+ recommended)
- Docker Engine 20.10 or later
- Docker Compose v2.0 or later
- 2+ CPU cores, 4+ GB RAM
- 50+ GB disk space

**Network Requirements**

- Domain name pointing to your server (for TLS)
- Ports 80, 443, 10514 accessible from clients
- Port 9100 accessible to the server (for client metrics)

**DNS Setup**

Create these DNS records pointing to your server:

- ``logs.example.com`` (or your chosen domain)
- ``grafana.logs.example.com`` (Grafana subdomain)

Step 1: Get the Files
---------------------

Clone the rsyslog repository or download the rosi-collector directory:

.. code-block:: bash

   git clone https://github.com/rsyslog/rsyslog.git
   cd rsyslog/deploy/docker-compose/rosi-collector

Or download just the deployment files:

.. code-block:: bash

   wget https://github.com/rsyslog/rsyslog/archive/refs/heads/main.zip
   unzip main.zip
   cd rsyslog-main/deploy/docker-compose/rosi-collector

Step 2: Configure Environment
-----------------------------

Copy the environment template and configure it:

.. code-block:: bash

   cp .env.template .env
   nano .env

Required settings:

.. code-block:: bash

   # Your main domain
   TRAEFIK_DOMAIN=logs.example.com
   
   # Email for Let's Encrypt notifications
   TRAEFIK_EMAIL=admin@example.com
   
   # Strong password for Grafana admin
   GRAFANA_ADMIN_PASSWORD=your-secure-password-here

Optional settings:

.. code-block:: bash

   # Grafana subdomain (default: grafana.${TRAEFIK_DOMAIN})
   GRAFANA_DOMAIN=grafana.logs.example.com
   
   # Write logs to JSON file (on/off)
   WRITE_JSON_FILE=off
   
   # Email alerting (true/false)
   SMTP_ENABLED=false
   SMTP_HOST=smtp.gmail.com
   SMTP_PORT=587
   SMTP_USER=alerts@example.com
   SMTP_PASSWORD=app-specific-password
   ALERT_EMAIL_FROM=alerts@example.com
   ALERT_EMAIL_TO=oncall@example.com

Step 3: Start the Stack
-----------------------

Start all services with Docker Compose:

.. code-block:: bash

   docker compose up -d

Check that all containers are running:

.. code-block:: bash

   docker compose ps

Expected output:

.. code-block:: text

   NAME                  STATUS
   rosi-grafana-1        Up
   rosi-loki-1           Up
   rosi-prometheus-1     Up
   rosi-rsyslog-1        Up
   rosi-traefik-1        Up

View startup logs:

.. code-block:: bash

   docker compose logs -f

Press ``Ctrl+C`` to stop following logs.

Step 4: Verify Deployment
-------------------------

**Check Traefik Dashboard** (optional)

Traefik provides a dashboard at ``https://traefik.your-domain.com``.

**Check Grafana**

1. Open ``https://grafana.your-domain.com`` in a browser
2. Log in with username ``admin`` and your configured password
3. Navigate to Dashboards to see pre-provisioned dashboards

**Check Loki**

.. code-block:: bash

   curl http://localhost:3100/ready
   # Expected: ready

**Check Prometheus**

.. code-block:: bash

   curl http://localhost:9090/-/healthy
   # Expected: Prometheus Server is Healthy.

Step 5: Configure Firewall
--------------------------

Ensure your firewall allows the required ports:

.. code-block:: bash

   # Using ufw
   sudo ufw allow 80/tcp
   sudo ufw allow 443/tcp
   sudo ufw allow 10514/tcp

   # Using firewalld
   sudo firewall-cmd --permanent --add-port=80/tcp
   sudo firewall-cmd --permanent --add-port=443/tcp
   sudo firewall-cmd --permanent --add-port=10514/tcp
   sudo firewall-cmd --reload

Step 6: (Optional) Systemd Service
----------------------------------

Create a systemd service for automatic startup:

.. code-block:: bash

   # Get your install directory from config (or use default)
   INSTALL_DIR=$(grep '^INSTALL_DIR=' ~/.config/rsyslog/rosi-collector.conf 2>/dev/null | cut -d= -f2 || echo '/opt/rosi-collector')
   
   sudo tee /etc/systemd/system/rosi-collector.service << EOF
   [Unit]
   Description=ROSI Collector Docker Compose Stack
   Requires=docker.service
   After=docker.service
   
   [Service]
   Type=oneshot
   RemainAfterExit=yes
   WorkingDirectory=${INSTALL_DIR}
   ExecStart=/usr/bin/docker compose up -d
   ExecStop=/usr/bin/docker compose down
   
   [Install]
   WantedBy=multi-user.target
   EOF
   
   sudo systemctl daemon-reload
   sudo systemctl enable rosi-collector

**Configuration Persistence**: The ``init.sh`` script saves your chosen
install directory to ``~/.config/rsyslog/rosi-collector.conf``. Both
``init.sh`` and ``monitor.sh`` automatically read this configuration.

Local Development (No TLS)
--------------------------

For local testing without a domain, modify ``docker-compose.yml`` to
expose services directly:

1. Comment out the Traefik service labels
2. Add port mappings to services:

.. code-block:: yaml

   grafana:
     ports:
       - "3000:3000"
   
   prometheus:
     ports:
       - "9090:9090"

Access Grafana at ``http://localhost:3000``.

Upgrading
---------

To upgrade to a newer version:

.. code-block:: bash

   cd /path/to/rosi-collector
   
   # Pull latest images
   docker compose pull
   
   # Restart with new images
   docker compose up -d

Data in volumes is preserved across upgrades.

Uninstalling
------------

To remove the stack and all data:

.. code-block:: bash

   # Stop and remove containers
   docker compose down
   
   # Remove volumes (deletes all data!)
   docker compose down -v
   
   # Remove images
   docker compose down --rmi all

Next Steps
----------

- :doc:`client_setup` - Configure clients to send logs
- :doc:`grafana_dashboards` - Explore the pre-built dashboards
- :doc:`troubleshooting` - Common issues and solutions
