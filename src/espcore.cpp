#include "espcore.h"

DRAM_ATTR volatile int TockCount = 0;
void IRAM_ATTR irq() { 
   TockCount++;
}

EspCoreClass ecc;

// ---- STATIC TABLE DEFINITIONS ----
EspCoreClass::GVarMap EspCoreClass::gVarTable[] = {
#define X(name, key, var, type, pub) \
   { EspCoreClass::I_##name, key, &ecc.vars.var, type, pub },
   GVAR_LIST(X)
#undef X
};

const int EspCoreClass::gVarCount =
   sizeof(EspCoreClass::gVarTable) / sizeof(EspCoreClass::gVarTable[0]);

// ---- CONSTRUCTOR ----
EspCoreClass::EspCoreClass() {
   serLogQ   = xQueueCreate(_LOG_QSIZE,     sizeof(LogMsg));
   mqttLogQ  = xQueueCreate(_MQTT_LOG_QSIZE,sizeof(MqttLogMsg));
   mqttDataQ = xQueueCreate(_MQTT_QSIZE,    sizeof(MqttMsg));
   sysCmdQ   = xQueueCreate(_SYS_CMD_QSIZE, sizeof(SysCmd));
   app2eccQ  = xQueueCreate(_APPQ_QSIZE,    _APPQ_BUF_MAXLEN);   // e.g. 64
   ecc2appQ  = xQueueCreate(_ECCQ_QSIZE,    _ECCQ_BUF_MAXLEN);
   //app2eccQ = xQueueCreate(_APPQ_QSIZE,    sizeof(String));
   //ecc2appQ = xQueueCreate(_ECCQ_QSIZE,    sizeof(String));
}

// ---- STATIC TASK THUNKS ----
void EspCoreClass::dqSerLogTaskThunk(void* param) {
   static_cast<EspCoreClass*>(param)->dqSerLogTask(param);
}

void EspCoreClass::dqMqttTaskThunk(void* param) {
   static_cast<EspCoreClass*>(param)->dqMqttTask(param);
}

// ---- TASK BODIES ----

// These 2 functions are FreeRTOS tasks that continuously processes messages 
// from their queues to either the serial monitor output and or mqtt broker.
// they run indefinitely, waiting for messages to arrive in their queue.

void EspCoreClass::dqSerLogTask(void* param) {
   (void)param;
   _fdpl("dqSerLogTask ...");
   LogMsg logMsg;

   for (;;) {
      if (xQueueReceive(serLogQ, &logMsg, portMAX_DELAY) == pdPASS) {
         while (vars.bl.appBusy) { delay(10); }

         String x = qDepth(_SER_LOG_Q, "dec");
         if (logMsg.addNewline) {
            Serial.println(logMsg.msg);
         } else {
            Serial.print(logMsg.msg);
         }
      }
   } 
}

void EspCoreClass::dqMqttTask(void* param) {
   (void)param;
   _fdpl("dqMqttTask ...");
   MqttMsg mqttMsg;
   MqttLogMsg mqttLogMsg;
   String  logMsg    = "";
   int pendingMqttLog = 0;
   int waitingMqttLogEOL = 0;
   for (;;) {
      // NB!!!! this IF by itself KILLS the TASK processing without adding a delay
      if (vars.enm.mqttState == _MQTT_CON) {
        // 2026-Sep-09 split MQTT in to 2 groups LOG and APP MQTT 
        // LOG MQTT msgs must wait if appBusy is flagged
        // APP MQTT msgs are always sent 
        //
        // to simulate print vs println eol flag is used to concat all print requests
        // 
        //Serial.printf("dqMqtt mqttDataQ=%p mqttLogQ=%p\n", mqttDataQ, mqttLogQ);

        if (xQueueReceive(mqttDataQ, &mqttMsg, 0) == pdPASS) {
           String x = qDepth(_MQTT_Q, "dec");
           mqtt.publish(mqttMsg.topic, mqttMsg.payload, false);
        }   

        if (pendingMqttLog > 0 && !vars.bl.appBusy) {
           mqtt.publish(mqttLogMsg.topic, logMsg.c_str(), false);
           logMsg = "";
           pendingMqttLog = 0;
        }

        //Serial.printf("dqMqtt BEFORE LOG mqttLogQ=%p\n", mqttLogQ);
           
        if (pendingMqttLog == 0 && xQueueReceive(mqttLogQ, &mqttLogMsg, 0) == pdPASS) {
           String x = qDepth(_MQTT_LOG_Q, "dec");
           logMsg = logMsg + mqttLogMsg.payload;
           if (mqttLogMsg.addNewline) { // println
             pendingMqttLog = 5000;
             waitingMqttLogEOL = 0;
           } else {                     // print
             waitingMqttLogEOL = 5000;
           }
        }
        delay(1);
        if (pendingMqttLog > 0) {
          pendingMqttLog--;
          if (pendingMqttLog == 0) logMsg = "";
        }
        
        if (waitingMqttLogEOL > 0) {
          waitingMqttLogEOL--;
          if (waitingMqttLogEOL == 0) logMsg = "";
        }
      } else { // REQUIRES A DELAY IF FALSE
        // NB!!!! WITHOUT A DELAY this KILLS the all TASK processing
        delay(100);
      }  
   }
}

void EspCoreClass::setAppHtml(const String &html) {
  vars.str.appHtml = html;
}


void EspCoreClass::setAppMqttTopic(const String &topic) {
  vars.str.appMqttTopic = topic;
}

// ---- QUEUE HELPERS ----
String EspCoreClass::qDepth(qDpth q, String req)
{
   int idx = (int)q;

   if (req == "inc") {
      vars.q.in.qdp[idx]++;

      if (vars.q.in.qdp[idx] > vars.q.in.qmx[idx])
         vars.q.in.qmx[idx] = vars.q.in.qdp[idx];
   }
   else if (req == "dec") {
      if (vars.q.in.qdp[idx] > 0)
         vars.q.in.qdp[idx]--;
   } 
   else if (req == "clr") {
      vars.q.in.qdp[idx] = 0;
      vars.q.in.qmx[idx] = 0;
   }
   return String(vars.q.in.qdp[idx]) + ":" +
          String(vars.q.in.qmx[idx]);
}

// qLogMsg() Queues log messages to: Serial and or Mqtt or neither 
void EspCoreClass::qLogMsg(const String& msg, bool addNewline, uint8_t msgLev) {

   // for debug purposes log messages are categorized using a bit flag which can 
   // be masked with the logLev Mask ... System Level MSGs are always logged
   // msgLev must be in logLev for the msg to be logged  
   // ignore msgs with msgLev != _SYS_MSG and msgLev >= logLev 
   if (msgLev == _SYS_MSG || (msgLev & vars.ui8.log.lev) != 0) {

      // SERIAL LOGGING
      if (vars.bl.logSer) {
         LogMsg logMsg;
         msg.toCharArray(logMsg.msg, _LOG_MSG_MAXLEN);
         logMsg.addNewline = addNewline;
         String x = qDepth(_SER_LOG_Q,"inc");
         xQueueSend(serLogQ, &logMsg, 0); // 0 = don't wait if q full (logMsg will be lost)
      }

      // MQTT LOGGING
      if (vars.bl.logMqtt) {
         MqttMsg mqttMsg;
         mqttMsg.isLog = true;
         String tpc = vars.str.devName + String("/log");
         tpc.toCharArray(mqttMsg.topic, _MQTT_TOPIC_MAXLEN);
         msg.toCharArray(mqttMsg.payload, _MQTT_PAYLOAD_MAXLEN);
         mqttMsg.addNewline = addNewline;
         String x = qDepth(_MQTT_LOG_Q,"inc");
         xQueueSend(mqttLogQ, &mqttMsg, 0); // 0 = don't wait if q full (mqttMsg will be lost) 
      }
   }
}

void EspCoreClass::qMqttMsg(const String& topic, const String& payload) {
  
   MqttMsg mqttMsg;

   mqttMsg.isLog = false;
   topic.toCharArray(mqttMsg.topic,_MQTT_TOPIC_MAXLEN);
   payload.toCharArray(mqttMsg.payload, _MQTT_PAYLOAD_MAXLEN);
   mqttMsg.addNewline = false;   // non‑log messages never add newline

   String x = qDepth(_MQTT_Q,"inc");
   xQueueSend(mqttDataQ, &mqttMsg, 0);
}

#if _HAS_FS == 1

// curl "http://<ip>/appfsget/blah" > C:\temp\blah
void EspCoreClass::appGetFSfile(const String &f, String &msg) {
   String pth = "/app/" + f;
   msg = "File " + pth + " not found";
   File gf = LittleFS.open(pth, "r"); // read only
   _fdpl("appGetFSfile " + f,_INFO_MSG);

   if (!gf) return; // file doesn't exist`

   //msg = ... some code to get the contents of the gf 
   msg = gf.readString();
   _fdpl("appGetFSfile-> " + msg,_INFO_MSG);

   gf.close(); // file can only be deleted if its not open
   return;           
}

/* 
 * url=http://10.213.69.13/appfsput/zns.json
 * curl -v -X PUT -H "Content-Type: application/json" --data-binary "@<pth> "<url>" 
 * type <pth> | curl -X PUT <url> --data-binary @-
*/
void EspCoreClass::appPutFSfile(const String &f, String &data, String &msg) {
   String pth = "/app/" + f;

   _fdpl("appPutFSfile " + f,_INFO_MSG);

   if (!LittleFS.exists("/app")) {
      LittleFS.mkdir("/app");
   }

   File pf = LittleFS.open(pth, "w"); // create/truncate for writing

   if (!pf) {
      msg = "File " + pth + " unable to open for writing";
      return;
   }

   size_t n = pf.print(data);

   pf.close();

   if (n == data.length()) {
      msg = "File " + pth + " written";
   } else {
      msg = "File " + pth + " write error";
   }

   return;
}

// curl "http://<ip>/appfsdel/blah"
void EspCoreClass::appDelFSfile(const String &f, String &msg)
{
   String pth = "/app/" + f;
   _fdpl("appDelFSfile " + f,_INFO_MSG);
   msg = "File " + pth + " not found";

   File df = LittleFS.open(pth, "r"); // read only

   if (!df) return; // file doesn't exist`

   df.close(); // file can only be deleted if its not open

   if (LittleFS.remove(pth)) {
     msg = "File " + pth + " deleted";
   }
   return;
}

void EspCoreClass::lsFSfldr(File &d, const String &path,
   const String &searchString, String &msg) {
   
   File f = d.openNextFile();

   while (f) {

      String name = String(f.name());
      String matchName = name;
      matchName.toLowerCase();

      String fsPath = (path == "/")
                    ? "/" + name
                    : path + "/" + name;

      if (fnmatch(searchString.c_str(), matchName.c_str(), 0) == 0) {

         if (f.isDirectory()) {
            msg += " dir: " + fsPath + "\n";
         } else {
            msg += "file: " + fsPath + " " +
                   String(f.size()) + "\n";
         } 
      }

      // ALWAYS descend into directories.
      if (f.isDirectory()) {
         lsFSfldr(f, fsPath, searchString, msg);
      }

      f = d.openNextFile();
   }
}

void EspCoreClass::lsFS(String searchString, String &msg)
{
   _fdpl("lsFS ... ", _DBG3_MSG);
   msg = "";

   searchString.toLowerCase();
   if (searchString == "") searchString = "*";

   File root = LittleFS.open("/");

   if (!root) {
      msg = "ERROR: unable to open LittleFS\n";
      return;
   }

   lsFSfldr(root, "/", searchString, msg);
}
#endif // _HAS_FS

void EspCoreClass::getBootCode() {
   switch (esp_reset_reason()) {
      case ESP_RST_POWERON:     vars.str.bootCode = "PwrOnRst"; _fdpl("Boot: POWERON_RESET"); break;
      case ESP_RST_SW:          vars.str.bootCode = "SwCpuRst"; _fdpl("Boot: SW_CPU_RESET");  break;
      case ESP_RST_PANIC:       vars.str.bootCode = "Panic";    _fdpl("Boot: PANIC");         break;
      case ESP_RST_BROWNOUT:    vars.str.bootCode = "BrnOut";   _fdpl("Boot: BROWNOUT");      break;
      case ESP_RST_WDT:         vars.str.bootCode = "Wdt";      _fdpl("Boot: WDT");           break;
      case ESP_RST_DEEPSLEEP:   vars.str.bootCode = "DpSlp";    _fdpl("Boot: DEEPSLEEP");     break;
      default:                  vars.str.bootCode = "Unknwn";   _fdpl("Boot: Unknown");       break;
   }
}

const char* EspCoreClass::ip2str(uint8_t l1, uint8_t l2, uint8_t l3, uint8_t l4) {
   _fdpl("ip2str ...", _DBG4_MSG);

   static char ipbuf[20];
   snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d", l1, l2, l3, l4);
   return ipbuf;
}

// create a SHA-256 hash (32 bytes) of the input string (used for passwords)
void EspCoreClass::initABC(const char* iStr, uint8_t oByte[32]) {
   _fdpl("initABC ...");
   mbedtls_sha256_context ctx;
   mbedtls_sha256_init(&ctx);
   mbedtls_sha256_starts_ret(&ctx, 0); // 0 = SHA-256 (not SHA-224)
   mbedtls_sha256_update_ret(&ctx, (const unsigned char*)iStr, strlen(iStr));
   mbedtls_sha256_finish_ret(&ctx, oByte);
   mbedtls_sha256_free(&ctx);
}

// in c++ functions can be defined with the same name to
// which allows for more than one type of a param like val
// this is a variation of pad() with val as a String
String EspCoreClass::pad(const String &val, int size, char padChar, char type) {
   _fdpl("pad string ...", _DBG4_MSG);
   int n = val.length();
   if (n >= size) return val;

   //String pads = String(padChar).repeat(size - n);
   String pads = "";
   pads.reserve(size - n);
   for (int i = 0; i < size - n; i++) pads += padChar;

   return (type == 'L') ? pads + val : val + pads;
}

// so this is a variation of pad() with val as int
String EspCoreClass::pad(int val, int size, char padChar, char type) {
   _fdpl("pad int ...", _DBG4_MSG);
   char buf[32];
   sprintf(buf, "%d", val);
   return pad(String(buf), size, padChar, type);
}

#if _HAS_RGB == 1   
String EspCoreClass::getRGBname(uint8_t cl) {
   _fdpl("getRGBname ... rtn colour RGB value", _DBG3_MSG); 
   switch (cl) {
      case _RED:     return "red";
      case _YELLOW:  return "yellow";                
      case _GREEN:   return "green";
      case _BLUE:    return "blue";
      case _WHITE:   return "white";
      case _MAGENTA: return "magenta";
      case _BLACK:   return "black";
      default:       return "unknown";
   }
}

uint32_t EspCoreClass::getRGBval(uint8_t cl) {
   _fdpl("getRGBval ... rtn colour RGB value", _DBG3_MSG); 
   switch (cl) {
      case _RED:     return (  0 << 16) | (255 << 8) |   0;
      case _YELLOW:  return (255 << 16) | (255 << 8) |   0;
      case _GREEN:   return (255 << 16) | (  0 << 8) |   0;
      case _BLUE:    return (  0 << 16) | (  0 << 8) | 255;
      case _WHITE:   return (255 << 16) | (255 << 8) | 255;
      case _MAGENTA: return (255 << 16) | (0   << 8) | 255;
      case _BLACK:
      default:       return 0;
   }
}

void EspCoreClass::setRGBLed(uint8_t CL){
   _fdpl("setRGBLed ..." + String(getRGBname(CL)), _DBG3_MSG); 
   uint32_t ledColour = getRGBval(CL);

   if (vars.bl.rgbEnabled) {
      _fdpl("setRGBLed --- " + String(ledColour),_DBG3_MSG);
      pixels.setPixelColor(0, ledColour);
      pixels.show();
   }   
}
#endif

void EspCoreClass::setLed(int State) {
   _fdpl("setLed ...", _DBG2_MSG); 
   if (State == _TOGGLE) {
      State = !digitalRead(vars.in.ledPin);
   }
   digitalWrite(vars.in.ledPin,State);
}

// set the currently defined led pin based on the following params
void EspCoreClass::ledAction(LED_ACTIONS action) {
   _fdpl(String("setLed ... ") + String(action),_DBG2_MSG);

   switch (action) {
      case _TEST_LED_ON:
         vars.bl.currLed = _ON;
#if _HAS_RGB == 1            
         vars.ui8.currRGB++;
         vars.ui8.currRGB = vars.ui8.currRGB > _BLUE ? _RED: vars.ui8.currRGB;
#endif            
         break;
      case _TEST_LED_OFF:
         vars.bl.currLed = _OFF;
         break;
      case _TEST_LED_TOGGLE:
         vars.bl.currLed = !vars.bl.currLed;
         if (vars.bl.currLed) {
#if _HAS_RGB == 1
            vars.ui8.currRGB++;
            vars.ui8.currRGB = vars.ui8.currRGB > _BLUE ? _RED: vars.ui8.currRGB;
#endif
         }
         break;
      case _LED_ON:
         vars.bl.sysLed = _ON;
         break;
      case _LED_OFF:
      default:
         vars.bl.sysLed = _OFF;
      break;
   }
   if (vars.bl.sysLed) {
      setLed(_ON);
#if _HAS_RGB == 1
      if (vars.bl.rgbEnabled) setRGBLed(_BLACK);
#endif         
   } else {
      setLed(vars.bl.currLed);

#if _HAS_RGB == 1        
      if (vars.bl.rgbEnabled) {
        
         if (vars.bl.currLed) {
            setRGBLed(vars.ui8.currRGB);
         } else {
            setRGBLed(_BLACK);
         }   
      }
#endif         
   }
}

// This function converts a time in seconds to a string in the format "HH:MM:SS".
String EspCoreClass::getHMS(long tm) {
   _fdpl("getHNS ...", _DBG2_MSG);
   int h = tm / 3600;
   int m = (tm % 3600) / 60;
   int s = tm % 60;
   char buf[9];  // "HH:MM:SS" + null terminator
   snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
   return String(buf);
}

String EspCoreClass::byte2Hex(uint8_t val) {
   _fdpl("byte2Hex ...", _DBG4_MSG);
   const char hex[] = "0123456789abcdef";
   String rtnVal;
   rtnVal += hex[(val >> 4) & 0x0F];
   rtnVal += hex[val & 0x0F];
   return rtnVal;
}

uint8_t EspCoreClass::hex2Byte(const String &val) {
   _fdpl("hex2Byte ...", _DBG4_MSG);
   if (val.length() < 2)
      return 0;

   char c1 = val[0];
   char c2 = val[1];

   uint8_t hi = (c1 >= '0' && c1 <= '9') ? c1 - '0' :
                (c1 >= 'a' && c1 <= 'f') ? c1 - 'a' + 10 :
                (c1 >= 'A' && c1 <= 'F') ? c1 - 'A' + 10 : 0;

   uint8_t lo = (c2 >= '0' && c2 <= '9') ? c2 - '0' :
                (c2 >= 'a' && c2 <= 'f') ? c2 - 'a' + 10 :
                (c2 >= 'A' && c2 <= 'F') ? c2 - 'A' + 10 : 0;

   return (hi << 4) | lo;
}

//   bool parseBool(const String &val, int type = _GV_BOOL_TF) {
bool EspCoreClass::parseBool(const String &val) {
   _fdpl("parseBool ...",_DBG2_MSG);    
   bool b = false;
   switch (val.length()) {

      case 0:
         // no val: always false
         break;
      case 1:
         // true if: (1) (t)rue (y)es
         b =  (val == "1" || val == "t" || val == "y");
         break;
      default:
         // true if: (on) (true) (yes)
         b = (val == "on" || val == "true" || val == "yes");
         break;
   }
   return b;
}

// core functions
// Clear all NVS persistent GVars memory
void EspCoreClass::factoryReset() {
   _fdpl("factoryReset ...");
   delay(1000);

   // clears the NVS flash memory which wipes out all stored GVars
   esp_err_t result = nvs_flash_erase();  // full wipe
   if (result == ESP_OK) {    
      nvs_flash_init();  // must re-init after erase 
   } else {
      _fdpl("Factory Reset ERROR " + String(result),_ERR_MSG);
   }
}

// init NVS memory
void EspCoreClass::initEE() {
   _fdpl("initEE ... ");
   eevars.begin(_EEVARS, _RW);
   eevars.end();
}  

// does the GVar exist in NVS memory
bool EspCoreClass::hasEEVar(String key) {
   _fdpl("hasEEVar ... " + key, _DBG2_MSG);
   eevars.begin(_EEVARS, _RO);
   bool rtnVal = eevars.isKey(key.c_str());
   eevars.end();
   return rtnVal;
}

// copy String to NVS memory
bool EspCoreClass::setEEVar(String key, String val) {
   _fdpl("setEEVar ... " + key + " " + val, _DBG2_MSG);
   eevars.begin(_EEVARS, _RW);
   bool rtnVal = eevars.putString(key.c_str(), val.c_str()) > 0;
   eevars.end();
   return rtnVal;
}

//  copy NVS memory to String 
String EspCoreClass::getEEVar(String key) {
   _fdpl("getEEVar ... " + key, _DBG2_MSG);
   String rtnVal = String();
   eevars.begin(_EEVARS, _RO);
   if (eevars.isKey(key.c_str())) {
      rtnVal = eevars.getString(key.c_str());
   }
   eevars.end();
   return rtnVal;
}

// delete EEVar from NVS memory
bool EspCoreClass::delEEVar(String key) {
   _fdpl("delEEVar ... " + key, _DBG2_MSG);
   bool rtnVal = false;
   eevars.begin(_EEVARS, _RW);
   if (eevars.isKey(key.c_str())) {
      rtnVal = eevars.remove(key.c_str());
   }
   eevars.end();
   return rtnVal;
}

int EspCoreClass::findGVarIdx(const String &key) {
   _fdpl("findGVarIdx ..." + key,_DBG2_MSG);
  
   // set rtn value to nothing found
   int idx = -1;
  
   // if the index is already provided j > 0
   int kdx = key.toInt();
  
   // if kdx is > gVarCount is an error return nothing found
   kdx =  (kdx >= gVarCount) ? -1 : kdx;
  
   // an idx of 0 indicates that this should be a key search
   if (kdx > 0) { // no search required this is a valid idx
      idx = kdx;
   }  else {      // search ... key is not an idx
      String lckey = key;
      lckey.toLowerCase();
      for (int i = 1; i < gVarCount; i++) {
         String lcmapkey = gVarTable[i].key;
         _fdpl("Find EE Var: " + lcmapkey,_DBG2_MSG);
         lcmapkey.toLowerCase();
         if (lckey == lcmapkey) {
            idx = i;
            break;
         }
      }
   }
   return idx; // returns the row index of the gVarMap
}

// returns the mapped global var value as a string
// key is either string(idx) or gVarMap row name
String EspCoreClass::getGVar(const String &key) {
   _fdpl("getGVar ... " + key, _DBG2_MSG);

   String rtnVal = String();
   int idx = findGVarIdx(key);
  
   if (idx > 0) {
      GVarMap &M = gVarTable[idx];

      switch (M.gVarType) {
         case _GV_INT:
            rtnVal = String(*static_cast<int*>(M.var));
            break;
         case _GV_STR:
            rtnVal = *static_cast<String*>(M.var);
            break;
         case _GV_BYTE_DEC:
            rtnVal = String(*static_cast<uint8_t*>(M.var));
            break;   
         case _GV_BYTE_HEX:
            rtnVal = byte2Hex(*static_cast<uint8_t*>(M.var));
            break;
         case _GV_UINT32:
            rtnVal = String(*static_cast<uint32_t*>(M.var));
            break;
         case _GV_BOOL:
            rtnVal = String(*static_cast<bool*>(M.var) ? "1" : "0");
            break;
         case _GV_BOOL_YN:
            rtnVal = String(*static_cast<bool*>(M.var) ? "yes" : "no");
            break;
         case _GV_BOOL_TF:
            rtnVal = String(*static_cast<bool*>(M.var) ? "true" : "false");
            break;
         case _GV_BOOL_ON_OFF:
            rtnVal = String(*static_cast<bool*>(M.var) ? "on" : "off");
            break;
         case _GV_BYTES_32: {
            uint8_t *p = static_cast<uint8_t*>(M.var);

            for (int i = 0; i < 32; i++) {
               rtnVal += byte2Hex(p[i]);
            }
            break;
         }
         case _GV_NUL:
         default:
            break;
      }
   }
   return rtnVal;
}

// update the global var pointed to by gVarMap[idx] from string using gVarType
bool EspCoreClass::setGVar(const String &key, String val) {
   _fdpl("setGVar ... " + key + " " + val, _DBG2_MSG);
 
   bool rtnVal = false;
   int idx = findGVarIdx(key);

   if (idx > 0) {
      GVarMap &M = gVarTable[idx];
      switch (M.gVarType) {
         case _GV_INT:
            *static_cast<int*>(M.var) = val.toInt();
            rtnVal = true;
            break;
         case _GV_STR:
            *static_cast<String*>(M.var) = val;
            rtnVal = true;
            break;
         case _GV_BYTE_DEC:
            *static_cast<uint8_t*>(M.var) = (uint8_t)val.toInt();
            rtnVal = true;
            break;
         case _GV_BYTE_HEX:
            *static_cast<uint8_t*>(M.var) = hex2Byte(val);
            rtnVal = true;
            break;         
         case _GV_UINT32:
            *static_cast<uint32_t*>(M.var) = strtoul(val.c_str(), nullptr, 10);
            rtnVal = true;
            break;
         case _GV_BOOL:
         case _GV_BOOL_YN:
         case _GV_BOOL_TF:
         case _GV_BOOL_ON_OFF: {
            bool b = parseBool(val);
            *static_cast<bool*>(M.var) = b;
            rtnVal = true;
            break;
         }
         case _GV_BYTES_32: {
            uint8_t *p = static_cast<uint8_t*>(M.var);

            for (int i = 0; i < 32; i++) {
               p[i] = hex2Byte(val.substring(i * 2, i * 2 + 2));
            }
        
            rtnVal = true;
            break;
         }
         case _GV_NUL:
         default:
            rtnVal = false;
            break;
      }
   }
   return rtnVal;
}

// this routine should only ever follow a factory or virgin boot
void EspCoreClass::initEEVars() {  
   _fdpl("initEEVars factory/virgin reboot ...");
   setEEVar("BootState",vars.str.bootState); 
   setEEVar("Mode",vars.str.activeMode); 
   setEEVar("devName", vars.str.devName); // save the device name   
  
   // generate unique Device ID
   // in the form: <devName> <ID> <ver> <verDate> <author>
   // where:
   // - <devName> max of 20 alphanumeric chars can include - or _ (NO spaces or punctuation) 
   //             also used as a MQTT topic root for logging <devName>/log
   // - <ID>      upper 32 bits of the esp32 64-bit chip ID in the form XXXX
   // - <ver>     in the form ##v#
   // - <verDate> in the form YYYY-MM-DD
   // - <Author>  VE7ABC

   uint64_t espChipID = ESP.getEfuseMac();

   char devIDbuf[80];
   snprintf(devIDbuf, sizeof(devIDbuf),
      "%s %04X %s %s %s",
      vars.str.devName.c_str(),
      (uint32_t)(espChipID >> 24),
      _APP_VER,
      _APP_VER_DATE,
      _AUTHOR);

   vars.str.devID = String(devIDbuf);
   setEEVar("devID", vars.str.devID);     // save the device ID

   // _ABC is a password defined in include.h which is being hashed
   initABC(_ABC, vars.ui8.ABC);
   String vv = getGVar(String(I_ABC));
   setEEVar("ABC",vv);
}

void EspCoreClass::initVars() { 
   _fdpl("initVars ..." + String(gVarCount));

   // NVS (persistent mem/flash) is used to retain Global Var overrides
   // loop through all the mapped vars and override Global Vars that have NVS Vars
   // row 0 of the Map is skipped 
   for (int i = 1; i < gVarCount; i++) {
      GVarMap &M = gVarTable[i];
     
      // the BootState Var must always exist in NVS otherwise this is a Factory or Virgin boot
      if (I_BootState == i && !hasEEVar(M.key)) {
         // initialize the NVS private vars
         initEEVars();
      } 

      // is there an NVS value for this mapped var override the global var
      if (hasEEVar(M.key)) {
         if (M.isPublic) {
            // at boot time all Global values have a compile time default value
            // convert this default value to string and store to the map defVal for public vars
            M.defVal = getGVar(String(M.idx));        // typed global -> String
         }

         // override the Global value with the typed value from NVS 
         setGVar(String(M.idx), getEEVar(M.key));  // String -> typed global
        
         _fdpl(String("override : ") + String(M.key) + "(" + String(M.idx) + ") " + getGVar(String(M.idx)),_DBG4_MSG);

         // the NVS Mode var should always return to LIVE Mode once its boot val has set the Global Var
         // 3 Modes; LIVE, OTA, GV ... so if the activeMode is NOT LIVE then set the NVS Mode to LIVE
         if (I_Mode == i && vars.str.activeMode != _LIVE) {
            _fdpl("initVars b4Mode: " + getGVar("Mode") + " " + getEEVar("Mode"), _DBG4_MSG);
            setEEVar(M.key,_LIVE);
            _fdpl("initVars setMode: " + getGVar("Mode") + " " + getEEVar("Mode"), _DBG4_MSG);
         }

         // the global BootState reflects the last state the defice was in prior to boot
         // the nvs BootState is updated here to the current state
         if (I_BootState == i) {
            if (vars.str.activeMode == _LIVE) {
               setEEVar(M.key,_WDT);
            } else {
               setEEVar(M.key,vars.str.activeMode);
            }
         } 
      }
   }  
   //_fdpl("BootState ... " + getGVar("BootState"), _DBG3_MSG);
   _fdpl("initVars chkMode: " + getGVar("Mode") + " " + getEEVar("Mode"), _DBG4_MSG);
   //_fdpl("ABC ... " + getGVar(String(I_ABC)), _DBG3_MSG);
}

String EspCoreClass::getEEVars() {
   _fdpl("getEEVars ...", _DBG2_MSG);

   String rtnVal;

   for (int i = 1; i < gVarCount; i++) {
      GVarMap &M = gVarTable[i];

      if (hasEEVar(M.key)) {
         rtnVal += (M.isPublic) ? "Pub- " : "Prv- ";
         rtnVal += String(M.key);
         rtnVal += "\n";
         rtnVal += "     GVR: ";
         rtnVal += getGVar(String(M.idx));
         rtnVal += "\n";
         rtnVal += "     EEV: ";
         rtnVal += getEEVar(M.key);
         rtnVal += "\n";
      }
   }
   return rtnVal;
}

String EspCoreClass::getGVars() {
   _fdpl("getGVars ...", _DBG2_MSG);

   String rtnVal;

   for (int i = 1; i < gVarCount; i++) {
      GVarMap &M = gVarTable[i];

      if (M.isPublic) {
         rtnVal += String(M.key);
         rtnVal += ":";
         rtnVal += getGVar(String(M.idx));
         rtnVal += "\n";
      }
   }

   return rtnVal;
}

bool EspCoreClass::updateGVar(const String &key, String val) {
   _fdpl("updateGVar ...", _DBG2_MSG);

   bool   rtnVal = false;
   int    idx    = findGVarIdx(key);

   if (idx > 0) {
      GVarMap &M  = gVarTable[idx];
      // Current global value as a String
      String gvs = getGVar(String(M.idx));                           // get global as string
      String dvs = (M.defVal == "") ? gvs :  M.defVal;               // determine default value as string
      String cvs = val;                                              // get value to check
      //String nvs = (hasEEVar(M.key)) ? getGVar(M.key) : String();  // hasEEVar?

      // normalize Bools 
      if (M.gVarType >= _GV_BOOL){
         gvs = (parseBool(gvs)) ? "T" : "F";
         dvs = (parseBool(dvs)) ? "T" : "F";
         cvs = (parseBool(cvs)) ? "T" : "F";
         //nvs = (nvs == "") ? nvs : (parseBool(nvs)) ? "T" : "F";
      }

      // GV is global variable of type M.gVarType 
      // 1) if cvs == dvs then GV = conv2type(dvs), delete(nvs), defVal = ""
      // 2) if cvs == gvs then do nothing
      // 3) if cvs != gvs && gvs == dvs then defVar = gvs, GV = type(cvs), nvs = cvs 

      if (cvs == dvs) {                 // 1) Check value is the default
         delEEVar(M.key);               // delete the nvs var
         M.defVal = "";                 // clear the defVal
         setGVar(String(M.idx), val);   // update the global var
         rtnVal = true;

      } else if (cvs == gvs) {          // 2) Check value is already the current global value
         rtnVal = true;                 // do nothing

      } else {                          // 3) Check value is neither default nor current global
         if (M.defVal == "") {          // If GV was still at default, preserve that default
            M.defVal = gvs;             // save the defVal
         }
         setEEVar(M.key, val);          // update the NVS var
         setGVar(String(M.idx), val);   // update the global var
         rtnVal = true;
      }
   }

   return rtnVal;
}

String EspCoreClass::getQdepth(void) {
  String info;

  for (int idx = 0; idx < _QS; idx++) {
    String qName;

    switch (idx) {
       case _SYS_Q:      qName = "SYS_CMD";  break;
       case _SER_LOG_Q:  qName = "SER_LOG";  break;
       case _MQTT_LOG_Q: qName = "MQTT_LOG"; break;
       case _MQTT_Q:     qName = "MQTT";     break;
       case _APP_Q:      qName = "APP";      break;
       case _ECC_Q:      qName = "ECC";      break;
       default:          qName = "UNKNOWN";  break;
    }
    
     info += qName + " " + qDepth((qDpth)idx) + "\n";
  }
  return info;
}


String EspCoreClass::getInfo(bool sysLog) {
   _fdpl("getInfo ...", _DBG2_MSG);
  
   String info;

   auto addInfo = [&](const String &line) {
      if (sysLog) {
         _fdp(line,_INFO_MSG);
      } else {
         info += line;
      }
   };

   addInfo("Esp32 Wifi connected\n");
   addInfo(String("Hostname: " + String(WiFi.getHostname()) + "\n"));
   addInfo(String("    SSID: " + WiFi.SSID() + "\n"));
   addInfo(String("      IP: " + WiFi.localIP().toString() + "\n"));
   addInfo(String("     MAC: " + WiFi.macAddress() + "\n"));
   addInfo(String("    RSSI: " + String(WiFi.RSSI()) + "\n"));
   addInfo(String(" Gateway: " + WiFi.gatewayIP().toString() + "\n"));
   addInfo(String("     DNS: " + WiFi.dnsIP().toString() + "\n"));
   addInfo(String("  Subnet: " + WiFi.subnetMask().toString() + "\n"));
   addInfo(String("    MASK: " + String(vars.ui8.MK1) + "." 
                               + String(vars.ui8.MK2) + "."
                               + String(vars.ui8.MK3) + "."
                               + String(vars.ui8.MK4) + "\n"));
   addInfo(String("      IP: " + String(vars.ui8.LN1) + "." 
                               + String(vars.ui8.LN2) + "."
                               + String(vars.ui8.LN3) + "."
                               + String(vars.ui8.LND) + "\n"));
   addInfo(String("     OTA: " + String(vars.ui8.LNO)
                 + " Router: " + String(vars.ui8.LNR) 
                 +  "  MQTT: " + String(vars.ui8.LNM) + "\n"));
   addInfo(String("   DevID: " + vars.str.devID + "\n"));
   addInfo(String("  Device: " + vars.str.devName + "\n"));
   addInfo(String("  Author: " + vars.str.author));
   addInfo(String(" App_Ver: " + vars.str.appVer));
   addInfo(String(" App_Ver_Date: " + vars.str.appVerDate + "\n"));
   addInfo(String(" Core_Ver: " + vars.str.coreVer));
   addInfo(String(" Core_Ver_Date: " + vars.str.coreVerDate + "\n"));
   String lvl = String(byte2Hex(vars.ui8.log.lev));
   lvl.toUpperCase();
   addInfo(String("Log_Level: 0x") + lvl + "\n" );
   addInfo(String("Boot State: " + getEEVar("BootState") + " : " + vars.str.bootState + "\n"));
   addInfo(String("Boot Reason: " + String(vars.str.bootCode + "\n")));
   if (vars.str.activeMode == _LIVE) {
      addInfo(String("    Mode: LIVE \n")); 
      if (vars.tst.in.testTime > 0) {
         addInfo(String("  TestMode: On   "));
         addInfo(String(" TestTime: " + getHMS(vars.tst.in.testTime / 2) + "\n"));
      } else {
         addInfo(String("  TestMode: Off  "));
         addInfo(String(" TestTime:  00:00:00"));
      }
   } else if (vars.str.activeMode == _OTA) { // _OTA Mode
      addInfo(String("    Mode: OTA "));
      addInfo(String(" OtaTime: " + getHMS(vars.in.otaTime / 2) + "\n"));
   } else if (vars.str.activeMode == _CFG) {
      addInfo(String("    Mode: CFG "));
   } else {
      addInfo(String("    Mode: UNKNOWN "));        
   }
   addInfo(String("\n"));
   return info;
}

void EspCoreClass::chkTestMode() {
   _fdpl("chkTestMode ..." + String(vars.tst.enm.chgMode),_DBG4_MSG);

   if (vars.tst.enm.chgMode != _NO_CHG) {
      _fdpl("Check Test Mode",_DBG2_MSG);
      vars.tst.in.testTime = 0;
#if _HAS_RGB == 1         
      vars.bl.currLed = _OFF;
      vars.ui8.currRGB = _BLUE + 1;
#endif         
      _fdpl(String("what is the sysLed status? ") + String(vars.bl.sysLed ? "T" : "F"),_DBG2_MSG);
      if (!vars.bl.sysLed) {
         setLed(_OFF);
#if _HAS_RGB == 1
         if (vars.bl.rgbEnabled) setRGBLed(_BLACK);
#endif                   
      }
    
      switch (vars.tst.enm.chgMode) {
         case _MODE_ON:
            _fdpl("chkTestMode -> _ON",_DBG3_MSG);
            vars.tst.bl.testMode = _ON;
            break;
         case _MODE_OFF:
            _fdpl("chkTestMode -> _OFF",_DBG3_MSG);
            vars.tst.bl.testMode = _OFF;
            break;
         case _MODE_TOGGLE:
            _fdpl("chkTestMode -> _TOGGLE",_DBG3_MSG);
            vars.tst.bl.testMode = !vars.tst.bl.testMode;
            break;
      }

      if (vars.tst.bl.testMode == _ON) vars.tst.in.testTime = _TOCKS_PER_MIN * vars.tst.in.test_to;
      _fdpl("chkTestMode " + String(vars.tst.bl.testMode ? "ON " : "OFF ") + String(vars.tst.in.testTime),_DBG3_MSG);
      sendWebpageUpdates(); // update the web page timer value

      vars.tst.enm.chgMode = _NO_CHG;
   }
}

void EspCoreClass::testMode(int State) {
   _fdpl("testMode ..." + String(State), _DBG2_MSG);   
 
   if (State == _ON) {
      vars.tst.enm.chgMode = _MODE_ON;
      _fdpl("testMode ... on " + String(State), _DBG4_MSG);   
   } else if (State == _OFF) {
      vars.tst.enm.chgMode = _MODE_OFF;
      _fdpl("testMode ... off " + String(State), _DBG4_MSG);   
   } else if (State == _TOGGLE) {
      vars.tst.enm.chgMode = _MODE_TOGGLE;
      _fdpl("testMode ... toggle " + String(State), _DBG4_MSG);   
   }
}

// ---------------------------------------------------------------------------
// Return the end index of the current item.
//
// Returns:
//   -2  start index at or beyond end of string
//   ln  last item (no more '/')
//   >=0 index of next '/'
// ---------------------------------------------------------------------------
int EspCoreClass::nextIdx(const String &eps, int strt_idx, const String &dl, int ln)
{
   _fdpl("nextIdx ...", _DBG2_MSG);   

   if (strt_idx >= ln)
      return -2;

   int end_idx = eps.indexOf(dl, strt_idx);

   if (end_idx < 0)
      return ln;

   return end_idx;
}

EspCoreClass::EndPoint EspCoreClass::parseEPS(const String &epsRaw) {
   _fdpl("parseEPS ...", _DBG2_MSG);   

   // create an empty End Point Container
   EndPoint epc;

   // copy raw to workable string buffer
   String eps = epsRaw;

   // get the length of the string ... 0 len = main webpage request
   uint ln = eps.length();

   _fdp("parseEPS eps Raw: ",_DBG4_MSG);
   _fdp(eps,_DBG4_MSG);
   // force to lower case
   eps.toLowerCase();

   // replace any _ with /
   eps.replace('_','/');
   _fdp("  eps: ",_DBG4_MSG);
   _fdpl(eps,_DBG4_MSG);

   // is this a json request?
   epc.isJson = eps.startsWith("/j/");
  
   // the start index will either be 3 or 1 (0 is a number)
   int strt_idx = epc.isJson ? 3 : 1;

   // get the next index 
   int end_idx = nextIdx(eps,strt_idx,"/",ln);

   // negative end index means we are done parsing 
   if (end_idx > 0) {
     
      // command is always the first term
      epc.cmd = eps.substring(strt_idx,end_idx);
     
      // if command was gvars then check for var name and value terms
      if (epc.cmd == "gvars"){
         strt_idx = end_idx + 1;
         end_idx = nextIdx(eps,strt_idx,"/",ln);
        
         // is there a Var Name term?
         if (end_idx > 0) {
            epc.gvar = eps.substring(strt_idx,end_idx);
            strt_idx = end_idx + 1;
            end_idx = nextIdx(eps,strt_idx,"/",ln);

            // is there a Value term?
            if (end_idx > 0) {
               epc.gval = eps.substring(strt_idx,end_idx);
            }
         }
      } else {
         strt_idx = end_idx + 1;
         end_idx = nextIdx(eps,strt_idx,"/",ln);
        
         // is there a command action term?
         if (end_idx > 0) {
            epc.action = eps.substring(strt_idx,end_idx);
         }
      }
   }

   _fdp("length ",_DBG4_MSG);
   _fdp(String(ln),_DBG4_MSG);
   _fdp("  epc.isJson? ",_DBG4_MSG);
   _fdp(String(epc.isJson),_DBG4_MSG);
   _fdp("  cmd ",_DBG4_MSG);
   _fdp(epc.cmd,_DBG4_MSG);
   _fdp("   action ",_DBG4_MSG);
   _fdp(epc.action,_DBG4_MSG);
   _fdp("   gvar ",_DBG4_MSG);
   _fdp(epc.gvar,_DBG4_MSG);
   _fdp("   gval ",_DBG4_MSG);
   _fdpl(epc.gval,_DBG4_MSG);
  
   return epc;
}

void EspCoreClass::sendResponse(AsyncWebServerRequest* req, const String& msg, bool isJson, bool isError)  
{
   _fdpl("sendResponse ...",_DBG1_MSG);
   if (isJson) {
      StaticJsonDocument<1024> doc;

      if (isError) {
         doc["oops"] = msg;
      } else {
         doc["msg"] = msg;
      }

      String json;
      serializeJson(doc, json);

      //if (vars.str.activeMode == _LIVE && vars.bl.appEnable) return;
      req->send(200, "application/json", json);
      return;
   }

   // Plain text fallback
   //if (vars.str.activeMode == _LIVE && vars.bl.appEnable) return;
   req->send(200, "text/plain", msg);
}

void EspCoreClass::handleWebRequest(AsyncWebServerRequest *req) {
   _fdpl("handleWebRequest ...", _DBG1_MSG);   
   EndPoint epp = parseEPS(req->url());

   //bool validEpp = false;

   //if (appHttpHandler) {
   //   appHttpHandler(epp);   // <-- app sees parsed endpoint
   //}

   // endpoints
   // cmd: / info ota reboot factoryreset test gvar
   // action: on off toggle
   // gvar must match a gvarmap var
   // gval must be correct to the servermap type
#if _USE_FS_HTML == 1
   if (vars.str.activeMode == _OTA && epp.cmd == "/") { // ota page
      _fdpl("/ OTA FS Web Page",_INFO_MSG);
      req->send(LittleFS, "/espcore/ota.html", "text/html");
      return;
   } else if (vars.str.activeMode == _CFG && epp.cmd == "/") { // gv page
      _fdpl("/ CFG FS Web Page",_DBG4_MSG);
      req->send(LittleFS, "/espcore/cfg.html", "text/html");
      return;
   } else if (vars.str.activeMode == _LIVE && epp.cmd == "/") { // main page
      _fdpl("/ LIVE FS Web Page",_DBG4_MSG);
      req->send(LittleFS, vars.str.appHtml, "text/html");
      return;
      
   }
#else
   if (vars.str.activeMode == _OTA && epp.cmd == "/") { // ota page
      _fdpl("/ OTA Web Page",_INFO_MSG);

      AsyncWebServerResponse *response = new AsyncChunkedResponse("text/html",
         [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            if (index > 0) return 0;  // Only serve once
            String part = FPSTR(CWP_OTA);
            size_t len = part.length();
            if (len > maxLen) len = maxLen;
            memcpy(buffer, part.c_str(), len);
            return len;
         }
      );

      req->send(response);
      return;

   } else if (vars.str.activeMode == _CFG && epp.cmd == "/") { // gv page
      _fdpl("/ GV Web Page",_DBG4_MSG);
      AsyncWebServerResponse *response = new AsyncChunkedResponse("text/html",
         [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            //if (index > 0) return 0;  // Only serve once
            //String part = FPSTR(CWP_GV);
            //size_t len = part.length();
            //if (len > maxLen) len = maxLen;
            //memcpy(buffer, part.c_str(), len);
            //return len;

            static int chunk = 0;
            String part;

            if (index == 0) {
               chunk = 0;  // reset on first call
            }

            if (chunk == 0) {
               part = FPSTR(CWP_GVAR_1);
            } else if (chunk == 1) {
               part = FPSTR(CWP_GVAR_2);
            }  else if (chunk == 2) {
               part = FPSTR(CWP_GVAR_3);
            } else if (chunk == 3) {
               part = FPSTR(CWP_GVAR_4);
            } else if (chunk == 4) {
               part = FPSTR(CWP_GVAR_5);
            } else if (chunk == 5) {
               part = FPSTR(CWP_GVAR_6);
            } else if (chunk == 6) {
               part = FPSTR(CWP_GVAR_7);
            } else if (chunk == 7) {
               part = FPSTR(CWP_GVAR_8);
            } else if (chunk == 8) {
               part = FPSTR(CWP_GVAR_9);
            } else {
               return 0;   // no more chunks
            }

            chunk++;  // advance to next chunk

            size_t len = part.length();
            if (len > maxLen) len = maxLen;
            memcpy(buffer, part.c_str(), len);
            return len;
         }
      );

      req->send(response);
      return;

   } else if (vars.str.activeMode == _LIVE && epp.cmd == "/") { // main page
      _fdpl("/ LIVE Web Page",_DBG4_MSG);
     
      AsyncWebServerResponse* response = new AsyncChunkedResponse(
         "text/html",
         [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {

            static int chunk = 0;
            String part;

            if (index == 0) {
               chunk = 0;  // reset on first call
            }

            if (chunk == 0) {
               part = FPSTR(CWP0_P1);
            } else if (chunk == 1) {
               part = FPSTR(CWP0_P2);
            } else if (chunk == 2) {
               part = FPSTR(CWP0_P3);
            } else {
               return 0;   // no more chunks
            }

            chunk++;  // advance to next chunk

            size_t len = part.length();
            if (len > maxLen) len = maxLen;
            memcpy(buffer, part.c_str(), len);
            return len;
         } 
      );
      req->send(response);
      return;
   }
#endif 

   String msg;
   bool isOk = true;
 
   _fdpl("Web Srv Req: J_" + String(epp.isJson ? "T" : "F")
                           + String(" c_" 
                                   + epp.cmd 
                                   + " A_" 
                                   + epp.action 
                                   + " N_" 
                                   + epp.gvar 
                                   + " V_" 
                                   + epp.gval),_DBG1_MSG);
 
   // need to add endpoint logic here
   msg = String("Unknown_Request");
 
   if (epp.cmd == "rssi") {
      _fdpl("awsr rssi",_INFO_MSG);
      msg = String("RSSI: ") + String(WiFi.RSSI());
 
   } else if (epp.cmd == "info") {
      _fdpl("awsr info",_INFO_MSG);
      msg = getInfo();
#if _HAS_FS == 1     
   } else if (epp.cmd == "lsfs") {
      _fdpl("awsr lsfs " + epp.action,_INFO_MSG);
      lsFS(epp.action,msg);
   } else if (epp.cmd == "appfsdel") {
      _fdpl("awsr appfsdel " + epp.action,_INFO_MSG);
      appDelFSfile(epp.action,msg);
   } else if (epp.cmd == "appfsget") {
      _fdpl("awsr appfsget " + epp.action,_INFO_MSG);
      appGetFSfile(epp.action,msg);
// } else if (epp.cmd == "appfsput") {
//    _fdpl("awsr appfsput " + epp.action,_INFO_MSG);
//    appPutFSfile(epp.action,msg);
//    return;
#endif
   } else if (epp.cmd == "qdp") {
      _fdpl("aswr qdp", _INFO_MSG);
      msg = getQdepth();
   } else if (epp.cmd == "gvars") {
      _fdpl("awsr gvars",_INFO_MSG);
      msg = getGVars();

   } else if (epp.cmd == "eevars") {
      _fdpl("awsr eevars",_INFO_MSG);
      msg = getEEVars();
 
   } else if (epp.cmd == "ota") {
      isOk = setSysCmd(_CMD_OTA);
     
      if (!isOk) { // error?
         _fdpl("awsr ota err",_ERR_MSG);
         msg = "System busy try again";
     
      } else {
     
         if (vars.str.activeMode == _OTA) {
            _fdpl("awsr cancel ota ",_WRN_MSG);
            msg = "Cancelling OTA Mode ... ";
         } else {
            _fdpl("awsr enter ota",_INFO_MSG);
            msg = "Entering OTA Mode ... ";
         }
      }
   } else if (epp.cmd == "cfg") {
      isOk = setSysCmd(_CMD_CFG);
     
      if (!isOk) { // error?
         _fdpl("awsr cfg err",_ERR_MSG);
         msg = "System busy try again";
     
      } else {
     
         if (vars.str.activeMode == _CFG) {
            _fdpl("awsr cancel cfg ",_WRN_MSG);
            msg = "Cancelling CFG Mode ... ";
         } else {
            _fdpl("awsr enter CFG Mode",_INFO_MSG);
            msg = "Entering CFG Mode ... ";
         }
      }
   } else if (epp.cmd == "reboot") {
     
      isOk = setSysCmd(_CMD_REBOOT);
      if (!isOk) { // error?
         _fdpl("awsr reboot err",_ERR_MSG);
         msg = "System busy try again";
      } else {
         _fdpl("awsr Reboot",_INFO_MSG);
         msg = "Rebooting ... ";
      }

   } else if (epp.cmd == "factory") {
     
      isOk = setSysCmd(_CMD_FACTORY);
      if (!isOk) { // eror?
         _fdpl("awsr factory err",_ERR_MSG);
         msg = "System busy try again";
      } else {
         _fdpl("awsr factory reset",_INFO_MSG);
         msg = "Factory Reset ... ";
      }
   } else if (epp.cmd == "test") {

      if (vars.str.activeMode == _LIVE) {
         if (epp.action == "") {
            if (vars.tst.in.testTime > 0) {
               msg = String("  TestMode: On   ");
               msg += String(" TestTime: " + getHMS(vars.tst.in.testTime / 2) + "\n");
            } else {
               msg = String("  TestMode: Off  ");
               msg += String(" TestTime:  00:00:00");
            }
        
         } else if (epp.action == "on") {
            msg = setSysCmd(_CMD_TEST_ON) ? "Test mode on" : "System busy try again";
         } else if (epp.action == "off") {
            msg = setSysCmd(_CMD_TEST_OFF) ? "Test mode off" : "System busy try again";
         } else if (epp.action == "toggle") {
            msg = setSysCmd(_CMD_TEST_TOGGLE) ? "Test mode Toggled" : "System busy try again";
         }
      } else {
         msg = "Test only available in Live Mode";
      }
   } else if (appEndPointExtension) {
     msg = appEndPointExtension(epp);
   }

   //if (vars.str.activeMode == _LIVE && vars.bl.appEnable) return;
   sendResponse(req,msg,epp.isJson,!isOk);
}

void EspCoreClass::sendWebpageUpdates(String req, JsonObject obj) {
   _fdpl("sendWebpageUpdates ...", _DBG1_MSG);   
   bool isOk = true;

   vars.in.iRSSI = WiFi.RSSI();
   DynamicJsonDocument doc(1024);

   if (req == "appGetFSfile") {

      String fn = obj["data"]; // "zns.json"
      String flc;
      appGetFSfile(fn, flc);  // <-- existing function
      _fdpl("sendWebpageUpdate appGetFSfile ", _DBG1_MSG);   

      doc["cmd"] = 122;

      if (flc == "") {
         doc["data"] = "not_found";
       } else {
         doc["data"] = flc;   // raw JSON string from file
       }

       _fdpl("sendWebpageUpdate -> " + flc, _INFO_MSG);

   } else if (req == "Refresh_CFG") {
     
      String GVars = getGVars();
      doc["msg"] = GVars;
      // send to webpage

   } else if (req == "Save_CFG") {

      // obj is ALREADY deserialized

      for (int i = 0; i < gVarCount; i++) {
         GVarMap &m = gVarTable[i];

         if (!obj.containsKey(m.key)) 
            continue;

         const char* val = obj[m.key];
         bool ok = updateGVar(m.key, val);

         if (!ok) {
            isOk = false;
            _fdpl("CFG save failed for " + String(m.key), _ERR_MSG);
         }
      }

      // Save CFG
      bool isOk = setSysCmd(_CMD_CFG);
      doc["cmd"] = 11;
      doc["ok"] = isOk; // {"cmd":11,"ok":true/false}

      // send to webpage

   } else if (req == "Cancel_CFG") {

      // cancel CFG
      bool isOk = setSysCmd(_CMD_CFG);
      doc["cmd"] = 12;
      doc["ok"] = isOk; // {"cmd":12,"ok":true/false}
      // send to webpage

   } else if (req == "Factory_CFG") {

      // Factory CFG
      bool isOk = setSysCmd(_CMD_CFG);
      doc["cmd"] = 13;
      doc["ok"] = isOk; // {"cmd":13,"ok":true/false}
      // send to webpage

   } else {

      if (vars.str.activeMode == _LIVE) {
         if (vars.tst.bl.testMode) {
            _fdpl("sendUpdates sU00 TurnOff",_DBG4_MSG);
            doc["tstBtn"] = "TurnOff";
         } else {
            _fdpl("sendUpdates sU01 TurnOn",_DBG4_MSG);
            doc["tstBtn"] = "TurnOn";       
         }
         doc["appTitle"] = vars.str.appTitle;
         doc["appBtn"] = vars.str.appBtn;
         // add more doc update links here
      }
      doc["rssi"] = vars.in.iRSSI;
      if (vars.str.activeMode == _LIVE) {
         _fdpl("sendUpdates sU02 testtime " + String(vars.tst.in.testTime),_DBG4_MSG);
         doc["rntm"] = vars.tst.in.testTime / 2;
      } else {
         _fdpl("sendUpdates sU03 otatime " + String(vars.in.otaTime),_DBG4_MSG);
         doc["rntm"] = vars.in.otaTime / 2;
         doc["timer"] = vars.in.otaTime / 2;
      }
   }   
   String msg;
   serializeJson(doc,msg);
   //if (vars.str.activeMode == _LIVE && vars.bl.appEnable) return;
   _fdpl("END-OF WebpagesUpdate " + msg, _DBG3_MSG);
   ws.textAll(msg);
}

// This function handles WebSocket events, including client connections, disconnections, and data reception.
void EspCoreClass::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
   AwsEventType type, void *arg, uint8_t *data, size_t len) {
   _fdpl("onWsEvent ...", _DBG1_MSG);            
   if (type == WS_EVT_CONNECT) {
      _fdpl("WebSocket client connected");
      client->text("Connected");
      //sendWebpageUpdates();
   } else if (type == WS_EVT_DISCONNECT) {
      _fdpl("WebSocket client disconnected",_ERR_MSG);
   } else if (type == WS_EVT_DATA) {
      _fdpl("wsev event start");
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
         data[len] = 0;
       
         DynamicJsonDocument doc(1024);
         DeserializationError err = deserializeJson(doc, data);
         if (err) {
            _fdpl("WS JSON parse failed",_ERR_MSG);
            return;
         }

         if (appWsHandler) {
            appWsHandler(doc);   // <-- app sees the WS event
         }

         String action = String();

         if (doc.containsKey("cmd")) {
            int cmd = doc["cmd"].as<int>();
            _fdpl(String("Got WS cmd: ") + String(cmd),_INFO_MSG);
            switch (cmd) {
               case  5: testMode(_TOGGLE);           _fdpl("wsev Toggle",      _INFO_MSG); break;
               case 10: action = "Refresh_CFG";      _fdpl("wsev refresh CFG", _INFO_MSG); break;
               case 11: action = "Save_CFG";         _fdpl("wsev save CFG",    _INFO_MSG); break;
               case 12: action = "Cancel_CFG";       _fdpl("wsev cancel CFG",  _INFO_MSG); break;
               case 13: action = "Factory_CFG";      _fdpl("wsev factory CFG", _INFO_MSG); break;
               case 90: action = "appGetFSfile";     _fdpl("wsev appGetFSfile",_INFO_MSG); break;       
               case 99: vars.in.iRSSI = WiFi.RSSI(); _fdpl("wsev RSSI",        _INFO_MSG); break;
            }
         }

         if (action == String()) {
            sendWebpageUpdates(); // <--- PUSH STATUS AFTER COMMAND
         } else if (action == "Save_CFG" || action == "appGetFSfile") {
            sendWebpageUpdates(action, doc.as<JsonObject>()); 
         } else if (action == "Factory_CFG") {
            sendWebpageUpdates(action); 
            delay(100);
            factoryReset();
         }  else {
            sendWebpageUpdates(action); 
         } 
      }
   }
}

bool EspCoreClass::setSysCmd(SysCmd cmd) {
   _fdpl("setSysCmd ...", _DBG2_MSG);

   String x = qDepth(_SYS_Q,"inc");
   if (xQueueSend(sysCmdQ, &cmd, 0) == pdPASS) {
      _fdpl("Queued SysCmd " + String(cmd), _DBG4_MSG);
      return true;
   }

   _fdpl("SysCmd queue full", _ERR_MSG);
   return false;
}

// begin functions   
void EspCoreClass::initGPIO() {
   _fdpl("initGPIO ...");
   pinMode(vars.in.ledPin,      OUTPUT);
   digitalWrite(vars.in.ledPin, HIGH);  // turn off led (active low)
   pinMode(vars.in.bootBtnPin,     INPUT_PULLUP);
}

void EspCoreClass::initWiFi() {
   _fdpl("Connecting to WiFi STA SSID: " + vars.str.wSSID );
   WiFi.disconnect(true);
   delay(500);

   vars.bl.wifiConnected = false;

   if (vars.str.activeMode == _CFG) {   // Configuration Mode (AP + STA)
      _fdpl("Configuring WiFi for GV AP mode ...",_INFO_MSG);

      WiFi.mode(WIFI_AP_STA);

      // AP interface (DEFAULT NETWORK: 192.168.4.1)
      // SSID ESP32_DEVICE_CONFIG
      _fdpl("GV AP SSID: ESP32_DEVICE_CONFIG",_INFO_MSG);
      WiFi.softAP("ESP32_DEVICE_CONFIG");

      // STA interface (STATIC — same mode used for LIVE)
      _fdpl("Attempting to Configure WiFi STA in GV mode ... ",_INFO_MSG);
      WiFi.config(Local, Local, Subnet, Local);

   } else {   // LIVE or OTA
      WiFi.mode(WIFI_STA);

      if (vars.str.activeMode == _LIVE) {
         _fdpl("Configuring WiFi for LIVE mode ... ",_INFO_MSG);
         WiFi.config(Local, Local, Subnet, Local);
      } else if (vars.str.activeMode == _OTA) {
         _fdpl("Configuring WiFi for OTA mode ... ",_INFO_MSG);
         WiFi.config(Ota, Gateway, Subnet, DNS);
      }

   }

   WiFi.setHostname(vars.str.devName.c_str());
   WiFi.begin(vars.str.wSSID.c_str(), vars.str.wABC.c_str());

   _fdpl("Local:   " + Local.toString(),_DBG4_MSG);
   _fdpl("initWiFi &Local(member): " + String((uint32_t)&this->Local, HEX),_DBG4_MSG);
   _fdpl("Gateway: " + Gateway.toString(),_DBG4_MSG);
   _fdpl("DNS:     " + DNS.toString(),_DBG4_MSG);
   _fdpl("Subnet:  " + Subnet.toString(),_DBG4_MSG);

   delay(100);

   int x = 0;

   _fdpl("Waiting for WiFi STA Connection ...",_INFO_MSG);

   while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      x++;

      if (x % 5 == 0) {
         _fdp(".",_DBG3_MSG);
      }

      if (vars.str.activeMode == _CFG) {
         // GV → SHORT timeout (5 seconds), NO reboot
         if (x > 50) {
            _fdpl("CFG mode STA connection failed. Continuing with AP only.",_WRN_MSG);
            break;
         }  
      } else {
         if (x > 200) {
            _fdpl("WiFi STA Connection failed. Restarting...",_ERR_MSG);
            ESP.restart();
         }
      }
          
   }

   vars.bl.wifiConnected =  (WiFi.status() == WL_CONNECTED);
   getInfo(_SYSLOG);
}

void EspCoreClass::initOTA() {
   _fdpl("Initializing OTA ...");
   vars.in.otaTime = _TOCKS_PER_MIN * _OTA_TIMEOUT;  // set OTA time to 5 minutes
   ArduinoOTA.setPassword(_OTA_ABC);
   ArduinoOTA.setHostname("ESP_OTA");
   ArduinoOTA.begin();
}

// add Subscription to Mqtt
//void EspCoreClass::addMqttSub(const String &topic) {
//    mqttSubs.push_back(topic);
//
//    // If MQTT is already connected, subscribe immediately
//    if (mqtt.connected()) {
//        mqtt.subscribe(topic.c_str());
//        _fdpl("mqtt late-subscribe " + topic);
//    }
//}
/*
void EspCoreClass::mqttCallback(char* topic, byte* payload, unsigned int length)
{ 
   _fdp("mqttCallback ...");
   _fdpl(String("MQTT RX = ") + String(topic),_DBG4_MSG);

   if (appMqttHandler) {
        appMqttHandler(topic, payload);   // non-blocking
   }

   String msg;

   for (unsigned int i = 0; i < length; i++) {
      msg += (char)payload[i];

      _fdpl(String("MQTT RX [%s] = %s") + String(topic) + String(msg.c_str()));
   }
   // place mqtt subscribe logic here
}
*/

// This function sends the current status of the device to all connected WebSocket clients.
void EspCoreClass::mqttCallback(char* topic, byte* payload, unsigned int length) {

  MqttMsg m;

  m.isLog      = false;
  m.addNewline = false;

  // copy topic
  strncpy(m.topic, topic, _MQTT_TOPIC_MAXLEN);
  m.topic[_MQTT_TOPIC_MAXLEN - 1] = '\0';

  // copy payload
  size_t copyLen = (length < _MQTT_PAYLOAD_MAXLEN - 1) ? length : _MQTT_PAYLOAD_MAXLEN - 1;
  memcpy(m.payload, payload, copyLen);
  m.payload[copyLen] = '\0';

  if (vars.enm.mqttState == _MQTT_CON) {
    if (appMqttHandler) {
      appMqttHandler(m);  
    }
  }
}
                       
void EspCoreClass::initMqttCB() {
   _fdpl("initMqttCB setup callback pointer ...");
   
   mqtt.setCallback(
      [this](char* topic, byte* payload, unsigned int length) {
         this->mqttCallback(topic, payload, length);
      }
   );
   
   vars.enm.mqttState = _MQTT_RESUB;

}

bool EspCoreClass::initMqtt() {
   
   bool mqttConnected = false;

   if (vars.bl.wifiConnected) {
      mqtt.setServer(ip2str(vars.ui8.LN1, vars.ui8.LN2, vars.ui8.LN3, vars.ui8.LNM), _MQTT_PORT);
      mqttConnected = mqtt.connect(vars.str.devName.c_str());

      if (!mqttConnected) {
         _fdpl("initMqtt connection error " + String(mqtt.state()), _ERR_MSG);
         vars.enm.mqttState = _MQTT_RECON;
      } else {
         vars.enm.mqttState = _MQTT_CB;
      }
   }
   return mqttConnected;
}

void EspCoreClass::initWebSocket() {
   _fdpl("Init Web Socket....");

   ws.onEvent(
      [this](AsyncWebSocket *server,
         AsyncWebSocketClient *client,
         AwsEventType type,
         void *arg,
         uint8_t *data,
         size_t len) {
            this->onWsEvent(server, client, type, arg, data, len);
         }
   );

   server.addHandler(&ws);
}

#if _HAS_FS == 1
/*
bool EspCoreClass::isFSfile(AsyncWebServerRequest *req)
{
   String pth = req->url();

   // Root is handled by handleWebRequest()
   if (pth == "/")
      return false;

   // Must have a file extension
   int dot = pth.lastIndexOf('.');
   int slash = pth.lastIndexOf('/');

   if (dot <= slash)
      return false;

   String ext = pth.substring(dot + 1);
   ext.toLowerCase();

   // Don't let these go through the static handler
   if (ext == "ico" || ext == "gz")
      return false;

   return true;
}
*/
bool EspCoreClass::isFSfile(AsyncWebServerRequest *req)
{
   String pth = req->url();

   // These are ESP32 API endpoints, not filesystem files.
   if (pth.startsWith("/lsfs") ||
       pth.startsWith("/rssi") ||
       pth.startsWith("/factory") ||
       pth.startsWith("/reboot") ||
       pth.startsWith("/info") ||
       pth.startsWith("/qdp") ||
       pth.startsWith("/eevars") ||
       pth.startsWith("/test") ||
       pth.startsWith("/ota") ||
       pth.startsWith("/cfg") ||
       pth.startsWith("/gvars") ||
       pth.startsWith("/appfsdel") ||
       pth.startsWith("/appfsget") ||
       pth.startsWith("/appfsput"))
      return false;

   // Root is handled by handleWebRequest()
   if (pth == "/")
      return false;

   // Must have a file extension
   int dot = pth.lastIndexOf('.');
   int slash = pth.lastIndexOf('/');

   if (dot <= slash)
      return false;

   String ext = pth.substring(dot + 1);
   ext.toLowerCase();

   // Don't let these go through the static handler
   if (ext == "ico" || ext == "gz")
      return false;

   return true;
}
#endif

void EspCoreClass::initWeb() {
   _fdpl("initWeb ...");   

#if _HAS_FS == 1   
   // server.serveStatic("<data Folder>", LittleFS, "<path>");

   // Serve files from LittleFS
   server.serveStatic("/", LittleFS, "/")
      .setFilter([this](AsyncWebServerRequest *req) {
        return this->isFSfile(req);
   });

   server.on("/appfsput/*", HTTP_PUT,
     [&](AsyncWebServerRequest *req) {
        _fdpl("APPFSPUT REQUEST", _INFO_MSG);
        //_fdpl("APPFSPUT URL = [" + req->url() + "]", _INFO_MSG);
        //_fdpl("APPFSPUT FILE = [" + appFSfile + "]", _INFO_MSG);
        //_fdpl("APPFSPUT DATA LEN = " + String(appFSdata.length()), _INFO_MSG);
        

        //String msg;
        //appPutFSfile(appFSfile, appFSdata, msg);

        //req->send(200, "text/plain", msg);

        appFSdata = "";
        appFSfile = req->url().substring(strlen("/appfsput/"));
     },
     NULL,
     [&](AsyncWebServerRequest *req, uint8_t *data, size_t len,
        size_t index, size_t total)
     {
        _fdpl("APPFSPUT BODY len=" + String(len) +
              " index=" + String(index) +
              " total=" + String(total),
              _INFO_MSG);

        //if (index == 0) {
        //   appFSdata = "";
        //   appFSfile = req->url().substring(strlen("/appfsput/"));
        //}

        appFSdata.concat((char *)data, len);

        // Last body chunk received
        if (index + len == total) {

           _fdpl("APPFSPUT COMPLETE file=[" +
              appFSfile + "] len=" +
              String(appFSdata.length()),
              _INFO_MSG);

           String msg;

           appPutFSfile(appFSfile, appFSdata, msg);

           _fdpl("APPFSPUT RESULT: " + msg, _INFO_MSG);

           req->send(200, "text/plain", msg);

           appFSdata = "";
           appFSfile = "";
        }
     }
   );
#endif

   server.on("*", HTTP_ANY, [&](AsyncWebServerRequest *req){
      handleWebRequest(req);
   });
   
   initWebSocket();   // your websocket init

   server.begin();
}

#if _HAS_RGB == 1
void EspCoreClass::initNeoPixel() {
   _fdpl("initNeoPixel ...");   
   pixels.begin();
   pixels.clear();
   pixels.show();
}
#endif

void EspCoreClass::initBounce() {
   _fdpl("initBounce ...");   
   debouncer.attach(_BOOT_BTN_PIN, INPUT_PULLUP);
   debouncer.interval(_BOOT_BTN_DB_TIME);
}

void EspCoreClass::initIrqTimer() {
   _fdpl("initIrqTimer ...");   
   irqTimer = timerBegin(0, _TMR_PRESCALE, true);   // prescale = 80
   timerAttachInterrupt(irqTimer, &irq, true);
   timerAlarmWrite(irqTimer, _TMR_PERIOD, true);    // 500000 ticks
   timerAlarmEnable(irqTimer);
   timerStart(irqTimer);
}

//long bx = 1000;
void EspCoreClass::begin() {
   //Serial.begin(115200);
   //while ((mqttDataQ == nullptr || mqttLogQ == nullptr) && bx > 0) {
   //   bx--;
   //   delay(10);
   //}
   //Serial.println("Core Begin delayed start by 10mS * " + String(bx));
   initEE();
   initVars();
   getBootCode();
   initGPIO();

   // initialize the watchdog timer if in LIVE mode
   if (vars.str.activeMode == _LIVE || vars.str.activeMode == _CFG) {
      _fdpl("initWDT ...");
      esp_task_wdt_init(_WDT_TIMEOUT, true);
      esp_task_wdt_add(NULL);      
 
      _fdpl("IPAddress ...",_DBG4_MSG);
      //  10, 213,  69,  13
      Local = IPAddress(  vars.ui8.LN1, vars.ui8.LN2, vars.ui8.LN3, vars.ui8.LND);  
      _fdpl("Local -> " + Local.toString(),_DBG4_MSG);
      //_fdpl("begin &Local(member): " + String((uint32_t)&this->Local, HEX),_DBG3_MSG);
      //_fdpl("begin &Local(local?): " + String((uint32_t)&Local, HEX),_DBG3_MSG);

   } else if (vars.str.activeMode == _OTA) {
      _fdpl("IPAddress ...",_DBG4_MSG);
      //  10, 213,  69, 254
      Ota = IPAddress(vars.ui8.LN1, vars.ui8.LN2, vars.ui8.LN3, vars.ui8.LNO);  
      //  10, 213,  69,  90
      Gateway = IPAddress(vars.ui8.LN1, vars.ui8.LN2, vars.ui8.LN3, vars.ui8.LNR);  
      //  10, 213,  69,  90)
      DNS = IPAddress(vars.ui8.LN1, vars.ui8.LN2, vars.ui8.LN3, vars.ui8.LNR);    
   }
   // 255, 255, 255,   0
   Subnet = IPAddress( vars.ui8.MK1, vars.ui8.MK2, vars.ui8.MK3, vars.ui8.MK4);  

   initWiFi();
   initMqtt(); 
   initWeb();
   initBounce();
#if _HAS_FS == 1
   LittleFS.begin();
#endif 

#if _HAS_RGB == 1
   initNeoPixel();
#endif 

   if (vars.str.activeMode == _LIVE || vars.str.activeMode == _CFG) {
      //_fdpl("begin init irq timer "  + vars.str.activeMode, _DBG4_MSG);
      initIrqTimer();

   } else if (vars.str.activeMode == _OTA) {
      initOTA();
   }

   //Serial.printf("begin() AFTER initSeri: ecc=%p mqttDataQ=%p mqttLogQ=%p app2=%p ecc2=%p\n",
   //         &ecc, ecc.mqttDataQ, ecc.mqttLogQ,
   //         ecc.app2eccQ, ecc.ecc2appQ);
   // SERIAL LOGGING
   if (vars.bl.logSer) {
      Serial.begin(115200);
      xTaskCreate(
         EspCoreClass::dqSerLogTaskThunk,
         "serLogTask",
         2048,
         this,
         1,
         NULL
      );
   } else {
      xQueueReset(serLogQ);   // turf early logs
      String x = qDepth(_SER_LOG_Q,"clr");                        
   }

   //Serial.printf("begin() AFTER serial task: ecc=%p mqttDataQ=%p mqttLogQ=%p app2=%p ecc2=%p\n",
   //         &ecc, ecc.mqttDataQ, ecc.mqttLogQ,
   //         ecc.app2eccQ, ecc.ecc2appQ);
   // MQTT
   xTaskCreate(
      EspCoreClass::dqMqttTaskThunk,
      "mqttTask",
      2048,
      this,
      2,
      NULL
   );  
   //Serial.printf("begin() AFTER mqtt task: ecc=%p mqttDataQ=%p mqttLogQ=%p app2=%p ecc2=%p\n",
   //         &ecc, ecc.mqttDataQ, ecc.mqttLogQ,
   //         ecc.app2eccQ, ecc.ecc2appQ);

   if (vars.str.activeMode != _CFG) {
      digitalWrite(vars.in.ledPin, LOW);  // turn off led (active low)
   }
}

void EspCoreClass::loop() {
   if (vars.str.activeMode == _CFG) {
  
      esp_task_wdt_reset();

      if (TockCount > 0) {      // 500mS Trigger time reached
         //_fdpl("loop_CFG ... Tock",_DBG4_MSG);
         vars.bl.wifiConnected =  (WiFi.status() == WL_CONNECTED);
         TockCount--;
      }

   } else if (vars.str.activeMode == _OTA) {

      // if handle detects an OTA handshake it takes over control of the ESP until completed
      // OTA reboots automatically as part of its upload process which automatically returns
      // to Live Mode
      ArduinoOTA.handle();
  
      TockCount--;
  
      // OTA Mode has a Tock of 100mS so TockCount = 5 provides for a 500mS Trigger 
      if (TockCount <= 0) {      // 500mS Trigger time reached
         _fdpl("loop_OTA ... Tock",_DBG4_MSG);

         vars.bl.wifiConnected =  (WiFi.status() == WL_CONNECTED);

         setLed(_TOGGLE);       // toggle the ESP status led to indicate OTA mode active
         vars.in.otaTime--;     // decrement the OTA timeout timer
         TockCount = 5;          // reset the counter for the next trigger time
         //sendUpdates();         // update the OTA web page timer value
         sendWebpageUpdates(); // update the OTA web page timer value
         // Check if the OTA timer has expired 
         if (vars.in.otaTime <= 0) {   
            setLed(_OFF);       // turn off the ESP status led
            // Set a System Command to Reboot the ESP (Cancels OTA Mode)
            vars.enm.sysCmd = _CMD_REBOOT; 
         }
      }

   } else if (vars.str.activeMode == _LIVE) {
      // kick the watchdog timer to prevent a reset
      esp_task_wdt_reset();

      // in Live Mode Tock time is 500mS set by IRQ triggered by Timer roll over
      // if a Tock is missed (TockCtr > 1) the next loop should compensate 
      if (TockCount > 0) {
         _fdpl("Loop Live Tock",_DBG4_MSG);
         vars.bl.wifiConnected =  (WiFi.status() == WL_CONNECTED);

         // under normal operation this should not grow past 1 meaning just sets back to zero
         TockCount--;
         if (bootBtnPressTime > 0) {
            bootBtnPressTime--;
            if (bootBtnPressTime == 0) {
               setLed(_ON);
            }
         }

         chkTestMode();
         //_fdpl("Loop after chkTestMode " + String(vars.tst.in.testTime),_DBG4_MSG);

         if (vars.tst.in.testTime > 0) {
            vars.tst.in.testTime--;        // decrement test timer
           
            //_fdpl(String("loop test mode ") + String(vars.tst.in.testTime) );
            sendWebpageUpdates(); // update the OTA web page timer value

            if (vars.tst.in.testTime <= 0) {
               //_fdpl(String("loop test mode <= 0 ") + String(vars.tst.in.testTime) );

               ledAction(_TEST_LED_OFF);
            } else {
               //_fdpl(String("loop test mode == 0 ") + String(vars.tst.in.testTime) );
               ledAction(_TEST_LED_TOGGLE);
            }
         }
      }   
   } // LIVE Mode    
    
   
   MqttState ms = vars.enm.mqttState;
   if (vars.bl.wifiConnected) {
      vars.enm.mqttState = (mqtt.connected() && ms >= _MQTT_PEND_CON) ? _MQTT_CON : ms;
      switch (vars.enm.mqttState) {
         case _MQTT_CON:
            if (mqtt.connected()) mqtt.loop();
            break;
         case _MQTT_INIT:
            initMqtt();
            break;  
         case _MQTT_RECON:
            mqtt.connect(vars.str.devName.c_str());
            if (mqtt.connected()) vars.enm.mqttState = _MQTT_CB;
            break;  
         case _MQTT_CB:
            if (vars.bl.appEnable) {
               initMqttCB();
            } else {
              vars.enm.mqttState = _MQTT_PEND_CON;
            }  
            break;  
      }
   } else {
      vars.enm.mqttState = (ms == _MQTT_INIT) ? _MQTT_INIT : _MQTT_RECON;
   }

/*
   if (vars.bl.wifiConnected) {
      bool mcs = false;
      if (vars.enm.mqttState == _MQTT_CON) {
         mqtt.loop();
      } else {   
         if (!vars.bl.mqttInitialized) {
            _fdpl("Loop MQTT initial connection", _INFO_MSG);
            mcs = initMqtt();
            vars.bl.mqttInitialized = mcs;
         } else {
            _fdpl("Loop MQTT disconnected attempting reconnect", _INFO_MSG);
            mcs = mqtt.connect(vars.str.devName.c_str());
         }
         _fdpl("Loop is MQTT connected? " + String(mcs? "T" : "F") + " "
             + String(mqtt.connected()? "T" : "F"), _INFO_MSG);
         if (mcs && mqtt.connected()){
            _fdpl("Loop request MQTT RECON");
            vars.enm.mqttState = _MQTT_RECON;
            //initMqttSubCB();
         }   
      }
   }

   if (vars.enm.mqttState == _MQTT_RECON) {
      _fdpl("Loop request MQTT RECALL");
      vars.enm.mqttState = _MQTT_RECALL;
      initMqttSubCB();
   }

   if (vars.enm.mqttState == _MQTT_CON) {
      if (!mqtt.connected()) {
         _fdpl("loop Mqtt connection LOST -> MQTT MIA");
         vars.enm.mqttState = _MQTT_MIA;
      }
   }
*/
   debouncer.update();

   if (debouncer.fell()) {
      // Button pressed
      if (vars.str.activeMode == _LIVE) {
         _fdpl("Loop Live button pressed", _INFO_MSG);   
         bootBtnPressTime = vars.in.bootBtnCfgT;
      }   
   }

   if (debouncer.rose()) {
      _fdpl("Loop button released", _INFO_MSG);   
 
      if (bootBtnPressTime == 0) {
         _fdpl("Loop Live Long button released", _INFO_MSG);   
         bootBtnPressTime = -1;
         // Long press from LIVE mode -> CFG
         vars.enm.sysCmd = _CMD_CFG;
        
      } else {
         _fdpl("Loop Live short button released", _DBG4_MSG);   
  
         // Short press
         if (vars.str.activeMode == _LIVE) {
            _fdpl("Loop Live short button released -> OTA", _DBG4_MSG);   

            // LIVE -> OTA
            vars.enm.sysCmd = _CMD_OTA;

         } else {
            _fdpl("Loop OTA or CFG short button released -> LIVE", _DBG4_MSG);   

            // OTA or CFG -> LIVE
            vars.enm.sysCmd = _CMD_REBOOT;
         }
      }
   }

   // process any sysCmds
   if (vars.enm.sysCmd != _CMD_NONE) {
        
      // some Commands can be completed in one loop ... nxtCmd provides for multi loop cmds
      SysCmd Cmd      = vars.enm.sysCmd;
      SysCmd nxtCmd   = _CMD_NONE;
      vars.enm.sysCmd = nxtCmd;

      // parse which command was requested
      if (Cmd != _CMD_NONE){
         switch (Cmd) {
            case _CMD_TEST_ON:
               _fdpl("Loop sysCmd _CMD_TEST_ON",_INFO_MSG);
               testMode(_ON);
               break;
            case _CMD_TEST_OFF:
               _fdpl("Loop sysCmd _CMD_TEST_OFF",_INFO_MSG);
               testMode(_OFF);
               break;
            case _CMD_TEST_TOGGLE:
               _fdpl("Loop sysCmd _CMD_TEST_TOGGLE",_INFO_MSG);
               testMode(_TOGGLE);
               break;
            case _CMD_LOG_SER_ON:
               _fdpl("Loop sysCmd _CMD_LOG_SER_ON",_INFO_MSG);
               setEEVar("LogSer","on");
               break;
            case _CMD_LOG_SER_OFF:  
               _fdpl("Loop sysCmd _CMD_LOG_SER_OFF",_INFO_MSG);
               setEEVar("LogSer","off");
               break;    
            case _CMD_LOG_MQTT_ON:
               _fdpl("Loop sysCmd _CMD_LOG_MQTT_ON",_INFO_MSG);
               setEEVar("LogMqtt","on");
               break;
            case _CMD_LOG_MQTT_OFF:  
               _fdpl("Loop sysCmd _CMD_LOG_MQTT_OFF",_INFO_MSG);
               setEEVar("LogMqtt","off");
               break;    
            case _CMD_OTA:
               // delay time to allow for any web or endpoint data
               delay(300);
               // when current mode is LIVE Set MODE to CFG and REBOOT
               if (vars.str.activeMode == _LIVE ) {
                  _fdpl("Loop sysCmd _CMD_OTA set MODE OTA",_INFO_MSG);
                  setEEVar("Mode",_OTA);
               }
               if (vars.str.activeMode == _OTA || vars.str.activeMode == _LIVE) {
                  // Set next command loop to reboot
                  nxtCmd = _CMD_REBOOT;  
               }
               break;
            case _CMD_CFG:
               // delay time to allow for any web or endpoint data
               delay(300);
               // when current mode is LIVE Set MODE to CFG and REBOOT
               if (vars.str.activeMode == _LIVE ) {
                  _fdpl("Loop sysCmd _CMD_CFG set MODE CFG",_INFO_MSG);
                  setEEVar("Mode",_CFG);
               }
               if (vars.str.activeMode == _CFG || vars.str.activeMode == _LIVE) {
                  // Set next command loop to reboot
                  nxtCmd = _CMD_REBOOT;  
               }
               break;
            case _CMD_REBOOT:
               _fdpl("Loop sysCmd _CMD_REBOOT",_INFO_MSG);
               // delay time to allow for any web or endpoint data
               delay(300);
               // reset the ESP immediately
               ESP.restart();
               break;
            case _CMD_FACTORY:
               _fdpl("Loop sysCmd _CMD_FACTORY",_INFO_MSG);
               // delay time to allow for any web or endpoint data
               delay(300);
               // Clear all NVM Persistent GVars Memory
               factoryReset();
               // Set next command loop to reboot
               nxtCmd = _CMD_REBOOT;  
               break;
            default:
            break;    
         }
         vars.enm.sysCmd = nxtCmd;
      }
   }

   // if the system command is empty then get the next q'd cmd      
   if (vars.enm.sysCmd == _CMD_NONE) {
      //_fdpl("Loop symCmd Q check ", _DBG4_MSG);
      SysCmd queuedCmd;

      if (xQueueReceive(sysCmdQ, &queuedCmd, 0) == pdPASS) {
         String x = qDepth(_SYS_Q,"dec");
         vars.enm.sysCmd = queuedCmd;
         _fdpl("Loop dQsymCmd " + String(vars.enm.sysCmd), _DBG4_MSG);
      }
   }
  
   // slow down Live Mode loop iteration
   // provides the 1/5 tock time in OTA Mode 
   delay(100); 
}
