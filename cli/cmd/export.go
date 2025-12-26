package cmd

import (
	"bufio"
	"encoding/json"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"sort"

	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type exportOptions struct {
	outDir     string
	collection string
	apiKeys    bool
	all        bool
}

var exportOpts exportOptions

var exportCmd = &cobra.Command{
	Use:   "export",
	Short: "Export collections, key/value pairs, and/or API keys",
	Args:  cobra.NoArgs,
	RunE: func(cmd *cobra.Command, args []string) error {
		if exportOpts.outDir == "" {
			return fmt.Errorf("missing required --out-dir")
		}
		if !exportOpts.all && exportOpts.collection == "" && !exportOpts.apiKeys {
			return fmt.Errorf("specify at least one of --col, --api-keys, or --all")
		}
		return withClient(func(client *fluxiondb.Client) error {
			result, err := runExport(client, exportOpts)
			if err != nil {
				return err
			}
			return printJSON(result)
		})
	},
}

type exportSummary struct {
	Collection string `json:"collection"`
	Documents  int    `json:"documents"`
	Records    int    `json:"records"`
	OutputDir  string `json:"outputDir"`
}

type exportResult struct {
	OutputDir   string          `json:"outputDir"`
	Collections []exportSummary `json:"collections,omitempty"`
	APIKeys     *apiKeysSummary `json:"apiKeys,omitempty"`
}

type apiKeysSummary struct {
	Keys       int    `json:"keys"`
	OutputFile string `json:"outputFile"`
}

func init() {
	rootCmd.AddCommand(exportCmd)
	exportCmd.Flags().StringVar(&exportOpts.outDir, "out-dir", "", "Directory where the export files will be written (required)")
	exportCmd.Flags().StringVar(&exportOpts.collection, "col", "", "Collection to export")
	exportCmd.Flags().BoolVar(&exportOpts.apiKeys, "api-keys", false, "Export API keys to apikeys.json (master key required)")
	exportCmd.Flags().BoolVar(&exportOpts.all, "all", false, "Export all collections and API keys")
	exportCmd.MarkFlagRequired("out-dir")
}

func runExport(client *fluxiondb.Client, opts exportOptions) (exportResult, error) {
	result := exportResult{OutputDir: opts.outDir}

	includeAPIKeys := opts.apiKeys || opts.all

	var collections []string
	switch {
	case opts.all:
		var err error
		collections, err = client.FetchCollections()
		if err != nil {
			return result, fmt.Errorf("fetch collections: %w", err)
		}
		sort.Strings(collections)
	case opts.collection != "":
		collections = []string{opts.collection}
	}

	if len(collections) == 0 && !includeAPIKeys {
		return result, fmt.Errorf("nothing to export")
	}

	for _, col := range collections {
		summary, err := exportCollection(client, col, opts.outDir)
		if err != nil {
			return result, fmt.Errorf("export collection %s: %w", col, err)
		}
		result.Collections = append(result.Collections, summary)
	}

	if includeAPIKeys {
		apiSummary, err := exportAPIKeys(client, opts.outDir)
		if err != nil {
			return result, err
		}
		result.APIKeys = &apiSummary
	}

	return result, nil
}

func exportCollection(client *fluxiondb.Client, col, outDir string) (exportSummary, error) {
	colDir := filepath.Join(outDir, col)
	if err := os.MkdirAll(colDir, 0o755); err != nil {
		return exportSummary{}, err
	}

	docNames, err := listDocuments(client, col)
	if err != nil {
		return exportSummary{}, err
	}

	totalRecords := 0
	for _, doc := range docNames {
		if doc == "" {
			continue
		}
		count, err := exportDocument(client, col, doc, colDir)
		if err != nil {
			return exportSummary{}, fmt.Errorf("export document %s: %w", doc, err)
		}
		totalRecords += count
	}

	if err := exportKeyValues(client, col, colDir); err != nil {
		return exportSummary{}, err
	}

	return exportSummary{
		Collection: col,
		Documents:  len(docNames),
		Records:    totalRecords,
		OutputDir:  colDir,
	}, nil
}

func listDocuments(client *fluxiondb.Client, col string) ([]string, error) {
	resp, err := client.FetchLatestRecords(fluxiondb.FetchLatestRecordsParams{
		Col: col,
		TS:  math.MaxInt64,
	})
	if err != nil {
		return nil, err
	}
	if resp == nil {
		return nil, nil
	}
	docs := make([]string, 0, len(resp))
	for doc := range resp {
		docs = append(docs, doc)
	}
	sort.Strings(docs)
	return docs, nil
}

func exportDocument(client *fluxiondb.Client, col, doc, colDir string) (int, error) {
	records, err := client.FetchDocument(fluxiondb.FetchRecordsParams{
		Col:  col,
		Doc:  doc,
		From: 0,
		To:   math.MaxInt64,
	})
	if err != nil {
		return 0, err
	}

	filename := fmt.Sprintf("%s%s", encodeDocumentName(doc), documentExt)
	path := filepath.Join(colDir, filename)
	file, err := os.Create(path)
	if err != nil {
		return 0, err
	}
	defer file.Close()

	writer := bufio.NewWriter(file)
	for _, record := range records {
		line := archiveRecord{TS: record.TS, Doc: doc, Data: record.Data}
		encoded, err := json.Marshal(line)
		if err != nil {
			return 0, err
		}
		if _, err := writer.Write(encoded); err != nil {
			return 0, err
		}
		if err := writer.WriteByte('\n'); err != nil {
			return 0, err
		}
	}
	if err := writer.Flush(); err != nil {
		return 0, err
	}
	return len(records), nil
}

func exportKeyValues(client *fluxiondb.Client, col, colDir string) error {
	values, err := client.GetValues(fluxiondb.GetValuesParams{Col: col})
	if err != nil {
		return err
	}
	if values == nil {
		values = map[string]string{}
	}
	path := filepath.Join(colDir, keyValueFile)
	file, err := os.Create(path)
	if err != nil {
		return err
	}
	defer file.Close()

	enc := json.NewEncoder(file)
	enc.SetIndent("", "  ")
	return enc.Encode(values)
}

func exportAPIKeys(client *fluxiondb.Client, outDir string) (apiKeysSummary, error) {
	if err := os.MkdirAll(outDir, 0o755); err != nil {
		return apiKeysSummary{}, err
	}

	keys, err := client.ListAPIKeys()
	if err != nil {
		return apiKeysSummary{}, err
	}
	if keys == nil {
		keys = []fluxiondb.APIKeyInfo{}
	}

	path := filepath.Join(outDir, apiKeysFile)
	file, err := os.Create(path)
	if err != nil {
		return apiKeysSummary{}, err
	}
	defer file.Close()

	enc := json.NewEncoder(file)
	enc.SetIndent("", "  ")
	if err := enc.Encode(keys); err != nil {
		return apiKeysSummary{}, err
	}

	return apiKeysSummary{
		Keys:       len(keys),
		OutputFile: path,
	}, nil
}
