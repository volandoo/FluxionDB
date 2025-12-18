package cmd

import (
	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type deleteValueOptions struct {
	col string
	key string
}

var deleteValueOpts deleteValueOptions

var deleteValueCmd = &cobra.Command{
	Use:   "delete-value",
	Short: "Delete a key/value entry",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			err := client.DeleteValue(fluxiondb.DeleteValueParams{
				Col: deleteValueOpts.col,
				Key: deleteValueOpts.key,
			})
			if err != nil {
				return err
			}
			return printJSON(map[string]string{"status": "ok"})
		})
	},
}

func init() {
	rootCmd.AddCommand(deleteValueCmd)

	deleteValueCmd.Flags().StringVar(&deleteValueOpts.col, "col", "", "Collection name (required)")
	deleteValueCmd.Flags().StringVar(&deleteValueOpts.key, "key", "", "Key name (required)")
	deleteValueCmd.MarkFlagRequired("col")
	deleteValueCmd.MarkFlagRequired("key")
}
