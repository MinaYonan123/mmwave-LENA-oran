FROM ubuntu:22.04 as buildenv
ARG log_level_e2sim=2
# log_level_e2sim = 0 ->  LOG_LEVEL_UNCOND   0
# log_level_e2sim = 1 -> LOG_LEVEL_ERROR     1
# log_level_e2sim = 2 -> LOG_LEVEL_INFO      2
# log_level_e2sim = 3 -> LOG_LEVEL_DEBUG     3

ENV DEBIAN_FRONTEND=noninteractive

# ---- Install dependencies ----
RUN apt-get update && apt-get install -y \
    software-properties-common \
    build-essential git cmake libsctp-dev autoconf automake libtool bison flex libboost-all-dev \
    g++ python3 python3-pip python3-distutils python3-apt && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# ---- Copy local E2sim ----
COPY e2sim-kpmv3 /workspace/e2sim

RUN mkdir -p /workspace/e2sim/e2sim/build
WORKDIR /workspace/e2sim/e2sim/build
RUN cmake .. -DDEV_PKG=1 -DLOG_LEVEL=${log_level_e2sim} && \
    make package && \
    dpkg --install ./e2sim-dev_1.0.0_amd64.deb && \
    ldconfig

WORKDIR /workspace

# ---- Copy local ns-3 mmwave-oran ----
COPY mmwave-LENA-oran /workspace/ns3-mmwave-oran

WORKDIR /workspace/ns3-mmwave-oran

RUN ./ns3 configure && \
    ./ns3 build

WORKDIR /workspace

CMD [ "/bin/sh" ]
