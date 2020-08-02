const char *settingsHTML = (char *)"/settings.html";
const char *stylesCSS = (char *)"/styles.css";
const char *mainJS = (char *)"/main.js";

struct Config {
  int randomNumber;
  bool enabled;
} config;

struct Metadata {
    int8_t version;
} meta;

#include "ConfigManager.h"
ConfigManager configManager;

//
// Lets serve something with a different Library!!!
// This library is only for ESP32
// This library has naming conflicts with <Webserver.h>
//
#if defined(ARDUINO_ARCH_ESP32)
namespace HTTP {
  #include "esp_http_server.h"
}
HTTP::httpd_handle_t h_server = NULL;
#endif

void customHTTP(int port=80);

void customWifi() {
    Serial.println("Setting up custom Wifi");
    WiFi.mode(WIFI_AP);

    const char* ssid = "CustomHTTPDemo";
    WiFi.softAP(ssid);

    // Need to wait to get IP
    delay(500);

    IPAddress ip(192, 168, 10, 1);
    IPAddress NMask(255, 255, 255, 0);
    WiFi.softAPConfig(ip, ip, NMask);
    Serial.print("SSID: ");
    Serial.println(ssid);
}

void setup() {
    Serial.begin(115200);
    DEBUG_MODE = true;

    /**********
    To toggle using ConfigManagers built in
    WiFi Setup or HTTP Server, toggle these values.

    When using HTTP and no WIFI, it will default to
    API Mode, and serve all pages defined.
    ***********/
    bool cmWIFI = true;
    bool cmHTTP = true;

    if (!cmWIFI) configManager.disableWIFI();
    if (!cmHTTP) configManager.disableHTTP();

    meta.version = 3;

    // Settings
    configManager.addParameter("randomNumber", &config.randomNumber, get);
    configManager.addParameter("enabled", &config.enabled);

    // Meta Settings
    configManager.addParameter("version", &meta.version, get);

    if(cmWIFI) {
        Serial.println("Setting up ConfigManager Wifi");
        configManager.setAPName("CM-Demo");
    } else customWifi();

    if (cmHTTP) {
        Serial.println("Setting up ConfigManager configApi");
        configManager.setAPFilename("/index.html");
        configManager.setAPCallback(APCallback);
        configManager.setAPICallback(APICallback);
    } else customHTTP();

    configManager.begin(config);
}


void loop() {
    configManager.loop();

    config.randomNumber = random(300);
    configManager.save();

    // Add your loop code here
}


void APCallback(WebServer *server) {
    server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
        configManager.streamFile(stylesCSS, mimeCSS);
    });

    DebugPrintln(F("AP Mode Enabled. You can call other functions that should run after a mode is enabled ... "));
}


void APICallback(WebServer *server) {
  server->on("/disconnect", HTTPMethod::HTTP_GET, [server](){
    configManager.clearWifiSettings(false);
  });

  server->on("/config", HTTPMethod::HTTP_GET, [server](){
    configManager.streamFile(settingsHTML, mimeHTML);
  });

  // NOTE: css/js can be embedded in a single page HTML
  server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
    configManager.streamFile(stylesCSS, mimeCSS);
  });

  server->on("/main.js", HTTPMethod::HTTP_GET, [server](){
    configManager.streamFile(mainJS, mimeJS);
  });

}

#if defined(ARDUINO_ARCH_ESP32)
static esp_err_t indexHandler(HTTP::httpd_req_t *req) {
    Serial.println("/");
    String resp = "Stored Config randomNumber: " + (String) config.randomNumber;
    return HTTP::httpd_resp_send(req, resp.c_str(), strlen(resp.c_str()));
}
void registerIndex() {
    HTTP::httpd_uri_t uri = {
    .uri       = "/",
    .method    = HTTP::HTTP_GET,
    .handler   = indexHandler,
    .user_ctx  = NULL
  };

  HTTP::httpd_register_uri_handler(h_server, &uri);
}

static esp_err_t resetHandler(HTTP::httpd_req_t *req) {
    Serial.println("/reboot");
    const char resp[] = "Rebooting";
    HTTP::httpd_resp_send(req, resp, strlen(resp));
    ESP.restart();
    return ESP_OK;
}
void registerReboot() {
    HTTP::httpd_uri_t uri = {
    .uri       = "/reboot",
    .method    = HTTP::HTTP_GET,
    .handler   = resetHandler,
    .user_ctx  = NULL
  };

  HTTP::httpd_register_uri_handler(h_server, &uri);
}

static esp_err_t scanHandler(HTTP::httpd_req_t *req) {
    String resp = configManager.scanNetworks();
    return HTTP::httpd_resp_send(req, resp.c_str(), strlen(resp.c_str()));
}
void registerScan() {
    HTTP::httpd_uri_t uri = {
    .uri       = "/scanner",
    .method    = HTTP::HTTP_GET,
    .handler   = scanHandler,
    .user_ctx  = NULL
  };

  HTTP::httpd_register_uri_handler(h_server, &uri);
}

void customHTTP(int port) {
    Serial.println("Setting up custom HTTP");
    HTTP::httpd_config_t h_config = HTTPD_DEFAULT_CONFIG();
    h_config.server_port = port;
    HTTP::httpd_start(&h_server, &h_config);

    registerIndex();
    registerScan();
    registerReboot();

    Serial.print("Ready at: http://");
    Serial.println(WiFi.softAPIP());
}
#else
void customHTTP(int port) {
    Serial.println("No HTTP Server defined for ESP8266");
}
#endif

