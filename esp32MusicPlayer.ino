#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SD.h>
// Digital I/O used
#define SD_CS 5
#define SPI_MOSI 23 // SD Card
#define SPI_MISO 19
#define SPI_SCK 18
char buf[50];
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
String receivedString;
String musicName = "";
#define SERVICE_UUID "00001234-0000-1000-8000-00805f9b34fb" // UART service UUID
#define CHARACTERISTIC_UUID_RX "00001235-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_TX "00001236-0000-1000-8000-00805f9b34fb"

const int chipSelect = 5; // SD card module's chip select pin

void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}

void deleteFile(fs::FS &fs, const char *path)
{
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path))
  {
    Serial.println("File deleted");
  }
  else
  {
    Serial.println("Delete failed");
  }
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0)
    {
      Serial.print("Received Value: ");
      Serial.println(rxValue.length());
      for (int i = 0; i < rxValue.length(); i++)
      {
        // Serial.print(rxValue[i]);
        receivedString += rxValue[i];
        // appendFile(SD, buf, rxValue[i]);
      }
      bool flag = true;
      if (receivedString.indexOf("MM") > -1)
      {
        Serial.println("setting music name");
        musicName = receivedString.substring(receivedString.indexOf("music_name"));
        Serial.println(musicName);
        musicName = "/" + musicName;
        musicName.toCharArray(buf, 50, 0);
        writeFile(SD, buf, " ");
        flag = false;
      }
      else if (receivedString.indexOf("done") > -1)
      {
        Serial.println("done");
        char receivedStrings[400];
        receivedString.toCharArray(receivedStrings, 400, 0);
        Serial.println(receivedStrings);

        Serial.println();
        Serial.println("*********");
        flag = false;
      }
      if (flag)
      {
        //================================================================================================
        // File dataFile = SD.open(musicName, FILE_WRITE);
        File file = SD.open(buf, FILE_APPEND);
        if (!file)
        {
          Serial.println("Failed to open file for appending");
          return;
        }
        for (int i = 0; i < rxValue.length(); i++)
        {

          if (file.print(rxValue[i]))
          {
            Serial.println("Message appended");
          }
          else
          {
            Serial.println("Append failed");
          }

         // delay(50);
          //====================================================================================
        }
        file.close();
        // Save the received data to a file on the SD card
        // File dataFile = SD.open(musicName, FILE_WRITE);
        /*File dataFile = SD.open(musicName, FILE_WRITE);
        if (dataFile)
        {
          for (int i = 0; i < rxValue.length(); i++)
            dataFile.write(rxValue[i]);
          dataFile.close();
          Serial.println("File saved successfully");
        }
        else
        {
          Serial.println("Error opening file");
        }*/
      }
      receivedString = "";
    }
  }
};

void setup()
{
  Serial.begin(115200);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  // Initialize the SD card
  if (!SD.begin(chipSelect))
  {
    Serial.println("SD card initialization failed!");
    return;
    // while (1)
    //   ; // Halt the program
  }
  Serial.println("SD card initialized successfully");

  // Create the BLE Device
  BLEDevice::init("JM Music Player");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX,
      BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX,
      BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting for a client connection to notify...");
}

void loop()
{

  if (deviceConnected)
  {
    pTxCharacteristic->setValue(&txValue, 1);
    pTxCharacteristic->notify();
    txValue++;
    delay(10);
  }

  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // Give the Bluetooth stack time to get ready
    pServer->startAdvertising(); // Restart advertising
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected)
  {
    // Do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}
