# Make benchmark resources

```sh
cd misc/stress-gen/
bb generate_nested_edn.bb  300000 > /tmp/bench-input.edn
cd ../test-utils/peltier-clj/
clojure -M:run freeze  /tmp/bench-input.edn  -o /tmp/bench-input.nippy
```

## Run Clj ver Thaw benchmark.
```sh
cd misc/thaw-bench/
clojure -M bench_thaw.clj /tmp/bench-input.nippy
```

## Environment
- AMD Ryzen 9 5900X
- Ubuntu24.04.3
- GLIBC 2.39-0ubuntu8.6
- gcc (Ubuntu 14.2.0-4ubuntu2~24.04) 14.2.0
- RAM 64GB

## Result
- Peltier result (mean: 2.26, SD 0.024)
- cljure result(s)→  (mean: 8.84, SD 0.444)


