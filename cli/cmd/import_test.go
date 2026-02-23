package cmd

import (
	"os"
	"path/filepath"
	"reflect"
	"testing"
)

func TestCollectionsToImportAllIncludesNestedDirs(t *testing.T) {
	root := t.TempDir()

	mustWriteFile(t, filepath.Join(root, "apikeys.json"), "[]")
	mustWriteFile(t, filepath.Join(root, "sensors", "device-1.jsonl"), "{\"ts\":1,\"data\":\"{}\"}\n")
	mustWriteFile(t, filepath.Join(root, "metrics", "key_value.json"), "{}")
	mustWriteFile(t, filepath.Join(root, "team", "prod", "server-1.jsonl"), "{\"ts\":1,\"data\":\"{}\"}\n")
	mustWriteFile(t, filepath.Join(root, "not-a-collection", "notes.txt"), "hello")

	got, err := collectionsToImport(importOptions{inDir: root, all: true})
	if err != nil {
		t.Fatalf("collectionsToImport returned error: %v", err)
	}

	want := []string{"metrics", "sensors", "team/prod"}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("unexpected collections: got %v want %v", got, want)
	}
}

func TestCollectionsToImportExplicitCollection(t *testing.T) {
	got, err := collectionsToImport(importOptions{collection: "sensors"})
	if err != nil {
		t.Fatalf("collectionsToImport returned error: %v", err)
	}
	want := []string{"sensors"}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("unexpected collections: got %v want %v", got, want)
	}
}

func TestCollectionsToImportAllLegacyLayout(t *testing.T) {
	root := t.TempDir()

	mustWriteFile(t, filepath.Join(root, "cell_info", "doc-1", "1771760179.json"), `{"data":"content","ts":1771760179}`)
	mustWriteFile(t, filepath.Join(root, "team", "prod", "server-1", "1771760180.json"), `{"data":"content","ts":1771760180}`)
	mustWriteFile(t, filepath.Join(root, "not-a-collection", "notes.txt"), "hello")

	got, err := collectionsToImport(importOptions{inDir: root, all: true, legacy: true})
	if err != nil {
		t.Fatalf("collectionsToImport returned error: %v", err)
	}

	want := []string{"cell_info", "team/prod"}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("unexpected collections: got %v want %v", got, want)
	}
}

func TestParseLegacyRecordsArray(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "1771760179.json")
	mustWriteFile(t, path, `[{"data":"content here","ts":1771760179},{"data":"more string content","ts":1771760180}]`)

	got, err := parseLegacyRecords(path)
	if err != nil {
		t.Fatalf("parseLegacyRecords returned error: %v", err)
	}
	if len(got) != 2 {
		t.Fatalf("unexpected record count: got %d want 2", len(got))
	}
	if got[0].TS != 1771760179 || got[0].Data != "content here" {
		t.Fatalf("unexpected first record: %+v", got[0])
	}
	if got[1].TS != 1771760180 || got[1].Data != "more string content" {
		t.Fatalf("unexpected second record: %+v", got[1])
	}
}

func TestParseLegacyRecordsUsesFilenameTimestamp(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "1771760190.json")
	mustWriteFile(t, path, `{"data":{"status":"ok"}}`)

	got, err := parseLegacyRecords(path)
	if err != nil {
		t.Fatalf("parseLegacyRecords returned error: %v", err)
	}
	if len(got) != 1 {
		t.Fatalf("unexpected record count: got %d want 1", len(got))
	}
	if got[0].TS != 1771760190 {
		t.Fatalf("unexpected timestamp: got %d want 1771760190", got[0].TS)
	}
	if got[0].Data != `{"status":"ok"}` {
		t.Fatalf("unexpected data payload: got %s", got[0].Data)
	}
}

func mustWriteFile(t *testing.T, path, data string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatalf("mkdir %s: %v", filepath.Dir(path), err)
	}
	if err := os.WriteFile(path, []byte(data), 0o644); err != nil {
		t.Fatalf("write %s: %v", path, err)
	}
}
