package cmd

import (
	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type deleteCollectionOptions struct {
	col string
}

var deleteCollectionOpts deleteCollectionOptions

var deleteCollectionCmd = &cobra.Command{
	Use:   "delete-collection",
	Short: "Delete an entire collection",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			err := client.DeleteCollection(fluxiondb.DeleteCollectionParams{
				Col: deleteCollectionOpts.col,
			})
			if err != nil {
				return err
			}
			return printJSON(map[string]string{"status": "ok"})
		})
	},
}

func init() {
	rootCmd.AddCommand(deleteCollectionCmd)
	deleteCollectionCmd.Flags().StringVar(&deleteCollectionOpts.col, "col", "", "Collection name (required)")
	deleteCollectionCmd.MarkFlagRequired("col")
}
