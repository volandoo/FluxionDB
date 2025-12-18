package cmd

import (
	"fmt"
	"strings"

	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type addKeyOptions struct {
	key   string
	scope string
}

var addKeyOpts addKeyOptions

var addKeyCmd = &cobra.Command{
	Use:   "add-key",
	Short: "Create a scoped API key (requires master key)",
	RunE: func(cmd *cobra.Command, args []string) error {
		scope := fluxiondb.ApiKeyScope(strings.ToLower(addKeyOpts.scope))
		switch scope {
		case fluxiondb.ApiKeyScopeReadOnly, fluxiondb.ApiKeyScopeReadWrite, fluxiondb.ApiKeyScopeReadWriteDelete:
		default:
			return fmt.Errorf("invalid scope %q (valid: readonly, read_write, read_write_delete)", addKeyOpts.scope)
		}

		return withClient(func(client *fluxiondb.Client) error {
			resp, err := client.AddAPIKey(addKeyOpts.key, scope)
			if err != nil {
				return err
			}
			return printJSON(resp)
		})
	},
}

type removeKeyOptions struct {
	key string
}

var removeKeyOpts removeKeyOptions

var removeKeyCmd = &cobra.Command{
	Use:   "remove-key",
	Short: "Remove a previously created API key (requires master key)",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			resp, err := client.RemoveAPIKey(removeKeyOpts.key)
			if err != nil {
				return err
			}
			return printJSON(resp)
		})
	},
}

func init() {
	rootCmd.AddCommand(addKeyCmd)
	rootCmd.AddCommand(removeKeyCmd)

	addKeyCmd.Flags().StringVar(&addKeyOpts.key, "key", "", "Key identifier to create (required)")
	addKeyCmd.Flags().StringVar(&addKeyOpts.scope, "scope", "readonly", "Scope: readonly | read_write | read_write_delete")
	addKeyCmd.MarkFlagRequired("key")

	removeKeyCmd.Flags().StringVar(&removeKeyOpts.key, "key", "", "Key identifier to remove (required)")
	removeKeyCmd.MarkFlagRequired("key")
}
