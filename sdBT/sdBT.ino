#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SD.h>
#include "Arduino.h"
#include "Audio.h"
#include "FS.h"
#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;
// Digital I/O used
#define SD_CS 5
#define SPI_MOSI 23 // SD Card
#define SPI_MISO 19
#define SPI_SCK 18

#define I2S_DOUT 25
#define I2S_BCLK 27 // I2S
#define I2S_LRC 26

Audio audio;
bool paused = false;
char buf[50];
BLEServer *pServer = NULL;
BLECharacteristic *pRxCharacteristic;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
String receivedString;
String musicName = "mesound";
String command = "play";
bool playing = true;
bool sending = true;
unsigned long playingTime = 0;
std::string oldrx = "";
std::string vals = "";
bool flag = true;
#define SERVICE_UUID "00001234-0000-1000-8000-00805f9b34fb" // UART service UUID
#define CHARACTERISTIC_UUID_RX "00001235-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_TX "00001236-0000-1000-8000-00805f9b34fb"
BLECharacteristic *pCharacteristic;
double data64 = 1;
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
};

unsigned long timeNow = 0;
unsigned long times = 0;
String fileNames[100]; // Maximum of 100 file names, adjust as per your needs
int fileCount = 0;
void readfiles()
{
    File directory = SD.open("/");

    while (true)
    {
        File entry = directory.openNextFile();
        if (!entry)
        {
            Serial.println("no more files");
            break; // No more files
        }
        if (entry.isDirectory())
        {
            continue; // Skip directories
        }
        fileNames[fileCount] = entry.name();
        fileCount++;
        entry.close();
    }
    directory.close();

    // Print the file names
    Serial.println("List of files:");
    for (int i = 0; i < fileCount; i++)
    {
        Serial.println(fileNames[i]);
    }
}

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
    // Read file names and save them in an array
    readfiles();
    //   Create the BLE Device
    BLEDevice::init("JM Music Player");
    // delay(10000);
    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

    pTxCharacteristic->addDescriptor(new BLE2902());

    pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE);

    // pRxCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    // Start advertising
    pServer->getAdvertising()->start();
    Serial.println("Waiting for a client connection to notify...");
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(20); // 0...21
                         // audio.connecttoFS(SD, "/music_namenewest.mp3");
    musicName = "/" + musicName + ".mp3";
    musicName.toCharArray(buf, 50, 0);
}

void loop()
{
    while (playing)
    {
        audio.loop();
        if (millis() - times > 5000)
        {
            times = millis();
            reads();
        }
        if (playing == false)
            break;
    }
    while (!playing)
    {
        reads();
        if (millis() - timeNow > 5000)
        {
            if (playing == true)
                break;
            // audio.connecttoFS(SD, "/music_namenewest.mp3");
            // audio.pauseResume();
        }
    }
}
// delay(1000);
void reads()
{
    std::string rxValue = pRxCharacteristic->getValue();
    if (rxValue.length() > 300)
        sending = true;
    if (rxValue.length() < 300 && oldrx != rxValue)
    {
        timeNow = 0;
        oldrx = rxValue;
        playing = false;
        sending = false;
        // Process the received value
        Serial.print("Received Value: ");
        Serial.println(rxValue.length());

        flag = true;

        for (int i = 0; i < rxValue.length(); i++)
        {
            // Serial.print(rxValue[i]);
            receivedString += rxValue[i];
            // appendFile(SD, buf, rxValue[i]);
        }
        if (receivedString.indexOf("M--@-===M") > -1)
        {
            Serial.println("setting music name");
            musicName = receivedString.substring(receivedString.indexOf("music_name") + 10);
            Serial.println(musicName);
            musicName = "/" + musicName + ".mp3";
            musicName.toCharArray(buf, 50, 0);
            bool check = true;
            sending = true;
            readfiles();
            for (int i = 0; i < fileCount + 1; i++)
            {
                if (musicName.substring(1) == fileNames[i])
                {
                    check = false;
                    Serial.print("musicName: ");
                    Serial.println(musicName.substring(1));
                    Serial.print("fileName: ");
                    Serial.println(fileNames[i]);
                    break;
                }
            }
            if (check)
            {
                Serial.println("name does not exist creating on sd card");
                writeFile(SD, buf, " ");
            }
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
            sending = false;
            flag = false;
        }
        else if (receivedString.indexOf("play") > -1)
        {
            Serial.println("play");
            Serial.println(buf);
            if (!paused)
            {
                audio.connecttoFS(SD, buf);
            }
            else
            {
                // audio.pauseResume();
                Serial.print("started plaing at :");
                Serial.println(playingTime);
                audio.connecttoFS(SD, buf);
                audio.setAudioPlayPosition(playingTime);
            }
            playing = true;
            paused = false;
            char receivedStrings[400];
            receivedString.toCharArray(receivedStrings, 400, 0);
            Serial.println(receivedStrings);
            Serial.println();
            command = "play";
            Serial.println("*********");
            flag = false;
        }
        else if (receivedString.indexOf("pause") > -1)
        {
            Serial.println("pause");
            paused = true;
            char receivedStrings[400];
            receivedString.toCharArray(receivedStrings, 400, 0);
            Serial.println(receivedStrings);
            command = "pause";
            playingTime = audio.getAudioCurrentTime();
            audio.setAudioPlayPosition(playingTime);
            // audio.stopSong();
            Serial.print("current time");
            Serial.println(playingTime);
            Serial.println();
            Serial.println("*********");
            flag = false;
        }
        else if (receivedString.indexOf("forward") > -1)
        {
            Serial.println("forward");
            char receivedStrings[400];
            receivedString.toCharArray(receivedStrings, 400, 0);
            Serial.println(receivedStrings);
            command = "forward";
            Serial.println();
            Serial.println("*********");
            flag = false;
        }
        else if (receivedString.indexOf("backward") > -1)
        {
            Serial.println("backward");
            char receivedStrings[400];
            receivedString.toCharArray(receivedStrings, 400, 0);
            Serial.println(receivedStrings);
            command = "backward";
            Serial.println();
            Serial.println("*********");
            flag = false;
        }
        else if (receivedString.indexOf("delete") > -1)
        {
            Serial.println("delete");
            char receivedStrings[400];
            receivedString.toCharArray(receivedStrings, 400, 0);
            Serial.println(receivedStrings);
            command = "delete";
            Serial.println();
            Serial.println("*********");
            flag = false;
        }
        if (!flag)
        {
            commands();
        }
    }

    else if (rxValue.length() > 300 && sending && oldrx != rxValue)
    {
        timeNow = 0;

        oldrx = rxValue;
        playing = false;
        // sending = false;
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
                // Serial.println("Message appended");
            }
            else
            {
                Serial.println("Append failed");
            }

            // delay(5);
            //====================================================================================
        }
        sending = false;
        playing = false;
        Serial.println("saved");
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
        pRxCharacteristic->setValue(vals);
        delay(50);
    }

    // ...
    // pRxCharacteristic->setValue(data64);
    rxValue = "";
    receivedString = "";
}

void commands()
{

    if (command == "pause")
    {
        Serial.println("pausing audio");
    }
    else if (command == "forward")
    {
        int timenow = audio.getAudioCurrentTime();
        if ((audio.getAudioCurrentTime() + 10) <= audio.getAudioFileDuration())
        {
            audio.setAudioPlayPosition(timenow + 10);
            Serial.println("forward audio");
            command = "play";
        }
    }
    else if (command == "backward")
    {
        Serial.println("back audio");
        int timenow = audio.getAudioCurrentTime();
        if (timenow > 10)
        {
            audio.setAudioPlayPosition(timenow - 10);
            command = "play";
        }
    }
    else if (command == "delete")
    {
        Serial.println("deleting audio");
        deleteFile(SD, buf);
        command = "play";
    }
    if (command == "play")
    {
        playing = true;
        Serial.println("playing audio");
    }
    if (deviceConnected)
    {
        //  pTxCharacteristic->setValue(&txValue, 1);
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