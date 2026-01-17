import com.volandoo.fluxiondb.FluxionDBClient;
import com.volandoo.fluxiondb.FluxionDBClientBuilder;
import com.volandoo.fluxiondb.model.requests.*;
import com.volandoo.fluxiondb.model.responses.*;
import com.volandoo.fluxiondb.model.enums.ApiKeyScope;

import java.util.*;

public class TestClient {
    public static void main(String[] args) {
        System.out.println("=== FluxionDB Java Client Test ===\n");

        FluxionDBClient client = new FluxionDBClientBuilder()
                .url("ws://localhost:8080")
                .apiKey("test-secret-key")
                .connectionName("java-test-client")
                .build();

        try {
            // Test 1: Connection
            System.out.println("1. Testing connection...");
            client.connect().get();
            System.out.println("   ✓ Connected successfully!\n");

            // Test 2: Insert records
            System.out.println("2. Testing insert operations...");
            long now = System.currentTimeMillis() / 1000;

            List<InsertMessageRequest> records = Arrays.asList(
                new InsertMessageRequest(now - 10, "device-1", "{\"temperature\": 22.5, \"humidity\": 65}", "sensors"),
                new InsertMessageRequest(now - 5, "device-1", "{\"temperature\": 23.0, \"humidity\": 64}", "sensors"),
                new InsertMessageRequest(now, "device-1", "{\"temperature\": 23.5, \"humidity\": 63}", "sensors"),
                new InsertMessageRequest(now, "device-2", "{\"temperature\": 20.0, \"humidity\": 70}", "sensors")
            );

            client.insertMultipleRecords(records).get();
            System.out.println("   ✓ Inserted " + records.size() + " records\n");

            // Test 3: Fetch collections
            System.out.println("3. Testing fetch collections...");
            List<String> collections = client.fetchCollections().get();
            System.out.println("   ✓ Collections: " + collections + "\n");

            // Test 4: Fetch latest records
            System.out.println("4. Testing fetch latest records...");
            FetchLatestRecordsParams latestParams = FetchLatestRecordsParams.builder()
                    .col("sensors")
                    .ts(now + 1)
                    .build();

            Map<String, RecordResponse> latest = client.fetchLatestRecords(latestParams).get();
            System.out.println("   ✓ Latest records count: " + latest.size());
            for (Map.Entry<String, RecordResponse> entry : latest.entrySet()) {
                System.out.println("     - " + entry.getKey() + ": " + entry.getValue().getData());
            }
            System.out.println();

            // Test 5: Fetch document history
            System.out.println("5. Testing fetch document history...");
            FetchRecordsParams historyParams = FetchRecordsParams.builder()
                    .col("sensors")
                    .doc("device-1")
                    .from(now - 20)
                    .to(now + 1)
                    .build();

            List<RecordResponse> history = client.fetchDocument(historyParams).get();
            System.out.println("   ✓ History records for device-1: " + history.size());
            for (RecordResponse record : history) {
                System.out.println("     - ts=" + record.getTs() + ", data=" + record.getData());
            }
            System.out.println();

            // Test 6: Regex pattern query
            System.out.println("6. Testing regex pattern query...");
            FetchLatestRecordsParams regexParams = FetchLatestRecordsParams.builder()
                    .col("sensors")
                    .ts(now + 1)
                    .doc("/device-.*/")  // Regex pattern
                    .build();

            Map<String, RecordResponse> regexResults = client.fetchLatestRecords(regexParams).get();
            System.out.println("   ✓ Regex query matched: " + regexResults.size() + " devices\n");

            // Test 7: Key-Value operations
            System.out.println("7. Testing key-value operations...");
            client.setValue(new SetValueParams("config", "app.version", "1.0.0")).get();
            client.setValue(new SetValueParams("config", "app.name", "TestApp")).get();
            client.setValue(new SetValueParams("config", "env.mode", "development")).get();

            String version = client.getValue(new GetValueParams("config", "app.version")).get();
            System.out.println("   ✓ Retrieved value: app.version = " + version);

            List<String> keys = client.getKeys(new CollectionParam("config")).get();
            System.out.println("   ✓ All keys: " + keys);

            Map<String, String> allValues = client.getValues(new GetValuesParams("config")).get();
            System.out.println("   ✓ All key-value pairs: " + allValues + "\n");

            // Test 8: Regex key-value query
            System.out.println("8. Testing regex key-value query...");
            Map<String, String> envVars = client.getValues(new GetValuesParams("config", "/env\\..*/")).get();
            System.out.println("   ✓ Env vars: " + envVars + "\n");

            // Test 9: Get connections
            System.out.println("9. Testing get connections...");
            List<ConnectionInfo> connections = client.getConnections().get();
            System.out.println("   ✓ Active connections: " + connections.size());
            for (ConnectionInfo conn : connections) {
                System.out.println("     - IP: " + conn.getIp() +
                                   ", Since: " + conn.getSince() +
                                   ", Self: " + conn.isSelf() +
                                   ", Name: " + conn.getName());
            }
            System.out.println();

            // Test 10: Delete operations
            System.out.println("10. Testing delete operations...");
            client.deleteValue(new DeleteValueParams("config", "env.mode")).get();
            System.out.println("    ✓ Deleted key-value pair");

            client.deleteRecord(new DeleteRecord("sensors", "device-1", now - 10)).get();
            System.out.println("    ✓ Deleted single record\n");

            // Test 11: Async chaining
            System.out.println("11. Testing async operations...");
            client.fetchCollections()
                    .thenAccept(cols -> System.out.println("    ✓ Async fetch result: " + cols))
                    .exceptionally(ex -> {
                        System.err.println("    ✗ Async error: " + ex.getMessage());
                        return null;
                    })
                    .get();
            System.out.println();

            System.out.println("=== ALL TESTS PASSED! ✓ ===\n");

        } catch (Exception e) {
            System.err.println("\n✗ Test failed: " + e.getMessage());
            e.printStackTrace();
        } finally {
            try {
                System.out.println("Closing connection...");
                client.closeAsync().get();
                System.out.println("Connection closed.\n");
            } catch (Exception e) {
                System.err.println("Error during cleanup: " + e.getMessage());
            }
        }
    }
}
