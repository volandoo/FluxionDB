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

func mustWriteFile(t *testing.T, path, data string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatalf("mkdir %s: %v", filepath.Dir(path), err)
	}
	if err := os.WriteFile(path, []byte(data), 0o644); err != nil {
		t.Fatalf("write %s: %v", path, err)
	}
}
