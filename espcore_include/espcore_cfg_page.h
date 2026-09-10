// ------------------ EVV MODE PAGE -------------------------
const char CWP_GVAR_1[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>ESP32 Core Configuration</title>
<style>
    body { background:#111; color:#eee; font-family:sans-serif; padding:20px; }
    h3 { margin-top:28px; }
    label { margin-right:18px; }
    input[type=text], input[type=password] {
        width: 220px; padding:6px; font-size:16px;
        background:#222; color:#eee; border:1px solid #444;
    }
    input[type=checkbox] {
        transform: scale(1.3);
        margin-right:6px;
    } 
  .row {
    display: flex;
    align-items: center;
    flex-wrap: nowrap;
    gap: 8px; /* consistent spacing between all items */
    margin-bottom: 8px;
  }

  .dot {
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 18px;
    line-height: 1;
    padding: 0 6px; /* horizontal space between boxes */
    color: #eee;
  }

  .srcButtons {
    margin-top: 20px;      /* space above buttons */
    display: flex;
    gap: 12px;             /* space between buttons */
  }

  .btn {
    padding:6px 20px;      /* match input vertical padding */
    font-size:16px;        /* match input font size */
    background:#333;
    color:#eee;
    border:1px solid #555;
    cursor:pointer;
    line-height:1.0;
    align-self:center;
    margin-left:20px;      /* spacing only, no vertical push */
  } 
  
  input.dateField {
    width: 140px;       /* enough for YYYY-MM-DD */
    padding: 6px;
    font-size: 16px;
    background:#222;
    color:#eee;
    border:1px solid #444;
    text-align:center;
  }

  .ipTable {
    border-collapse: collapse;
    margin-top: 10px;
  }

  .ipTable td {
    padding: 4px 6px;
    vertical-align: middle;
  }
  
  .ipTable td span {
    display: block;
    width: 100%;
    text-align: center;
  }
  
  .ipTable .label {
    text-align: right;
    padding-right: 10px;
    white-space: nowrap;
  }

  .ipTable .dot {
    text-align: center;
    font-size: 18px;
    color: #eee;
    width: 10px;
  }

  .nbrField {
    width: 50px;
    text-align: center;
    background: #222;
    color: #eee;
    border: 1px solid #444;
    font-size: 16px;
    padding: 6px;
  }
  
  input.nbrField {
    width: 60px;        /* enough for 4 chars */
    padding: 6px;
    font-size: 16px;
    background:#222;
    color:#eee;
    border:1px solid #444;
    text-align:center;
  }

</style>
</head>
<body>
)rawliteral";
const char CWP_GVAR_2[] PROGMEM = R"rawliteral(

<h2>ESP32 Core Configuration</h2>

<form id="eevForm">

    <h3>ESP32 Device</h3>

    <div class="row">
        <label for="devName">Device Name</label>
        <input type="text" id="devName" name="devName">
        <label for="author" style="margin-left:20px;">Author</label>
        <input type="text" id="author" name="author">
    </div>

    <div class="row">
        <label for="ver">Version</label>
    <input type="text" id="ver" name="ver"
         class="nbrField"
         maxlength="4">
        <label for="verDate" style="margin-left:20px;">Version Date</label>
    <input type="text" id="verDate" name="verDate"
           class="dateField"
               pattern="\d{4}-\d{2}-\d{2}"
               placeholder="YYYY-MM-DD"
               maxlength="10">
    <button type="button" class="btn" onclick="setToday()" style="margin-left:20px;">Today</button>
     
    </div>

    <div class="row">
      <label for="espAppTitle">App Title</label>
      <input type="text" id="espAppTitle" name="espAppTitle">
      <label for "espAppBtn">App Button</label>
      <input type="text" id="espAppBtn" name="epsAppBtn">
    </div>  

    <div class="row">
      <label for="espAppHtml">App Html</label>
      <input type="text" id="espAppHtml" name="espAppHtml">
      <label><input type="checkbox" id="espAppEnable" name="espAppEnable">App Enable</label>
    </div>  

    <h3>Boot Button</h3>

    <div class="row">
        <label for="bootBtnPin">Pin</label>
        <input class="nbrField" type="text" id="bootBtnPin" name="bootBtnPin" maxlength="3" required>
        <label for="bootBtnDbTime" style="margin-left:20px;">Debounce Time(mS)</label>
        <input class="nbrField" type="text" id="bootBtnDbT" name="bootBtnDbT" maxlength="3" required>
        <label for="bootBtnCfgTime" style="margin-left:20px;">Boot BTN CFG Time(mS)</label>
        <input class="nbrField" type="text" id="bootBtnCfgT" name="bootBtnCfgT" maxlength="4" required>
    </div>
    <h3>ESP32 Led (blue)</h3>
  <div class="row">
        <label for="ledPin">Pin</label>
        <input class="nbrField" type="text" id="ledPin" name="ledPin" maxlength="3" required>
    </div>

    <h3>Local WiFi</h3>

    <div class="row">
        <label for="wSSID">SSID</label>
        <input type="text" id="wSSID" name="wSSID">
        <label for="wABC"></label>
        <input type="password" id="wABC" name="wABC">
        <button type="button" class="btn" onclick="togglePW()" style="margin-left:20px;">View</button>
    </div>

    <h3>Log To</h3>

    <div class="row">
        <label><input type="checkbox" id="logSer" name="logSer">Serial</label>
        <label><input type="checkbox" id="logMqtt" name="logMqtt">MQTT</label>
    </div>

    <h3>Log Msgs</h3>

    <div class="row">
        <label><input type="checkbox" id="logInfo" name="logInfo">Info</label>
        <label><input type="checkbox" id="logErr" name="logErr">Error</label>
        <label><input type="checkbox" id="logWrn" name="logWrn">Warning</label>
        <label><input type="checkbox" id="logMisc" name="logMisc">Misc</label>
        <label><input type="checkbox" id="logDbg1" name="logDbg1">Dbg1</label>
        <label><input type="checkbox" id="logDbg2" name="logDbg2">Dbg2</label>
        <label><input type="checkbox" id="logDbg3" name="logDbg3">Dbg3</label>
        <label><input type="checkbox" id="logDbg4" name="logDbg4">Dbg4</label>
        <label for="logMaskHex">Mask</label>
               <input class="nbrField"type="text"id="logMaskHex" maxlength="4" size="4" readonly>
    </div>

    <h3>Network Static IP</h3>

    )rawliteral";
const char CWP_GVAR_3[] PROGMEM = R"rawliteral(

<table class="ipTable">
  <tr>
    <td class="label">Subnet Mask :</td>
    <td><input class="nbrField" id="MK1" maxlength="3" name="MK1" required></td>
    <td class="dot">.</td>
    <td><input class="nbrField" id="MK2" maxlength="3" name="MK2" required></td>
    <td class="dot">.</td>
    <td><input class="nbrField" id="MK3" maxlength="3" name="MK3" required></td>
    <td class="dot">.</td>
    <td><input class="nbrField" id="MK4" maxlength="3" name="MK4" required></td>
  </tr>

  <tr>
    <td class="label">Device IP :</td>
    <td><input class="nbrField" id="LN1" maxlength="3" name="LN1" required></td>
    <td class="dot">.</td>
    <td><input class="nbrField" id="LN2" maxlength="3" name="LN2" required></td>
    <td class="dot">.</td>
    <td><input class="nbrField" id="LN3" maxlength="3" name="LN3" required></td>
    <td class="dot">.</td>
    <td><input class="nbrField" id="LND" maxlength="3" name="LND" required></td>
  </tr>

  <tr>
    <td class="label">OTA IP :</td>
    <td>
     <span id="LN1_view"></span>
    </td>
    <td class="dot">.</td>
  <td>
     <span id="LN2_view"></span>
    </td>
    <td class="dot">.</td>
  <td>
     <span id="LN3_view"></span>
    </td>
    <td class="dot">.</td>
    <td><input class="nbrField" id="LNO" name="LNO" maxlength="3" required></td>
  </tr>

  <tr>
    <td class="label">Router IP :</td>
  <td>
     <span id="LN1_view2" ></span>
    </td>
    <td class="dot">.</td>
  <td>
     <span id="LN2_view2"></span>
    </td>
    <td class="dot">.</td>
  <td>
     <span id="LN3_view2"></span>
    </td>
    <td class="dot">.</td>
    <td><input class="nbrField" id="LNR" name="LNR" maxlength="3" required></td>
  </tr>

  <tr>
    <td class="label">MQTT Broker IP :</td>
  <td>
     <span id="LN1_view3"></span>
    </td>
    <td class="dot">.</td>
  <td>
     <span id="LN2_view3"></span>
    </td>
    <td class="dot">.</td>
  <td>
     <span id="LN3_view3"></span>
    </td>
    <td class="dot">.</td>
    <td><input class="nbrField" id="LNM" name="LNM" maxlength="3" required></td>
  </tr>
</table>

    <h3>TimeOut Values</h3>

    <div class="row">
        <label for="wdt_to">Watchdog(Sec)</label>
        <input class="nbrField" type="text" id="wdt_to" name="wdt_to"
         maxlength="4"
         required>    

        <label for="ota_to" style="margin-left:20px;">OTA(Min)</label>
        <input class="nbrField" type="text" id="ota_to" name="ota_to"
         maxlength="4"
         required>    

        <label for="test_to" style="margin-left:20px;">Test(Min)</label>
        <input class="nbrField" type="text" id="test_to" name="test_to"
         maxlength="4"
         required>
    </div>

    <div class="srcButtons">
    <button class="btn" type="button" onclick="saveGVar()">Save</button>
    <button class="btn" type="button" onclick="refreshGVar()">Refresh</button>
    <button class="btn" type="button" onclick="cancelCFG()">Cancel</button>
    </div>
</form>
)rawliteral";
const char CWP_GVAR_4[] PROGMEM = R"rawliteral(
<script>
/*
 * --------------------------------------------------------------------------
 * CFG WebSocket Interface
 *
 * ESP32 WebSocket commands:
 *
 *   10 = Refresh CFG
 *   11 = Save CFG
 *   12 = Cancel CFG
 *
 * Refresh response:
 *
 *   {"msg":"key:value\nkey:value\n..."}
 *
 * Save request:
 *
 *   {"cmd":11,"key":"value","key":"value",...}
 *
 * Special case:
 *
 *   logLev is one byte represented by the eight log checkboxes.
 * --------------------------------------------------------------------------
 */

/* ==========================================================================
 * LOG MESSAGE MASK
 * ========================================================================== */
let logMsgMask = 0;

const logBits = {
    logInfo: 128,
    logErr:   64,
    logWrn:   32,
    logMisc:  16,
    logDbg1:   8,
    logDbg2:   4,
    logDbg3:   2,
    logDbg4:   1
};


/*
 * Load logLev received from the ESP32.
 *
 * ESP32 sends:
 *
 *     "fe"
 *
 * JS uses it as:
 *
 *     0x + FE  
 */
function setLogMsgMask(value) {

    logMsgMask = parseInt(value, 16) & 0xFF;

    for (const [id, bit] of Object.entries(logBits)) {

        document.getElementById(id).checked =
            (logMsgMask & bit) !== 0;
    }

    document.getElementById("logMaskHex").value =
        "0x" + 
        logMsgMask.toString(16).toUpperCase().padStart(2, "0");
}

/*
 * Recalculate the mask when the user changes a checkbox.
 */
for (const [id, bit] of Object.entries(logBits)) {

    document.getElementById(id).addEventListener("change", function() {

        if (this.checked) {
            logMsgMask |= bit;
        } else {
            logMsgMask &= ~bit;
        }

        logMsgMask &= 0xFF;

        document.getElementById("logMaskHex").value =
            "0x" +
            logMsgMask.toString(16).toUpperCase().padStart(2, "0");
    });
}    
/* ==========================================================================
 * PASSWORD
 * ========================================================================== */

function togglePW() {

    const p = document.getElementById("wABC");

    if (p) {
        p.type = (p.type === "password") ? "text" : "password";
    }
}

)rawliteral";
const char CWP_GVAR_5[] PROGMEM = R"rawliteral(

/* ==========================================================================
 * VERSION
 * ========================================================================== */

const ver = document.getElementById("ver");

if (ver) {

    ver.addEventListener("input", function(e) {

        let raw = e.target.value.toLowerCase();

        raw = raw.replace(/[^0-9v.]/g, "");

        let sep = raw.search(/[v.]/);

        if (sep >= 0) {

            let major =
                raw.substring(0, sep)
                   .replace(/\D/g, "")
                   .substring(0, 2);

            let minor =
                raw.substring(sep + 1)
                   .replace(/\D/g, "")
                   .substring(0, 1);

            e.target.value = major + "v" + minor;

        } else {

            e.target.value =
                raw.replace(/\D/g, "").substring(0, 2);
        }
    });

    ver.addEventListener("blur", function(e) {

        let raw = e.target.value.toLowerCase();

        let parts = raw.split(/[v.]/);

        let major = parts[0] || "0";
        let minor = parts[1] || "0";

        major = major.padStart(2, "0").substring(0, 2);
        minor = minor.substring(0, 1);

        e.target.value = `${major}v${minor}`;
    });
}

/* ==========================================================================
 * TODAY BUTTON
 * ========================================================================== */

function setToday() {

    const d = new Date();

    const yyyy = d.getFullYear();
    const mm   = String(d.getMonth() + 1).padStart(2, "0");
    const dd   = String(d.getDate()).padStart(2, "0");

    const el = document.getElementById("verDate");

    if (el) {
        el.value = `${yyyy}-${mm}-${dd}`;
    }
}

/* ==========================================================================
 * NUMERIC VALIDATION
 * ========================================================================== */

function enforceNBR(id, min, max) {

    const el = document.getElementById(id);

    if (!el) return;

    let n = parseInt(el.value, 10);

    if (isNaN(n)) n = min;
    if (n < min) n = min;
    if (n > max) n = max;

    el.value = n;
}


function enforceOctet(id) {

    enforceNBR(id, 0, 255);
    syncIP();
}

function enforceTO(id) {

    enforceNBR(id, 1, 9999);
}

)rawliteral";
const char CWP_GVAR_6[] PROGMEM = R"rawliteral(
/* ==========================================================================
 * IP DISPLAY
 *
 * The *_view fields are display-only.
 * They are NOT part of the ESP32 JSON package.
 * ========================================================================== */

function syncIP() {

    const ln1 = document.getElementById("LN1")?.value || "0";
    const ln2 = document.getElementById("LN2")?.value || "0";
    const ln3 = document.getElementById("LN3")?.value || "0";

    const views = [
        "LN1_view",  "LN2_view",  "LN3_view",
        "LN1_view2", "LN2_view2", "LN3_view2",
        "LN1_view3", "LN2_view3", "LN3_view3"
    ];

    const values = [
        ln1, ln2, ln3,
        ln1, ln2, ln3,
        ln1, ln2, ln3
    ];

    for (let i = 0; i < views.length; i++) {

        const el = document.getElementById(views[i]);

        if (el) {
            el.textContent = values[i];
        }
    }
}

/* ==========================================================================
 * WEBSOCKET RECEIVE
 * ========================================================================== */
function handleWSMessage(raw) {

    console.log("CFG WebSocket RX:", raw);

    // ESP32 connection acknowledgement
    if (raw === "Connected") {
        console.log("CFG WebSocket acknowledged");
        return;
    }

    let packet;

    try {
        packet = JSON.parse(raw);
    } catch (err) {
        console.error("Invalid WebSocket JSON:", raw);
        return;
    }

    /*
     * CFG refresh response:
     *
     * {"msg":"devName:...\nauthor:...\n..."}
     */
    if (packet.msg !== undefined) {
        loadGVar(packet.msg);
        return;
    }

    /*
     * Save response:
     *
     * {"cmd":11,"ok":true}
     */
    if (packet.cmd === 11) {

         if (packet.ok) {
            console.log("CFG save successful");
            fnExit();
         } else {
            console.error("CFG save failed");
         }
        
         return;
      }

    /*
     * Cancel response:
     *
     * {"cmd":12,"ok":true}
     */
    if (packet.cmd === 12) {

         if (packet.ok) {
            console.log("CFG cancel successful");
            fnExit();
         } else {
            console.error("CFG cancel failed");
         }

         return;
        
    }
}
/* ==========================================================================
 * WEBSOCKET
 * ========================================================================== */

let ws = null;

/*
 * Connect to the ESP32 WebSocket.
 *
 * This assumes the ESP32 WebSocket endpoint is /ws.
 */
function connectWS() {

    const protocol =
        (window.location.protocol === "https:") ? "wss:" : "ws:";

    const url = protocol + "//" + window.location.host + "/ws";

    console.log("Opening WebSocket:", url);

    ws = new WebSocket(url);

    ws.onopen = function() {
        console.log("CFG WebSocket OPEN");

        refreshGVar();
    };

    ws.onclose = function(event) {
        console.log("CFG WebSocket CLOSED:", event);
    };

    ws.onerror = function(error) {
        console.error("CFG WebSocket ERROR:", error);
    };

    ws.onmessage = function(event) {
        console.log("CFG WebSocket RX:", event.data);

        handleWSMessage(event.data);
    };
}


)rawliteral";
const char CWP_GVAR_7[] PROGMEM = R"rawliteral(
let gvKeys = new Set();

/* ==========================================================================
 * LOAD CFG DATA
 *
 * Input is the EXACT string returned by getCFGars():
 *
 *     devName:ESP32_CORE_OTA_ABC
 *     author:VE7ABC
 *     ver:01v3
 *     ...
 *
 * Only the first ':' is significant.
 * ========================================================================== */
function loadGVar(msg) {

    gvKeys.clear();

    const lines = msg.split(/\r?\n/);

    for (const line of lines) {

        if (!line) continue;

        const sep = line.indexOf(":");

        if (sep < 0) continue;

        const key = line.substring(0, sep);
        const value = line.substring(sep + 1);

        gvKeys.add(key);

        if (key === "logLev") {
            setLogMsgMask("0x" + value);
            continue;
        }

        const el = document.getElementById(key);

        if (el) {

            if (el.type === "checkbox") {

                el.checked =
                    value.toLowerCase() === "yes" ||
                    value.toLowerCase() === "true" ||
                    value === "1";

            } else {

                el.value = value;
            }
        }
    }

    syncIP();

    console.log("CFG gvKeys after load:", [...gvKeys]);
}

)rawliteral";
const char CWP_GVAR_8[] PROGMEM = R"rawliteral(

/* ==========================================================================
 * REFRESH
 * ========================================================================== */

function refreshGVar() {

    if (!ws || ws.readyState !== WebSocket.OPEN) {

        console.error("CFG WebSocket is not connected");
        return;
    }

    ws.send(JSON.stringify({
        cmd: 10
    }));
}

/* ==========================================================================
 * SAVE
 * ========================================================================== */
function saveGVar() {
    console.log("CFG saveGVar s1:");

    if (!ws || ws.readyState !== WebSocket.OPEN) {

        console.error("CFG WebSocket is not connected");
        return;
    }

    console.log("CFG saveGVar s2:");
    const packet = {
        cmd: 11
    };

    console.log("CFG saveGVar s3:");
    for (const key of gvKeys) {

        console.log("CFG saveGVar s4:" + key);
        let value;

        if (key === "logLev") {

            const el = document.getElementById("logMaskHex");

            if (!el) {
                console.error("CFG SAVE: logMaskHex field not found");
                continue;
            }

            value = el.value
                .replace(/^0x/i, "")
                .toLowerCase();

        } else {

            const el = document.getElementById(key);

            if (!el) {
                console.error("CFG SAVE: HTML element missing:", key);
                continue;
            }

            if (el.type === "checkbox") {

                value = el.checked ? "yes" : "no";

            } else {

                value = el.value;
            }
        }

        packet[key] = value;
    }

    console.log("CFG SAVE:", packet);

    ws.send(JSON.stringify(packet));
}

)rawliteral";
const char CWP_GVAR_9[] PROGMEM = R"rawliteral(

/* ==========================================================================
 * CANCEL
 * ========================================================================== */

function cancelCFG() {

    if (!ws || ws.readyState !== WebSocket.OPEN) {

        console.error("CFG WebSocket is not connected");
        return;
    }

    ws.send(JSON.stringify({
        cmd: 12
    }));
}

/* ==========================================================================
 * DOM SETUP
 * ========================================================================== */
window.addEventListener("DOMContentLoaded", function() {
   function fnExit() {
      if (window.history.length > 1) {
         window.history.back();
      } else {
         window.location.href = "/";
      }
   }

   // expose it so onclick="fnExit()" works
   window.fnExit = fnExit;

    /*
     * IP octets.
     */
    [
        "LN1", "LN2", "LN3",
        "LND", "LNO", "LNR", "LNM",
        "MK1", "MK2", "MK3", "MK4"
    ].forEach(id => {

        const el = document.getElementById(id);

        if (el) {

            el.addEventListener("input", function() {
                enforceOctet(id);
            });
        }
    });

    /*
     * Boot / LED values.
     */
    [
        "bootBtnPin",
        "bootBtnDbTime",
        "ledPin"
    ].forEach(id => {

        const el = document.getElementById(id);

        if (el) {

            el.addEventListener("input", function() {
                enforceNBR(id, 0, 255);
            });
        }
    });

    /*
     * Timeout values.
     */
    [
        "wdt_to",
        "ota_to",
        "test_to"
    ].forEach(id => {

        const el = document.getElementById(id);

        if (el) {

            el.addEventListener("input", function() {
                enforceTO(id);
            });
        }
    });

    /*
     * Initial IP display.
     */
    syncIP();

    /*
     * Start WebSocket.
     */
    connectWS();
});

</script>
)rawliteral";
