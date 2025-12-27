# Use Ubuntu 24.04 with Qt 6.4
FROM ubuntu:24.04

# Install necessary packages for building the Qt application
RUN apt-get update && apt-get install -y \
    build-essential \
    qt6-base-dev \
    libqt6websockets6-dev \
    && rm -rf /var/lib/apt/lists/*


    # Create working directory
WORKDIR /app

# Copy the project files
COPY . .

# Download the latest released FluxionDB CLI into a temp dir we can copy from later.
RUN chmod +x cli/install.sh \
    && INSTALL_DIR=/tmp/fluxiondb-cli ./cli/install.sh

    # Build the project
RUN mkdir -p build && cd build \
    && qmake6 --version \
    && qmake6 ../fluxiondb.pro CONFIG+=release \
    && make -j$(nproc)


# Expose the WebSocket port
EXPOSE 8080

# Set the entrypoint
ENTRYPOINT [ "./build/fluxiondb" ]