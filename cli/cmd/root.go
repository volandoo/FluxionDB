package cmd

import (
	"encoding/json"
	"fmt"
	"os"

	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

// cliConfig holds shared connection parameters.
type cliConfig struct {
	url            string
	apiKey         string
	connectionName string
}

var (
	cfg     cliConfig
	rootCmd = &cobra.Command{
		Use:   "fluxiondb",
		Short: "CLI for interacting with FluxionDB over WebSockets",
		PersistentPreRunE: func(cmd *cobra.Command, _ []string) error {
			if cfg.url == "" {
				return fmt.Errorf("missing required --url")
			}
			if cfg.apiKey == "" {
				return fmt.Errorf("missing required --apikey")
			}
			return nil
		},
	}
)

// Execute runs the root command.
func Execute() error {
	return rootCmd.Execute()
}

func init() {
	rootCmd.PersistentFlags().StringVar(&cfg.url, "url", "", "FluxionDB WebSocket URL (e.g. wss://localhost:8080)")
	rootCmd.PersistentFlags().StringVar(&cfg.apiKey, "apikey", "", "API key for authentication")
	rootCmd.PersistentFlags().StringVar(&cfg.connectionName, "name", "", "Optional connection name for observability")
	rootCmd.MarkPersistentFlagRequired("url")
	rootCmd.MarkPersistentFlagRequired("apikey")
}

// withClient connects using shared flags and invokes fn. The connection is
// closed after fn returns.
func withClient(fn func(client *fluxiondb.Client) error) error {
	client := fluxiondb.NewClient(cfg.url, cfg.apiKey)
	if cfg.connectionName != "" {
		client.SetConnectionName(cfg.connectionName)
	}
	if err := client.Connect(); err != nil {
		return err
	}
	defer client.Close()
	return fn(client)
}

// printJSON renders the given value with indentation to stdout.
func printJSON(v interface{}) error {
	enc := json.NewEncoder(os.Stdout)
	enc.SetIndent("", "  ")
	return enc.Encode(v)
}
