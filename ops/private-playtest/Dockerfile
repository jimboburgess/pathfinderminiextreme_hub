# Prepared offline by prepare_runtime.py; never pulls a base image or installs packages.
FROM scratch
COPY rootfs/ /
USER 1000:1000
ENTRYPOINT ["/app/pfme_server"]
