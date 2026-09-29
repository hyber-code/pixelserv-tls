# pixelserv-tls (modernised fork)

[![build](https://github.com/hyber-code/pixelserv-tls/actions/workflows/build.yml/badge.svg)](https://github.com/hyber-code/pixelserv-tls/actions/workflows/build.yml)

_pixelserv-tls_ is a tiny bespoke HTTP/1.1 webserver with HTTPS and SNI support. It acts on behalf of hundreds of thousands of advert/tracker servers and responds to all requests with nothing, to speed up web browsing. Pair it with a DNS blocker (Pi-hole, AdGuard Home, dnsmasq) that sends blocked domains to the machine running it.

Server certificates for any given advert/tracker domain are generated automatically on first use and saved to disk. It can log access and HTTP/1.1 POST contents to syslog, which is useful to spot wrongly blocked domains and trackers.

**This fork** is based on upstream [kvic-z/pixelserv-tls](https://github.com/kvic-z/pixelserv-tls) v2.4 and adds:

- builds on OpenSSL 3 (upstream fails to link)
- TLS 1.2 and 1.3 only, AEAD ciphers only
- ECDSA P-256 certificates by default (fast to generate on a Raspberry Pi), 398-day validity, random serial numbers
- security fixes: POST body overflow, crashes without a CA or with an unknown `-u` user, stricter privilege drop
- systemd unit, Docker image (amd64, arm64, armv7), native `.deb` packaging, one-command installer
- CI with a smoke test and an AddressSanitizer/UBSan fuzz run

See `ChangeLog` for details.

## Quick start (Debian, Ubuntu, Raspberry Pi OS, Proxmox LXC or VM)

    git clone https://github.com/hyber-code/pixelserv-tls
    cd pixelserv-tls
    sudo bash deploy/install.sh

This builds it, installs to `/usr/local/bin`, creates an unprivileged `pixelserv` user, creates a CA in `/var/lib/pixelserv` if there is none, and starts the systemd service. Then trust `/var/lib/pixelserv/ca.crt` on your devices and keep `ca.key` private.

## Other ways to install

| Method | Command |
|---|---|
| Docker Compose | `mkdir certs` and create a CA (see `deploy/README.md`), then `docker compose -f deploy/docker-compose.yml up -d` |
| Docker image (built by CI from `master`) | `docker run -d -p 80:80 -p 443:443 -v $PWD/certs:/var/cache/pixelserv ghcr.io/hyber-code/pixelserv-tls:latest` (the package must be set to public in your GitHub package settings) |
| Debian package | `sudo apt install debhelper autoconf automake libssl-dev && dpkg-buildpackage -us -uc -b && sudo apt install ../pixelserv-tls_*.deb` |
| Build from source | `autoreconf -i && ./configure && make && sudo make install` (needs OpenSSL 1.1.1 or newer, 3.x recommended) |

Full instructions, CA creation, systemd, compose and Proxmox notes: `deploy/README.md`.

### Status of each method

Tested: building and running on x86-64 Linux with OpenSSL 3, HTTP and TLS 1.2/1.3 requests, the `.deb` build and its post-install script, and the installer script (with systemd stubbed). Not yet tested: real ARM hardware, the Docker image, the systemd unit under a real systemd. CI builds the ARM images under QEMU, so check the Actions tab after your first push.

## Launch manually

    pixelserv-tls -f -z /path/to/cert-dir <listening ip>

`ca.crt` and `ca.key` must be in the cert directory. See the [man page](pixelserv-tls.1) or [upstream wiki](https://github.com/kvic-z/pixelserv-tls/wiki/Command-Line-Options) for options. Useful ones: `-f` stay in foreground, `-u USER` user to drop to, `-l LEVEL` log level, `-z DIR` certificate directory.

## Tests

    tests/smoke.sh ./pixelserv-tls      # HTTP, 204, TLS 1.2/1.3, TLS 1.1 rejected (needs curl, openssl, root)
    tests/fuzz.sh ./pixelserv-tls       # ~600 malformed requests; build with -fsanitize=address,undefined

## Upstream install methods (not this fork)

The methods below install the **original upstream builds**, not the changes in this fork.

### Install on Entware

Binary packages are distributed by Entware. Beta version binaries during development are distributed from this GitHub repository.

##### Pre-built binaries
````
opkg install pixelserv-tls
````

### Install on Arch Linux

A package is available from Arch User Repository (AUR). This [package](https://aur.archlinux.org/packages/pixelserv-tls/) works on all Arch Linux derived distributions such as Manjaro, Antergos and Chakra.

##### Pre-built binaries using `yay`
````
yay -S pixelserv-tls
````
##### Build from source package
````
git clone https://aur.archlinux.org/pixelserv-tls.git
cd pixelserv-tls
makepkg -si
````

### Install on EdgeRouter X

See this [installation guide](https://kazoo.ga/run-pixelserv-tls-on-erx/). Or simply:

##### Pre-built binary
````
sudo -i
cd /tmp
curl -O https://raw.githubusercontent.com/kvic-z/goodies-edgemax/master/pixelserv-tls_2.2.1-1_mipsel.deb
dpkg -i pixelserv-tls_2.2.1-1_mipsel.deb
````
The binary is built for and tested on EdgeOS v1.x. It's not tested on EdgeOS v2.x and most likely it won't be compatible.

### Install on Homebrew (macOS) and Linuxbrew

```
brew install https://kazoo.ga/pixelserv-tls/pixelserv-tls.rb
```

### Install as a Docker container

See https://hub.docker.com/r/imthai/pixelserv-tls

### Install on Raspberry Pi

Binary packages are available from this [Github](https://github.com/jumpsmm7/). Should work on all Raspberry Pi's running Raspbian (Debian 10). For installation issues, you may refer to this [tracker](https://github.com/kvic-z/pixelserv-tls/issues/32).

##### Pre-built binary
````
sudo -i
cd /tmp
curl -O https://raw.githubusercontent.com/jumpsmm7/pixelserv-tls_2.4_armhf.deb/master/pixelserv-tls_2.4_armhf.deb
dpkg -i pixelserv-tls_2.4_armhf.deb
````
and follow the on-screen instructions.

## Notes

Announcements for the upstream project are made through [kazoo.ga/pixelserv-tls](https://kazoo.ga/pixelserv-tls/). A discussion [thread](http://www.snbforums.com/threads/pixelserv-a-better-one-pixel-webserver-for-adblock.26114) is also available on SNBforums.
