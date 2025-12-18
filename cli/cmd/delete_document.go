package cmd

import (
	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type deleteDocumentOptions struct {
	col string
	doc string
}

var deleteDocOpts deleteDocumentOptions

var deleteDocumentCmd = &cobra.Command{
	Use:   "delete-document",
	Short: "Delete an entire document",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			err := client.DeleteDocument(fluxiondb.DeleteDocumentParams{
				Doc: deleteDocOpts.doc,
				Col: deleteDocOpts.col,
			})
			if err != nil {
				return err
			}
			return printJSON(map[string]string{"status": "ok"})
		})
	},
}

func init() {
	rootCmd.AddCommand(deleteDocumentCmd)

	deleteDocumentCmd.Flags().StringVar(&deleteDocOpts.col, "col", "", "Collection name (required)")
	deleteDocumentCmd.Flags().StringVar(&deleteDocOpts.doc, "doc", "", "Document name (required)")
	deleteDocumentCmd.MarkFlagRequired("col")
	deleteDocumentCmd.MarkFlagRequired("doc")
}
