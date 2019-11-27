(ns thunder-cnc.core
  (:require [cheshire.core :as json]
            [clojure.java.io :as io]
            [ring.util.response :as response]
            [ring.core.protocols :as ring-proto]
            [ring.adapter.jetty :as jetty]
            [compojure.core :refer :all]))

(defn led-timer-action
  [{:keys [led-mode] :as state}]
  (if (= led-mode :blink)
    (let [blink-state (case (:led-blink-state state)
                        nil :off
                        :on :off
                        :off :on)
          cmd (case blink-state
                :on "ledon"
                :off "ledoff")]
          (assoc state
                 :led-blink-state blink-state
                 :thunder-commands [cmd]))
    state))

(def led-blink-delay-ms 100)
(def led-blink-period-ms 200)

(defn led-init-timer-action
  [{:keys [executor] :as state} a]
  (let [task (.scheduleAtFixedRate
              executor
              (fn []
                (try
                  (send a led-timer-action)
                  (catch Exception e
                    (.printStackTrace e))))
              led-blink-delay-ms
              led-blink-period-ms
              java.util.concurrent.TimeUnit/MILLISECONDS)]
    (assoc state :led-blink-task task)))

(defn cnc-init-state
  []
  (let [executor (java.util.concurrent.ScheduledThreadPoolExecutor. 4)
        base-state {:executor executor
                    :led-mode :off       ;; (:off :on :blink)
                    :fire-control :off   ;; (:off :on)
                    :move-control :off   ;; (:off :up :down :left :right)
                    :thunder-commands [] ;; commands queued to be delivered via watch
                    }
        a (agent base-state)]
    (send a led-init-timer-action a)))

(defn calc-next-led-mode
  [current]
  (let [modes [:off :on :blink]
        idx (inc (.indexOf modes current))
        idx (if (= idx (count modes)) 0 idx)]
    (get modes idx)))

(defn toggle-led-mode
  [state value]
  (cond 
    (= value "down")
    (let [state (update state :led-mode calc-next-led-mode)]
      (println "toggled led-mode: " (:led-mode state))
      state)
    :else state))

(defn manual-api-action
  [state {:keys [control value] :as cmd}]
  (cond

    (and (= control "led-toggle"))
    (toggle-led-mode state value)

    :else
    (do
      (println "unknown manual command: " cmd)
      state)))

(def cnc-state (cnc-init-state))
    
;; manual control api ;;;;;;;;;;;;;;;;;

(defn manual-api-handler
  "This handler streams commands indefinitely, the request only finishes when
  the client closes the connection. It translates manual api calls into commands
  which are sent into the cnc agent."
  [request]
  (let [input (:body request)
        r (io/reader input)
        s (line-seq r)]
    (doseq [l s]
      (send-off cnc-state manual-api-action (json/parse-string l true))))
  (response/response "ok"))

(defroutes myapp
  (POST "/manual" [] manual-api-handler))

(comment

  (prn @cnc-state)

  (restart-agent cnc-state {} :clear-actions true)

  (def server (jetty/run-jetty myapp {:port 3000 :join? false}))

  (.stop server)

  (doseq [cmd ["fire" "left" "stop"]]
    (append-command cmd))

  @command-buffer

  )















;   (comment 
;     (->> (iterate led-timer-action @cnc-state)
;          (map :led-blink-state)
;          (take 10)))
; 
; 
; 
; 
; 
; (def command-buffer (atom []))
; 
; (defn await-change [r timeout-ms]
;   (let [p (promise)]
;     (try
;       (add-watch r :await-change ;; keyword must be unique per ref!
;                  (fn [_ _ old new]
;                    (when-not (= old new) (deliver p :changed))))
;       (deref p timeout-ms :timed-out)
;       (finally
;         (remove-watch r :await-change)))))
; 
; (defn consume-commands [current] [])
; 
; (defn append-command [cmd]
;   (swap! command-buffer #(conj % cmd)))
; 
; (def streamer
;   (reify ring-proto/StreamableResponseBody
;     (write-body-to-stream [body response output-stream]
; 
;       (with-open [w (io/writer output-stream)]
;         (loop []
; 
;           (println "starting loop")
; 
;           (when (zero? (count @command-buffer))
;             (println "waiting for change")
;             (await-change command-buffer 5000))
; 
;           (let [[consumed _] (swap-vals! command-buffer consume-commands)]
;             (doseq [cmd consumed]
;               (.write w (str cmd "\n"));
;               (.flush w)))
; 
;           (recur))))))
; 
; (defn streaming [request]
;   (response/response streamer))
; 
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 
; (defn manual [request]
;   ;(prn request)
; 
;   (let [input (:body request)
;         r (io/reader input)
;         s (line-seq r)]
; 
;     (doseq [l s]
;       (prn (json/parse-string l))))
; 
;   (response/response "ok"))
; 
; ;;;;;;;;;;;;;;;;;;;;;;;;;;
; 
; (defroutes myapp
;   (GET "/thunder" [] streaming)
;   (POST "/manual" [] manual))


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
