package cmd

import (
	"net/url"
	"strings"
)

const (
	documentExt     = ".jsonl"
	keyValueFile    = "key_value.json"
	apiKeysFile     = "apikeys.json"
	insertBatchSize = 500
)

// archiveRecord is the JSON shape stored per line in exported document files.
type archiveRecord struct {
	TS   int64  `json:"ts"`
	Doc  string `json:"doc,omitempty"`
	Data string `json:"data"`
}

func encodeDocumentName(name string) string {
	escaped := url.PathEscape(name)
	if escaped == "" {
		return "_"
	}
	return escaped
}

func decodeDocumentName(filename string) (string, bool) {
	if !strings.HasSuffix(filename, documentExt) {
		return "", false
	}
	base := strings.TrimSuffix(filename, documentExt)
	if base == "" {
		return "", false
	}
	name, err := url.PathUnescape(base)
	if err != nil {
		return "", false
	}
	return name, true
}
