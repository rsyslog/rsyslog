# call this via "python[3] script name"
import sys
from pysnmp.entity import engine, config
from pysnmp.carrier.asyncore.dgram import udp
from pysnmp.entity.rfc3413 import ntfrcv
from pysnmp.smi import builder, view, compiler, rfc1902
from pyasn1.type.univ import OctetString

# Global variables
snmpport = 10162
snmpip = "127.0.0.1"
szOutputfile = "snmp.out"
szSnmpLogfile = "snmp_server.log"

# For vrebose output
bDebug = False

# Read command line params
if len(sys.argv) > 1:
    snmpport = int(sys.argv[1])
if len(sys.argv) > 2:
    snmpip = sys.argv[2]
if len(sys.argv) > 3:
    szOutputfile = sys.argv[3]
if len(sys.argv) > 4:
    szSnmpLogfile = sys.argv[4]

# Create output files
outputFile = open(szOutputfile,"w+")
logFile = open(szSnmpLogfile,"a+")

# Assemble MIB viewer
mibBuilder = builder.MibBuilder()
compiler.addMibCompiler(mibBuilder, sources=['file:///usr/share/snmp/mibs', 'file:///var/lib/snmp/mibs', '/usr/local/share/snmp/mibs/'])
mibViewController = view.MibViewController(mibBuilder)
# Pre-load MIB modules we expect to work with
try:
    mibBuilder.loadModules('SNMPv2-MIB', 'SNMP-COMMUNITY-MIB', 'SYSLOG-MSG-MIB')
except Exception:
    print("Failed loading MIBs")

# Create SNMP engine with autogenernated engineID and pre-bound to socket transport dispatcher
snmpEngine = engine.SnmpEngine()

# Transport setup
# UDP over IPv4, add listening interface/port
config.addTransport(
    snmpEngine,
    udp.domainName + (1,),
    udp.UdpTransport().openServerMode((snmpip, snmpport))
)

# SNMPv1/2c setup
# SecurityName <-> CommunityName mapping
config.addV1System(snmpEngine, 'my-area', 'public')

print("Started SNMP Trap Receiver: %s, %s, Output: %s" % (snmpport, snmpip, szOutputfile))
logFile.write("Started SNMP Trap Receiver: %s, %s, Output: %s" % (snmpport, snmpip, szOutputfile))
logFile.flush()

# Add PID file creation after startup message
import os
with open(szSnmpLogfile + ".started", "w") as f:
    f.write(str(os.getpid()))

# Callback function for receiving notifications
# noinspection PyUnusedLocal,PyUnusedLocal,PyUnusedLocal
def cbReceiverSnmp(snmpEngine, stateReference, contextEngineId, contextName, varBinds, cbCtx):
    transportDomain, transportAddress = snmpEngine.msgAndPduDsp.getTransportInfo(stateReference)
    if (bDebug):
        szDebug = str("Notification From: %s, Domain: %s, SNMP Engine: %s, Context: %s" %
            (transportAddress, transportDomain, contextEngineId.prettyPrint(), contextName.prettyPrint()))
        print(szDebug)
        logFile.write(szDebug)
        logFile.flush()

    # Create output String
    szOut = "Trap Source{}, Trap OID {}".format(transportAddress, transportDomain)

    varBinds = [rfc1902.ObjectType(rfc1902.ObjectIdentity(x[0]), x[1]).resolveWithMib(mibViewController) for x in varBinds]

    for name, val in varBinds:
        # Append to output String
        szOut = szOut + ", Oid: {}, Value: {}".format(name.prettyPrint(), val.prettyPrint())

        if isinstance(val, OctetString):
            if (name.prettyPrint() != "SNMP-COMMUNITY-MIB::snmpTrapAddress.0"):
                szOctets = val.asOctets()#.rstrip('\r').rstrip('\n')
                szOut = szOut + ", Octets: {}".format(szOctets)
        if (bDebug):
            print('%s = %s' % (name.prettyPrint(), val.prettyPrint()))
    outputFile.write(szOut)
    if "\n" not in szOut:
        outputFile.write("\n")
    outputFile.flush()


# Register SNMP Application at the SNMP engine
ntfrcv.NotificationReceiver(snmpEngine, cbReceiverSnmp)

# this job would never finish
snmpEngine.transportDispatcher.jobStarted(1)

# Run I/O dispatcher which would receive queries and send confirmations
try:
    snmpEngine.transportDispatcher.runDispatcher()
except:
    os.remove(szOutputfile + ".started")  # Remove PID file on shutdown
    snmpEngine.transportDispatcher.closeDispatcher()
    raise