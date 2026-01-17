import com.volandoo.fluxiondb.FluxionDBClient;
import com.volandoo.fluxiondb.FluxionDBClientBuilder;
import com.volandoo.fluxiondb.model.enums.ApiKeyScope;
import com.volandoo.fluxiondb.model.requests.*;
import com.volandoo.fluxiondb.model.responses.ConnectionInfo;
import com.volandoo.fluxiondb.model.responses.RecordResponse;

import java.util.List;
import java.util.Map;

/**
 * Basic usage example for FluxionDB Java Client.
 */
public class BasicUsageExample {

    public static void main(String[] args) {
        // Create client using builder pattern
        FluxionDBClient client = new FluxionDBClientBuilder()
                .url("ws://localhost:8080")
                .apiKey("YOUR_SECRET_KEY")
                .connectionName("java-example")
                .build();

        try {
            // Connect to server
            System.out.println("Connecting to FluxionDB...");
            client.connect().get();
            System.out.println("Connected successfully!");

            // Insert a single time series record
            long now = System.currentTimeMillis() / 1000;
            InsertMessageRequest record = new InsertMessageRequest(
                    now,
                    "device-1",
                    "{\"temperature\": 22.5, \"humidity\": 65.2}",
                    "sensors"
            );

            client.insertSingleRecord(record).get();
            System.out.println("Inserted record");

            // Fetch collections
            List<String> collections = client.fetchCollections().get();
            System.out.println("Collections: " + collections);

            // Fetch latest records
            FetchLatestRecordsParams fetchParams = FetchLatestRecordsParams.builder()
                    .col("sensors")
                    .ts(now)
                    .doc("device-1")
                    .build();

            Map<String, RecordResponse> latest = client.fetchLatestRecords(fetchParams).get();
            System.out.println("Latest records: " + latest);

            // Fetch document history
            FetchRecordsParams historyParams = FetchRecordsParams.builder()
                    .col("sensors")
                    .doc("device-1")
                    .from(now - 3600)
                    .to(now + 100)
                    .build();

            List<RecordResponse> history = client.fetchDocument(historyParams).get();
            System.out.println("Document history: " + history.size() + " records");

            // Set a key-value pair
            SetValueParams setParams = new SetValueParams("config", "app_version", "1.0.0");
            client.setValue(setParams).get();
            System.out.println("Set key-value");

            // Get a value
            GetValueParams getParams = new GetValueParams("config", "app_version");
            String value = client.getValue(getParams).get();
            System.out.println("app_version = " + value);

            // Get all keys in a collection
            CollectionParam colParam = new CollectionParam("config");
            List<String> keys = client.getKeys(colParam).get();
            System.out.println("Keys: " + keys);

            // Get connections
            List<ConnectionInfo> connections = client.getConnections().get();
            System.out.println("Active connections: " + connections.size());
            for (ConnectionInfo conn : connections) {
                System.out.println("  - " + conn);
            }

            // Example with async style (non-blocking)
            client.fetchCollections()
                    .thenAccept(cols -> System.out.println("Async fetch: " + cols))
                    .exceptionally(ex -> {
                        System.err.println("Error: " + ex.getMessage());
                        return null;
                    })
                    .join(); // Wait for completion

        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
            e.printStackTrace();
        } finally {
            // Clean shutdown
            try {
                System.out.println("Closing connection...");
                client.closeAsync().get();
                System.out.println("Disconnected");
            } catch (Exception e) {
                System.err.println("Error during shutdown: " + e.getMessage());
            }
        }
    }
}
