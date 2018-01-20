## Configuring the syslog appliance

### Files

Container and rsyslog configuration are read from the /config directory. You should
mount it to a volume:

  $ docker run -v myconfig:/config ...

Upon initial creation of the volume, it is populated with a default file that
you can modify. Note that this happens only for **volume** mounts.

### Environment Variables

- TZ
  Default: /etc/localtime
  Change it to set a specific timezone, e.g. TZ=UTC

- RSYSLOG_CONF
  Default: /etc/rsyslog.cong
  If you want to totally replace the default rsyslog configuration with
  your custom config,

  1. create a config file in myconfig: volume or bind mount, e.g. myrsyslog.conf
  2. set RSYSLOG_CONF=/config/myrsyslog.conf

  Keep in mind that the myconfig: volume is accessible via /config inside the
  container.
