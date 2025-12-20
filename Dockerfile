# Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    git \
    zlib1g-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Build and install prometheus-cpp
RUN git clone https://github.com/jupp0r/prometheus-cpp.git /tmp/prometheus-cpp && \
    cd /tmp/prometheus-cpp && \
    git checkout v1.2.4 && \
    git submodule init && \
    git submodule update && \
    cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DENABLE_PUSH=OFF \
      -DENABLE_COMPRESSION=OFF \
      -DENABLE_TESTING=OFF && \
    cmake --build build -j $(nproc) && \
    cmake --install build && \
    cd / && rm -rf /tmp/prometheus-cpp

# Set working directory
WORKDIR /app

# Copy source files
COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/

# Build the application
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies (if any)
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -u 1000 mqtt && \
    mkdir -p /app && \
    chown -R mqtt:mqtt /app

WORKDIR /app

# Copy binary from builder
COPY --from=builder --chown=mqtt:mqtt /app/build/mqtt-broker .

# Switch to non-root user
USER mqtt

# Expose MQTT port and Prometheus metrics port
EXPOSE 1883 9090

# Run the broker
CMD ["./mqtt-broker"]