FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake python3 strace procps ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /lab
CMD ["bash"]
