#!/bin/bash
# Sends ~600 malformed HTTP requests and fails if the server dies or a sanitizer reports an error.
# usage: tests/fuzz.sh ./pixelserv-tls   (build with -fsanitize=address,undefined for best results; needs python3, curl, openssl)
BIN=$(readlink -f "$1"); HERE=$(dirname "$(readlink -f "$0")")
export NO_PROXY='*' ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} UBSAN_OPTIONS=print_stacktrace=1
D=$(mktemp -d); mkdir -p $D/pem
openssl req -x509 -newkey rsa:2048 -nodes -keyout $D/pem/ca.key -out $D/pem/ca.crt -subj "/CN=test-ca" -days 30 2>/dev/null
chmod -R a+rwX $D
$BIN -f -u root -p 18080 -k 18443 -z $D/pem -l 4 >$D/log 2>&1 &
PID=$!; sleep 2
python3 $HERE/fuzz.py
ALIVE=$(curl -s --noproxy '*' -o /dev/null -w '%{http_code}' http://127.0.0.1:18080/x.gif)
kill $PID 2>/dev/null; wait $PID 2>/dev/null
ERRS=$(grep -cE "ERROR: AddressSanitizer|runtime error" $D/log)
echo "server alive after fuzz: $ALIVE (want 200); sanitizer errors: $ERRS (want 0)"
[ "$ALIVE" = "200" ] && [ "$ERRS" = "0" ]
