#!/bin/bash
# usage: tests/smoke.sh ./pixelserv-tls   (needs curl and openssl; runs as root, ports 18080/18443)
BIN=$1
export NO_PROXY='*' no_proxy='*'
D=$(mktemp -d); mkdir -p $D/pem
openssl req -x509 -newkey rsa:2048 -nodes -keyout $D/pem/ca.key -out $D/pem/ca.crt -subj "/CN=test-ca" -days 30 2>/dev/null
chmod -R a+rwX $D
pkill -f "[p]ixelserv-tls -u root -p 18080" 2>/dev/null; sleep 1
$BIN -u root -p 18080 -k 18443 -z $D/pem -l 4 >$D/log 2>&1 &
PID=$!; sleep 2
c() { curl -s --noproxy '*' --cacert $D/pem/ca.crt --resolve ads.example.com:18443:127.0.0.1 -o /dev/null -w '%{http_code} verify=%{ssl_verify_result}\n' "$@"; }
echo "http:        $(curl -s --noproxy '*' -o /dev/null -w '%{http_code}' http://127.0.0.1:18080/x.gif)"
echo "generate_204:$(curl -s --noproxy '*' -o /dev/null -w '%{http_code}' http://127.0.0.1:18080/generate_204)"
# the first TLS request for a new hostname triggers cert generation and is expected to fail; retry until it works
echo "tls first:   $(c https://ads.example.com:18443/a.js)"
for i in $(seq 1 20); do
  [ "$(c https://ads.example.com:18443/a.js)" = "200 verify=0" ] && break
  sleep 1
done
echo "tls default: $(c https://ads.example.com:18443/a.js)"
echo "tls1.2 only: $(c --tlsv1.2 --tls-max 1.2 https://ads.example.com:18443/a.js)"
echo "tls1.3 only: $(c --tlsv1.3 https://ads.example.com:18443/a.js)"
echo "tls1.1:      $(echo | openssl s_client -connect 127.0.0.1:18443 -servername ads.example.com -tls1_1 -cipher 'DEFAULT:@SECLEVEL=0' 2>&1 | grep -cE 'Cipher is \(NONE\)|alert|no protocols') (1 = rejected)"
echo "--- log tail"; tail -5 $D/log
pkill -f "[p]ixelserv-tls -u root -p 18080" 2>/dev/null; ls $D/pem
