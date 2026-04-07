# Stage 1: Build environment
FROM ubuntu:22.04 AS builder

# Avoid interactive prompts during apt-get
ENV DEBIAN_FRONTEND=noninteractive

# Update and install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libasio-dev \
    libcurl4-openssl-dev \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code and external libraries
COPY src/ ./src/
COPY public/ ./public/
COPY CMakeLists.txt .

# Ensure the external directory exists AND download crow_all.h
# Using v1.1.0 which is a stable version with the amalgamated header
RUN mkdir -p external && \
    curl -L https://github.com/CrowCpp/Crow/releases/download/v1.1.0/crow_all.h -o external/crow_all.h

# Build the project
RUN mkdir build && cd build && \
    cmake .. && \
    make

# Stage 2: Runtime environment
FROM ubuntu:22.04

# Set working directory
WORKDIR /app

# Install curl runtime library for routing table HTTP forwarding
RUN apt-get update && apt-get install -y libcurl4 && rm -rf /var/lib/apt/lists/*

# Copy the executable from the builder stage
COPY --from=builder /app/build/RobotControlServer .

# Copy public assets (HTML, CSS, JS)
# We place them in the same directory as the executable
COPY public/ .

# Expose the requested port
EXPOSE 6769

# Start the application
CMD ["./RobotControlServer"]
