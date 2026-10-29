ARG PFME_CONNECTOR_BASE
ARG PFME_ACCOUNTS_BASE
FROM ${PFME_CONNECTOR_BASE} AS original
FROM ${PFME_ACCOUNTS_BASE} AS permissions
USER 0:0
COPY --from=original /etc/ssl/certs/ca-certificates.crt /bundle/etc/ssl/certs/ca-certificates.crt
RUN chmod 0755 /bundle /bundle/etc /bundle/etc/ssl /bundle/etc/ssl/certs && chmod 0444 /bundle/etc/ssl/certs/ca-certificates.crt
FROM scratch
COPY --from=original --chmod=0555 /cloudflared /cloudflared
COPY --from=original --chmod=0555 /connector_gate /connector_gate
COPY --from=permissions /bundle/etc/ /etc/
USER 994:979
ENTRYPOINT ["/connector_gate"]