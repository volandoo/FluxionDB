package cmd

import (
	"time"

	"github.com/spf13/cobra"
	fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

type fetchLatestOptions struct {
	col  string
	doc  string
	ts   int64
	from int64
}

var fetchLatestOpts fetchLatestOptions

var fetchLatestCmd = &cobra.Command{
	Use:   "fetch-latest",
	Short: "Fetch latest records per document for a collection",
	RunE: func(cmd *cobra.Command, args []string) error {
		return withClient(func(client *fluxiondb.Client) error {
			params := fluxiondb.FetchLatestRecordsParams{
				Col: fetchLatestOpts.col,
				TS:  fetchLatestOpts.ts,
				Doc: fetchLatestOpts.doc,
			}
			if cmd.Flags().Changed("from") {
				params.From = &fetchLatestOpts.from
			}

			records, err := client.FetchLatestRecords(params)
			if err != nil {
				return err
			}
			return printJSON(records)
		})
	},
}

func init() {
	rootCmd.AddCommand(fetchLatestCmd)

	fetchLatestCmd.Flags().StringVar(&fetchLatestOpts.col, "col", "", "Collection name (required)")
	fetchLatestCmd.Flags().StringVar(&fetchLatestOpts.doc, "doc", "", "Document literal or /regex/ filter")
	fetchLatestCmd.Flags().Int64Var(&fetchLatestOpts.ts, "ts", time.Now().Unix(), "Timestamp (seconds) to anchor the query")
	fetchLatestCmd.Flags().Int64Var(&fetchLatestOpts.from, "from", 0, "Optional lower bound timestamp (seconds)")
	fetchLatestCmd.MarkFlagRequired("col")
}
