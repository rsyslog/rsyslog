# rsyslog-maxminddb

MaxMindDB is the new file format for storing information about IP addresses in a highly optimized, flexible database format. GeoIP2 Databases are available in the MaxMind DB format.

MaxMindDB vs GeoIP:

1. 4 ~ 6 faster
2. MaxMindDB Writer

## Compile

1. download and install libfastjson > 0.99.3 from https://github.com/rgerhards/libfastjson/commit/c437cad46af1998e3ad2dafa058c9e2c715df261
```
	git clone https://github.com/rgerhards/libfastjson
```

2. download rsyslog source
```
git clone https://github.com/rsyslog/rsyslog.git
```

3. copy the code into rsyslog contrib path：
```
cp -r src/contrib/mmdblookup ../rsyslog/contrib/
cp src/configure.ac ../rsyslog/
cp src/Makefile.am ../rsyslog/
cp src/libfastjson.env ../rsyslog/
# cp src/tests ../rsyslog/
```

4. configure
```
export PKG_CONFIG_PATH=/lib64/pkgconfig/
yum install -y libestr liblogging libmaxminddb-devel
yum install -y git-core valgrind autoconf automake flex bison json-c-devel libuuid-devel libgcrypt-devel zlib-devel openssl-devel libcurl-devel gnutls-devel mysql-devel postgresql-devel libdbi-dbd-mysql libdbi-devel net-snmp-devel
cd ../rsyslog
source libfastjson.env
autoconf
./configure --enable-mmdblookup --enable-mmjsonparse --***
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

template( type="string" string="{\"@timestamp\":\"%timereported:::date-rfc3339%\",\"host\":\"%hostname%\",\"geoip2\":%$!iplocation%,%msg:7:$%" name="clientlog" )
ruleset ( name="test"){
    action( type="mmjsonparse" )
    if ( $parsesuccess == "OK" ) then {
        action( type="mmdblookup" mmdbfile="/etc/rsyslog.d/GeoLite2-City.mmdb" fields=["!continent!code","!location"] key="!clientip" )
        action(type="omfwd" Target="10.211.55.3" port="514" Protocol="tcp" template="clientlog")
        stop
    }

}
```

### test

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

