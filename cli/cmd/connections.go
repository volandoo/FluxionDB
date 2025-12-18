package cmd

import (
	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

var connectionsCmd = &cobra.Command{
	Use:   "connections",
	Short: "List active FluxionDB connections",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			conns, err := client.GetConnections()
			if err != nil {
				return err
			}
			return printJSON(conns)
		})
	},
}

func init() {
	rootCmd.AddCommand(connectionsCmd)
}
