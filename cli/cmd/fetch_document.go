package cmd

import (
	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type fetchDocOptions struct {
	col     string
	doc     string
	from    int64
	to      int64
	limit   int
	reverse bool
}

var fetchDocOpts fetchDocOptions

var fetchDocumentCmd = &cobra.Command{
	Use:   "fetch-document",
	Short: "Fetch records for a document within a time range",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			params := fluxiondb.FetchRecordsParams{
				Col:  fetchDocOpts.col,
				Doc:  fetchDocOpts.doc,
				From: fetchDocOpts.from,
				To:   fetchDocOpts.to,
			}

			if cmd.Flags().Changed("limit") {
				params.Limit = &fetchDocOpts.limit
			}
			if cmd.Flags().Changed("reverse") {
				params.Reverse = &fetchDocOpts.reverse
			}

			records, err := client.FetchDocument(params)
			if err != nil {
				return err
			}
			return printJSON(records)
		})
	},
}

func init() {
	rootCmd.AddCommand(fetchDocumentCmd)

	fetchDocumentCmd.Flags().StringVar(&fetchDocOpts.col, "col", "", "Collection name (required)")
	fetchDocumentCmd.Flags().StringVar(&fetchDocOpts.doc, "doc", "", "Document name or /regex/ (required)")
	fetchDocumentCmd.Flags().Int64Var(&fetchDocOpts.from, "from", 0, "Start timestamp (seconds, inclusive)")
	fetchDocumentCmd.Flags().Int64Var(&fetchDocOpts.to, "to", 0, "End timestamp (seconds, inclusive)")
	fetchDocumentCmd.Flags().IntVar(&fetchDocOpts.limit, "limit", 0, "Optional maximum number of records to return")
	fetchDocumentCmd.Flags().BoolVar(&fetchDocOpts.reverse, "reverse", false, "Reverse order (newest first)")

	fetchDocumentCmd.MarkFlagRequired("col")
	fetchDocumentCmd.MarkFlagRequired("doc")
	fetchDocumentCmd.MarkFlagRequired("from")
	fetchDocumentCmd.MarkFlagRequired("to")
}
