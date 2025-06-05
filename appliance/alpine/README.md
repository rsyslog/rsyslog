**Deprecated** - This directory contains the original Alpine-based appliance and is kept for reference only.

## Configuring the syslog appliance

### Files

Container and rsyslog configuration are read from the /config directory. You should
mount it to a volume:

  $ docker run -v myconfig:/config ...

Upon initial creation of the volume, it is populated with a default file that
you can modify. Note that this happens only for **volume** mounts.

To show the current container config, run

  $ docker run ... tools/show-config

You can also use this to get a template for your own container config if you use
bind mounts instead of volumes. In this case, make sure that you have **not**
mounted /config - the container will then use it's own default file.

Note: volumes are automatically populated with the default file upon creation.

### Environment Variables

- TZ

  Default: /etc/localtime

  Change it to set a specific timezone, e.g. TZ=UTC

- RSYSLOG_CONF

  Default: /etc/rsyslog.conf

  If you want to totally replace the default rsyslog configuration with
  your custom config,

  1. create a config file in /config volume or bind mount, e.g. myrsyslog.conf
  2. set RSYSLOG_CONF=/config/myrsyslog.conf

  Keep in mind that the myconfig: volume is accessible via /config inside the
  container.

- LOGSENE_TOKEN

  Default: disabled

  If you are using Sematext Logsene, set this to your Logsene token.

- LOGSENE_URL

  Default: disabled

  If you are using Sematext Logsene, set this to the Logsene URL. Ex: logsene-receiver.sematext.com or logsene-receiver.eu.sematext.com

- RSYSLOG_CONFIG_BASE64

  Default: disabled

  If you would like to overwite the `/etc/rsyslog.conf` file, _without_ mounting a configuration file into the container, you can use this variable. The contents are the base64 encoded `rsyslog.conf` file contents, without newlines. This can be generated with the following command: `cat rsyslog.conf | base64 | tr -d '\n'`. On startup, the contents of the environment variable will be decoded and overwrite the `/etc/rsyslog.conf` file.

# Runtime Environment

## Volumes

### /config

Holds the container configuration, also the recommended place for overwriting
the rsyslog configuration.

This volume can be mounted read-only after initial population with sample files.

### /work

The rsyslog work directory. This is used for spool files and other files that
rsyslog needs to be persisted over runs.

This volume needs to be mounted writable and **must** be persisted between
container invocations.

**Warning: this volume is specific to one rsyslog instance.** It **must not**
be shared between multiple container instances, else strange problems may
occur.

### /logs

This holds log files if the container is configured to write them.

Needs to be mounted writable.
