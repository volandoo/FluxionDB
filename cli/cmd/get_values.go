package cmd

import (
	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type getValuesOptions struct {
	col string
	key string
}

var getValuesOpts getValuesOptions

var getValuesCmd = &cobra.Command{
	Use:   "get-values",
	Short: "Fetch key/value entries, optionally filtered by literal or /regex/ key",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			params := fluxiondb.GetValuesParams{Col: getValuesOpts.col}
			if cmd.Flags().Changed("key") && getValuesOpts.key != "" {
				params.Key = &getValuesOpts.key
			}
			values, err := client.GetValues(params)
			if err != nil {
				return err
			}
			return printJSON(values)
		})
	},
}

func init() {
	rootCmd.AddCommand(getValuesCmd)

	getValuesCmd.Flags().StringVar(&getValuesOpts.col, "col", "", "Collection name (required)")
	getValuesCmd.Flags().StringVar(&getValuesOpts.key, "key", "", "Literal or /regex/ key filter")
	getValuesCmd.MarkFlagRequired("col")
}
