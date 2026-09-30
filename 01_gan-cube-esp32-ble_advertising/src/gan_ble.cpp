#include "gan_ble.h"
#include "solve_timer.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <mbedtls/aes.h>

#include <cstring>

namespace {

constexpr const char* targetCubeName = "GANicV2S_E9B1";
constexpr uint8_t aesBlockSize = 16;
constexpr uint8_t saltSize = 6;
constexpr uint8_t baseKey[16] = {
    0x01, 0x02, 0x42, 0x28, 0x31, 0x91, 0x16, 0x07,
    0x20, 0x05, 0x18, 0x54, 0x42, 0x11, 0x12, 0x53};
constexpr uint8_t baseIv[16] = {
    0x11, 0x03, 0x32, 0x28, 0x21, 0x01, 0x76, 0x27,
    0x20, 0x95, 0x78, 0x14, 0x32, 0x12, 0x02, 0x43};

uint8_t cubeSalt[6] = {};
bool haveCubeSalt = false;
uint16_t lastMoveSerial = 0xffff;
NimBLERemoteCharacteristic* gen3CommandCharacteristic = nullptr;
std::string lastFacelets;
bool haveFacelets = false;
std::string solvedReferenceFacelets;
bool haveSolvedReference = false;
bool lastSolved = false;

void printHex(const std::string& data);

uint32_t readBits(const uint8_t* data, uint16_t start, uint8_t count) {
  uint32_t value = 0;
  for (uint8_t index = 0; index < count; ++index) {
    const uint16_t bit = start + index;
    value = (value << 1) | ((data[bit / 8] >> (7 - (bit % 8))) & 1);
  }
  return value;
}

std::string toFacelets(const uint8_t* data) {
  const uint8_t cornerMap[8][3] = {
      {8, 9, 20}, {6, 18, 38}, {0, 36, 47}, {2, 45, 11},
      {29, 26, 15}, {27, 44, 24}, {33, 53, 42}, {35, 17, 51}};
  const uint8_t edgeMap[12][2] = {
      {5, 10}, {7, 19}, {3, 37}, {1, 46}, {32, 16}, {28, 25},
      {30, 43}, {34, 52}, {23, 12}, {21, 41}, {50, 39}, {48, 14}};
  uint8_t cp[8], co[8], ep[12], eo[12];
  uint8_t facelets[54];
  const char faces[] = "URFDLB";

  for (uint8_t i = 0; i < 54; ++i) facelets[i] = faces[i / 9];
  for (uint8_t i = 0; i < 7; ++i) {
    cp[i] = readBits(data, 40 + i * 3, 3);
    co[i] = readBits(data, 61 + i * 2, 2);
  }
  cp[7] = 28; co[7] = 0;
  for (uint8_t i = 0; i < 7; ++i) { cp[7] -= cp[i]; co[7] = (co[7] + co[i]) % 3; }
  co[7] = (3 - co[7]) % 3;
  for (uint8_t i = 0; i < 11; ++i) {
    ep[i] = readBits(data, 77 + i * 4, 4);
    eo[i] = readBits(data, 121 + i, 1);
  }
  ep[11] = 66; eo[11] = 0;
  for (uint8_t i = 0; i < 11; ++i) { ep[11] -= ep[i]; eo[11] = (eo[11] + eo[i]) % 2; }
  eo[11] = (2 - eo[11]) % 2;

  for (uint8_t i = 0; i < 8; ++i) for (uint8_t p = 0; p < 3; ++p)
    facelets[cornerMap[i][(p + co[i]) % 3]] = faces[cornerMap[cp[i]][p] / 9];
  for (uint8_t i = 0; i < 12; ++i) for (uint8_t p = 0; p < 2; ++p)
    facelets[edgeMap[i][(p + eo[i]) % 2]] = faces[edgeMap[ep[i]][p] / 9];

  return std::string(reinterpret_cast<const char*>(facelets), 54);
}

void decodeGen3Packet(const uint8_t* encrypted, size_t length) {
  // GAN encrypts the first and (for packets longer than 16 bytes) last block.
  if (!haveCubeSalt || length < 16 || length > 32) return;
  uint8_t key[16], iv[16], plain[32] = {};
  for (uint8_t i = 0; i < 16; ++i) {
    key[i] = baseKey[i]; iv[i] = baseIv[i];
    if (i < saltSize) {
      key[i] = (key[i] + cubeSalt[i]) % 0xff;
      iv[i] = (iv[i] + cubeSalt[i]) % 0xff;
    }
  }
  memcpy(plain, encrypted, length);
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, key, 128);
  uint8_t inputBlock[16] = {};
  uint8_t decryptedBlock[16] = {};
  size_t offset = length > aesBlockSize ? length - aesBlockSize : 0;
  memcpy(inputBlock, encrypted + offset, sizeof(inputBlock));
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, aesBlockSize, iv, inputBlock,
                        decryptedBlock);
  memcpy(plain + offset, decryptedBlock, sizeof(decryptedBlock));
  if (length > aesBlockSize) {
    memcpy(iv, baseIv, sizeof(iv));
    for (uint8_t i = 0; i < saltSize; ++i) {
      iv[i] = (baseIv[i] + cubeSalt[i]) % 0xff;
    }
    // The reference implementation decrypts the first chunk from the
    // buffer after the end chunk has already been written back. The chunks
    // overlap for a 19-byte Gen3 facelets packet, so this ordering matters.
    memcpy(inputBlock, plain, sizeof(inputBlock));
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, aesBlockSize, iv,
                          inputBlock, decryptedBlock);
    memcpy(plain, decryptedBlock, sizeof(decryptedBlock));
  }
  mbedtls_aes_free(&aes);
  if (plain[0] != 0x55) return;
  if (plain[1] == 0x02) {
    const std::string facelets = toFacelets(plain);
    // The cube reports a stable internal reference state that is not
    // necessarily the canonical Kociemba solved permutation. The user starts
    // this application with the cube solved, so use the first state as the
    // device-specific solved reference.
    if (!haveSolvedReference) {
      solvedReferenceFacelets = facelets;
      haveSolvedReference = true;
    }
    const bool solved = facelets == solvedReferenceFacelets;
    if (!haveFacelets || facelets != lastFacelets) {
      lastFacelets = facelets;
      haveFacelets = true;
    }
    if (solved && !lastSolved) {
      solveTimerSolved();
    }
    lastSolved = solved;
    return;
  }
  if (plain[1] != 0x01) return;
  uint16_t serial = plain[7] | (plain[8] << 8);
  if (serial == lastMoveSerial) return;
  lastMoveSerial = serial;
  solveTimerMove();
  const uint8_t codes[] = {2, 32, 8, 1, 16, 4};
  int face = -1; for (uint8_t i = 0; i < 6; ++i) if (codes[i] == (plain[9] & 0x3f)) face = i;
  Serial.print("MOVE ");
  if (face >= 0) { Serial.print("URFDLB"[face]); if (((plain[9] >> 6) & 3) == 1) Serial.print('\''); }
  else Serial.print("unknown");
  Serial.print(" serial=");
  Serial.println(serial);
}

void requestFacelets() {
  if (!haveCubeSalt || gen3CommandCharacteristic == nullptr) return;
  uint8_t key[16], iv[16], command[16] = {};
  for (uint8_t i = 0; i < 16; ++i) {
    key[i] = baseKey[i]; iv[i] = baseIv[i];
    if (i < saltSize) {
      key[i] = (key[i] + cubeSalt[i]) % 0xff;
      iv[i] = (iv[i] + cubeSalt[i]) % 0xff;
    }
  }
  command[0] = 0x68;
  command[1] = 0x01;
  mbedtls_aes_context aes; mbedtls_aes_init(&aes); mbedtls_aes_setkey_enc(&aes, key, 128);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, aesBlockSize, iv, command,
                        command);
  mbedtls_aes_free(&aes);
  gen3CommandCharacteristic->writeValue(command, sizeof(command), true);
}

void onCubeNotification(NimBLERemoteCharacteristic* characteristic,
                        uint8_t* data, size_t length, bool isNotify) {
  decodeGen3Packet(data, length);
}

void printHex(const std::string& data) {
  if (data.empty()) {
    Serial.println("(keine Daten)");
    return;
  }

  for (uint8_t byte : data) {
    if (byte < 0x10) {
      Serial.print('0');
    }
    Serial.print(byte, HEX);
  }
  Serial.println();
}

const char* addressTypeName(uint8_t type) {
  switch (type) {
    case BLE_ADDR_PUBLIC:
      return "public";
    case BLE_ADDR_RANDOM:
      return "random";
    case BLE_ADDR_PUBLIC_ID:
      return "public identity";
    case BLE_ADDR_RANDOM_ID:
      return "random identity";
    default:
      return "unknown";
  }
}

class GanScanCallbacks final : public NimBLEScanCallbacks {
 public:
  void onResult(const NimBLEAdvertisedDevice* device) override {
    if (connectedOrConnecting) {
      return;
    }

    const std::string name = device->getName();
    if (!device->haveName() || name != targetCubeName) {
      return;
    }

    connectedOrConnecting = true;

    Serial.println();
    Serial.println("--- GAN device ---");
    Serial.print("Name:           ");
    Serial.println(device->getName().c_str());
    Serial.print("Address:        ");
    Serial.println(device->getAddress().toString().c_str());
    Serial.print("Address Type:   ");
    Serial.print(device->getAddress().getType());
    Serial.print(" (");
    Serial.print(addressTypeName(device->getAddress().getType()));
    Serial.println(')');
    Serial.print("RSSI:           ");
    Serial.print(device->getRSSI());
    Serial.println(" dBm");
    Serial.print("Manufacturer:   ");
    printHex(device->getManufacturerData());
    Serial.println("Service UUIDs:");

    const uint8_t serviceCount = device->getServiceUUIDCount();
    if (serviceCount == 0) {
      Serial.println("  (keine Service UUIDs im Advertising)");
    } else {
      for (uint8_t index = 0; index < serviceCount; ++index) {
        Serial.print("  ");
        Serial.println(device->getServiceUUID(index).toString().c_str());
      }
    }

    pendingAddress = device->getAddress();
    const std::string manufacturer = device->getManufacturerData();
    if (manufacturer.size() >= 6) {
      // The reference implementation extracts the MAC backwards and then
      // reverses its bytes again when creating the AES salt. For the raw
      // manufacturer payload this is therefore the original byte order.
      for (uint8_t index = 0; index < 6; ++index) {
        cubeSalt[index] = static_cast<uint8_t>(
            manufacturer[manufacturer.size() - 6 + index]);
      }
      haveCubeSalt = true;
      Serial.print("Cube salt:      ");
      printHex(std::string(reinterpret_cast<const char*>(cubeSalt), sizeof(cubeSalt)));
    }
    hasPendingAddress = true;
    NimBLEDevice::getScan()->stop();
  }

  void connectPendingCube() {
    if (!hasPendingAddress) {
      return;
    }
    hasPendingAddress = false;

    Serial.println();
    Serial.println("Verbinde fuer GATT-Service-Discovery ...");
    NimBLEClient* client = NimBLEDevice::createClient();
    if (!client->connect(pendingAddress)) {
      Serial.println("Verbindung fehlgeschlagen.");
      NimBLEDevice::deleteClient(client);
      connectedOrConnecting = false;
      NimBLEDevice::getScan()->start(0, false);
      return;
    }

    Serial.println("Verbunden.");
    discoverServices(client);
    Serial.println("Schritt 2 abgeschlossen. GATT-Struktur ausgegeben.");
    subscribeNotifications(client);
    requestFacelets();
  }

 private:
  bool connectedOrConnecting = false;
  bool hasPendingAddress = false;
  NimBLEAddress pendingAddress;

  static void discoverServices(NimBLEClient* client) {
    Serial.println();
    Serial.println("--- GATT Services ---");

    const auto& services = client->getServices(true);
    for (NimBLERemoteService* service : services) {
      Serial.print("Service: ");
      Serial.println(service->getUUID().toString().c_str());

      const auto& characteristics = service->getCharacteristics(true);
      for (NimBLERemoteCharacteristic* characteristic : characteristics) {
        if (characteristic->getUUID().toString() ==
            "8653000c-43e6-47b7-9cb0-5fc21d4ae340") {
          gen3CommandCharacteristic = characteristic;
        }
        Serial.print("  Characteristic: ");
        Serial.print(characteristic->getUUID().toString().c_str());
        Serial.print(" [");

        bool firstProperty = true;
        if (characteristic->canRead()) {
          Serial.print("read");
          firstProperty = false;
        }
        if (characteristic->canWrite()) {
          if (!firstProperty) Serial.print(", ");
          Serial.print("write");
          firstProperty = false;
        }
        if (characteristic->canNotify()) {
          if (!firstProperty) Serial.print(", ");
          Serial.print("notify");
          firstProperty = false;
        }
        if (characteristic->canIndicate()) {
          if (!firstProperty) Serial.print(", ");
          Serial.print("indicate");
        }
        Serial.println("]");
      }
    }
  }

  static void subscribeNotifications(NimBLEClient* client) {
    Serial.println();
    Serial.println("--- Schritt 3: Notifications abonnieren ---");

    bool subscribed = false;
    const auto& services = client->getServices();
    for (NimBLERemoteService* service : services) {
      const auto& characteristics = service->getCharacteristics();
      for (NimBLERemoteCharacteristic* characteristic : characteristics) {
        if (!characteristic->canNotify() && !characteristic->canIndicate()) {
          continue;
        }

        const bool useNotify = characteristic->canNotify();
        if (characteristic->subscribe(useNotify, onCubeNotification)) {
          Serial.print("Notification abonniert: ");
          Serial.println(characteristic->getUUID().toString().c_str());
          subscribed = true;
        } else {
          Serial.print("Notification konnte nicht abonniert werden: ");
          Serial.println(characteristic->getUUID().toString().c_str());
        }
      }
    }

    if (subscribed) {
      Serial.println("Schritt 3 abgeschlossen. Warte auf Cube-Drehungen ...");
    } else {
      Serial.println("Keine Notify- oder Indicate-Characteristic gefunden.");
    }
  }
};

GanScanCallbacks scanCallbacks;

}  // namespace

void ganBleStartScan() {
  NimBLEDevice::init("");

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scanCallbacks, false);
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(15);
  scan->start(0, false);
}

void ganBleLoop() {
  scanCallbacks.connectPendingCube();
}
