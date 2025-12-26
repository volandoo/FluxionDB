package cmd

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"

	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type importOptions struct {
	inDir string
}

var importOpts importOptions

var importCmd = &cobra.Command{
	Use:   "import [collection]",
	Short: "Import a collection from previously exported files",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if importOpts.inDir == "" {
			return fmt.Errorf("missing required --in-dir")
		}
		collection := args[0]
		return withClient(func(client *fluxiondb.Client) error {
			summary, err := importCollection(client, collection, importOpts.inDir)
			if err != nil {
				return err
			}
			return printJSON(summary)
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

func init() {
	rootCmd.AddCommand(importCmd)
	importCmd.Flags().StringVar(&importOpts.inDir, "in-dir", "", "Directory that holds the exported collection (required)")
	importCmd.MarkFlagRequired("in-dir")
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
