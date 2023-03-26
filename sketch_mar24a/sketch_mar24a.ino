#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>


byte data[33];
byte PM25, PM10, HCHO, TVOC, CO2[2], TEMP, RH;

// Default WiFi credentials for AP mode
const char *defSSID = "MyESP8266AP";
const char *default_password = "password";

// WiFi credentials
char ssid[32];
char password[64];

// Addresses for storing WiFi credentials in EEPROM
const int ssid_address = 0;
const int password_address = 32;

// Web server on port 80
ESP8266WebServer server(80);

// OTA update server on port 81
ESP8266HTTPUpdateServer httpUpdater;

// Number of attempts to connect to stored WiFi credentials
const int wifi_attempts = 5;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  // Load WiFi credentials from EEPROM
  EEPROM.get(ssid_address, ssid);
  EEPROM.get(password_address, password);

  // Start WiFi with stored credentials, or start AP mode with default credentials
  if (strlen(ssid) > 0 && strlen(password) > 0) {
    connectWiFi(wifi_attempts);
  } else {
    startAP();
  }

  // Start mDNS responder
  if (MDNS.begin("airmaster")) {
    Serial.println("mDNS responder started");
  }

  // Set up OTA update server
  httpUpdater.setup(&server, "/update");

  // Set up web server paths
  server.on("/", handleRoot);
  server.on("/wifi", handleWifi);
  server.on("/reset", handleReset);
  server.onNotFound(handleNotFound);




  // Start web server
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  // Wait for the start of a new packet (indicated by 0xFF 0xFF)
  while (Serial.available() < 2 || Serial.read() != 0xFF || Serial.read() != 0xFF);

  // Read the rest of the packet into the data array
  Serial.readBytes(data, 33);

  // Extract data bytes and store in variables
  PM25 = data[19-1];
  PM10 = data[21-1];
  HCHO = data[23-1];
  TVOC = data[25-1];
  CO2[0] = data[26-1];
  CO2[1] = data[27-1];
  TEMP = data[28-1];
  RH = data[30-1];
/*
  // Print the variable values to the serial monitor
  Serial.print("PM2.5: ");
  Serial.println(PM25);
  Serial.print("PM10: ");
  Serial.println(PM10);
  Serial.print("HCHO: ");
  Serial.println(HCHO);
  Serial.print("TVOC: ");
  Serial.println(TVOC);
  Serial.print("CO2: ");
  Serial.println((CO2[0] << 8) | CO2[1]);
  Serial.print("TEMP: ");
  Serial.println(TEMP);
  Serial.print("RH: ");
  Serial.println(RH);
*/
}

// Connect to WiFi with stored credentials
void connectWiFi(int attempts) {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && attempts-- > 0) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi");
    // Clear stored credentials and start AP mode
    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));
    EEPROM.put(ssid_address, "");
    EEPROM.put(password_address, "");
    EEPROM.commit();
    startAP();
  } else {
    Serial.println("Connected to WiFi");
  }
}

// Start AP mode with default credentials
void startAP() {
  Serial.println("Starting AP mode...");
  WiFi.softAP(defSSID, default_password);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apIP);
}

// Handle requests to the root path
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Hello from ESP8266!v3</h1>";
  html += "<p>"+String((CO2[0] <<8) |(CO2[1]),DEC)+"</p>";
  html += "<p>Temp: "+String(TEMP, HEX)+"</p>";
    html += "<p>Temp: "+String(TEMP, DEC)+"</p>";
  html += "<p>PM2.5: "+String(PM25)+"</p>";
  html += "<p>PM10: "+String(PM10)+"</p>";
  html += "<p><a href=\"/wifi\">WiFi Settings</a></p>";
  html +="<p>18 -> "+String(data[18])+" | "+String(data[19])+" | "+String(data[20])+" | "+String(data[21])+" | "+String(data[22])+" | "+String(data[23])+" | "+String(data[24])+" | "+"</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Handle requests to the /wifi path
void handleWifi() {
  if (server.method() == HTTP_POST) {
    // Update WiFi credentials
    String ssid_new = server.arg("ssid");
    String password_new = server.arg("password");
    if (ssid_new.length() > 0 && password_new.length() > 0) {
  Serial.println("Updating WiFi credentials...");
  memset(ssid, 0, sizeof(ssid));
  memset(password, 0, sizeof(password));
  ssid_new.toCharArray(ssid, sizeof(ssid));
  password_new.toCharArray(password, sizeof(password));
  EEPROM.put(ssid_address, ssid);
  EEPROM.put(password_address, password);
  EEPROM.commit();
  connectWiFi(wifi_attempts);
  server.send(200, "text/plain", "WiFi credentials updated");
} else {
  server.send(400, "text/plain", "Invalid request");
}
} else {
// Show WiFi settings form
String html = "<html><body>";
html += "<h1>WiFi Settings</h1>";
html += "<form method=\"POST\" action=\"/wifi\">";
html += "<p>SSID: <input type=\"text\" name=\"ssid\"></p>";
html += "<p>Password: <input type=\"password\" name=\"password\"></p>";
html += "<p><input type=\"submit\" value=\"Save\"></p>";
html += "</form>";
html += "</body></html>";
server.send(200, "text/html", html);
}
}

// Handle requests to the /reset path
void handleReset() {
// Clear stored WiFi credentials and start AP mode
memset(ssid, 0, sizeof(ssid));
memset(password, 0, sizeof(password));
EEPROM.put(ssid_address, "");
EEPROM.put(password_address, "");
EEPROM.commit();
startAP();
server.send(200, "text/plain", "WiFi credentials reset");
}

// Handle 404 not found errors
void handleNotFound() {
String html = "<html><body>";
html += "<h1>404 Not Found</h1>";
html += "<p>The requested URL " + server.uri() + " was not found on this server.</p>";
html += "</body></html>";
server.send(404, "text/html", html);
}
