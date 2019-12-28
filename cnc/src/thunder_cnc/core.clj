(ns thunder-cnc.core
  (:require [cheshire.core :as json]
            [clojure.java.io :as io]
            [ring.util.response :as response]
            [ring.core.protocols :as ring-proto]
            [ring.adapter.jetty :as jetty]
            [compojure.core :refer :all]))

;; One thing to think about is how this fits into ports and adapters. The subscribe
;; and then later notify pattern is one that should operate independently of the
;; actual protocols.
;;
;; 1. register a new thunder device. 
;; 
;;   - it must have an api key that cnc recognizes (authorization)
;;     that key represents a unique device. that device can then
;;     connect its launcher and camera to the cnc system.
;;
;;   - the launcher listens to its own dedicated command queue, if it
;;     is not connected while commands are issued, those commands are
;;     never execurted. it is presumed that commands that cannot be
;;     delivered to the launcher quickly are too out of date to be
;;     executed at all.
;;
;;   - the device also connects its video stream. this could be a feed of mkv/h264
;;     frames captured directly from the device
;;
;;   - where does the state live on the server side?
;;     - the device configs live in dynamo, reloaded perioodically
;;     - the kinesis streams and infra associated with the devices are part of the device configs, dynamically created
;;     - the internal representation of the device state lives in redis? or dynamo
;;     - the thunder heartbeat timer lives in the container where the request terminates
;;     - the led timer lives where? a distributed scheduler? it is latency-sensitive soo...
;;
;;   - each cnc-side rep of the device is allowed these two http connections, which
;;
;; 2. 

;;
;; the api servers are also the router processes, we can dispatch from http to zmq internally, and then from there it is all zmq

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

(defn thunder-heartbeat-timer-action
  [state]
  (-> state
      clear-commands
      (assoc :thunder-commands ["heartbeat"])))

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
  [state {:keys [type name value] :as cmd}]
  (let [state (clear-commands state)]
    (cond

      (= type "heartbeat")
      (do
        (println "heartbeat received")
        state)

      (and (= type "control") (= name "led-toggle"))
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

(def thunder-heartbeat-delay-ms 100)
(def thunder-heartbeat-period-ms 20000)

(defn thunder-heartbeat-init-timer-action
  [{:keys [executor] :as state} a]
  (let [task (.scheduleAtFixedRate
              executor
              (fn []
                (try
                  (send a thunder-heartbeat-timer-action)
                  (catch Exception e
                    (.printStackTrace e))))
              thunder-heartbeat-delay-ms
              thunder-heartbeat-period-ms
              java.util.concurrent.TimeUnit/MILLISECONDS)]
    (-> state
        clear-commands
        (assoc :thunder-heartbeat-task task))))

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

    (send a led-init-timer-action a)
    #_(send a thunder-heartbeat-init-timer-action a)))

(def cnc-state (cnc-init-state))

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
            (println "sending cmd: " cmd)
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

  (-> @cnc-state :thunder-heartbeat-task (.cancel true))


;; while true; do ./ps4 | curl --trace-ascii - -H "Transfer-Encoding: chunked" -H "Content-Type: application/json" -X POST -T -  http://localhost:3000/manual; done

;; while true; do curl http://localhost:3000/thunder | ./thunder; done


