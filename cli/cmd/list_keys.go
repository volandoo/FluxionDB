package cmd

import (
	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type listKeysOptions struct {
	col string
}

var listKeysOpts listKeysOptions

var listKeysCmd = &cobra.Command{
	Use:   "list-keys",
	Short: "List all keys within a key/value collection",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			keys, err := client.GetKeys(fluxiondb.CollectionParam{Col: listKeysOpts.col})
			if err != nil {
				return err
			}
			return printJSON(keys)
		})
	},
}

func init() {
	rootCmd.AddCommand(listKeysCmd)
	listKeysCmd.Flags().StringVar(&listKeysOpts.col, "col", "", "Collection name (required)")
	listKeysCmd.MarkFlagRequired("col")
}
