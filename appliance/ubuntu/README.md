Note: we currently focus on the Alpine container, because

- Alpine is frequently used in docker due to the small footprint
- We have a firm grip on Alpine packaging and can quickly build
  ourselfs all components that we need (quick turn-around cycle)
- the appliance container is not yet mature, so duplicating work
  sounds counter-productive

Once the Alpine container is really production grade and if there is
demand for others, we plan to create appliances with other base OS.
Ubuntu and CentOS would be choices.

see also: https://github.com/rsyslog/rsyslog-docker/issues/3
