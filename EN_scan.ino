#include <DNSServer.h>
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;

#include <WiFi.h>
#include <WiFiAP.h>

const char *ssid = "xxxx";
const char *password = "xxxx";

#include <Arduino.h>
#include <BLEAddress.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>

#include <algorithm>
#include <list>

int scanTime = 19; // In seconds
BLEScan *pBLEScan;
BLEAddress *exposureDevice[255];

#include <thread>

std::thread *scan_loop_thread;
std::thread *update_display_thread;

#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1306_128X32_UNIVISION_1_SW_I2C
    u8g2(U8G2_R0, /* clock=*/4, /* data=*/5,
         /* reset=*/U8X8_PIN_NONE); // Adafruit Feather M0 Basic Proto +
                                    // FeatherWing OLED

class MyENDevice {
public:
  String address_;
  int timestamp_;
  int rssi_;
  MyENDevice();
  MyENDevice(String address, int rssi) {
    address_ = address;
    rssi_ = rssi;
    timestamp_ = millis();
  }
  String address() { return address_; }
  int timestamp() { return timestamp_; }
  int rssi() { return rssi_; }
  String toString() {
    char str[100];
    sprintf(str, "%d,%s,%d", timestamp_, address_.c_str(), rssi_);
    return str;
  }
};

std::list<MyENDevice> currentDevices;
std::list<MyENDevice> latestDevices;
int intFoundDevices;

int lastIntFoundDevices[100]{0};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID()) {
      BLEUUID service = advertisedDevice.getServiceUUID();
      if (service.equals(BLEUUID((uint16_t)0xfd6f))) {
        Serial.printf("Exposure Notification from: %s\n",
                      advertisedDevice.getAddress().toString().c_str());
        currentDevices.push_front(
            MyENDevice(advertisedDevice.getAddress().toString().c_str(),
                       advertisedDevice.getRSSI()));
      }
    }
  }
};

void update_display() {
  while (true) {
    u8g2.firstPage();
    do {
      drawAddress();
      drawDiagram(lastIntFoundDevices,
                  *std::max_element(std::begin(lastIntFoundDevices),
                                    std::end(lastIntFoundDevices)),
                  sizeof(lastIntFoundDevices) / sizeof(int));
    } while (u8g2.nextPage());
    // std::rotate(latestDevices.begin(), std::next(latestDevices.begin()),
    // latestDevices.end());
    latestDevices.splice(latestDevices.end(), latestDevices,
                         latestDevices.begin());
    delay(2000);
  }
}

void drawAddress(void) {
  char str[100];
  u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
  if (WiFi.status() == WL_CONNECTED) {
    u8g2.drawStr(0, 32, "\x50"); // wifi: \x50 exclamation: \x47 bluetooth: \x4a
  } else {
    u8g2.drawStr(0, 32, "\x47");
  }

  u8g2.setFont(u8g2_font_profont10_tf);

  if (latestDevices.size() > 0) {
    sprintf(str, "%s%s%d%s", latestDevices.front().address().c_str(), " ",
            latestDevices.front().rssi(), "dBM");
  } else {
    sprintf(str, "%s%s%d%s", "--:--:--:--:--:--", " ", 0, "dBM");
  }
  u8g2.drawStr(8, 32, str);
  //u8g2.drawStr(102, 6, "#EXNO");

  u8g2.setFont(u8g2_font_profont22_tn);
  sprintf(str, "%*d", 2, latestDevices.size());
  u8g2.drawStr(102, 22, str);
}

void drawDiagram(int points[], int maxValue, int size) {
  if (maxValue < 1)
    maxValue = 1;
  char str[100];
  sprintf(str, "%d", maxValue);
  String s = str;
  u8g2.setFont(u8g2_font_profont10_tf);
  u8g2.drawStr(1, 6, str);
  for (int i = 0; i < size - 1; i++) {
    u8g2.drawLine(i, 22 - (points[i] * 22. / (1.1 * maxValue)), i + 1,
                  22 - (points[i + 1] * 22. / (1.1 * maxValue)));
  }
}

void scan_loop() {
  while (true) {

    Serial.println("Scan started.");
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    Serial.println("Scan done!");
    pBLEScan
        ->clearResults(); // delete results fromBLEScan buffer to release memory
    Serial.print("Found Exposure Notification Devices: ");
    intFoundDevices = currentDevices.size();
    latestDevices.assign(currentDevices.begin(), currentDevices.end());
    Serial.println(intFoundDevices);
    memmove(lastIntFoundDevices, &lastIntFoundDevices[1], (sizeof(int) * (99)));
    lastIntFoundDevices[99] = intFoundDevices;
    for (auto &lastIntFoundDevice : lastIntFoundDevices) {
      Serial.print(lastIntFoundDevice);
      Serial.print(" ");
    }
    Serial.println(" ");
    delay(2000);
    currentDevices.clear();
  }
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void handleRoot(AsyncWebServerRequest *request) {
  char temp[2048];
  snprintf(temp, sizeof(temp), "<html>\
  <head>\
     <meta http-equiv='refresh' content='5'/>\
     <title>Exposure Notification Scanner</title>\
     <style>\
      body { background-color: #2e2e2e; font-family: Fira Sans, FiraGO, Noto, Sans-Serif; Color: #d6d6d6; }\
      h1, h2, h3 {Color: #e87d3e}\
     </style>\
   </head>\
   <body>\
     <h1>Exposure Notification Scanner</h1>\
     <h2>Latest devices seen</h2>\
     <table>\
      <tr>\
        <th>Timestamp</th>\
        <th>Address</th>\
        <th>RSSI</th>\
      </tr>\
  ");
  for (auto &i : latestDevices) {
    snprintf(temp, sizeof(temp), "%s%s%d%s%s%s%d%s", temp, "<tr><td>",
             i.timestamp(), "</td><td>", i.address().c_str(), "</td><td>",
             i.rssi(), " dBM</td></tr>");
    // Serial.println(i.address);
  }

  int maximum = *std::max_element(std::begin(lastIntFoundDevices),
                                  std::end(lastIntFoundDevices));
  int yScale = 1;
  if (maximum > 0) {
    yScale = (100.0 / (1.1 * maximum));
  }

  snprintf(temp, sizeof(temp), "%s%s", temp, "  </table>\
     <h2>Graph</h2>\
     <svg viewBox=\"0 0 500 100\" class=\"chart\">\
     <polyline\
     fill=\"none\" \
     stroke=\"#9e86c8\" \
     stroke-width=\"1\" \
     points=\"\n ");

  for (int i = 0; i < 100; i++) {
    snprintf(temp, sizeof(temp), "%s%d%s%d\n", temp, i * 5, ",",
             100 - yScale * lastIntFoundDevices[i]);
  }

  snprintf(temp, sizeof(temp), "%s%s", temp, " \"/>\
  </svg>\
     </body>\
     </html>\  
  ");
  request->send(200, "text/html", temp);
}

void setup() {

  Serial.begin(115200);

  u8g2.begin();
  u8g2.setFlipMode(0);
  u8g2.setFont(u8g2_font_profont10_tf);

  Serial.println("ESP32 started");

  server.on("/", handleRoot);
  server.onNotFound(notFound);

  int c = 0;
  Serial.print("Connecting to ");
  u8g2.drawStr(0, 8, "Connecting to Wifi");
  Serial.println(ssid);
  u8g2.sendBuffer();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && c <= 20) {
    delay(500);
    Serial.print(".");
    c++;
  }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
    Serial.println("HTTP server started");
  } else {
    // WiFi.disconnect();
    Serial.println("Can't connect to Wifi. Starting Access Point");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("Exposure Notification Scanner");

    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);
    server.begin();
    Serial.println("HTTP server started");
  }

  Serial.println("BLE init");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(
      true); // active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99); // less or equal setInterval value
  delay(2000);

  Serial.println("Starting scan loop");
  scan_loop_thread = new std::thread(scan_loop);
  update_display_thread = new std::thread(update_display);
}

void loop() {
  if (WiFi.getMode() == WIFI_AP) {
    dnsServer.processNextRequest();
  }
}
