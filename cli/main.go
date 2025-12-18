package main

import (
	"log"

	"github.com/volandoo/fluxiondb/cli/cmd"
)

func main() {
	// Direct cobra errors to stderr and exit non-zero on failure.
	if err := cmd.Execute(); err != nil {
		log.Fatalf("fluxiondb: %v", err)
	}
}
