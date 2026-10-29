ARG PFME_CONNECTOR_IMAGE
ARG PFME_PYTHON_BASE
FROM ${PFME_CONNECTOR_IMAGE} AS connector
FROM ${PFME_PYTHON_BASE} AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends nftables && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY requirements.txt /app/requirements.txt
RUN python -m pip install --no-cache-dir --requirement requirements.txt
COPY --from=connector /connector_gate /connector_gate
COPY . /app/
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 DJANGO_SETTINGS_MODULE=pfme_accounts.settings
USER 995:980
ENTRYPOINT ["python"]
CMD ["-m","pfme_accounts.server"]
