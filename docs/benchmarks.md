# Make benchmark resources

```sh
cd misc/stress-gen/
bb boolean_uuid_keyword_edn.bb  300000 > /tmp/bench-input1.edn
bb generate_nested_edn.bb  300000 > /tmp/bench-input2.edn
cd ../test-utils/peltier-clj/
clojure -M:run freeze  /tmp/bench-input1.edn  -o /tmp/bench-input1.nippy
clojure -M:run freeze  /tmp/bench-input2.edn  -o /tmp/bench-input2.nippy
```

## Run Clj ver Thaw benchmark.
```sh
cd misc/thaw-bench/
clojure -M bench_thaw.clj /tmp/bench-input1.nippy
clojure -M bench_thaw.clj /tmp/bench-input2.nippy
```

## Environment
- Intel(R) Core(TM) i7-8650U CPU @ 1.90GHz
- RAM 16GB
- GLIBC version 2.39-0ubuntu8.7

## Result(benchmark1)
- Peltier result (mean: 6.5, SD: 0.14)
- clojure result -> (mean: 18.50, SD: 1.04)
## Result(benchmark2)
- Peltier result (mean: 3.29, SD 0.10)
- cljure result(s)→  (mean: 15.29, SD 0.57)
