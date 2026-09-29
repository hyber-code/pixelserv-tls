# Multi-arch build: docker buildx build --platform linux/amd64,linux/arm64,linux/arm/v7 -f deploy/Dockerfile .
FROM alpine:3.20 AS build
RUN apk add --no-cache build-base autoconf automake openssl-dev linux-headers
WORKDIR /src
COPY . .
RUN autoreconf -i && ./configure && make

FROM alpine:3.20
RUN apk add --no-cache libssl3 libcrypto3 && mkdir -p /var/cache/pixelserv
COPY --from=build /src/pixelserv-tls /usr/local/bin/pixelserv-tls
# Put ca.crt and ca.key in this volume (see deploy/README.md)
VOLUME /var/cache/pixelserv
EXPOSE 80 443
HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
  CMD wget -q -O /dev/null http://127.0.0.1:80/generate_204 || exit 1
ENTRYPOINT ["pixelserv-tls"]
# -f keeps it in the foreground; the server drops from root to "nobody" after binding ports
CMD ["-f", "-z", "/var/cache/pixelserv", "-l", "2"]
