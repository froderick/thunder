(ns thunder-cnc.core
  (:require [cheshire.core :as json]
            [clojure.java.io :as io]
            [ring.util.response :as response]
            [ring.core.protocols :as ring-proto]
            [ring.adapter.jetty :as jetty]
            [compojure.core :refer :all]
            ))

(def command-buffer (atom []))

(defn await-change [r timeout-ms]
  (let [p (promise)]
    (try
      (add-watch r :await-change ;; keyword must be unique per ref!
                 (fn [_ _ old new]
                   (when-not (= old new) (deliver p :changed))))
      (deref p timeout-ms :timed-out)
      (finally
        (remove-watch r :await-change)))))

(defn consume-commands [current] [])

(defn append-command [cmd]
  (swap! command-buffer #(conj % cmd)))

(def streamer
  (reify ring-proto/StreamableResponseBody
    (write-body-to-stream [body response output-stream]

      (with-open [w (io/writer output-stream)]
        (loop []

          (println "starting loop")

          (when (zero? (count @command-buffer))
            (println "waiting for change")
            (await-change command-buffer 5000))

          (let [[consumed _] (swap-vals! command-buffer consume-commands)]
            (doseq [cmd consumed]
              (.write w (str cmd "\n"));
              (.flush w)))

          (recur))))))

(defn streaming [request]
  (response/response streamer))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defn manual [request]
  ;(prn request)

  (let [input (:body request)
        r (io/reader input)
        s (line-seq r)]

    (doseq [l s]
      (prn (json/parse-string l))))

  (response/response "ok"))

;;;;;;;;;;;;;;;;;;;;;;;;;;

(defroutes myapp
  (GET "/thunder" [] streaming)
  (POST "/manual" [] manual))

(comment

  (def server (jetty/run-jetty myapp {:port 3000 :join? false}))

  (.stop server)

  (doseq [cmd ["fire" "left" "stop"]]
    (append-command cmd))

  @command-buffer

  )

;; curl http://localhost:3000/thunder | ./thunder

;; ./ps4 | curl --trace-ascii - -H "Transfer-Encoding: chunked" -H "Content-Type: application/json" -X POST -T -  http://localhost:3000/manual

; (def streamer (reify ring-proto/StreamableResponseBody
;                 (write-body-to-stream [body response output-stream]
;                   (with-open [w (io/writer output-stream)]
;                     (doseq [r (range 4)]
;                       (.write w "fire\n");
;                       (.flush w)
;                       (java.lang.Thread/sleep 1000))))))

; (defn thingum [request]
;   (if (zero? (count @command-buffer))
;     (await-change command-buffer 5000))
; 
;   (let [[consumed _] (swap-vals! command-buffer consume-commands)]
;     {:status 200
;      :headers {"Content-Type" "application/json"}
;      :body (json/generate-string {:commands consumed})}))



                                        ; (ring-io/piped-input-stream
                                        ;  (fn [out]
                                        ;    (let [w (io/make-writer out {})]

                                        ;      (doseq [r (range 4)]

                                        ;        (.write w "fire\n");
                                        ;        (.flush w)
                                        ;        (java.lang.Thread/sleep 1000)

;; (println "start of loop")

;; (when (zero? (count @command-buffer))
;;   (println "waiting for change")
;;   (await-change command-buffer 5000))

;; (let [[consumed _] (swap-vals! command-buffer consume-commands)
;;       s (json/generate-string {:commands consumed})]

;;   (println "writing: " s)

;;   (.write w s)
;;   (.flush w))

                                        ;      ))))))
