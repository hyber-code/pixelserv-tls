# Deploying on Proxmox and Raspberry Pi

pixelserv-tls answers ad/tracker requests with an empty response. Your DNS blocker
(Pi-hole, AdGuard Home, dnsmasq) must point blocked domains at the machine running it.

## 1. Create your own CA (once)

    mkdir -p certs
    openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
      -keyout certs/ca.key -out certs/ca.crt -subj "/CN=pixelserv-tls CA"
    chmod 600 certs/ca.key

Install `ca.crt` as a trusted root on the devices that should see no certificate
warnings. Keep `ca.key` private: anyone holding it can impersonate sites to those devices.

## 2. Run with Docker (Proxmox LXC or VM, Raspberry Pi 64-bit or 32-bit OS)

    docker build -f deploy/Dockerfile -t pixelserv-tls .
    docker run -d --name pixelserv --restart unless-stopped \
      -p 80:80 -p 443:443 -v "$PWD/certs:/var/cache/pixelserv" pixelserv-tls

In a Proxmox LXC, enable nesting (Options > Features) to run Docker, or skip Docker and
build natively as below.

## 3. Or build natively (Debian/Ubuntu/Raspberry Pi OS)

    sudo apt install build-essential autoconf automake libssl-dev
    autoreconf -i && ./configure && make
    sudo install pixelserv-tls /usr/local/bin/
    sudo mkdir -p /var/cache/pixelserv && sudo cp certs/ca.* /var/cache/pixelserv/
    sudo pixelserv-tls -z /var/cache/pixelserv -l 2
