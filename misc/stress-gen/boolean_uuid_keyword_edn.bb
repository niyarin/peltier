#!/usr/bin/env bb

;; Generate nested EDN data containing booleans, UUIDs, and keywords.
;; Usage: bb boolean_uuid_keyword_edn.bb <inner-map-size>
(import java.util.UUID)

(def seed 12345)
(def rng (java.util.Random. seed))

(defn rand-choice [coll]
  (nth coll (.nextInt rng (count coll))))

(defn rand-uuid []
  (UUID. (.nextLong rng) (.nextLong rng)))

(defn generate-inner-value []
  [(rand-choice [true false])
   (rand-uuid)
   (rand-choice [:alpha :beta :gamma :delta :epsilon])])

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
