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
	outDir string
}

var exportOpts exportOptions

var exportCmd = &cobra.Command{
	Use:   "export [collection]",
	Short: "Export all documents and key/value pairs from a collection",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if exportOpts.outDir == "" {
			return fmt.Errorf("missing required --out-dir")
		}
		collection := args[0]
		return withClient(func(client *fluxiondb.Client) error {
			summary, err := exportCollection(client, collection, exportOpts.outDir)
			if err != nil {
				return err
			}
			return printJSON(summary)
		})
	},
}

type exportSummary struct {
	Collection string `json:"collection"`
	Documents  int    `json:"documents"`
	Records    int    `json:"records"`
	OutputDir  string `json:"outputDir"`
}

func init() {
	rootCmd.AddCommand(exportCmd)
	exportCmd.Flags().StringVar(&exportOpts.outDir, "out-dir", "", "Directory where the export files will be written (required)")
	exportCmd.MarkFlagRequired("out-dir")
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
