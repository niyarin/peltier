#!/usr/bin/env bb

;; Generate EDN vector of random ATGC strings (each 32KB) for benchmarking large string reads
;; Usage: bb dna_string_edn.bb <count>

(def seed 12345)
(def rng (java.util.Random. seed))
(def bases [\A \T \G \C])

(defn rand-dna-string []
  (apply str (repeatedly (* 32 1024) #(nth bases (.nextInt rng 4)))))

(defn generate-dna-vector [n]
  (vec (repeatedly n rand-dna-string)))

(let [n (if (seq *command-line-args*)
          (parse-long (first *command-line-args*))
          5)]
  (prn (generate-dna-vector n)))
