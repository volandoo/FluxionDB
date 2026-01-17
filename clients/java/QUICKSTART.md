# FluxionDB Java Client - Quick Start

Get started with FluxionDB in under 5 minutes.

## 1. Add Dependency

**Maven:**
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

**Gradle:**
```gradle
repositories {
    maven { url 'https://jitpack.io' }
}

dependencies {
    implementation 'com.github.volandoo:fluxiondb:main-SNAPSHOT:java-client'
}
```

## 2. Start FluxionDB Server

```bash
docker run --init --rm -p 8080:8080 \
  volandoo/fluxiondb:latest \
  --secret-key=my-secret-key
```

## 3. Write Your First App

```java
import com.volandoo.fluxiondb.*;
import com.volandoo.fluxiondb.model.requests.*;
import com.volandoo.fluxiondb.model.responses.*;

public class QuickStart {
    public static void main(String[] args) throws Exception {
        // 1. Create client
        FluxionDBClient client = new FluxionDBClientBuilder()
            .url("ws://localhost:8080")
            .apiKey("my-secret-key")
            .build();

        // 2. Connect
        client.connect().get();

        // 3. Insert data
        long now = System.currentTimeMillis() / 1000;
        client.insertSingleRecord(new InsertMessageRequest(
            now,
            "sensor-1",
            "{\"temperature\": 22.5}",
            "readings"
        )).get();

        // 4. Query data
        FetchLatestRecordsParams params = FetchLatestRecordsParams.builder()
            .col("readings")
            .ts(now + 1)
            .build();

        var latest = client.fetchLatestRecords(params).get();
        System.out.println("Latest: " + latest);

        // 5. Close
        client.closeAsync().get();
    }
}
```

## 4. Run It

```bash
javac -cp fluxiondb-client.jar QuickStart.java
java -cp fluxiondb-client.jar:. QuickStart
```

## What's Next?

- **Full Examples**: See [examples/BasicUsageExample.java](examples/BasicUsageExample.java)
- **API Reference**: Read [DOCUMENTATION.md](DOCUMENTATION.md)
- **Installation Options**: Check [INSTALL.md](INSTALL.md)

## Common Patterns

### Async Operations

```java
client.fetchCollections()
    .thenAccept(collections -> {
        System.out.println("Collections: " + collections);
    })
    .exceptionally(ex -> {
        System.err.println("Error: " + ex.getMessage());
        return null;
    });
```

### Try-with-Resources

```java
try (FluxionDBClient client = new FluxionDBClientBuilder()
        .url("ws://localhost:8080")
        .apiKey("my-secret-key")
        .build()) {

    client.connect().get();
    // Use client...
} // Auto-closes
```

### Batch Inserts

```java
List<InsertMessageRequest> batch = List.of(
    new InsertMessageRequest(now, "sensor-1", "{\"temp\": 22}", "readings"),
    new InsertMessageRequest(now, "sensor-2", "{\"temp\": 23}", "readings"),
    new InsertMessageRequest(now, "sensor-3", "{\"temp\": 24}", "readings")
);

client.insertMultipleRecords(batch).get();
```

### Regex Queries

```java
// Match all sensors
FetchLatestRecordsParams params = FetchLatestRecordsParams.builder()
    .col("readings")
    .ts(now)
    .doc("/sensor-.*/")  // Regex pattern
    .build();

var records = client.fetchLatestRecords(params).get();
```

## Troubleshooting

**Can't connect?**
- Ensure server is running: `docker ps`
- Check URL is correct: `ws://localhost:8080`
- Verify API key matches server's `--secret-key`

**Build errors?**
- Ensure Java 11+: `java -version`
- Clear Maven cache: `mvn clean install -U`
- Clear Gradle cache: `./gradlew build --refresh-dependencies`

**JitPack issues?**
- Check build log: `https://jitpack.io/com/github/volandoo/fluxiondb/main-SNAPSHOT/build.log`
- Try clicking "Get it" at: `https://jitpack.io/#volandoo/fluxiondb`

## Support

- **Issues**: https://github.com/volandoo/fluxiondb/issues
- **Discussions**: https://github.com/volandoo/fluxiondb/discussions
- **Examples**: `clients/java/examples/`
