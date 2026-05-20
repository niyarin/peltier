#!/usr/bin/env bb

;; Generate nested EDN data for testing
;; Usage: bb generate_nested_edn.bb <inner-map-size>
(import java.util.UUID)

(def seed 12345)
(def rng (java.util.Random. seed))

(defn rand-choice [coll]
  (nth coll (.nextInt rng (count coll))))

(defn rand-float []
  (.nextDouble rng))

(defn generate-inner-value []
  (let [tp (rand-choice [:string :double :int :uuid])]
    (case tp
      :string "looooooooooooooooooooooooooooooooooooooooooooooooooooooooooong"
      :double (rand-float)
      :int 1234567
      :uuid  (UUID/randomUUID))))

(defn generate-internal-vector []
  (vec (repeatedly 1000 generate-inner-value)))

(defn generate-outer-vector [size]
  (vec (repeatedly size
                   generate-internal-vector)))

(let [size (if (seq *command-line-args*)
                   (parse-long (first *command-line-args*))
                   5)]
  (prn (generate-outer-vector size)))
