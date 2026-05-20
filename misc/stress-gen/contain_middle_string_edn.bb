#!/usr/bin/env bb

;; Generate nested EDN data for testing
;; Usage: bb generate_nested_edn.bb <inner-map-size>

(def seed 12345)
(def rng (java.util.Random. seed))

(defn rand-choice [coll]
  (nth coll (.nextInt rng (count coll))))

(defn rand-float []
  (.nextDouble rng))

(defn generate-inner-value []
  [(rand-choice [(apply str (repeat 50 "ABC")) "0123456789" "あいうえおかきくけこさしすせそたちつてと" nil])
   (rand-float)])

(defn generate-inner-map [size]
  (into {} (for [i (range size)]
             [i (generate-inner-value)])))

(defn generate-outer-map [inner-size]
  (into {} (for [c (map char (range (int \a) (inc (int \z))))]
             [(keyword (str c)) (generate-inner-map inner-size)])))

(let [inner-size (if (seq *command-line-args*)
                   (parse-long (first *command-line-args*))
                   5)]
  (prn (generate-outer-map inner-size)))
