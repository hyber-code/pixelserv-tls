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

## Step-by-step install guides

Pick ONE. All three end with the same check (step "Check it works" below). pixelserv-tls answers on **port 80 (HTTP) and port 443 (HTTPS)**, so nothing else may use those ports on the same IP (see "Pi-hole on the same machine").

### Guide 1: Raspberry Pi (Raspberry Pi OS or any Debian/Ubuntu, 32 or 64 bit)

1. Log in over SSH and become root: `sudo -i`
2. Get the code and run the installer:

       apt-get install -y git
       git clone https://github.com/hyber-code/pixelserv-tls
       cd pixelserv-tls
       bash deploy/install.sh

3. Wait for it to finish (a few minutes on a Pi). At the end it prints a summary and saves a log to `/tmp/pixelserv-install.log`.
4. Copy `/var/lib/pixelserv/ca.crt` to your phone and computers and trust it as a root certificate (only needed to avoid HTTPS warnings). Never share `ca.key`.
5. Go to "Check it works".

The installer builds the program, creates a locked-down `pixelserv` user, creates a CA if you have none, installs a systemd service and starts it on ports 80 and 443. Update later with `git pull && bash deploy/install.sh` (your CA is kept).

### Guide 2: Docker (Raspberry Pi, Proxmox VM/LXC, any Linux)

1. Become root: `sudo -i`, and install Docker if you don't have it (`curl -fsSL https://get.docker.com | sh`).
2. Get the code and create a CA. The container drops to the unprivileged user `nobody` (uid 65534) after it opens port 80/443, so that user must own the certificate folder:

       git clone https://github.com/hyber-code/pixelserv-tls
       cd pixelserv-tls
       mkdir -p certs
       openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
         -keyout certs/ca.key -out certs/ca.crt -subj "/CN=pixelserv-tls CA"
       chown -R 65534:65534 certs
       chmod 600 certs/ca.key

3. Start it (builds the image on the machine itself, so it works on any Pi):

       docker compose -f deploy/docker-compose.yml up -d --build

4. Trust `certs/ca.crt` on your devices, then go to "Check it works".

Logs: `docker logs pixelserv-tls`. Update: `git pull && docker compose -f deploy/docker-compose.yml up -d --build`. Stop: `docker compose -f deploy/docker-compose.yml down`.
If you prefer the ready-made image built by CI, replace the `build:` block in the compose file with `image: ghcr.io/hyber-code/pixelserv-tls:latest` (the package must be public in your GitHub package settings).

### Guide 3: Any Linux, run by hand

Good for a first test or a quick LXC. Root (or `sudo`) is only needed to open the low ports 80 and 443 and to install packages; the program does not keep root, and Guide 1 gives an unprivileged user that permission instead. Commands below assume a root shell (`sudo -i`):

    apt-get install -y build-essential autoconf automake libssl-dev openssl git
    git clone https://github.com/hyber-code/pixelserv-tls
    cd pixelserv-tls
    autoreconf -i && ./configure && make
    install -m 0755 pixelserv-tls /usr/local/bin/

    mkdir -p /var/lib/pixelserv
    openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
      -keyout /var/lib/pixelserv/ca.key -out /var/lib/pixelserv/ca.crt -subj "/CN=pixelserv-tls CA"
    chmod 600 /var/lib/pixelserv/ca.key
    chown -R nobody:nogroup /var/lib/pixelserv

    pixelserv-tls -f -z /var/lib/pixelserv -l 2

It runs in the foreground so you can see it (`Ctrl+C` stops it). Started as root it opens ports 80 and 443 and then drops itself to the user `nobody`, which is why the folder is owned by `nobody`. To run without root at all, add `-p 8080 -k 8443` (any ports above 1024) and make the folder belong to your own user; then point clients at those ports or forward 80/443 to them. To keep it running after you log out, use Guide 1 instead (systemd) or `nohup pixelserv-tls -z /var/lib/pixelserv -l 2 &`.

### Check it works

From another machine (replace `IP` with the address of the machine running pixelserv-tls):

    curl -i http://IP/generate_204          # expect: HTTP/1.1 204 No Content
    curl -s --cacert ca.crt --resolve test.example.com:443:IP https://test.example.com/ ; sleep 2
    curl -i --cacert ca.crt --resolve test.example.com:443:IP https://test.example.com/

The first HTTPS request for a new domain always fails: the certificate for it is created on that first request, and later requests work. Copy `ca.crt` to the machine you test from first.

### Pi-hole on the same machine (port 80 and 443 conflict)

Pi-hole's web interface also wants port 80, so only one of the two can have it. Two simple ways:

- **Move Pi-hole's web interface** to other ports (for example 8080 and 8443). Pi-hole v6: Settings > All settings > Webserver and API > `webserver.port`. Pi-hole v5: change `server.port` in `/etc/lighttpd/lighttpd.conf` and restart `lighttpd`. Then start pixelserv-tls. Check the current setting names in your Pi-hole version, they have changed between releases.
- **Run pixelserv-tls somewhere else** (another Pi, a Proxmox LXC or VM) and point Pi-hole at that machine's IP.

Then tell Pi-hole to answer blocked domains with the IP of the machine running pixelserv-tls (Pi-hole's "blocking mode" setting, custom IP option) instead of a blank answer. Blocked ads and trackers now get an instant empty reply instead of timing out.

### Upgrading an existing install (keeps your certificates)

You do not have to redo anything on your devices. Upgrading keeps your CA and every certificate already generated:

- Certificates already in your certificate folder keep being used until they expire (they may be RSA, valid up to 825 days). They are not deleted or regenerated. When one expires it is replaced automatically.
- Certificates for new domains are made as ECDSA with a 398-day lifetime.
- The CA (`ca.crt` and `ca.key`) is never changed, so devices that already trust it keep working.
- The installer (Guide 1) reuses an existing CA folder by itself: it looks in `/var/lib/pixelserv`, then `/var/cache/pixelserv`, or you can force one with `CERT_DIR=/path bash deploy/install.sh`. Stop the old pixelserv-tls first (`systemctl stop`, or `docker stop`), because two copies cannot share ports 80 and 443.
- After a restart, the first HTTPS request for a domain can fail once while its certificate is loaded, then it works. This is normal.
- TLS 1.0 and 1.1 clients are no longer accepted (the statistics page shows how many you had: the "TLS 1.0" counter).
- To make everything ECDSA right away, stop the service, delete the generated certificate files in the certificate folder (everything except `ca.crt` and `ca.key`), and start it again.

### Troubleshooting

- `Address already in use`: something else owns port 80 or 443 (`ss -ltnp | grep -E ':80 |:443 '`). Pi-hole, nginx and Apache are the usual ones.
- HTTPS always fails even on the second try: the certificate folder is not writable by the run-as user (`nobody` in Guides 2 and 3, `pixelserv` in Guide 1). Re-run the `chown` step.
- HTTPS warnings on a device: `ca.crt` is not installed as a trusted root on that device.
- Guide 1 logs: `journalctl -u pixelserv-tls -e`. Guide 2 logs: `docker logs pixelserv-tls`.

## Other install methods

| Method | Command |
|---|---|
| Debian package | `apt install debhelper autoconf automake libssl-dev && dpkg-buildpackage -us -uc -b && apt install ../pixelserv-tls_*.deb` (then create a CA as the post-install message says) |
| Build from source | `autoreconf -i && ./configure && make && make install` (OpenSSL 1.1.1 or newer, 3.x recommended) |

More detail on CA creation, systemd and Proxmox: `deploy/README.md`.

### Status of each method

Tested: building and running on x86-64 Linux with OpenSSL 3, running as root with the drop to `nobody` on ports 80/443-style binding, HTTP and TLS 1.2/1.3 requests, the `.deb` build and its post-install script, and the installer script (with systemd stubbed). Not yet tested: real Raspberry Pi hardware, the Docker image, the systemd unit under a real systemd. CI builds the ARM images under QEMU, so check the Actions tab after your first push.

## Command line

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
