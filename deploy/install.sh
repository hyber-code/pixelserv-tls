#!/bin/bash
# One-command native install for Debian, Ubuntu and Raspberry Pi OS (systemd).
#   sudo bash deploy/install.sh
# Builds from this checkout, installs to /usr/local/bin, creates an unprivileged
# "pixelserv" user, creates a CA if none exists, and enables the systemd service.
# Output is also saved to /tmp/pixelserv-install.log
set -euo pipefail
exec > >(tee /tmp/pixelserv-install.log) 2>&1

[ "$(id -u)" = 0 ] || { echo "Run as root: sudo bash $0"; exit 1; }
cd "$(dirname "$(readlink -f "$0")")/.."

echo "== 1/6 dependencies"
if command -v apt-get >/dev/null; then
  apt-get update -qq
  apt-get install -y --no-install-recommends build-essential autoconf automake libssl-dev openssl ca-certificates
else
  echo "apt-get not found: install a C compiler, autoconf, automake and OpenSSL development files yourself"
fi

echo "== 2/6 build"
autoreconf -i
./configure
make -j"$(nproc)"

echo "== 3/6 install binary"
install -m 0755 pixelserv-tls /usr/local/bin/pixelserv-tls

echo "== 4/6 user and state directory"
id pixelserv >/dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin pixelserv
mkdir -p /var/lib/pixelserv
if [ ! -f /var/lib/pixelserv/ca.crt ] || [ ! -f /var/lib/pixelserv/ca.key ]; then
  echo "creating a new CA (10 years) in /var/lib/pixelserv"
  openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
    -keyout /var/lib/pixelserv/ca.key -out /var/lib/pixelserv/ca.crt -subj "/CN=pixelserv-tls CA"
  NEWCA=1
else
  echo "keeping existing CA"
  NEWCA=0
fi
chown -R pixelserv:pixelserv /var/lib/pixelserv
chmod 750 /var/lib/pixelserv
chmod 600 /var/lib/pixelserv/ca.key

echo "== 5/6 systemd service"
install -m 0644 deploy/pixelserv-tls.service /etc/systemd/system/pixelserv-tls.service
systemctl daemon-reload
systemctl enable --now pixelserv-tls
sleep 2

echo "== 6/6 summary"
systemctl is-active pixelserv-tls || true
echo "http check (want 204): $(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1/generate_204 2>/dev/null || echo fail)"
echo "binary: /usr/local/bin/pixelserv-tls   state: /var/lib/pixelserv   service: pixelserv-tls"
if [ "$NEWCA" = 1 ]; then
  echo "NEXT: copy /var/lib/pixelserv/ca.crt to your devices and trust it as a root certificate."
  echo "      Keep /var/lib/pixelserv/ca.key private."
fi
echo "Point your DNS blocker (Pi-hole, AdGuard Home, dnsmasq) at this machine's IP for blocked domains."
