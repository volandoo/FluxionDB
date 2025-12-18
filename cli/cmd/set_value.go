package cmd

import (
	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type setValueOptions struct {
	col   string
	key   string
	value string
}

var setValueOpts setValueOptions

var setValueCmd = &cobra.Command{
	Use:   "set-value",
	Short: "Set a key/value entry",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			err := client.SetValue(fluxiondb.SetValueParams{
				Col:   setValueOpts.col,
				Key:   setValueOpts.key,
				Value: setValueOpts.value,
			})
			if err != nil {
				return err
			}
			return printJSON(map[string]string{"status": "ok"})
		})
	},
}

func init() {
	rootCmd.AddCommand(setValueCmd)

	setValueCmd.Flags().StringVar(&setValueOpts.col, "col", "", "Collection name (required)")
	setValueCmd.Flags().StringVar(&setValueOpts.key, "key", "", "Key name (required)")
	setValueCmd.Flags().StringVar(&setValueOpts.value, "value", "", "Value string (required)")

	setValueCmd.MarkFlagRequired("col")
	setValueCmd.MarkFlagRequired("key")
	setValueCmd.MarkFlagRequired("value")
}
