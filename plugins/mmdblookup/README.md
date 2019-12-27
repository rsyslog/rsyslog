# Rsyslog - MMDBLookup

Fast geoip lookups straight from Rsyslog.

[Maxmind](https://www.maxmind.com/en/home) provides free and pay-for memory mapped ip-lookup databases.
The free city-level database is about 22 mB, and can be found on the [geolite page](http://dev.maxmind.com/geoip/geoip2/geolite2/).


## Compile

1. Ensure you have [libfastjson](https://github.com/rgerhards/libfastjson/) installed, check your package manager or install from source.
2. Ensure you have [libmaxminddb](https://github.com/maxmind/libmaxminddb) installed, check your package manager.
3. configure
```
export PKG_CONFIG_PATH=/lib64/pkgconfig/
yum install -y libestr liblogging libmaxminddb-devel
yum install -y autoconf automake flex bison json-c-devel libuuid-devel libgcrypt-devel zlib-devel openssl-devel libcurl-devel gnutls-devel
cd ../rsyslog
./autogen.sh --enable-mmdblookup --enable-mmjsonparse --***
make
make install
```

## Usage

### Configuration

```
module( load="imfile" )
module( load="mmdblookup" )
module( load="mmjsonparse" )

input (
	type="imfile"
	File="/tmp/access.log"
	addMetadata="off"
	Severity="info"
	Facility="user"
	tag="test"
	ruleset="test"
)

template ( type="string" string="{\"@timestamp\":\"%timereported:::date-rfc3339%\",\"host\":\"%hostname%\",\"geoip2\":%$!iplocation%,%msg:7:$%" name="clientlog" )
ruleset  ( name="test" ) {
	action ( type="mmjsonparse" )
	if ( $parsesuccess == "OK" ) then {
		action( type="mmdblookup" mmdbfile="/etc/rsyslog.d/GeoLite2-City.mmdb" fields=["!continent!code","!location"] key="!clientip" )
		action( type="omfwd" Target="10.211.55.3" port="514" Protocol="tcp" template="clientlog" )
		stop
	}
}
```

### Testing

```
cat /root/a
@cee:{"clientip":"202.106.0.2","os_ver":"ios8","weibo_ver":"5.4.0","uid":1234567890,"rtt":0.123456,"error_code":-10005,"error_msg":"你以为我会告诉你么"}
cat /root/a > /tmp/access
```

get the result from logstash-input:

```
{
       "message" => "{\"@timestamp\":\"2016-07-23T10:35:21.572373+08:00\",\"host\":\"localhost\",\"geoip2\":{ \"continent\": { \"code\": \"AS\" }, \"location\": { \"accuracy_radius\": 50, \"latitude\": 34.772500, \"longitude\": 113.726600 } },\"clientip\":\"202.106.0.2\",\"os_ver\":\"ios8\",\"weibo_ver\":\"5.4.0\",\"uid\":1234567890,\"rtt\":0.123456,\"error_code\":-10005,\"error_msg\":\"你以为我会告诉你么\"}",
      "@version" => "1",
    "@timestamp" => "2016-07-23T02:35:21.586Z",
          "host" => "10.211.55.3",
          "port" => 58199
}
```
