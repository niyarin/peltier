(ns bench-thaw
  (:require [clojure.java.io :as io]
            [taoensso.nippy :as nippy]
            [criterium.core :refer [quick-bench]]))

;;clojure -M bench_thaw.clj <input.nippy> [output.edn]

(defn run-benchmark [input-path output-path]
  (let [input-file (io/file input-path)
        output-file (io/file output-path)]

    (println "=== Nippy to EDN Conversion Benchmark ===")
    (println "Input file:" (.getAbsolutePath input-file))
    (println "Output file:" (.getAbsolutePath output-file))
    (println)

    (when-not (.exists input-file)
      (println "ERROR: Input file does not exist!")
      (System/exit 1))

    (println "Input file size:" (.length input-file) "bytes")
    (println)

    ;; Warmup
    (println "Running warmup...")
    (let [nippy-data (nippy/thaw-from-file (.getAbsolutePath input-file))
          edn-str (pr-str nippy-data)]
      (spit output-file edn-str)
      (println "Output file size:" (.length output-file) "bytes"))
    (println)

    ;; Benchmark
    (println "Running benchmark with criterium...")
    (println)
    (quick-bench
      (let [nippy-data (nippy/thaw-from-file (.getAbsolutePath input-file))
            edn-str (pr-str nippy-data)]
        (spit output-file edn-str)))

    (println)
    (println "Benchmark complete!")))

(defn -main [& args]
  (if (< (count args) 1)
    (do
      (println "Usage: clojure -M -m bench-thaw <input.nippy> [output.edn]")
      (System/exit 1))
    (let [input-path (first args)
          output-path (or (second args) "/tmp/bench-input.edn")]
      (run-benchmark input-path output-path))))

;; Run when executed directly
(apply -main *command-line-args*)
