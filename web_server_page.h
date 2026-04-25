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

    .route-map {
      position: relative;
      height: 190px;
      margin-top: 20px;
    }

    .segment {
      position: absolute;
      top: 82px;
      height: 8px;
      background: #2d6cdf;
      border-radius: 999px;
    }

    .transition-circle {
      position: absolute;
      top: 68px;
      width: 32px;
      height: 32px;
      border: 4px solid #0a6;
      background: white;
      border-radius: 50%;
      transform: translateX(-50%);
      z-index: 2;
    }

    .transition-circle.inactive {
      border-color: #aaa;
      background: #eee;
    }

    .robot-marker {
      position: absolute;
      top: 62px;
      left: 4%;
      font-size: 42px;
      color: #d62828;
      transform: translateX(-50%);
      transition: left 0.35s ease, transform 0.25s ease;
      z-index: 5;
    }

    .route-label {
      position: absolute;
      top: 110px;
      width: 130px;
      transform: translateX(-50%);
      text-align: center;
      font-size: 0.85rem;
      font-weight: bold;
    }

    .graph-status {
      margin-top: 8px;
      padding: 10px 14px;
      border-radius: 8px;
      background: #eefaf3;
      color: #0a6;
      font-weight: bold;
    }

    .graph-status.inactive {
      background: #f2f2f2;
      color: #777;
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

      <div>
        <div>Location</div>
        <div class="state-box" id="locationState">Unknown</div>
      </div>

      <div>
        <div>Heading</div>
        <div class="state-box" id="headingState">Unknown</div>
      </div>
    </div>
  </div>

  <div class="card">
    <h2>Realtime Status Graph</h2>

    <div class="route-map">

      <div class="transition-circle" id="circle0" style="left: 2%;"></div>
      <!-- Segment 1 -->
      <div class="segment active" style="left: 4%; width: 12%;"></div>
      <div class="transition-circle" id="circle1" style="left: 18%;"></div>

      <!-- Segment 2 -->
      <div class="segment active" style="left: 20%; width: 12%;"></div>
      <div class="transition-circle" id="circle2" style="left: 34%;"></div>

      <!-- Segment 3 -->
      <div class="segment active" style="left: 36%; width: 12%;"></div>
      <div class="transition-circle" id="circle3" style="left: 50%;"></div>

      <!-- Segment 4 -->
      <div class="segment active" style="left: 52%; width: 12%;"></div>
      <div class="transition-circle" id="circle4" style="left: 66%;"></div>

      <!-- Segment 5 -->
      <div class="segment active" style="left: 68%; width: 12%;"></div>
      <div class="transition-circle" id="circle5" style="left: 82%;"></div>

      <!-- Segment 6 -->
      <div class="segment active" style="left: 84%; width: 12%;"></div>

      <div id="robotMarker" class="robot-marker">➤</div>

      <div class="route-label" style="left: 10%;">1<br>Home</div>
      <div class="route-label" style="left: 26%;">2<br>HomeToJunction1</div>
      <div class="route-label" style="left: 42%;">3<br>Junction1ToJunction2</div>
      <div class="route-label" style="left: 58%;">4<br>Junction2ToJunction3</div>
      <div class="route-label" style="left: 74%;">5<br>Junction3ToJunction4</div>
      <div class="route-label" style="left: 90%;">6<br>Junction4ToRoadEnd</div>
    </div>

    <div id="graphStatus" class="graph-status">
      Transition circles active
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
        <option value="Startup">Startup</option>
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
    const locationStateEl = document.getElementById("locationState");
    const headingStateEl = document.getElementById("headingState");
    const sendBtn = document.getElementById("sendBtn");
    const stopBtn = document.getElementById("stopBtn");
    const stateSelect = document.getElementById("stateSelect");
    const statusMsg = document.getElementById("statusMsg");

    const robotMarkerEl = document.getElementById("robotMarker");
    const graphStatusEl = document.getElementById("graphStatus");
    const transitionCircles = [
      document.getElementById("circle0"),
      document.getElementById("circle1"),
      document.getElementById("circle2"),
      document.getElementById("circle3"),
      document.getElementById("circle4"),
      document.getElementById("circle5")
    ];

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

    const locationPositionMap = {
      Home: 10,
      HomeBorderCrossed1: 10,
      HomeBorderCrossed2: 10,

      HomeToJunction1: 26,
      Junction1ToJunction2: 42,
      Junction2ToJunction3: 58,
      Junction3ToJunction4: 74,
      Junction4ToRoadEnd: 90
    };

    function getActiveCircleIndex(location, heading) {
      const movingForward = heading === "Forward";
      const movingBackward = heading === "Backward";

      if (!movingForward && !movingBackward) {
        return -1;
      }

      if (movingForward) {
        switch (location) {
          case "Home":
            return 0;
          case "HomeBorderCrossed1":
          case "HomeBorderCrossed2":
          case "HomeToJunction1":
            return 1; // circle1
          case "Junction1ToJunction2":
            return 2; // circle2
          case "Junction2ToJunction3":
            return 3; // circle3
          case "Junction3ToJunction4":
            return 4; // circle4
          case "Junction4ToRoadEnd":
            return 5; // circle5
          default:
            return -1;
        }
      }

      if (movingBackward) {
        switch (location) {
          case "Home":
            return 0; // circle0
          case "HomeBorderCrossed1":
          case "HomeBorderCrossed2":
            return 1; // circle1
          case "HomeToJunction1":
            return 2; // circle2
          case "Junction1ToJunction2":
            return 3; // circle3
          case "Junction2ToJunction3":
            return 4; // circle4
          case "Junction3ToJunction4":
          case "Junction4ToRoadEnd":
            return 5; // circle5
          default:
            return -1;
        }
      }

      return -1;
    }

    function updateTransitionCircles(location, heading) {
      const enabled = shouldEnableTransitionCircles(currentStateEl.textContent);
      const activeIndex = enabled ? getActiveCircleIndex(location, heading) : -1;

      for (let i = 0; i < transitionCircles.length; i++) {
        transitionCircles[i].classList.toggle("inactive", i !== activeIndex);
      }

      if (enabled && activeIndex >= 0) {
        robotMarkerEl.style.display = "block";
        robotMarkerEl.style.left = transitionCircles[activeIndex].style.left;
      } else if (enabled) {
        robotMarkerEl.style.display = "none";
      } else {
        robotMarkerEl.style.display = "block";
      }

      graphStatusEl.classList.toggle("inactive", !enabled);
      graphStatusEl.textContent = enabled
        ? "Junction active"
        : "Robot moving";
    }

    function updateRobotGraph(location, heading) {
      const enabled = shouldEnableTransitionCircles(currentStateEl.textContent);

      if (!enabled) {
        const pos = locationPositionMap[location];

        if (pos !== undefined) {
          robotMarkerEl.style.left = pos + "%";
        }
      }

      const deg = headingToRotationDeg(heading);
      robotMarkerEl.style.transform = `translateX(-50%) rotate(${deg}deg)`;

      updateTransitionCircles(location, heading);
    }

    function headingToRotationDeg(heading) {
      switch (heading) {
        case "Forward":
          return 0;
        case "Backward":
          return 180;
        case "Left":
        case "RotateCCW":
          return -90;
        case "Right":
        case "RotateCW":
          return 90;
        default:
          return 0;
      }
    }

    function shouldEnableTransitionCircles(state) {
      return state !== "BackHome" &&
            state !== "MoveToNextJunction";
    }

    async function loadInitialState() {
      try {
        const res = await fetch("/state");
        const data = await res.json();

        currentStateEl.textContent = data.currentState;
        updateTeamUI(data.currentTeam);
        beaconStateEl.textContent = data.currentBeaconState;
        locationStateEl.textContent = data.currentLocationState;
        headingStateEl.textContent = data.currentHeadingState;
        logEl.innerHTML = "";

        updateRobotGraph(data.currentLocationState, data.currentHeadingState);

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
      updateRobotGraph(locationStateEl.textContent, headingStateEl.textContent);
    });

    evtSource.addEventListener("team", (event) => {
      updateTeamUI(event.data);
    });
    
    evtSource.addEventListener("beacon", (event) => {
      beaconStateEl.textContent = event.data;
    });

    evtSource.addEventListener("location", (event) => {
      locationStateEl.textContent = event.data;
      updateRobotGraph(locationStateEl.textContent, headingStateEl.textContent);
    });

    evtSource.addEventListener("heading", (event) => {
      headingStateEl.textContent = event.data;
      updateRobotGraph(locationStateEl.textContent, headingStateEl.textContent);
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

