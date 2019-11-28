(ns thunder-cnc.core
  (:require [cheshire.core :as json]
            [clojure.java.io :as io]
            [ring.util.response :as response]
            [ring.core.protocols :as ring-proto]
            [ring.adapter.jetty :as jetty]
            [compojure.core :refer :all]))

;; the first step of every command is to clear the previous actions
(defn clear-commands
  [state]
  (assoc state :thunder-commands []))

;; commands

(defn led-timer-action
  [{:keys [led-mode] :as state}]
  (let [state (clear-commands state)]

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
      state)))

(defn calc-next-led-mode
  [current]
  (let [modes [:off :on :blink]
        idx (inc (.indexOf modes current))
        idx (if (= idx (count modes)) 0 idx)]
    (get modes idx)))

(defn toggle-led-mode
  [state value]
  (if (= value "down")
    (let [{:keys [led-mode] :as state } (update state :led-mode calc-next-led-mode)
          state (if (#{:on :off} led-mode)
                  (assoc state :thunder-commands [(case led-mode
                                                    :on "ledon"
                                                    :off "ledoff")])
                  state)]
      (println "toggled led-mode: " led-mode)
      state)
    state))

(defn manual-api-action
  [state {:keys [control value] :as cmd}]
  (let [state (clear-commands state)]
    (cond

      (= control "led-toggle")
      (toggle-led-mode state value)

      :else
      (do
        (println "unknown manual command: " cmd)
        state))))

;; state init

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
    (assoc state
           :led-blink-task task
           :thunder-commands [])))

(defn cnc-init-state
  []
  (let [executor (java.util.concurrent.ScheduledThreadPoolExecutor. 4)
        queue (java.util.concurrent.LinkedBlockingQueue. 1024)
        base-state {:executor executor
                    :led-mode :off       ;; (:off :on :blink)
                    :fire-control :off   ;; (:off :on)
                    :move-control :off   ;; (:off :up :down :left :right)
                    :thunder-commands [] ;; each action clears and sets this, watch picks it up and queues it
                    :thunder-queue queue}
        a (agent base-state)]

    (add-watch a :action-watch ;; keyword must be unique per ref!
               (fn [_ _ old new]
                 (let [cmds (:thunder-commands new)]
                   (when-not (empty? cmds)
                     (.offer queue (:thunder-commands new))))))

    (send a led-init-timer-action a)))

(def cnc-state (cnc-init-state))

;; TODO: just make the polling cycle line up with the timeouts
;; TODO: disable idle timeouts until it starts working
;; TODO: make a heart-beat so it keeps working
;; TODO: reload properly, shutting down the state and recreating it

    
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
      (send cnc-state manual-api-action (json/parse-string l true))))
  (response/response "ok"))

;; thunder command api

(defn thunder-response-streamer
  [queue]
  (reify ring-proto/StreamableResponseBody
    (write-body-to-stream [body response output-stream]
      (println "stream begin");
      (with-open [w (io/writer output-stream)]
        (loop []
          (doseq [cmd (.take queue)]
            (.write w (str cmd "\n"));
            (.flush w))
          (recur))))))

(defn thunder-api-handler [request]
  (let [queue (-> @cnc-state :thunder-queue)]
    (response/response (thunder-response-streamer queue))))

(defroutes myapp
  (POST "/manual" [] manual-api-handler)
  (GET "/thunder" [] thunder-api-handler))

(comment

  (prn @cnc-state)

  (restart-agent cnc-state {} :clear-actions true)

  (def server (jetty/run-jetty myapp {:port 3000 :join? false}))

  (.stop server)

  (doseq [cmd ["fire" "left" "stop"]]
    (append-command cmd))

  @command-buffer

  )


;; while true; do ./ps4 | curl --trace-ascii - -H "Transfer-Encoding: chunked" -H "Content-Type: application/json" -X POST -T -  http://localhost:3000/manual; done

;; while true; do curl http://localhost:3000/thunder | ./thunder; done










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
