# CREATE KEY
#	openssl genpkey -algorithm RSA -out client-revoked-key.pem
# CREATE REQUEST
#	openssl req -new -key client-revoked-key.pem -out client-revoked.csr
# CREATE A CERT
# 	openssl ca -config openssl.cnf -in client-revoked.csr -out client-revoked.pem -keyfile ca-key.pem -cert ca.pem
# REVOKE A CERT
# 	openssl ca -config openssl.cnf -revoke client-revoked.pem -keyfile ca-key.pem -cert ca.pem
# CREATE crl.pem
#	openssl ca -config openssl.cnf -gencrl -out crl.pem -keyfile ca-key.pem -cert ca.pem

