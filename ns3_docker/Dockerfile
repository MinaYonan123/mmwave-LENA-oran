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

# ---- Install GCC 13 for FlexRIC ----
RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
    apt-get update && \
    apt-get install -y gcc-13 g++-13 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
                        --slave /usr/bin/g++ g++ /usr/bin/g++-13 && \
    gcc --version && g++ --version

WORKDIR /workspace

# ---- Build and install ASN.1C ----
RUN apt remove -y asn1c && \
    apt update && \
    apt install -y git autoconf automake libtool make gcc g++ m4 && \
    git clone https://github.com/mouse07410/asn1c /tmp/asn1c && \
    cd /tmp/asn1c && \
    git checkout 940dd5fa9f3917913fd487b13dfddfacd0ded06e && \
    autoreconf -iv && \
    ./configure --prefix=/opt/asn1c && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/asn1c

# Add ASN.1C to PATH for all future stages
ENV PATH="/opt/asn1c/bin:${PATH}"
WORKDIR /workspace

RUN git clone https://gitlab.eurecom.fr/mosaic5g/flexric.git && \
    cd flexric && \
    git checkout dev && \
    # Comment out the first line of CMakeLists.txt - fix to install FlexRIC
   # sed -i '1s/^/#/' examples/xApp/c/CMakeLists.txt && \
    mkdir build && cd build && \
    cmake .. -DE2AP_VERSION=E2AP_V1 -DKPM_VERSION=KPM_V3_00 && \
    make -j$(nproc) && \
    make install && \
    ldconfig

WORKDIR /workspace

CMD [ "/bin/sh" ]
