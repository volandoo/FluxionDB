package cmd

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type importOptions struct {
	inDir      string
	collection string
	apiKeys    bool
	all        bool
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
		summary, err := importCollection(client, col, opts.inDir)
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
		var cols []string
		for _, entry := range entries {
			if !entry.IsDir() {
				continue
			}
			colPath := filepath.Join(opts.inDir, entry.Name())
			if !isCollectionExportDir(colPath) {
				continue
			}
			cols = append(cols, entry.Name())
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

func importCollection(client *fluxiondb.Client, col, inDir string) (importSummary, error) {
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
