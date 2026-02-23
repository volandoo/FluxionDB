package cmd

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type importOptions struct {
	inDir      string
	collection string
	apiKeys    bool
	all        bool
	legacy     bool
}

var importOpts importOptions

var importCmd = &cobra.Command{
	Use:   "import",
	Short: "Import collections and/or API keys from exported files",
	Args:  cobra.NoArgs,
	RunE: func(cmd *cobra.Command, args []string) error {
		if importOpts.inDir == "" {
			return fmt.Errorf("missing required --in-dir")
		}
		if !importOpts.all && importOpts.collection == "" && !importOpts.apiKeys {
			return fmt.Errorf("specify at least one of --col, --api-keys, or --all")
		}
		return withClient(func(client *fluxiondb.Client) error {
			result, err := runImport(client, importOpts)
			if err != nil {
				return err
			}
			return printJSON(result)
		})
	},
}

type importSummary struct {
	Collection string `json:"collection"`
	Documents  int    `json:"documents"`
	Records    int    `json:"records"`
	KeyValues  int    `json:"keyValues"`
	InputDir   string `json:"inputDir"`
}

type importResult struct {
	InputDir    string           `json:"inputDir"`
	Collections []importSummary  `json:"collections,omitempty"`
	APIKeys     *apiKeysImported `json:"apiKeys,omitempty"`
}

type apiKeysImported struct {
	Keys      int    `json:"keys"`
	InputFile string `json:"inputFile"`
}

func init() {
	rootCmd.AddCommand(importCmd)
	importCmd.Flags().StringVar(&importOpts.inDir, "in-dir", "", "Directory that holds the exported collection (required)")
	importCmd.Flags().StringVar(&importOpts.collection, "col", "", "Collection to import")
	importCmd.Flags().BoolVar(&importOpts.apiKeys, "api-keys", false, "Import API keys from apikeys.json (master key required)")
	importCmd.Flags().BoolVar(&importOpts.all, "all", false, "Import every collection and API keys from the directory")
	importCmd.Flags().BoolVar(&importOpts.legacy, "legacy", false, "Import legacy layout: <in-dir>/<collection>/<document>/*.json")
	importCmd.MarkFlagRequired("in-dir")
}

func runImport(client *fluxiondb.Client, opts importOptions) (importResult, error) {
	result := importResult{InputDir: opts.inDir}
	includeAPIKeys := opts.apiKeys || opts.all

	collections, err := collectionsToImport(opts)
	if err != nil {
		return result, err
	}

	for _, col := range collections {
		summary, err := importCollection(client, col, opts.inDir, opts.legacy)
		if err != nil {
			return result, err
		}
		result.Collections = append(result.Collections, summary)
	}

	if includeAPIKeys {
		apiSummary, err := importAPIKeys(client, opts.inDir)
		if err != nil {
			return result, err
		}
		result.APIKeys = &apiSummary
	}

	if len(result.Collections) == 0 && !includeAPIKeys {
		return result, fmt.Errorf("nothing to import")
	}
	return result, nil
}

func collectionsToImport(opts importOptions) ([]string, error) {
	switch {
	case opts.all:
		entries, err := os.ReadDir(opts.inDir)
		if err != nil {
			return nil, err
		}

		// Discover collections recursively so names that include path separators
		// (exported as nested directories) are still imported by --all.
		dirs := []string{}
		for _, entry := range entries {
			if entry.IsDir() {
				dirs = append(dirs, filepath.Join(opts.inDir, entry.Name()))
			}
		}

		seen := map[string]struct{}{}
		var cols []string
		for len(dirs) > 0 {
			dir := dirs[len(dirs)-1]
			dirs = dirs[:len(dirs)-1]

			dirEntries, err := os.ReadDir(dir)
			if err != nil {
				return nil, err
			}
			for _, entry := range dirEntries {
				if entry.IsDir() {
					dirs = append(dirs, filepath.Join(dir, entry.Name()))
				}
			}

			if !isCollectionImportDir(dir, opts.legacy) {
				continue
			}

			rel, err := filepath.Rel(opts.inDir, dir)
			if err != nil {
				return nil, err
			}
			rel = filepath.ToSlash(rel)
			if rel == "." || rel == "" {
				continue
			}
			if _, ok := seen[rel]; ok {
				continue
			}
			seen[rel] = struct{}{}
			cols = append(cols, rel)
		}
		sort.Strings(cols)
		return cols, nil
	case opts.collection != "":
		return []string{opts.collection}, nil
	default:
		return nil, nil
	}
}

func importAPIKeys(client *fluxiondb.Client, inDir string) (apiKeysImported, error) {
	path := filepath.Join(inDir, apiKeysFile)
	file, err := os.Open(path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return apiKeysImported{}, fmt.Errorf("api keys file %s not found", path)
		}
		return apiKeysImported{}, err
	}
	defer file.Close()

	var keys []fluxiondb.APIKeyInfo
	decoder := json.NewDecoder(file)
	if err := decoder.Decode(&keys); err != nil {
		if errors.Is(err, io.EOF) {
			keys = []fluxiondb.APIKeyInfo{}
		} else {
			return apiKeysImported{}, err
		}
	}

	count := 0
	for _, entry := range keys {
		if entry.Key == "" {
			continue
		}
		scope, err := parseAPIKeyScope(entry.Scope)
		if err != nil {
			return apiKeysImported{}, fmt.Errorf("key %s: %w", entry.Key, err)
		}
		if _, err := client.AddAPIKey(entry.Key, scope); err != nil {
			return apiKeysImported{}, fmt.Errorf("add key %s: %w", entry.Key, err)
		}
		count++
	}

	return apiKeysImported{
		Keys:      count,
		InputFile: path,
	}, nil
}

func parseAPIKeyScope(raw string) (fluxiondb.ApiKeyScope, error) {
	normalized := strings.ToLower(raw)
	normalized = strings.ReplaceAll(normalized, "_", "")
	normalized = strings.ReplaceAll(normalized, "-", "")
	normalized = strings.ReplaceAll(normalized, " ", "")

	switch normalized {
	case "readonly":
		return fluxiondb.ApiKeyScopeReadOnly, nil
	case "readwrite":
		return fluxiondb.ApiKeyScopeReadWrite, nil
	case "readwritedelete":
		return fluxiondb.ApiKeyScopeReadWriteDelete, nil
	default:
		return "", fmt.Errorf("invalid scope %q", raw)
	}
}

func isCollectionExportDir(path string) bool {
	entries, err := os.ReadDir(path)
	if err != nil {
		return false
	}
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		if entry.Name() == keyValueFile || strings.HasSuffix(entry.Name(), documentExt) {
			return true
		}
	}
	return false
}

func isCollectionLegacyDir(path string) bool {
	entries, err := os.ReadDir(path)
	if err != nil {
		return false
	}
	for _, entry := range entries {
		if entry.IsDir() && hasLegacyJSONFileInDocumentDir(filepath.Join(path, entry.Name())) {
			return true
		}
		if !entry.IsDir() && entry.Name() == keyValueFile {
			return true
		}
	}
	return false
}

func hasLegacyJSONFileInDocumentDir(path string) bool {
	entries, err := os.ReadDir(path)
	if err != nil {
		return false
	}
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		if strings.EqualFold(filepath.Ext(entry.Name()), ".json") {
			return true
		}
	}
	return false
}

func isCollectionImportDir(path string, legacy bool) bool {
	if legacy {
		return isCollectionLegacyDir(path)
	}
	return isCollectionExportDir(path)
}

func importCollection(client *fluxiondb.Client, col, inDir string, legacy bool) (importSummary, error) {
	if legacy {
		return importCollectionLegacy(client, col, inDir)
	}
	return importCollectionJSONL(client, col, inDir)
}

func importCollectionJSONL(client *fluxiondb.Client, col, inDir string) (importSummary, error) {
	colDir := filepath.Join(inDir, col)
	entries, err := os.ReadDir(colDir)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return importSummary{}, fmt.Errorf("collection path %s not found", colDir)
		}
		return importSummary{}, err
	}

	type docFile struct {
		doc  string
		path string
	}

	files := make([]docFile, 0, len(entries))
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		name := entry.Name()
		docName, ok := decodeDocumentName(name)
		if !ok {
			continue
		}
		files = append(files, docFile{
			doc:  docName,
			path: filepath.Join(colDir, name),
		})
	}

	sort.Slice(files, func(i, j int) bool {
		return files[i].doc < files[j].doc
	})

	totalRecords := 0
	for _, item := range files {
		count, err := importDocumentFile(client, col, item.doc, item.path)
		if err != nil {
			return importSummary{}, err
		}
		totalRecords += count
	}

	keyValues, err := importKeyValues(client, col, colDir)
	if err != nil {
		return importSummary{}, err
	}

	return importSummary{
		Collection: col,
		Documents:  len(files),
		Records:    totalRecords,
		KeyValues:  keyValues,
		InputDir:   colDir,
	}, nil
}

func importCollectionLegacy(client *fluxiondb.Client, col, inDir string) (importSummary, error) {
	colDir := filepath.Join(inDir, col)
	entries, err := os.ReadDir(colDir)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return importSummary{}, fmt.Errorf("collection path %s not found", colDir)
		}
		return importSummary{}, err
	}

	type docFiles struct {
		doc   string
		files []string
	}

	docs := make([]docFiles, 0, len(entries))
	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}
		docName := entry.Name()
		docDir := filepath.Join(colDir, entry.Name())
		files, err := collectLegacyJSONFiles(docDir)
		if err != nil {
			return importSummary{}, err
		}
		if len(files) == 0 {
			continue
		}
		docs = append(docs, docFiles{doc: docName, files: files})
	}

	sort.Slice(docs, func(i, j int) bool {
		return docs[i].doc < docs[j].doc
	})

	totalRecords := 0
	for _, doc := range docs {
		count, err := importLegacyDocumentFiles(client, col, doc.doc, doc.files)
		if err != nil {
			return importSummary{}, err
		}
		totalRecords += count
	}

	keyValues, err := importKeyValues(client, col, colDir)
	if err != nil {
		return importSummary{}, err
	}

	return importSummary{
		Collection: col,
		Documents:  len(docs),
		Records:    totalRecords,
		KeyValues:  keyValues,
		InputDir:   colDir,
	}, nil
}

func importDocumentFile(client *fluxiondb.Client, col, doc, path string) (int, error) {
	file, err := os.Open(path)
	if err != nil {
		return 0, err
	}
	defer file.Close()

	decoder := json.NewDecoder(file)
	batch := make([]fluxiondb.InsertMessageRequest, 0, insertBatchSize)
	total := 0

	for {
		var rec archiveRecord
		if err := decoder.Decode(&rec); err != nil {
			if errors.Is(err, io.EOF) {
				break
			}
			return total, fmt.Errorf("decode %s: %w", path, err)
		}

		if rec.Doc != "" && rec.Doc != doc {
			return total, fmt.Errorf("document mismatch in %s: got %s, expected %s", path, rec.Doc, doc)
		}

		batch = append(batch, fluxiondb.InsertMessageRequest{
			TS:   rec.TS,
			Doc:  doc,
			Data: rec.Data,
			Col:  col,
		})
		if len(batch) == insertBatchSize {
			if err := client.InsertMultipleRecords(batch); err != nil {
				return total, err
			}
			total += len(batch)
			batch = batch[:0]
		}
	}

	if len(batch) > 0 {
		if err := client.InsertMultipleRecords(batch); err != nil {
			return total, err
		}
		total += len(batch)
	}

	return total, nil
}

func collectLegacyJSONFiles(docDir string) ([]string, error) {
	files := []string{}
	err := filepath.WalkDir(docDir, func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			return nil
		}
		if strings.EqualFold(filepath.Ext(d.Name()), ".json") {
			files = append(files, path)
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	sort.Strings(files)
	return files, nil
}

func importLegacyDocumentFiles(client *fluxiondb.Client, col, doc string, paths []string) (int, error) {
	batch := make([]fluxiondb.InsertMessageRequest, 0, insertBatchSize)
	total := 0

	flush := func() error {
		if len(batch) == 0 {
			return nil
		}
		if err := client.InsertMultipleRecords(batch); err != nil {
			return err
		}
		total += len(batch)
		batch = batch[:0]
		return nil
	}

	for _, path := range paths {
		records, err := parseLegacyRecords(path)
		if err != nil {
			return total, err
		}
		for _, rec := range records {
			batch = append(batch, fluxiondb.InsertMessageRequest{
				TS:   rec.TS,
				Doc:  doc,
				Data: rec.Data,
				Col:  col,
			})
			if len(batch) == insertBatchSize {
				if err := flush(); err != nil {
					return total, err
				}
			}
		}
	}

	if err := flush(); err != nil {
		return total, err
	}
	return total, nil
}

func parseLegacyRecords(path string) ([]archiveRecord, error) {
	content, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	trimmed := bytes.TrimSpace(content)
	if len(trimmed) == 0 {
		return nil, nil
	}

	switch trimmed[0] {
	case '[':
		var items []map[string]json.RawMessage
		if err := json.Unmarshal(trimmed, &items); err != nil {
			return nil, fmt.Errorf("decode %s: %w", path, err)
		}

		records := make([]archiveRecord, 0, len(items))
		for i, item := range items {
			rec, err := legacyRecordFromObject(item, path, i)
			if err != nil {
				return nil, err
			}
			records = append(records, rec)
		}
		return records, nil
	case '{':
		var item map[string]json.RawMessage
		if err := json.Unmarshal(trimmed, &item); err != nil {
			return nil, fmt.Errorf("decode %s: %w", path, err)
		}
		rec, err := legacyRecordFromObject(item, path, -1)
		if err != nil {
			return nil, err
		}
		return []archiveRecord{rec}, nil
	default:
		return nil, fmt.Errorf("decode %s: expected JSON object or array", path)
	}
}

func legacyRecordFromObject(item map[string]json.RawMessage, path string, index int) (archiveRecord, error) {
	ts, err := extractLegacyTimestamp(item, path)
	if err != nil {
		if index >= 0 {
			return archiveRecord{}, fmt.Errorf("%s[%d]: %w", path, index, err)
		}
		return archiveRecord{}, fmt.Errorf("%s: %w", path, err)
	}

	data, err := extractLegacyData(item)
	if err != nil {
		if index >= 0 {
			return archiveRecord{}, fmt.Errorf("%s[%d]: %w", path, index, err)
		}
		return archiveRecord{}, fmt.Errorf("%s: %w", path, err)
	}

	return archiveRecord{TS: ts, Data: data}, nil
}

func extractLegacyTimestamp(item map[string]json.RawMessage, path string) (int64, error) {
	for _, key := range []string{"ts", "timestamp", "time"} {
		raw, ok := item[key]
		if !ok {
			continue
		}
		ts, err := parseInt64JSON(raw)
		if err != nil {
			return 0, fmt.Errorf("invalid %s: %w", key, err)
		}
		return ts, nil
	}

	base := strings.TrimSuffix(filepath.Base(path), filepath.Ext(path))
	ts, err := strconv.ParseInt(base, 10, 64)
	if err != nil {
		return 0, fmt.Errorf("missing ts/timestamp/time and filename is not unix timestamp")
	}
	return ts, nil
}

func parseInt64JSON(raw json.RawMessage) (int64, error) {
	var i int64
	if err := json.Unmarshal(raw, &i); err == nil {
		return i, nil
	}

	var f float64
	if err := json.Unmarshal(raw, &f); err == nil {
		return int64(f), nil
	}

	var s string
	if err := json.Unmarshal(raw, &s); err == nil {
		i, err := strconv.ParseInt(s, 10, 64)
		if err != nil {
			return 0, err
		}
		return i, nil
	}

	return 0, fmt.Errorf("expected number or numeric string")
}

func extractLegacyData(item map[string]json.RawMessage) (string, error) {
	raw, ok := item["data"]
	if !ok {
		encoded, err := json.Marshal(item)
		if err != nil {
			return "", err
		}
		return compactJSONString(encoded)
	}

	var asString string
	if err := json.Unmarshal(raw, &asString); err == nil {
		return asString, nil
	}
	return compactJSONString(raw)
}

func compactJSONString(raw []byte) (string, error) {
	var b bytes.Buffer
	if err := json.Compact(&b, raw); err != nil {
		return "", err
	}
	return b.String(), nil
}

func importKeyValues(client *fluxiondb.Client, col, colDir string) (int, error) {
	path := filepath.Join(colDir, keyValueFile)
	file, err := os.Open(path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return 0, nil
		}
		return 0, err
	}
	defer file.Close()

	var values map[string]string
	decoder := json.NewDecoder(file)
	if err := decoder.Decode(&values); err != nil {
		if errors.Is(err, io.EOF) {
			values = map[string]string{}
		} else {
			return 0, err
		}
	}

	count := 0
	for key, value := range values {
		if err := client.SetValue(fluxiondb.SetValueParams{Col: col, Key: key, Value: value}); err != nil {
			return count, fmt.Errorf("set key %s: %w", key, err)
		}
		count++
	}
	return count, nil
}
