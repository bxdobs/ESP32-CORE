// espcore.h
#ifndef ESPCORE_H
#define ESPCORE_H

#include "include.h"

#include "includes.h"

// espcore webpages"
#include "espcore_live_page.h"
#include "espcore_ota_page.h"
#include "espcore_cfg_page.h"

// Declarations
#define _LIVE            "LIVE"          // Mode LIVE
#define _OTA              "OTA"          // Mode OTA
#define _CFG              "CFG"          // Mode CFG
#define _WDT              "WDT"          // Live mode Boot State
#define _ON                true
#define _OFF              false
#define _RO                true          // Read Only Preferences 
#define _RW               false          // Read/Write Preferences
#define _PUBLIC            true          // used in eeVarMap with isPublic
#define _PRIVATE          false          // used in eeVarMap with isPublic
#define _SYSLOG            true          // used with getInfo()
#define _TOGGLE              -2
#define _EEVARS         "EEVars"

class EspCoreClass {
public:
    // ---- PUBLIC MEMBER VARIABLES ----
    Preferences    eevars;
    AsyncWebServer server{_AWS_PORT};
    AsyncWebSocket ws{"/ws"};
    WiFiClient     wifiClient;
    PubSubClient   mqtt{wifiClient};
    Bounce         debouncer;
    QueueHandle_t  serLogQ;
    QueueHandle_t  mqttLogQ;
    QueueHandle_t  mqttDataQ;
    QueueHandle_t  sysCmdQ;
    QueueHandle_t  app2eccQ;
    QueueHandle_t  ecc2appQ;
    String         appFSdata;
    String         appFSfile;

#if _HAS_RGB == 1
    Adafruit_NeoPixel pixels{_RGB_PIXELS, _RGB_PIN, NEO_GRB + NEO_KHZ800};
#endif

    IPAddress Local, Ota, Gateway, DNS, Subnet;
    hw_timer_t* irqTimer = nullptr;

    // ---- ENUMS ----
    enum MqttState {_MQTT_INIT, _MQTT_RECON, _MQTT_CB, _MQTT_RESUB, _MQTT_PEND_CON, _MQTT_CON}; 
    enum qDpth  { _SYS_Q, _SER_LOG_Q, _MQTT_LOG_Q, _MQTT_Q, _APP_Q, _ECC_Q, _QS };
    enum SysCmd { _CMD_NONE, _CMD_OTA, _CMD_CFG, _CMD_REBOOT, _CMD_FACTORY,
                  _CMD_LOG_SER_ON, _CMD_LOG_SER_OFF, _CMD_LOG_MQTT_ON, _CMD_LOG_MQTT_OFF,
                  _CMD_TEST, _CMD_TEST_ON, _CMD_TEST_OFF, _CMD_TEST_TOGGLE };

    enum TestModeChange { _NO_CHG, _MODE_ON, _MODE_OFF, _MODE_TOGGLE };
    enum LED_ACTIONS { _LED_OFF, _LED_ON, _TEST_LED_TOGGLE, _TEST_LED_ON, _TEST_LED_OFF };

    enum GVarType { _GV_NUL, _GV_INT, _GV_STR, _GV_BYTE_HEX, _GV_BYTE_DEC, _GV_BYTES_32,
                    _GV_UINT32, _GV_BOOL, _GV_BOOL_YN, _GV_BOOL_TF, _GV_BOOL_ON_OFF };

#if _HAS_RGB == 1
    enum rgbColour : uint8_t { _BLACK, _RED, _YELLOW, _GREEN, _BLUE, _WHITE, _MAGENTA };
#endif

    // ---- STRUCTS ----
    struct EndPoint {
        bool   isJson = false;
        String cmd    = "/";
        String action = "";
        String gvar   = "";
        String gval   = "";
    };

    struct LogMsg {
        char msg[_LOG_MSG_MAXLEN];
        bool addNewline;
    };

    struct MqttMsg {
        bool    isLog = true;
        char    topic[_MQTT_TOPIC_MAXLEN];
        char    payload[_MQTT_PAYLOAD_MAXLEN];
        bool    addNewline;
    };

    struct MqttLogMsg {
        bool    isLog = true;
        char    topic[_MQTT_TOPIC_MAXLEN];
        char    payload[_MQTT_PAYLOAD_MAXLEN];
        bool    addNewline;
    };

    struct GVarInfo {
        String key;
        int    idx;
        String val;
    };

    struct GVarMap {
        int        idx;
        const char *key;
        void       *var;
        GVarType   gVarType;
        bool       isPublic;
        String     defVal;

        //GVarMap(int I, const char* K, void* V, GVarType T, bool P = true, String D = String());
    };

    static GVarMap gVarTable[];
    static const int gVarCount;

    int bootBtnPressTime = -1;

    typedef union {
        uint8_t lev;
        struct {
            bool Info : 1;
            bool Err  : 1;
            bool Wrn  : 1;
            bool Misc : 1;
            bool Dbg1 : 1;
            bool Dbg2 : 1;
            bool Dbg3 : 1;
            bool Dbg4 : 1;
        } bl;
    } LOG_LEV;

    struct Variables {
      struct {
         struct { 
            struct {
              int qdp[_QS] = {};
              int qmx[_QS] = {};
            };
         } in;
      } q;
      struct {
         struct {
            TestModeChange chgMode = _NO_CHG;
         } enm;

         struct {
            bool testMode          =  _OFF;
         } bl;

         struct {
            int  test_to           = _TEST_TIMEOUT; // time in minutes
            int  testTime          = 0;          // this is number of 500mS (toks)
         } in;

      } tst;

      struct {
         bool unknown           = false;
         bool wifiConnected     = false;
         bool appEnable         = _APP_ENABLE;
         bool appBusy           = false;
         bool logSer            = _LOG_SER;
         bool logMqtt           = _LOG_MQTT;
         bool sysLed            = _OFF;
         bool currLed           = _OFF;

#if _HAS_RGB == 1
         bool rgbEnabled        =  _RGB_ENABLED;
#endif         
      } bl;

      struct {
         int  ota_to            = _OTA_TIMEOUT;
         int  otaTime           = 0;
         int  wdt_to            = _WDT_TIMEOUT;
         int  iRSSI             = -99;
         int  bootBtnDbT        = _BOOT_BTN_DB_TIME;
         int  bootBtnCfgT       = _BOOT_BTN_CFG_TIME;
         int  bootBtnPin        = _BOOT_BTN_PIN;
         int  ledPin            = _LED_PIN;
      } in;

      struct {
         String  activeMode     = _LIVE;
         String  bootState      = "Factory";
         String  appBtn         = _APP_BTN;
         String  appTitle       = _APP_TITLE;
         String  appHtml        = _APP_HTML;
         String  appMqttTopic   = _APP_TOPIC;
         String  devID          = ""; 
         String  devName        = _DEV_NAME;
         String  author         = _AUTHOR;
         String  appVer         = _APP_VER;
         String  appVerDate     = _APP_VER_DATE;
         String  coreVer        = _CORE_VER;
         String  coreVerDate    = _CORE_VER_DATE;
         String  wSSID          = _WSSID;
         String  wABC           = _WABC;
         String  bootCode       = "";
      } str;

      struct {
         LOG_LEV log = {
            .lev = (_INFO_MSG | _ERR_MSG | _WRN_MSG | _MISC_MSG)
               //| _DBG1_MSG | _DBG2_MSG | _DBG3_MSG | _DBG4_MSG)
         };
         uint8_t LN1            = _LN1;         // ip 1 device
         uint8_t LN2            = _LN2;         // ip 2 device 
         uint8_t LN3            = _LN3;         // ip 3 device 
         uint8_t LND            = _LND;         // ip 4 device 
         uint8_t LNM            = _LNM;         // ip 4 MQTT broker
         uint8_t LNO            = _LNO;         // ip 4 OTA
         uint8_t LNR            = _LNR;         // ip 4 Router
         uint8_t MK1            = _MK1;         // mask 1 
         uint8_t MK2            = _MK2;         // mask 2 
         uint8_t MK3            = _MK3;         // mask 3
         uint8_t MK4            = _MK4;         // mask 4
         uint8_t ABC[32];

#if _HAS_RGB == 1
         uint8_t currRGB        =  _BLUE;
#endif         
      } ui8;

      struct {
         SysCmd    sysCmd       = _CMD_NONE;
         MqttState mqttState    = _MQTT_INIT;
      } enm;
    } vars;

    // ---- GVAR LIST MACRO ----
    #define GVAR_LIST(X) \
      X(unknown,     "unknown",     bl.unknown,      _GV_BOOL,     _PRIVATE ) \
      X(BootState,   "BootState",   str.bootState,   _GV_STR,      _PRIVATE ) \
      X(Mode,        "Mode",        str.activeMode,  _GV_STR,      _PRIVATE ) \
      X(devID,       "devID",       str.devID,       _GV_STR,      _PRIVATE ) \
      X(devName,     "devName",     str.devName,     _GV_STR,      _PUBLIC ) \
      X(author,      "author",      str.author,      _GV_STR,      _PUBLIC ) \
      X(ver,         "ver",         str.appVer,      _GV_STR,      _PUBLIC ) \
      X(verDate,     "verDate",     str.appVerDate,  _GV_STR,      _PUBLIC ) \
      X(wSSID,       "wSSID",       str.wSSID,       _GV_STR,      _PUBLIC ) \
      X(wABC,        "wABC",        str.wABC,        _GV_STR,      _PUBLIC ) \
      X(appTitle,    "appTitle",    str.appTitle,    _GV_STR,      _PUBLIC ) \
      X(appBtn,      "appBtn",      str.appBtn,      _GV_STR,      _PUBLIC ) \
      X(appHtml,     "appHtml",     str.appHtml,     _GV_STR,      _PUBLIC ) \
      X(appEnable,   "appEnable",   bl.appEnable,    _GV_BOOL_TF,  _PUBLIC ) \
      X(logSer,      "logSer",      bl.logSer,       _GV_BOOL_YN,  _PUBLIC ) \
      X(logMqtt,     "logMqtt",     bl.logMqtt,      _GV_BOOL_YN,  _PUBLIC ) \
      X(ota_to,      "ota_to",      in.ota_to,       _GV_INT,      _PUBLIC ) \
      X(wdt_to,      "wdt_to",      in.wdt_to,       _GV_INT,      _PUBLIC ) \
      X(ledPin,      "ledPin",      in.ledPin,       _GV_INT,      _PUBLIC ) \
      X(bootBtnPin,  "bootBtnPin",  in.bootBtnPin,   _GV_INT,      _PUBLIC ) \
      X(bootBtnDbT,  "bootBtnDbT",  in.bootBtnDbT,   _GV_INT,      _PUBLIC ) \
      X(bootBtnCfgT, "bootBtnCfgT", in.bootBtnCfgT,  _GV_INT,      _PUBLIC ) \
      X(test_to,     "test_to",     tst.in.test_to,  _GV_INT,      _PUBLIC ) \
      X(LN1,         "LN1",         ui8.LN1,         _GV_BYTE_DEC, _PUBLIC ) \
      X(LN2,         "LN2",         ui8.LN2,         _GV_BYTE_DEC, _PUBLIC ) \
      X(LN3,         "LN3",         ui8.LN3,         _GV_BYTE_DEC, _PUBLIC ) \
      X(LND,         "LND",         ui8.LND,         _GV_BYTE_DEC, _PUBLIC ) \
      X(LNM,         "LNM",         ui8.LNM,         _GV_BYTE_DEC, _PUBLIC ) \
      X(LNO,         "LNO",         ui8.LNO,         _GV_BYTE_DEC, _PUBLIC ) \
      X(LNR,         "LNR",         ui8.LNR,         _GV_BYTE_DEC, _PUBLIC ) \
      X(MK1,         "MK1",         ui8.MK1,         _GV_BYTE_DEC, _PUBLIC ) \
      X(MK2,         "MK2",         ui8.MK2,         _GV_BYTE_DEC, _PUBLIC ) \
      X(MK3,         "MK3",         ui8.MK3,         _GV_BYTE_DEC, _PUBLIC ) \
      X(MK4,         "MK4",         ui8.MK4,         _GV_BYTE_DEC, _PUBLIC ) \
      X(logLev,      "logLev",      ui8.log.lev,     _GV_BYTE_HEX, _PUBLIC ) \
      X(ABC,         "ABC",         ui8.ABC,         _GV_BYTES_32, _PRIVATE)

    enum GVarIdx {
#define X(name, key, var, type, pub) I_##name,
        GVAR_LIST(X)
#undef X
    };

    // ---- CONSTRUCTOR ----
    EspCoreClass();

    // ---- PUBLIC API (only what should be public) ----
    void begin();
    void loop();

    String qDepth(qDpth q, String req = "");
    String getQdepth(void);

    void mqttCallback(char* topic, byte* payload, unsigned int length);
    bool setSysCmd(SysCmd cmd);

    // Web
    void handleWebRequest(AsyncWebServerRequest *req);
    void sendResponse(AsyncWebServerRequest* req, const String& msg, bool isJson, bool isError = false);
    void sendWebpageUpdates(String req = String(), JsonObject obj = JsonObject());
    EndPoint parseEPS(const String &epsRaw);

    // WebSocket
    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                   AwsEventType type, void *arg, uint8_t *data, size_t len);

#if _HAS_FS == 1
    void appGetFSfile(const String &f, String &msg);
    void appPutFSfile(const String &f, String &data, String &msg);
    void appDelFSfile(const String &f, String &msg);
    void lsFSfldr(File &d, const String &path, const String &searchString, String &msg);
    void lsFS(String searchString, String &msg);
#endif

    void qLogMsg(const String& msg, bool addNewline, uint8_t msgLev = _SYS_MSG);
    void qMqttMsg(const String& topic, const String& payload);

#define _fdpl(msg, ...)  qLogMsg(msg, true,  ##__VA_ARGS__)
#define _fdp(msg, ...)   qLogMsg(msg, false, ##__VA_ARGS__)

    void ledAction(LED_ACTIONS action = _LED_OFF);
    void setLed(int State);

    String getHMS(long tm);
    String byte2Hex(uint8_t val);
    uint8_t hex2Byte(const String &val);
    bool parseBool(const String &val);
    String pad(const String &val, int size, char padChar=' ', char type='L');
    String pad(int val, int size, char padChar=' ', char type='L');

    // app helpers
    void setAppMqttTopic(const String &topic); 
    void setAppHtml(const String &html); 
    std::function<bool(JsonDocument&)> appWsHandler;
    //std::function<bool(const EndPoint&)> appHttpHandler;
    std::function<String(const EndPoint&)> appEndPointExtension;
    std::function<bool(const MqttMsg&)> appMqttHandler;
 
private:
    // ---- PRIVATE TASKS ----
    // ---- STATIC TASK THUNKS ----
    static void dqSerLogTaskThunk(void* param);
    static void dqMqttTaskThunk(void* param);

    void dqSerLogTask(void* param);
    void dqMqttTask(void* param);

    // ---- PRIVATE INIT FUNCTIONS ----
    void initGPIO();
    void initWiFi();
    void initOTA();
    void initMqttCB();
    bool initMqtt();
    bool isFSfile(AsyncWebServerRequest *req);
    void initWebSocket();
    void initWeb();
    void initBounce();
    void initIrqTimer();
#if _HAS_RGB == 1
    void initNeoPixel();
#endif

    // ---- PRIVATE UTILITY FUNCTIONS ----
    void getBootCode();
    const char* ip2str(uint8_t l1, uint8_t l2, uint8_t l3, uint8_t l4);
    void initABC(const char* iStr, uint8_t oByte[32]);

#if _HAS_RGB == 1
    String getRGBname(uint8_t cl);
    uint32_t getRGBval(uint8_t cl);
    void setRGBLed(uint8_t CL);
#endif

    // EE / GVar
    void factoryReset();
    void initEE();
    void initEEVars();
    void initVars();
    bool hasEEVar(String key);
    bool setEEVar(String key, String val);
    String getEEVar(String key);
    String getEEVars();
    bool delEEVar(String key);

    int nextIdx(const String &eps, int strt_idx, const String &dl, int ln);
    int findGVarIdx(const String &key);
    bool setGVar(const String &key, String val);
    String getGVar(const String &key);
    String getGVars();
    bool updateGVar(const String &key, String val);
    String getInfo(bool sysLog = false);

    // Test mode
    void chkTestMode();
    void testMode(int State);
};

// ---- GLOBAL INSTANCE ----
extern EspCoreClass ecc;

#endif // ESP_CORE_H
