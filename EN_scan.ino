/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/

#include <WiFi.h>

const char* ssid     = "xxxx";
const char* password = "xxxx";

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEAddress.h>

#include <list>

int scanTime = 19; //In seconds
BLEScan* pBLEScan;
BLEAddress* exposureDevice[255];

#include <thread>

std::thread *scan_loop_thread;

#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

class MyENDevice{
  public:
    String address;
    int timestamp;
    MyENDevice();
    MyENDevice(String address1){
      address = address1;
      timestamp = millis();
      }
    void tst();
  };

std::list <MyENDevice> currentDevices;
std::list <MyENDevice> latestDevices;
int intFoundDevices;

int lastIntFoundDevices[100] {0};
//MyENDevice testDevice();

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (advertisedDevice.haveServiceUUID()) {
        BLEUUID service = advertisedDevice.getServiceUUID();
        if (service.equals(BLEUUID((uint16_t)0xfd6f))) {
          Serial.printf("Exposure Notification from: %s\n",advertisedDevice.getAddress().toString().c_str());
          currentDevices.push_front(MyENDevice(advertisedDevice.getAddress().toString().c_str()));
          }
      }
    }
};

void scan_loop() {
  while(true) {

    Serial.println("Scan started.");
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    Serial.println("Scan done!");
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
    Serial.print("Found Exposure Notification Devices: ");
    intFoundDevices = currentDevices.size();
    latestDevices.assign(currentDevices.begin(), currentDevices.end());
    Serial.println(intFoundDevices);
    memmove(lastIntFoundDevices, &lastIntFoundDevices[1], (sizeof(int)*(99)));
    lastIntFoundDevices[99] = intFoundDevices;
    for (auto& lastIntFoundDevice : lastIntFoundDevices) {
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
  /*char temp[400];
  snprintf(temp, 400, 
  "<html>\
   <head>\
     <meta http-equiv='refresh' content='5'/>\
     <title>Exposure Notification Scanner</title>\
     <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
     </style>\
   </head>\
   <body>\
     <h1>Exposure Notification Scanner</h1>\
     <p>Exposure Notifications Found: %02d</p>\
   </body>\
   </html>", intFoundDevices
  );*/
  char temp[2048]; 
  snprintf(temp, sizeof(temp), 
  "<html>\
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
      </tr>\
  ");
  for (auto& i : latestDevices) {
    snprintf(temp, sizeof(temp), "%s%s%d%s%s%s", temp, "<tr><td>", i.timestamp, "</td><td>",i.address.c_str(),"</td></tr>");
   // Serial.println(i.address); 
  }

  int maximum = *std::max_element(std::begin(lastIntFoundDevices), std::end(lastIntFoundDevices));
  int yScale = 1;
  if (maximum > 0) {
    yScale = (100.0/(1.1 * maximum));
    }
  
  snprintf(temp, sizeof(temp), "%s%s", temp, 
  "  </table>\
     <h2>Graph</h2>\
     <svg viewBox=\"0 0 500 100\" class=\"chart\">\
     <polyline\
     fill=\"none\" \
     stroke=\"#9e86c8\" \
     stroke-width=\"1\" \
     points=\"\n ");
  
  for (int i = 0; i < 100; i++) {
    snprintf(temp, sizeof(temp), "%s%d%s%d\n", temp, i*5, ",", 100-yScale*lastIntFoundDevices[i]);
  }
  
  snprintf(temp, sizeof(temp), "%s%s", temp, 
  " \"/>\
  </svg>\
     </body>\
     </html>\  
  ");
  request->send(200, "text/html", temp);
}
int LED_BUILTIN = 1;
void setup() {
  
  
  Serial.begin(115200);

  Serial.println("ESP32 started");
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  server.on("/", handleRoot);
  server.onNotFound(notFound);

  int c = 0;
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && c <=20) {
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
    digitalWrite(LED_BUILTIN,LOW);
  }
  else {
    // TODO: Start AP Mode with captive Portal
    Serial.println("No Wifi Connection");
    
  }

  
  Serial.println("BLE init");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value
  delay(2000);

  Serial.println("Starting scan loop");
  scan_loop_thread =new std::thread(scan_loop);
}

void loop() {

}
