# peltier-clj

Clojure implementation of peltier for comparison/benchmarking.

## Usage

```bash
# Convert Nippy to EDN
clojure -M:run thaw input.nippy

# Pretty print
clojure -M:run thaw -p input.nippy

# Output to file
clojure -M:run thaw -o output.edn input.nippy

# Help
clojure -M:run thaw --help
```

## Example

```bash
cd misc/test-utils/peltier-clj
clojure -M:run thaw ../../../benchmark_nocomp.nippy -o /tmp/output.edn
```
