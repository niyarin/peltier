(ns peltier.core
  (:require [clojure.java.io :as io]
            [clojure.pprint :refer [pprint]]
            [clojure.tools.cli :refer [parse-opts]]
            [clojure.edn :as edn]
            [taoensso.nippy :as nippy])
  (:gen-class))

(def cli-options
  [["-p" "--pretty" "Pretty print output"]
   ["-o" "--output FILE" "Output file (default: stdout)"]
   ["-h" "--help" "Show help"]])

(defn thaw-nippy [input-file output-file pretty?]
  (let [nippy-data (with-open [in (io/input-stream input-file)]
                     (let [bytes (byte-array (.length (io/file input-file)))]
                       (.read in bytes)
                       (nippy/thaw bytes)))
        edn-str (if pretty?
                  (with-out-str (pprint nippy-data))
                  (pr-str nippy-data))]
    (if output-file
      (spit output-file edn-str)
      (print edn-str))))

(defn freeze-edn [input-file output-file]
  (let [edn-data (edn/read-string (slurp input-file))
        nippy-bytes (nippy/freeze edn-data {:compressor nil})]
    (if output-file
      (with-open [out (io/output-stream output-file)]
        (.write out nippy-bytes))
      (.write System/out nippy-bytes))))

(defn -main [& args]
  (let [{:keys [options arguments summary errors]} (parse-opts args cli-options)]
    (cond
      (:help options)
      (do
        (println "Usage: peltier <thaw|freeze> [options] <input-file>")
        (println)
        (println "Commands:")
        (println "  thaw    Decode Nippy data to EDN")
        (println "  freeze  Encode EDN data to Nippy")
        (println)
        (println "Options:")
        (println summary)
        (System/exit 0))

      errors
      (do
        (doseq [error errors]
          (println "Error:" error))
        (System/exit 1))

      (empty? arguments)
      (do
        (println "Usage: peltier <thaw|freeze> [options] <input-file>")
        (System/exit 1))

      :else
      (let [command (first arguments)]
        (if-let [input-file (second arguments)]
          (try
            (case command
              "thaw"
              (thaw-nippy input-file (:output options) (:pretty options))
              "freeze"
              (freeze-edn input-file (:output options))
              (do
                (println "Unknown command:" command)
                (println "Available commands: thaw, freeze")
                (System/exit 1)))
            (catch Exception e
              (println "Error:" (.getMessage e))
              (System/exit 1)))
          (do
            (println "Error: No input file specified for command" command)
            (System/exit 1)))))))
