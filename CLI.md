# FluxionDB CLI

Prebuilt binaries are published for every tag named `cli-v*` on the GitHub Releases page.

## Download

1. Open the GitHub Releases page for this repository.
2. Pick the latest `cli-v*` release.
3. Download the archive for your platform:
   - `fluxiondb-linux-amd64.tar.gz`
   - `fluxiondb-linux-arm64.tar.gz`
   - `fluxiondb-darwin-amd64.tar.gz`
   - `fluxiondb-darwin-arm64.tar.gz`
   - `fluxiondb-windows-amd64.zip`

## Install

### Linux / macOS

```bash
tar -xzf fluxiondb-<os>-<arch>.tar.gz
chmod +x fluxiondb
sudo mv fluxiondb /usr/local/bin/
```

### Windows

```powershell
Expand-Archive -Path fluxiondb-windows-amd64.zip -DestinationPath .
Move-Item -Path .\\fluxiondb.exe -Destination $Env:ProgramFiles\\fluxiondb\\
setx PATH \"%PATH%;%ProgramFiles%\\fluxiondb\"
```

## Quick usage

```bash
fluxiondb fetch-collections --url wss://your-host:8080 --apikey YOUR_KEY
```

See `cli/README.md` or `fluxiondb --help` for all commands and flags.

### Legacy imports

If your backup is from the old file-based layout (pre-SQLite), run import with `--legacy`.
Expected shape:

- `<in-dir>/<collection>/<document>/*.json`

Each JSON file can be:

- a single record object: `{"data":"...", "ts": 1771760179}`
- or an array of record objects: `[{"data":"...", "ts": 1771760179}]`

> **AI agents:** use `SKILLS.md` for the FluxionDB CLI skill definition (trigger metadata plus workflow notes). Point them there when automations need to build, run, or script against this binary.
