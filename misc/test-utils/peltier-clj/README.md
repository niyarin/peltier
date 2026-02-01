# peltier-clj

Clojure implementation of peltier for comparison/benchmarking.

## Usage

### Thaw (Nippy → EDN)

```bash
# Convert Nippy to EDN
clojure -M:run thaw input.nippy

# Pretty print
clojure -M:run thaw -p input.nippy

# Output to file
clojure -M:run thaw -o output.edn input.nippy
```

### Freeze (EDN → Nippy)

```bash
# Convert EDN to Nippy
clojure -M:run freeze input.edn -o output.nippy

# Write to stdout (binary)
clojure -M:run freeze input.edn > output.nippy
```

### Help

```bash
clojure -M:run --help
```

## Examples

```bash
cd misc/test-utils/peltier-clj

# Thaw: Nippy to EDN
clojure -M:run thaw ../../../benchmark_nocomp.nippy -o /tmp/output.edn

# Freeze: EDN to Nippy
clojure -M:run freeze /tmp/output.edn -o /tmp/test.nippy

# Round-trip test
clojure -M:run thaw /tmp/test.nippy
```
