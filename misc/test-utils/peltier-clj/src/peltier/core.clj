(ns peltier.core
  (:require [clojure.java.io :as io]
            [clojure.pprint :refer [pprint]]
            [clojure.tools.cli :refer [parse-opts]]
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

(defn -main [& args]
  (let [{:keys [options arguments summary errors]} (parse-opts args cli-options)]
    (cond
      (:help options)
      (do
        (println "Usage: peltier thaw [options] <input.nippy>")
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
        (println "Usage: peltier thaw [options] <input.nippy>")
        (System/exit 1))

      :else
      (let [command (first arguments)]
        (case command
          "thaw"
          (if-let [input-file (second arguments)]
            (try
              (thaw-nippy input-file (:output options) (:pretty options))
              (catch Exception e
                (println "Error:" (.getMessage e))
                (System/exit 1)))
            (do
              (println "Error: No input file specified")
              (System/exit 1)))

          (do
            (println "Unknown command:" command)
            (println "Available commands: thaw")
            (System/exit 1)))))))
