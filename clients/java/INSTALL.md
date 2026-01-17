# FluxionDB Java Client - Installation Guide

This guide explains how to add the FluxionDB Java client to your project.

## Prerequisites

- Java 11 or higher
- Maven, Gradle, or manual JAR management

## Installation Methods

### Method 1: Maven (Recommended)

Add JitPack repository and the FluxionDB dependency to your `pom.xml`:

```xml
<repositories>
    <repository>
        <id>jitpack.io</id>
        <url>https://jitpack.io</url>
    </repository>
</repositories>

<dependencies>
    <dependency>
        <groupId>com.github.volandoo</groupId>
        <artifactId>fluxiondb</artifactId>
        <version>main-SNAPSHOT</version>
        <classifier>java-client</classifier>
    </dependency>
</dependencies>
```

**For stable releases**, replace `main-SNAPSHOT` with a specific tag:
- `v1.0.0` - First stable release
- `main-SNAPSHOT` - Latest development version

### Method 2: Gradle

Add to your `build.gradle` or `build.gradle.kts`:

**Groovy (`build.gradle`):**
```gradle
repositories {
    maven { url 'https://jitpack.io' }
}

dependencies {
    implementation 'com.github.volandoo:fluxiondb:main-SNAPSHOT:java-client'
}
```

**Kotlin DSL (`build.gradle.kts`):**
```kotlin
repositories {
    maven { url = uri("https://jitpack.io") }
}

dependencies {
    implementation("com.github.volandoo:fluxiondb:main-SNAPSHOT:java-client")
}
```

### Method 3: Direct JAR Download

1. Download the latest JAR from [GitHub Releases](https://github.com/volandoo/fluxiondb/releases)
2. Add it to your project:

**Command line:**
```bash
# Compile
javac -cp fluxiondb-client.jar:. YourApp.java

# Run
java -cp fluxiondb-client.jar:. YourApp
```

**IDE (IntelliJ IDEA, Eclipse, etc.):**
1. Right-click project → Properties/Settings
2. Add JAR to Libraries/Build Path
3. Import classes as normal

### Method 4: Build from Source

Clone the repository and build:

```bash
git clone https://github.com/volandoo/fluxiondb.git
cd fluxiondb/clients/java
./build.sh
```

The JAR will be created at `clients/java/fluxiondb-client.jar`.

## Verification

After installation, verify it works:

```java
import com.volandoo.fluxiondb.FluxionDBClient;
import com.volandoo.fluxiondb.FluxionDBClientBuilder;

public class VerifyInstallation {
    public static void main(String[] args) {
        FluxionDBClient client = new FluxionDBClientBuilder()
            .url("ws://localhost:8080")
            .apiKey("test-key")
            .build();

        System.out.println("FluxionDB client created successfully!");
    }
}
```

If this compiles and runs without errors, the installation is successful.

## Troubleshooting

### JitPack build fails

JitPack builds on first request. If the build fails:
1. Check the build log at `https://jitpack.io/com/github/volandoo/fluxiondb/<version>/build.log`
2. Try a different version tag
3. Clear JitPack cache: `https://jitpack.io/#volandoo/fluxiondb` and click "Get it"

### Dependency resolution errors

**Maven:** Run `mvn clean install -U` to force update
**Gradle:** Run `./gradlew build --refresh-dependencies`

### Java version mismatch

Ensure your project uses Java 11 or higher:

**Maven:**
```xml
<properties>
    <maven.compiler.source>11</maven.compiler.source>
    <maven.compiler.target>11</maven.compiler.target>
</properties>
```

**Gradle:**
```gradle
java {
    sourceCompatibility = JavaVersion.VERSION_11
    targetCompatibility = JavaVersion.VERSION_11
}
```

### Classifier not found

If you get "classifier not found" errors, you may be using the wrong artifact. The Java client is in a subdirectory, so the `java-client` classifier is required.

## Next Steps

- Read the [README](README.md) for quick start examples
- Check [DOCUMENTATION.md](DOCUMENTATION.md) for complete API reference
- See [examples/BasicUsageExample.java](examples/BasicUsageExample.java) for working code

## Support

- Issues: https://github.com/volandoo/fluxiondb/issues
- Discussions: https://github.com/volandoo/fluxiondb/discussions
