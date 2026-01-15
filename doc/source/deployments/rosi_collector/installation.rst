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

- Linux server (Ubuntu 24.04 recommended, tested)
- Docker Engine 20.10 or later
- Docker Compose v2.0 or later
- 2+ CPU cores, 4+ GB RAM
- 50+ GB disk space

.. note::
   The installation scripts (``init.sh``, ``install-server.sh``) have been
   tested on Ubuntu 24.04 LTS. Other Debian-based distributions should work
   with minor adjustments. RHEL/CentOS-based systems may require additional
   configuration for firewall (firewalld) and package management.

**Network Requirements**

- Domain name pointing to your server (for TLS)
- Ports 80, 443, 10514 accessible from clients
- Port 9100 accessible to the server (for client metrics)

**DNS Setup**

Create a DNS record pointing to your server:

- ``logs.example.com`` (or your chosen domain)

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

Step 2: Initialize Environment
------------------------------

Run the initialization script (as root):

.. code-block:: bash

   sudo TRAEFIK_DOMAIN=logs.example.com \
        TRAEFIK_EMAIL=admin@example.com \
        ./scripts/init.sh

This script will:

- Prompt for installation directory (default: ``/opt/rosi-collector``)
- Copy all configuration files to the installation directory
- Generate ``.env`` with secure passwords
- Create Docker network
- Set up systemd service
- Install CLI management tools (``rosi-monitor``, ``prometheus-target``)

**Custom Grafana password**: Add ``GRAFANA_ADMIN_PASSWORD=your-password``
to the command.

**Configuration Persistence**: Your chosen install directory is saved to
``~/.config/rsyslog/rosi-collector.conf`` and automatically used for future
runs.

Config file locations (in priority order):

1. Environment variable: ``INSTALL_DIR=/path ./scripts/init.sh``
2. User config: ``~/.config/rsyslog/rosi-collector.conf``
3. System config: ``/etc/rsyslog/rosi-collector.conf``
4. Default: ``/opt/rosi-collector``

Optional settings in ``.env``:


.. list-table:: rsyslog Configuration
   :header-rows: 1
   :widths: 35 65

   * - Variable
     - Description
   * - ``WRITE_JSON_FILE``
     - Write logs to JSON file in addition to Loki (``on``/``off``, default: ``off``)

.. list-table:: Email Alerting Configuration
   :header-rows: 1
   :widths: 35 65

   * - Variable
     - Description
   * - ``SMTP_ENABLED``
     - Enable email alerting (``true``/``false``, default: ``false``)
   * - ``SMTP_HOST``
     - SMTP server hostname (e.g., ``smtp.gmail.com``)
   * - ``SMTP_PORT``
     - SMTP server port (usually ``587`` for TLS or ``465`` for SSL)
   * - ``SMTP_USER``
     - SMTP authentication username
   * - ``SMTP_PASSWORD``
     - SMTP authentication password
   * - ``SMTP_SKIP_VERIFY``
     - Skip TLS certificate verification (``true``/``false``, default: ``false``)
   * - ``ALERT_EMAIL_FROM``
     - Email address for sending alerts
   * - ``ALERT_EMAIL_TO``
     - Email address(es) to receive alerts

.. list-table:: TLS Syslog Configuration
   :header-rows: 1
   :widths: 35 65

   * - Variable
     - Description
   * - ``SYSLOG_TLS_ENABLED``
     - Enable TLS encrypted syslog on port 6514 (``true``/``false``, default: ``false``)
   * - ``SYSLOG_TLS_HOSTNAME``
     - Server hostname for TLS certificate. Clients use this to connect.
       Should match ``TRAEFIK_DOMAIN`` or a dedicated logs subdomain.
   * - ``SYSLOG_TLS_CA_DAYS``
     - CA certificate validity in days (default: ``3650`` = 10 years)
   * - ``SYSLOG_TLS_SERVER_DAYS``
     - Server certificate validity in days (default: ``365``)
   * - ``SYSLOG_TLS_CLIENT_DAYS``
     - Client certificate validity in days (default: ``365``)
   * - ``SYSLOG_TLS_AUTHMODE``
     - TLS authentication mode: ``anon`` (server-only), ``x509/certvalid``
       (mTLS, any valid cert), or ``x509/name`` (mTLS with name validation).
       Default: ``anon``
   * - ``SYSLOG_TLS_PERMITTED_PEERS``
     - For ``x509/name`` mode: comma-separated list of permitted client
       certificate names. Supports wildcards (e.g., ``*.example.com``).
   * - ``SYSLOG_TLS_TOKEN_VALIDITY``
     - One-time download token validity in seconds (default: ``300`` = 5 minutes)

Step 3: Start the Stack
-----------------------

Change to the installation directory and start all services:

.. code-block:: bash

   cd /opt/rosi-collector  # or your chosen install directory
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

**Using the Monitor Script**

After running ``init.sh``, the ``rosi-monitor`` command is available system-wide:

.. code-block:: bash

   rosi-monitor status   # Show container status and Docker internal IPs
   rosi-monitor logs     # Show recent logs
   rosi-monitor health   # Quick health check
   rosi-monitor debug    # Interactive debug menu

The ``status`` command displays:

- Docker Compose container status
- Individual container health
- Docker network information (network name, subnet, gateway)
- Internal container IPs (useful for debugging connectivity)
- Resource usage (CPU, memory, network I/O)

**Check Traefik Dashboard** (optional)

Traefik provides a dashboard at ``https://traefik.your-domain.com``.

**Check Grafana**

1. Open ``https://your-domain.com`` in a browser
2. Log in with username ``admin`` and your configured password
3. Navigate to Dashboards to see pre-provisioned dashboards

**Check Loki**

.. code-block:: bash

   curl http://localhost:3100/ready
   # Expected: ready

**Check Prometheus**

.. code-block:: bash

   docker compose ps prometheus
   # Expected: Status "Up" with health "healthy"

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

Step 6: (Optional) Enable TLS Encryption
-----------------------------------------

ROSI Collector supports TLS-encrypted syslog on port 6514. This encrypts
log traffic between clients and the collector.

**Enable TLS in .env**

.. code-block:: bash

   # Enable TLS (auto-generates certificates on first run)
   SYSLOG_TLS_ENABLED=true
   
   # Hostname for the TLS certificate (clients connect to this)
   SYSLOG_TLS_HOSTNAME=logs.example.com

**Choose Authentication Mode**

Three modes are available:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Mode
     - Description
   * - ``anon``
     - Server-only auth. Clients verify the server but anyone can send logs.
       Good for internal networks.
   * - ``x509/certvalid``
     - Mutual TLS. Clients must have valid certificates signed by the CA.
       Any client with a valid cert can connect.
   * - ``x509/name``
     - Mutual TLS with name validation. Client cert CN must match
       ``SYSLOG_TLS_PERMITTED_PEERS`` patterns.

Configure in ``.env``:

.. code-block:: bash

   # Server-only auth (default)
   SYSLOG_TLS_AUTHMODE=anon
   
   # OR: mTLS with any valid CA-signed cert
   SYSLOG_TLS_AUTHMODE=x509/certvalid
   
   # OR: mTLS with specific permitted clients
   SYSLOG_TLS_AUTHMODE=x509/name
   SYSLOG_TLS_PERMITTED_PEERS=*.example.com,server1.myorg.com

**Run Init Script**

The init script generates CA and server certificates automatically:

.. code-block:: bash

   sudo ./scripts/init.sh

**Restart the Stack**

After enabling TLS, restart the stack:

.. code-block:: bash

   docker compose up -d

The ``rsyslog-tls`` container will automatically start on port 6514
when ``SYSLOG_TLS_ENABLED=true``.

**Generate Client Certificates**

For mTLS modes, generate client certificates:

.. code-block:: bash

   # Generate cert with secure download package
   sudo rosi-generate-client-cert --download webserver1
   
   # Lists download URL with one-time token

Clients download the package, which includes certificates and rsyslog config.

**Open Firewall Port**

.. code-block:: bash

   sudo ufw allow 6514/tcp

See :doc:`client_setup` for configuring clients with TLS.

Step 7: (Optional) Systemd Service
----------------------------------

The ``init.sh`` script creates a systemd service automatically. If you need
to create it manually:

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

**For TLS syslog**: TLS is automatically enabled when ``SYSLOG_TLS_ENABLED=true``
in your ``.env`` file. No manual service override is needed.

**Configuration Persistence**: The ``init.sh`` script saves your chosen
install directory to ``~/.config/rsyslog/rosi-collector.conf``. Both
``init.sh`` and ``rosi-monitor`` automatically read this configuration.

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
