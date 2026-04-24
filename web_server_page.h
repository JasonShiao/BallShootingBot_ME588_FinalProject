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
    select, button, input[type="range"] {
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

    .motion-card.disabled {
      opacity: 0.5;
    }
    .motion-status {
      font-size: 0.95rem;
      margin-bottom: 12px;
      color: #555;
    }
    .speed-row {
      display: flex;
      align-items: center;
      gap: 12px;
      flex-wrap: wrap;
      margin-bottom: 14px;
    }
    #speedSlider {
      width: 240px;
    }
    #speedValue {
      min-width: 48px;
      font-weight: bold;
    }
    #teamState {
      padding: 10px 14px;
      border-radius: 8px;
      font-weight: bold;
      min-width: 80px;
      text-align: center;
    }
    /* Team colors */
    #teamState.red {
      background: #ff4d4d;
      color: white;
    }
    #teamState.blue {
      background: #4d79ff;
      color: white;
    }

    #teamState.unknown {
      background: #cccccc;
      color: #333;
    }
    .dpad-8 {
      display: grid;
      grid-template-columns: 80px 80px 80px;
      grid-template-rows: 56px 56px 56px;
      gap: 8px;
      justify-content: start;
      align-items: stretch;
    }
    .motion-btn {
      border: none;
      border-radius: 10px;
      background: #2d6cdf;
      color: white;
      font-weight: bold;
      cursor: pointer;
      box-shadow: 0 2px 6px rgba(0,0,0,0.12);
      transition: transform 0.08s ease, background 0.2s ease;
    }
    .motion-btn:hover:not(:disabled) {
      background: #1f57bb;
    }
    .motion-btn:active:not(:disabled) {
      transform: scale(0.97);
    }
    .motion-btn:disabled {
      background: #b8c2d6;
      color: #eef2f8;
      cursor: not-allowed;
      box-shadow: none;
    }
    .motion-stop {
      background: #666;
    }
    .motion-stop:hover:not(:disabled) {
      background: #4f4f4f;
    }
    .empty-cell {
      visibility: hidden;
    }
  </style>
</head>
<body>
  <div class="card">
    <h2>Game Context</h2>

    <div class="row">
      <div>
        <div>Team</div>
        <div class="state-box" id="teamState">Red</div>
      </div>

      <div>
        <div>Beacon</div>
        <div class="state-box" id="beaconState">Unknown</div>
      </div>
    </div>
  </div>

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
        <option value="MoveToNextJunction">MoveToNextJunction</option>
        <option value="CheckHillLoyalty">CheckHillLoyalty</option>
        <option value="BallLoading">BallLoading</option>
        <option value="BallLaunching">BallLaunching</option>
        <option value="BackHome">BackHome</option>
        <option value="WaitBucketReload">WaitBucketReload</option>
        <option value="ManualControl">ManualControl</option>
        <option value="Error">Error</option>
      </select>
      <button id="sendBtn">Send</button>
      <button id="stopBtn" class="stop-btn" title="Force Stop">STOP</button>
    </div>
    <div id="statusMsg"></div>
  </div>

  <div class="card motion-card" id="motionCard">
    <h2>Manual Motion Control</h2>
    <div class="motion-status" id="motionStatus">
      Motion controls are enabled only in ManualControl state.
    </div>

    <div class="speed-row">
      <label for="speedSlider">Speed</label>
      <input type="range" id="speedSlider" min="0" max="255" value="120">
      <span id="speedValue">120</span>
    </div>

    <!-- D-pad with diagonals -->
    <div class="dpad-8">
      <button class="motion-btn" id="btnForwardLeft">↖</button>
      <button class="motion-btn" id="btnForward">↑</button>
      <button class="motion-btn" id="btnForwardRight">↗</button>

      <button class="motion-btn" id="btnRotateCCW">⟲</button>
      <div class="dpad-center"></div>
      <button class="motion-btn" id="btnRotateCW">⟳</button>

      <button class="motion-btn" id="btnBackwardLeft">↙</button>
      <button class="motion-btn" id="btnBackward">↓</button>
      <button class="motion-btn" id="btnBackwardRight">↘</button>
    </div>
  </div>

  <div class="card">
    <h2>Transition Log</h2>
    <div id="log"></div>
  </div>

  <script>
    const currentStateEl = document.getElementById("currentState");
    const logEl = document.getElementById("log");
    const teamStateEl = document.getElementById("teamState");
    const beaconStateEl = document.getElementById("beaconState");
    const sendBtn = document.getElementById("sendBtn");
    const stopBtn = document.getElementById("stopBtn");
    const stateSelect = document.getElementById("stateSelect");
    const statusMsg = document.getElementById("statusMsg");

    const btnForward = document.getElementById("btnForward");
    const btnForwardRight = document.getElementById("btnForwardRight");
    const btnForwardLeft = document.getElementById("btnForwardLeft");
    const btnBackward = document.getElementById("btnBackward");
    const btnBackwardRight = document.getElementById("btnBackwardRight");
    const btnBackwardLeft = document.getElementById("btnBackwardLeft");
    const btnRotateCW = document.getElementById("btnRotateCW");
    const btnRotateCCW = document.getElementById("btnRotateCCW");
    
    const motionButtons = [
        btnForward,
        btnForwardRight,
        btnForwardLeft,
        btnBackward,
        btnBackwardRight,
        btnBackwardLeft,
        btnRotateCW,
        btnRotateCCW
    ];

    const motionBtnMap = {
      btnForward: "Forward",
      btnForwardRight: "ForwardRight",
      btnForwardLeft: "ForwardLeft",
      btnBackward: "Backward",
      btnBackwardRight: "BackwardRight",
      btnBackwardLeft: "BackwardLeft",
      btnRotateCW: "RotateCW",
      btnRotateCCW: "RotateCCW",
    };

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

    function isManualControlState() {
      return currentStateEl.textContent === "ManualControl";
    }

    function updateMotionWidgets() {
      const enabled = isManualControlState();

      motionCard.classList.toggle("disabled", !enabled);
      speedSlider.disabled = !enabled;
      for (const btn of motionButtons) {
        btn.disabled = !enabled;
      }

      motionStatus.textContent = enabled
        ? "Manual motion control enabled."
        : "Motion controls are enabled only in ManualControl state.";
    }

    function updateTeamUI(team) {
      teamStateEl.textContent = team;

      // remove old classes
      teamStateEl.classList.remove("red", "blue", "unknown");

      // apply new class
      if (team === "Red") {
        teamStateEl.classList.add("red");
      } else if (team === "Blue") {
        teamStateEl.classList.add("blue");
      } else {
        teamStateEl.classList.add("unknown");
      }
    }

    async function loadInitialState() {
      try {
        const res = await fetch("/state");
        const data = await res.json();

        currentStateEl.textContent = data.currentState;
        updateTeamUI(data.currentTeam);
        beaconStateEl.textContent = data.currentBeaconState;
        logEl.innerHTML = "";

        for (const entry of data.log) {
          addLogEntry(entry);
        }

        updateMotionWidgets();
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

    async function sendMotion(dir) {
      if (!isManualControlState()) {
        statusMsg.textContent = "Motion control is only available in ManualControl state";
        updateMotionWidgets();
        return;
      }

      const speed = speedSlider.value;
      statusMsg.textContent = "";

      try {
        const res = await fetch("/motionCmd", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: "dir=" + encodeURIComponent(dir) + "&speed=" + encodeURIComponent(speed)
        });

        const text = await res.text();
        statusMsg.textContent = text;
      } catch (err) {
        statusMsg.textContent = "Motion request failed";
      }
    }

    speedSlider.addEventListener("input", () => {
      speedValue.textContent = speedSlider.value;
    });

    sendBtn.addEventListener("click", async () => {
      await sendState(stateSelect.value);
    });

    stopBtn.addEventListener("click", async () => {
      await sendState("Idle");
    });

    for (const id in motionBtnMap) {
      const el = document.getElementById(id);
      if (!el) continue;

      el.addEventListener("click", async () => {
        await sendMotion(motionBtnMap[id]);
      });
    }

    const evtSource = new EventSource("/events");

    evtSource.addEventListener("state", (event) => {
      currentStateEl.textContent = event.data;
      updateMotionWidgets();
    });

    evtSource.addEventListener("team", (event) => {
      updateTeamUI(event.data);
    });
    
    evtSource.addEventListener("beacon", (event) => {
      beaconStateEl.textContent = event.data;
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
    updateMotionWidgets();
  </script>
</body>
</html>
)rawliteral";

#endif

