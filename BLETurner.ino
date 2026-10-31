
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>
#include <BLESecurity.h>

//  PageTurner für iPad
// ESP32 Dev Module - ESP32 Core 3.3.12

const int pinLeft = 14;
const int pinRight = 27;

const uint8_t KEY_LEFT = 0x50;
const uint8_t KEY_RIGHT = 0x4F;

BLEHIDDevice* hid = nullptr;
BLECharacteristic* inputKeyboard = nullptr;

volatile bool connected = false;

bool lastLeft = HIGH;
bool lastRight = HIGH;

unsigned long lastLeftChange = 0;
unsigned long lastRightChange = 0;

const unsigned long debounceMs = 40;

// Bluetooth-Verbindung
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    connected = true;
    Serial.println("Device connected!");
  }

  void onDisconnect(BLEServer* server) override {
    connected = false;
    Serial.println("disconnected");

    server->getAdvertising()->start();
  }
};

// HID Keyboard Report Descriptor
const uint8_t reportMap[] = {
  0x05, 0x01,       // Generic Desktop
  0x09, 0x06,       // Keyboard
  0xA1, 0x01,       // Application Collection

  0x85, 0x01,       // Report ID 1

  0x05, 0x07,       // Keyboard Usage Page
  0x19, 0xE0,
  0x29, 0xE7,
  0x15, 0x00,
  0x25, 0x01,
  0x75, 0x01,
  0x95, 0x08,
  0x81, 0x02,

  0x95, 0x01,
  0x75, 0x08,
  0x81, 0x01,

  0x95, 0x05,
  0x75, 0x01,
  0x05, 0x08,
  0x19, 0x01,
  0x29, 0x05,
  0x91, 0x02,

  0x95, 0x01,
  0x75, 0x03,
  0x91, 0x01,

  0x95, 0x06,
  0x75, 0x08,
  0x15, 0x00,
  0x25, 0x65,
  0x05, 0x07,
  0x19, 0x00,
  0x29, 0x65,
  0x81, 0x00,

  0xC0
};


void sendKey(uint8_t keyCode) {
  if (!connected || inputKeyboard == nullptr) return;

  uint8_t press[8] = {
    0, 0, keyCode, 0, 0, 0, 0, 0
  };

  inputKeyboard->setValue(press, sizeof(press));
  inputKeyboard->notify();

  delay(60);

  uint8_t release[8] = {0};

  inputKeyboard->setValue(release, sizeof(release));
  inputKeyboard->notify();
}


void checkButton(int pin, bool &lastState,
                 unsigned long &lastChange,
                 uint8_t keyCode) {

  bool currentState = digitalRead(pin);

  if (currentState != lastState &&
      millis() - lastChange >= debounceMs) {

    lastChange = millis();
    lastState = currentState;

    if (currentState == LOW && connected) {
      sendKey(keyCode);

      Serial.print("Sent key ");
      Serial.println(keyCode, HEX);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(pinLeft, INPUT_PULLUP);
  pinMode(pinRight, INPUT_PULLUP);

  Serial.println("PageTurner started");

  BLEDevice::init("PageTurner");

  // Bluetooth-Sicherheit für iOS
  BLESecurity* security = new BLESecurity();

  security->setAuthenticationMode(
    ESP_LE_AUTH_REQ_SC_BOND
  );

  security->setCapability(ESP_IO_CAP_NONE);

  security->setInitEncryptionKey(
    ESP_BLE_ENC_KEY_MASK |
    ESP_BLE_ID_KEY_MASK
  );

  // BLE Server
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new MyServerCallbacks());

  // Bluetooth HID
  hid = new BLEHIDDevice(server);

  inputKeyboard = hid->inputReport(1);
  hid->outputReport(1);

  hid->manufacturer()->setValue("DIY");
  hid->pnp(0x02, 0xE502, 0xA111, 0x0210);
  hid->hidInfo(0x00, 0x01);

  hid->reportMap(
    (uint8_t*)reportMap,
    sizeof(reportMap)
  );

  hid->startServices();
  hid->setBatteryLevel(100);

  // Bluetooth Advertising
  BLEAdvertising* advertising = server->getAdvertising();

  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(
    hid->hidService()->getUUID()
  );

  advertising->start();

  Serial.println("Bluetooth ready");
  Serial.println("Name: PageTurner");
  Serial.println("waiting for connection");
}

void loop() {
  checkButton(
    pinLeft,
    lastLeft,
    lastLeftChange,
    KEY_LEFT
  );

  checkButton(
    pinRight,
    lastRight,
    lastRightChange,
    KEY_RIGHT
  );

  delay(10);
}
