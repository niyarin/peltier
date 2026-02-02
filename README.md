# Peltier
Fast CLI tool to convert Nippy and EDN.

## Usage

### Thaw (Nippy → EDN)
```sh
# output edn
peltier thaw input.nippy

# Pretty print
peltier thaw -p input.nippy

# Output to file
peltier thaw -o output.edn input.nippy
```

### Freeze (EDN → Nippy)
```sh
# Convert EDN to Nippy
peltier freeze input.edn -o output.nippy

# Write to stdout (binary)
peltier freeze input.edn > output.nippy
```

## Compile
```
make
```
