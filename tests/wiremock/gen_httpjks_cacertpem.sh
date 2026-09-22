#remove existing one
rm -rf https.jks
#create server keypair in https keystore (temorarily self-signed)
keytool -genkeypair -alias wiremock-https -keyalg RSA -keysize 2048 -sigalg SHA256withRSA -dname "CN=localhost, O=WireMock, ST=London, C=GB" -ext "SAN=dns:localhost,ip:127.0.0.1,ip:0.0.0.0" -ext "EKU=serverAuth" -ext "BC=ca:false" -validity 825 -keystore https.jks -storepass password -keypass password -storetype JKS
#create CSR from https keypair
keytool -certreq -alias wiremock-https -keystore https.jks -storepass password -file server.csr
#sign CSR using existing CA key in ca-cert.jks
keytool -gencert -alias "1" -keystore ca-cert.jks -storepass password -infile server.csr -outfile server.crt -rfc -validity 825 -ext "KU=digitalSignature,keyEncipherment" -ext "EKU=serverAuth" -ext "SAN=dns:localhost,ip:127.0.0.1,ip:0.0.0.0" -ext "BC=ca:false"
#export CA public cert to PEM
keytool -exportcert -alias "1" -keystore ca-cert.jks -storepass password -rfc > ca-cert.pem
#import CA cert into https keystore
keytool -importcert -alias wiremock-ca -file ca-cert.pem -keystore https.jks -storepass password -noprompt
#replace server cert with CA-signed cert
keytool -importcert -alias wiremock-https -file server.crt -keystore https.jks -storepass password -noprompt

