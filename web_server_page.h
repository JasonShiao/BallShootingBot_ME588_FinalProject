#ifndef WEB_SERVER_PAGE_H
#define WEB_SERVER_PAGE_H

// ============================
// HTML page
// ============================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 State Monitor</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 20px;
      background: #f5f5f5;
      color: #222;
    }
    .card {
      background: white;
      padding: 16px;
      border-radius: 12px;
      margin-bottom: 16px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
    }
    h1, h2 {
      margin-top: 0;
    }
    .state-box {
      font-size: 1.2rem;
      font-weight: bold;
      padding: 10px 14px;
      display: inline-block;
      border-radius: 8px;
      background: #eef3ff;
    }
    .row {
      display: flex;
      gap: 10px;
      align-items: center;
      flex-wrap: wrap;
    }
    select, button {
      font-size: 1rem;
      padding: 8px 12px;
    }
    #statusMsg {
      margin-top: 10px;
      color: #0a6;
    }
    #log {
      max-height: 400px;
      overflow-y: auto;
      background: #111;
      color: #eee;
      padding: 10px;
      border-radius: 8px;
      font-family: monospace;
      white-space: pre-wrap;
    }
    .log-entry {
      padding: 4px 0;
      border-bottom: 1px solid rgba(255,255,255,0.08);
    }
    .stop-btn {
      width: 72px;
      height: 72px;
      border-radius: 50%;
      border: none;
      background: #d62828;
      color: white;
      font-weight: bold;
      font-size: 1rem;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 4px 10px rgba(0,0,0,0.2);
      transition: transform 0.08s ease, background 0.2s ease;
    }
    .stop-btn:hover {
      background: #b71c1c;
    }
    .stop-btn:active {
      transform: scale(0.96);
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>ESP32 State Monitor</h1>
    <div>Current state:</div>
    <div class="state-box" id="currentState">Unknown</div>
  </div>

  <div class="card">
    <h2>Force New State</h2>
    <div class="row">
      <select id="stateSelect">
        <option value="Idle">Idle</option>
        <option value="Started">Started</option>
        <option value="CheckHillLoyalty">CheckHillLoyalty</option>
        <option value="LaunchingBall">LaunchingBall</option>
        <option value="ForceStopped">ForceStopped</option>
        <option value="Error">Error</option>
      </select>
      <button id="sendBtn">Send</button>
      <button id="stopBtn" class="stop-btn" title="Force Stop">STOP</button>
    </div>
    <div id="statusMsg"></div>
  </div>

  <div class="card">
    <h2>Transition Log</h2>
    <div id="log"></div>
  </div>

  <script>
    const currentStateEl = document.getElementById("currentState");
    const logEl = document.getElementById("log");
    const sendBtn = document.getElementById("sendBtn");
    const stopBtn = document.getElementById("stopBtn");
    const stateSelect = document.getElementById("stateSelect");
    const statusMsg = document.getElementById("statusMsg");

    function escapeHtml(text) {
      const div = document.createElement("div");
      div.textContent = text;
      return div.innerHTML;
    }

    function addLogEntry(text) {
      const div = document.createElement("div");
      div.className = "log-entry";
      div.innerHTML = escapeHtml(text);
      logEl.appendChild(div);
      logEl.scrollTop = logEl.scrollHeight;
    }

    async function loadInitialState() {
      try {
        const res = await fetch("/state");
        const data = await res.json();

        currentStateEl.textContent = data.currentState;
        logEl.innerHTML = "";

        for (const entry of data.log) {
          addLogEntry(entry);
        }
      } catch (err) {
        addLogEntry("Failed to load initial state");
      }
    }

    async function sendState(state) {
      statusMsg.textContent = "";

      try {
        const res = await fetch("/setState", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: "state=" + encodeURIComponent(state)
        });

        const text = await res.text();
        statusMsg.textContent = text;
      } catch (err) {
        statusMsg.textContent = "Request failed";
      }
    }

    sendBtn.addEventListener("click", async () => {
      await sendState(stateSelect.value);
    });

    stopBtn.addEventListener("click", async () => {
      await sendState("ForceStopped");
    });

    const evtSource = new EventSource("/events");

    evtSource.addEventListener("state", (event) => {
      currentStateEl.textContent = event.data;
    });

    evtSource.addEventListener("log", (event) => {
      addLogEntry(event.data);
    });

    evtSource.addEventListener("hello", (event) => {
      console.log("SSE connected:", event.data);
    });

    evtSource.onerror = () => {
      console.log("SSE connection issue");
    };

    loadInitialState();
  </script>
</body>
</html>
)rawliteral";

#endif

