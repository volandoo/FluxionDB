#!/bin/bash
set -e

echo "Building FluxionDB Java Client..."

# Create build directory
mkdir -p build/classes

# Find and compile all source files
echo "Compiling source files..."
find src/main/java -name "*.java" -print0 | \
    xargs -0 javac -d build/classes --release 11

# Create JAR file
echo "Creating JAR file..."
jar cf fluxiondb-client.jar -C build/classes .

echo "Build complete: fluxiondb-client.jar"

# Optional: Compile example
if [ -f "examples/BasicUsageExample.java" ]; then
    echo ""
    echo "Compiling example..."
    javac -cp fluxiondb-client.jar -d build/classes examples/BasicUsageExample.java
    echo "Example compiled. Run with:"
    echo "  java -cp fluxiondb-client.jar:build/classes BasicUsageExample"
fi

echo ""
echo "To use the client in your project:"
echo "  javac -cp fluxiondb-client.jar YourClass.java"
echo "  java -cp fluxiondb-client.jar:. YourClass"
