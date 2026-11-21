# libpdfium.so in ./pdfium is x86-64 only, so both stages are pinned to amd64.

# ---- build stage ----
FROM --platform=linux/amd64 ubuntu:24.04 AS build

RUN apt-get -y update && \
    apt-get install -y cmake git g++ libcurl4-openssl-dev libssl-dev pkg-config \
    build-essential \
    libwebsockets-dev \
    nlohmann-json3-dev \
    meson \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src
COPY ./src ./src
COPY ./pdfium ./pdfium
COPY CMakeLists.txt CMakeLists.txt

RUN mkdir -p build && \
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel $(nproc)

# ---- runtime stage: no compilers, non-root ----
FROM --platform=linux/amd64 ubuntu:24.04

RUN apt-get -y update && \
    apt-get install -y --no-install-recommends libwebsockets19t64 libssl3t64 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --no-create-home --uid 10001 noteworthy

COPY --from=build /usr/src/build/api-server /opt/noteworthy/api-server
COPY --from=build /usr/src/external/lib/libpistache.so* /opt/noteworthy/lib/
COPY --from=build /usr/src/pdfium/lib/libpdfium.so /opt/noteworthy/lib/
ENV LD_LIBRARY_PATH=/opt/noteworthy/lib

USER noteworthy
# The binary is built with -pg and writes gmon.out to the working directory on exit.
WORKDIR /tmp

EXPOSE 8080 8081
CMD ["/opt/noteworthy/api-server"]
