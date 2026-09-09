# Dockerfile - 在 Windows/Mac 上无需装 32 位 Linux 工具链即可编译
# 用法:
#   docker build -t libcn_clone .
#   docker run --rm -v ${PWD}:/build libcn_clone make

FROM i386/ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        libc6-dev \
        gcc \
        make \
        libc6-dev-i386 \
        libssl-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /build

# 默认编译
CMD ["make"]