ARG NODE_IMAGE
FROM ${NODE_IMAGE}

ARG XPILOT_BASE_IMAGE
ARG XPILOT_REVISION
ARG XPILOT_SOURCE=https://github.com/kekyo/xpilot-infinity
ARG XPILOT_VERSION

LABEL org.opencontainers.image.title="XPilot Infinity Server" \
      org.opencontainers.image.description="Node.js/Wasm game server with a bundled browser client" \
      org.opencontainers.image.source="$XPILOT_SOURCE" \
      org.opencontainers.image.documentation="$XPILOT_SOURCE#readme" \
      org.opencontainers.image.version="$XPILOT_VERSION" \
      org.opencontainers.image.revision="$XPILOT_REVISION" \
      org.opencontainers.image.licenses="GPL-2.0-only" \
      org.opencontainers.image.base.name="$XPILOT_BASE_IMAGE"

RUN groupadd --gid 10001 xpilot && \
    useradd --uid 10001 --gid 10001 \
        --home-dir /var/lib/xpilot-infinity-server \
        --no-create-home --shell /usr/sbin/nologin xpilot && \
    install -d -o 10001 -g 10001 -m 0750 /var/lib/xpilot-infinity-server

COPY xpilot-server/dist/ /app/dist/
COPY COPYING /app/COPYING

WORKDIR /var/lib/xpilot-infinity-server
USER 10001:10001

EXPOSE 15347/tcp
STOPSIGNAL SIGTERM

ENTRYPOINT ["node", "/app/dist/cli.mjs", "--data-dir", "/var/lib/xpilot-infinity-server", "-listen", "udp:15345,tcp:15346,websocket:15347", "-clientPortStart", "15400", "-clientPortEnd", "15499", "-noQuit", "+reportMeta"]
CMD ["-map", "ndh.xp2"]
