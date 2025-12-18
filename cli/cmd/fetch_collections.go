package cmd

import (
	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

var fetchCollectionsCmd = &cobra.Command{
	Use:   "fetch-collections",
	Short: "List all collections",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			cols, err := client.FetchCollections()
			if err != nil {
				return err
			}
			return printJSON(cols)
		})
	},
}

func init() {
	rootCmd.AddCommand(fetchCollectionsCmd)
}
