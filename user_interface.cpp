#include "user_interface.h"
#include <Arduino.h>
#include "FreeRTOS.h"
#include "globals.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "manual_control.h"

#include "web_server_page.h"
#include "hfsm.h"

bool parseInt(const char* str, int& out);

void userIntefaceTask(void *parameter);
static TaskHandle_t userInterfaceTaskHandle = nullptr;
static QueueHandle_t userInterfaceUpdateQueue = nullptr; // for receiving update from FSM task

// ============================
// Shared app state
// ============================
struct TransitionRecord {
    uint32_t timestampMs;
    RobotState from;
    RobotState to;
    //String reason;
};

static constexpr size_t LOG_CAPACITY = 200;

struct AppState {
    RobotState currentState = RobotState::Startup;
    RobotTeam team = RobotTeam::Red;
    BeaconState currentBeaconState = BeaconState::Unknown;
    RobotHeading heading = RobotHeading::Forward;
    RobotLocation location = RobotLocation::Home;

    TransitionRecord log[LOG_CAPACITY];
    size_t logHead = 0;   // index of oldest entry
    size_t logCount = 0;  // number of valid entries
};
AppState appState;

size_t getLogPhysicalIndex(size_t logicalIndex);
void pushLogRecord(const TransitionRecord& rec);
static void appendTransition(RobotState from, RobotState to, const char* reason);

void connectWiFi();
void startSoftAP();
void setupWebServer();
static size_t appendJsonEscaped(char* dst, size_t dstSize, size_t offset, const char* src);
static void formatLogEntry(const TransitionRecord& rec, char* out, size_t outSize);

// Push update to client (server side event, SSE)
void pushStateUpdateToClients(RobotState state);
void pushLogUpdateToClients(const TransitionRecord& rec);
void pushTeamUpdateToClients(RobotTeam team);
void pushBeaconUpdateToClients(BeaconState state);
void pushLocationUpdateToClients(RobotLocation location);
void pushHeadingUpdateToClients(RobotHeading head);

void initUserInterface() {
    
#if SOFT_AP_MODE
    startSoftAP();
#else
    connectWiFi();
#endif
    setupWebServer();

    userInterfaceUpdateQueue = xQueueCreate(
        10, sizeof(UserInterfaceUpdateMsg));
    if (userInterfaceUpdateQueue == nullptr) {
        DEBUG_LEVEL_1("User Interface update queue creation failed");
    }

    xTaskCreatePinnedToCore(
        userIntefaceTask,      // Task function
        "UserIntefaceTask",    // Task name
        4096,                // Stack size (bytes)
        NULL,                // Parameters
        1,                   // Priority
        &userInterfaceTaskHandle,  // Task handle
        1                  // Core 1
    );
}

void userIntefaceTask(void *parameter) {
    char rcvdChar;
    UserInterfaceUpdateMsg uiUpdateMsg;

    for (;;) {
        // Receive status update from FSM, update web server
        if (xQueueReceive(
                userInterfaceUpdateQueue, 
                &uiUpdateMsg, 
                0) == pdPASS) {

            DEBUG_LEVEL_2("Update msg rcvd by UserInterface");

            if (uiUpdateMsg.currentState != appState.currentState) {
                appendTransition(appState.currentState, uiUpdateMsg.currentState, "FSM update");
            }
            if (uiUpdateMsg.team != appState.team) {
                appState.team = uiUpdateMsg.team;
                pushTeamUpdateToClients(appState.team);
            }
            if (uiUpdateMsg.currentBeaconState != appState.currentBeaconState) {
                appState.currentBeaconState = uiUpdateMsg.currentBeaconState;
                pushBeaconUpdateToClients(appState.currentBeaconState);
            }
            if (uiUpdateMsg.location != appState.location) {
                appState.location = uiUpdateMsg.location;
                pushLocationUpdateToClients(appState.location);
            }
            if (uiUpdateMsg.heading != appState.heading) {
                appState.heading = uiUpdateMsg.heading;
                pushHeadingUpdateToClients(appState.heading);
            }
        }
        // Receive req from serial port
        if (Serial.available() > 0) {
            rcvdChar = Serial.read();
            if (rcvdChar == 'b') {
                DEBUG_LEVEL_1("Force ball launching...");
                FsmEventQueueItem ev{};
                ev.type = FsmEventType::UserStateChangeReq;
                ev.data.newState = RobotState::BallLaunching;
                BaseType_t ok = sendFsmEventItem(ev);
            } else if (rcvdChar == 'w') {
                Serial.println(WiFi.localIP());
            } else if (rcvdChar == 'l') { // manual trigger left juntion
                FsmEventQueueItem ev{};
                ev.type = FsmEventType::JunctionCrossed;
                ev.data.junctionCrossed.left = true;
                BaseType_t ok = sendFsmEventItem(ev);
            } else if (rcvdChar == 'r') { // manual trigger right juntion
                DEBUG_LEVEL_1("Fake junction on right sensor...");
                FsmEventQueueItem ev{};
                ev.type = FsmEventType::JunctionCrossed;
                ev.data.junctionCrossed.right = true;
                BaseType_t ok = sendFsmEventItem(ev);
            } else if (rcvdChar == 'h') { // manual trigger left + right juntion
                DEBUG_LEVEL_1("Fake junction on left sensor...");
                FsmEventQueueItem ev{};
                ev.type = FsmEventType::JunctionCrossed;
                ev.data.junctionCrossed.left = true;
                ev.data.junctionCrossed.right = true;
                BaseType_t ok = sendFsmEventItem(ev);
            } else if (rcvdChar != '\n') {
              DEBUG_LEVEL_1("Rcvd: %c", rcvdChar);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ============================
// Wi-Fi: either connect to router or serve as a AP itself
// ============================
void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PSWD);

    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    Serial.print("WiFi Connected");
    //Serial.print("Connected, IP address: ");
    //Serial.println(WiFi.localIP());
}

void startSoftAP() {
    WiFi.mode(WIFI_AP);

    bool ok = WiFi.softAP(WIFI_SSID, WIFI_PSWD);
    if (!ok) {
        Serial.println("SoftAP start failed");
        return;
    }

    Serial.println("SoftAP started");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
}

// ============================
// Web server
// ============================
AsyncWebServer server(80);
AsyncEventSource events("/events");

// setup APIs handler
void setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    server.on("/state", HTTP_GET, [](AsyncWebServerRequest* request) {
        static constexpr size_t JSON_BUF_SIZE = 8192;
        char json[JSON_BUF_SIZE];
        char logLine[128];

        size_t off = 0;

        off += snprintf(
            json + off,
            JSON_BUF_SIZE - off,
            "{\"currentState\":\"%s\",\"currentTeam\":\"%s\",\"currentBeaconState\":\"%s\",\"currentLocationState\":\"%s\",\"currentHeadingState\":\"%s\",\"log\":[",
            stateToString(appState.currentState),
            teamToString(appState.team),
            beaconStateToString(appState.currentBeaconState),
            locationToString(appState.location),
            headingToString(appState.heading)
        );

        for (size_t i = 0; i < appState.logCount && off < JSON_BUF_SIZE; ++i) {
            const TransitionRecord& rec = appState.log[getLogPhysicalIndex(i)];
            formatLogEntry(rec, logLine, sizeof(logLine));

            if (i > 0 && off + 1 < JSON_BUF_SIZE) {
                json[off++] = ',';
                json[off] = '\0';
            }

            if (off + 1 < JSON_BUF_SIZE) {
                json[off++] = '"';
                json[off] = '\0';
            }

            off = appendJsonEscaped(json, JSON_BUF_SIZE, off, logLine);

            if (off + 1 < JSON_BUF_SIZE) {
                json[off++] = '"';
                json[off] = '\0';
            }
        }

        if (off + 3 < JSON_BUF_SIZE) {
            json[off++] = ']';
            json[off++] = '}';
            json[off] = '\0';
        } else {
            request->send(500, "text/plain", "Response buffer too small");
            return;
        }

        request->send(200, "application/json", json);
    });

    server.on("/setState", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("state", true)) {
            request->send(400, "text/plain", "Missing state parameter");
            return;
        }

        // const char* stateStr = request->getParam("state", true)->value().c_str();
        RobotState newState;
        const AsyncWebParameter* pState = request->getParam("state", true);
        const char* stateStr = pState->value().c_str();

        DEBUG_LEVEL_1("Server rcvd new state: %s", stateStr);

        if (!stringToState(stateStr, newState)) {
            request->send(400, "text/plain", "Invalid state");
            return;
        }

        // Send state change req to FSM task
        FsmEventQueueItem ev{};
        ev.type = FsmEventType::UserStateChangeReq;
        ev.data.newState = newState;
        BaseType_t ok = sendFsmEventItem(ev);
        if (ok == pdPASS) {
            request->send(200, "text/plain", "State command accepted");
        } else {
            request->send(500, "text/plain", "Command queue full");
        }
    });

    server.on("/motionCmd", HTTP_POST, [](AsyncWebServerRequest* request) {        
        if (!request->hasParam("dir", true)) {
            request->send(400, "text/plain", "Missing dir parameter");
            return;
        }
        if (!request->hasParam("speed", true)) {
            request->send(400, "text/plain", "Missing speed parameter");
            return;
        }

        // const char* dirStr = request->getParam("dir", true)->value().c_str();
        // const char* speedStr = request->getParam("speed", true)->value().c_str();
        const AsyncWebParameter* pDir = request->getParam("dir", true);
        const AsyncWebParameter* pSpeed = request->getParam("speed", true);
        const char* dirStr = pDir->value().c_str();
        const char* speedStr = pSpeed->value().c_str();        

        DEBUG_LEVEL_1("Server rcvd motion dir: %s, speed: %s", dirStr, speedStr);

        MotionDir newDir;
        int newSpeed;
        if (!stringToDir(dirStr, newDir)) {
            request->send(400, "text/plain", "Invalid dir");
            return;
        }
        if (!parseInt(speedStr, newSpeed)) {
            request->send(400, "text/plain", "Invalid speed");
            return;
        }

        // Send ManualControlMotion item to Manual Control task
        ManualControlCmd cmd{};
        cmd.type = ManualControlCmdType::MotionCmd;
        cmd.data.motion.dir = newDir;
        cmd.data.motion.speed = newSpeed;
        if (sendManualControlCmd(cmd)) {
            request->send(200, "text/plain", "Motion command accepted");
        } else {
            request->send(500, "text/plain", "Command queue full or not initialized");
        }
    });

    events.onConnect([](AsyncEventSourceClient* client) {
        client->send("connected", "hello", millis());

        RobotState currentState = appState.currentState;
        
        client->send(stateToString(currentState), "state", millis());
    });

    server.addHandler(&events);
    server.begin();
}


// ============================
// Helpers
// ============================
static size_t appendJsonEscaped(char* dst, size_t dstSize, size_t offset, const char* src) {
    if (!dst || dstSize == 0 || !src) return offset;

    while (*src && offset + 1 < dstSize) {
        char c = *src++;
        if (c == '\\' || c == '"') {
            if (offset + 2 >= dstSize) break;
            dst[offset++] = '\\';
            dst[offset++] = c;
        } else if (c == '\n') {
            if (offset + 2 >= dstSize) break;
            dst[offset++] = '\\';
            dst[offset++] = 'n';
        } else if (c == '\r') {
            if (offset + 2 >= dstSize) break;
            dst[offset++] = '\\';
            dst[offset++] = 'r';
        } else if (c == '\t') {
            if (offset + 2 >= dstSize) break;
            dst[offset++] = '\\';
            dst[offset++] = 't';
        } else {
            dst[offset++] = c;
        }
    }

    if (offset < dstSize) {
        dst[offset] = '\0';
    } else {
        dst[dstSize - 1] = '\0';
    }

    return offset;
}

static void formatLogEntry(const TransitionRecord& rec, char* out, size_t outSize) {
    if (out == nullptr || outSize == 0) return;

    snprintf(
        out,
        outSize,
        "[%lu ms] %s -> %s",
        static_cast<unsigned long>(rec.timestampMs),
        stateToString(rec.from),
        stateToString(rec.to)
    );
}

void pushStateUpdateToClients(RobotState state) {
    char msg[32];
    snprintf(msg, sizeof(msg), "%s", stateToString(state));
    events.send(msg, "state", millis());
}

void pushLogUpdateToClients(const TransitionRecord& rec) {
    char msg[128];
    formatLogEntry(rec, msg, sizeof(msg));
    events.send(msg, "log", millis());
}

void pushBeaconUpdateToClients(BeaconState state) {
    char msg[32];
    snprintf(msg, sizeof(msg), "%s", beaconStateToString(state));
    events.send(msg, "beacon", millis());
}

void pushTeamUpdateToClients(RobotTeam team) {
    char msg[16];
    snprintf(msg, sizeof(msg), "%s", teamToString(team));
    events.send(msg, "team", millis());
}

void pushLocationUpdateToClients(RobotLocation loc) {
    char msg[32];
    snprintf(msg, sizeof(msg), "%s", locationToString(loc));
    events.send(msg, "location", millis());
}

void pushHeadingUpdateToClients(RobotHeading head) {
    char msg[16];
    snprintf(msg, sizeof(msg), "%s", headingToString(head));
    events.send(msg, "heading", millis());
}


size_t getLogPhysicalIndex(size_t logicalIndex) {
    return (appState.logHead + logicalIndex) % LOG_CAPACITY;
}

void pushLogRecord(const TransitionRecord& rec) {
    if (appState.logCount < LOG_CAPACITY) {
        size_t tail = (appState.logHead + appState.logCount) % LOG_CAPACITY;
        appState.log[tail] = rec;
        appState.logCount++;
    } else {
        // overwrite oldest
        appState.log[appState.logHead] = rec;
        appState.logHead = (appState.logHead + 1) % LOG_CAPACITY;
    }
}

static void appendTransition(RobotState from, RobotState to, const char* reason) {
    (void)reason;

    TransitionRecord rec{};
    rec.timestampMs = millis();
    rec.from = from;
    rec.to = to;

    appState.currentState = to;
    pushLogRecord(rec);

    pushStateUpdateToClients(to);
    pushLogUpdateToClients(rec);
}

bool parseInt(const char* str, int& out) {
    if (str == nullptr || *str == '\0') return false;

    char* endptr;
    long val = strtol(str, &endptr, 10);

    // 1. no digits found
    if (endptr == str) return false;

    // 2. extra characters (e.g. "123abc")
    if (*endptr != '\0') return false;

    // 3. optional: range check (important!)
    if (val < INT_MIN || val > INT_MAX) return false;

    out = (int)val;
    return true;
}

bool sendUserInterfaceUpdate(const UserInterfaceUpdateMsg& msg) {
    if (userInterfaceUpdateQueue == nullptr) {
        return false;
    }
    return xQueueSend(userInterfaceUpdateQueue, &msg, 0) == pdPASS;
}
