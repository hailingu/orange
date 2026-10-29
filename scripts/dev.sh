#!/bin/sh
# ADR: docs/adr/ADR-0003-local-runtime-path.md
# The one documented local path (ADR-0003 T-3): build both sides and serve
# the two fixed loopback origins. Certificate material is generated into the
# gitignored run/ location on first use and reused afterwards; Ctrl-C stops
# both listeners cleanly (exit 0).
set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

echo "==> building the service"
cmake -S service -B service/build >/dev/null
cmake --build service/build --parallel "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "==> building the web client"
if [ ! -d web/node_modules ]; then
    npm ci --prefix web --no-fund --no-audit
fi
npm run build --prefix web >/dev/null

echo "==> starting orange (Ctrl-C stops both listeners)"
exec ./service/build/orange_service --root web/dist
