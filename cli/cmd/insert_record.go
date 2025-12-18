package cmd

import (
	"time"

	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type insertRecordOptions struct {
	col string
	doc string
	ts  int64
	raw string
}

var insertOpts insertRecordOptions

var insertRecordCmd = &cobra.Command{
	Use:   "insert-record",
	Short: "Insert a single record into a document",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			item := fluxiondb.InsertMessageRequest{
				TS:   insertOpts.ts,
				Doc:  insertOpts.doc,
				Data: insertOpts.raw,
				Col:  insertOpts.col,
			}
			if err := client.InsertSingleDocumentRecord(item); err != nil {
				return err
			}
			return printJSON(map[string]string{"status": "ok"})
		})
	},
}

func init() {
	rootCmd.AddCommand(insertRecordCmd)

	insertRecordCmd.Flags().StringVar(&insertOpts.col, "col", "", "Collection name (required)")
	insertRecordCmd.Flags().StringVar(&insertOpts.doc, "doc", "", "Document name (required)")
	insertRecordCmd.Flags().StringVar(&insertOpts.raw, "data", "", "Raw JSON string payload (required)")
	insertRecordCmd.Flags().Int64Var(&insertOpts.ts, "ts", time.Now().Unix(), "Record timestamp in seconds")

	insertRecordCmd.MarkFlagRequired("col")
	insertRecordCmd.MarkFlagRequired("doc")
	insertRecordCmd.MarkFlagRequired("data")
}
