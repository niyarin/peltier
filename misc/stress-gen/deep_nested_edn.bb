#!/usr/bin/env bb

;; Generate deeply nested EDN data for benchmarking nesting handling.
;; Usage: bb deep_nested_edn.bb <depth> <leaf-size>

(def seed 12345)
(def rng (java.util.Random. seed))

(defn rand-choice [coll]
  (nth coll (.nextInt rng (count coll))))

(defn rand-float []
  (.nextDouble rng))

(defn generate-leaf-value [i]
  [(keyword (str "leaf-" (mod i 16)))
   (rand-choice [true false nil])
   i
   (rand-float)])

(defn generate-leaf [size]
  (vec (map generate-leaf-value (range size))))

(defn wrap-level [level child]
  (case (mod level 4)
    0 {:level level
       :kind :map
       :next child}
    1 [:level level :kind :vector :next child]
    2 (list :level level :kind :list :next child)
    3 {:level level
       :kind :mixed
       :items [child {:flag (even? level)
                      :tag (keyword (str "level-" level))}]}))

(defn generate-deep-nested [depth leaf-size]
  (reduce (fn [child level]
            (wrap-level level child))
          (generate-leaf leaf-size)
          (range (dec depth) -1 -1)))

(let [depth (if (seq *command-line-args*)
              (parse-long (first *command-line-args*))
              100)
      leaf-size (if (second *command-line-args*)
                  (parse-long (second *command-line-args*))
                  10)]
  (prn (generate-deep-nested depth leaf-size)))
