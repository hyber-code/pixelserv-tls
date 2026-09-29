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

echo "== 4/6 user and certificate folder"
# Certificate folder: use CERT_DIR if you set it, otherwise reuse an existing CA folder
# (so an upgrade keeps your CA and already generated certificates), otherwise /var/lib/pixelserv.
if [ -z "${CERT_DIR:-}" ]; then
  if [ -f /var/lib/pixelserv/ca.crt ]; then CERT_DIR=/var/lib/pixelserv
  elif [ -f /var/cache/pixelserv/ca.crt ] && [ -f /var/cache/pixelserv/ca.key ]; then
    CERT_DIR=/var/cache/pixelserv
    echo "found an existing CA in $CERT_DIR: reusing it, your devices keep trusting it"
  else CERT_DIR=/var/lib/pixelserv; fi
fi
echo "certificate folder: $CERT_DIR"
id pixelserv >/dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin pixelserv
mkdir -p "$CERT_DIR"
if [ ! -f "$CERT_DIR/ca.crt" ] || [ ! -f "$CERT_DIR/ca.key" ]; then
  echo "creating a new CA (10 years) in $CERT_DIR"
  openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
    -keyout "$CERT_DIR/ca.key" -out "$CERT_DIR/ca.crt" -subj "/CN=pixelserv-tls CA"
  NEWCA=1
else
  echo "keeping existing CA and certificates"
  NEWCA=0
fi
chown -R pixelserv:pixelserv "$CERT_DIR"
chmod 750 "$CERT_DIR"
chmod 600 "$CERT_DIR/ca.key"

if command -v ss >/dev/null && ss -ltn 2>/dev/null | grep -qE ':(80|443) '; then
  echo "WARNING: something is already listening on port 80 or 443. If it is an older pixelserv-tls,"
  echo "stop it first (systemctl stop <name>, or docker stop <name>); Pi-hole's web interface also uses port 80."
  ss -ltnp 2>/dev/null | grep -E ':(80|443) ' || true
fi

echo "== 5/6 systemd service"
sed "s|/var/lib/pixelserv|$CERT_DIR|g" deploy/pixelserv-tls.service > /etc/systemd/system/pixelserv-tls.service
chmod 0644 /etc/systemd/system/pixelserv-tls.service
systemctl daemon-reload
systemctl enable --now pixelserv-tls
sleep 2

echo "== 6/6 summary"
systemctl is-active pixelserv-tls || true
echo "http check (want 204): $(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1/generate_204 2>/dev/null || echo fail)"
echo "binary: /usr/local/bin/pixelserv-tls   certificates: $CERT_DIR   service: pixelserv-tls"
if [ "$NEWCA" = 1 ]; then
  echo "NEXT: copy $CERT_DIR/ca.crt to your devices and trust it as a root certificate."
  echo "      Keep $CERT_DIR/ca.key private."
fi
echo "Point your DNS blocker (Pi-hole, AdGuard Home, dnsmasq) at this machine's IP for blocked domains."
