// AWTRIX Controller
// Copyright (C) 2020
// by Blueforcer & Mazze2000
#include <FS.h>
// #include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <Fonts/TomThumb.h>
#include <LightDependentResistor.h>
#include <Wire.h>
#include <SparkFun_APDS9960.h>
#include "SoftwareSerial.h"
#include "RTClib.h"
#include "DHT.h"
#include "Lite/ApePixelClock.h"

#include <WiFiManager.h>
#include <DoubleResetDetect.h>
#include <Wire.h>
#include <BME280_t.h>
#include <SHT3x.h>
#include "Adafruit_HTU21DF.h"
#include "DFRobotDFPlayerMini.h"

// instantiate temp sensor
BME280<> BMESensor;
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
RTC_DS1307 rtc;
DHT dht(10, DHT11);
SHT3x Sensor;

int tempState = false;		// 0 = None ; 1 = BME280 ; 2 = htu21d ; 3 = DHT11，4 = SHT30
int ldrState = 1000;		// 0 = None
bool USBConnection = false; // true = usb...
bool WIFIConnection = false;
int connectionTimout;

bool MatrixType2 = false;
int matrixTempCorrection = 0;

String version = "0.3";
char awtrix_server[16] = "192.168.";
char Port[5] = "7001"; // AWTRIX Host Port, default = 7001
IPAddress Server;
WiFiClient espClient;
PubSubClient client(espClient);

WiFiManager wifiManager;

//update
ESP8266WebServer server(80);
const char *serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

ApcConfigDef apcConfigDef;

//resetdetector
#define DRD_TIMEOUT 5.0
#define DRD_ADDRESS 0x00
DoubleResetDetect drd(DRD_TIMEOUT, DRD_ADDRESS);

bool firstStart = true;
int myTime;	 //need for loop
int myTime2; //need for loop
int myTime3; //need for loop3
int myCounter;
int myCounter2;
//boolean getLength = true;
//int prefix = -5;

bool ignoreServer = false;
int menuePointer;

//Taster_mid
int tasterPin[] = {D0, D4, D8};
int timeoutTaster[] = {0, 0, 0, 0};
bool pushed[] = {false, false, false, false};
int blockTimeTaster[] = {0, 0, 0, 0};
bool blockTaster[] = {false, false, false, false};
bool blockTaster2[] = {false, false, false, false};
bool tasterState[3];
bool allowTasterSendToServer = true;
int pressedTaster = 0;

//Reset time (Touch Taster)
int resetTime = 6000; //in milliseconds

boolean awtrixFound = false;
int myPointer[14];
uint32_t messageLength = 0;
uint32_t SavemMessageLength = 0;

//USB Connection:
byte myBytes[1000];
int bufferpointer;

//Zum speichern...
int cfgStart = 0;

//flag for saving data
bool shouldSaveConfig = false;

/// LDR Config
#define LDR_RESISTOR 1000 //ohms
#define LDR_PIN A0
#define LDR_PHOTOCELL LightDependentResistor::GL5516
LightDependentResistor photocell(LDR_PIN, ldrState, LDR_PHOTOCELL);

// Gesture Sensor
#define APDS9960_INT D6
#define I2C_SDA D3
#define I2C_SCL D1
SparkFun_APDS9960 apds = SparkFun_APDS9960();
volatile bool isr_flag = 0;

#ifndef ICACHE_RAM_ATTR
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif

bool updating = false;

// Audio
//DFPlayerMini_Fast myMP3;
DFRobotDFPlayerMini myMP3;

SoftwareSerial mySoftwareSerial(D7, D5); // RX, TX

// Matrix Settings
CRGB leds[256];
FastLED_NeoMatrix *matrix;

static byte c1; // Last character buffer
byte utf8ascii(byte ascii)
{
	if (ascii < 128) // Standard ASCII-set 0..0x7F handling
	{
		c1 = 0;
		return (ascii);
	}
	// get previous input
	byte last = c1; // get last char
	c1 = ascii;		// remember actual character
	switch (last)	// conversion depending on first UTF8-character
	{
	case 0xC2:
		return (ascii)-34;
		break;
	case 0xC3:
		return (ascii | 0xC0) - 34;
		break;
	case 0x82:
		if (ascii == 0xAC)
			return (0xEA);
	}
	return (0);
}

bool saveConfig()
{
	DynamicJsonBuffer jsonBuffer;
	JsonObject &json = jsonBuffer.createObject();
	json["awtrix_server"] = awtrix_server;
	json["MatrixType"] = MatrixType2;
	json["LiteMode"] = apcConfigDef.liteMode;
	json["OffLine"] = apcConfigDef.offLine;
	json["matrixCorrection"] = matrixTempCorrection;
	json["Port"] = Port;

	//json["temp"] = tempState;
	//json["usbWifi"] = USBConnection;
	//json["ldr"] = ldrState;
	//json["gesture"] = gestureState;
	//json["audio"] = audioState;

	File configFile = SPIFFS.open("/awtrix.json", "w");
	if (!configFile)
	{
		if (!USBConnection)
		{
			Serial.println("failed to open config file for writing");
		}
		return false;
	}

	json.printTo(configFile);
	configFile.close();

	

	//end save
	return true;
}

bool saveLiteConfig()
{
	DynamicJsonBuffer jsonBuffer;
	JsonObject &json = jsonBuffer.createObject();
	json["alarm_enable"] = apcConfigDef.alarm_enable;
	json["alarm_time"] = apcConfigDef.alarm_time;
	json["cdd_date"] = apcConfigDef.cdd_date;
	json["volume"] = apcConfigDef.volume;
	json["brightness"] = apcConfigDef.brightness;
	File liteConfigFile = SPIFFS.open("/LiteConfig.json", "w");
	json.printTo(liteConfigFile);
	liteConfigFile.close();

	return true;
}

void debuggingWithMatrix(String text)
{
	matrix->setCursor(7, 6);
	matrix->clear();
	matrix->print(text);
	matrix->show();
}

void sendToServer(String s)
{
	if (USBConnection)
	{
		uint32_t laenge = s.length();
		Serial.printf("%c%c%c%c%s", (laenge & 0xFF000000) >> 24, (laenge & 0x00FF0000) >> 16, (laenge & 0x0000FF00) >> 8, (laenge & 0x000000FF), s.c_str());
	}
	else
	{
		if (!apcConfigDef.liteMode)
		{
			client.publish("matrixClient", s.c_str());
		}
		else
		{
			APC.publish(s);
		}
	}
}

void logToServer(String s)
{
	StaticJsonBuffer<400> jsonBuffer;
	JsonObject &root = jsonBuffer.createObject();
	root["type"] = "log";
	root["msg"] = s;
	String JS;
	root.printTo(JS);
	sendToServer(JS);
}

int checkTaster(int nr)
{
	tasterState[0] = !digitalRead(tasterPin[0]);
	tasterState[1] = digitalRead(tasterPin[1]);
	tasterState[2] = !digitalRead(tasterPin[2]);

	switch (nr)
	{
	case 0:
		if (tasterState[0] == LOW && !pushed[nr] && !blockTaster2[nr] && tasterState[1] && tasterState[2])
		{
			pushed[nr] = true;
			timeoutTaster[nr] = millis();
			delay(20);
		}
		break;
	case 1:
		if (tasterState[1] == LOW && !pushed[nr] && !blockTaster2[nr] && tasterState[0] && tasterState[2])
		{
			pushed[nr] = true;
			timeoutTaster[nr] = millis();
			delay(20);
		}
		break;
	case 2:
		if (tasterState[2] == LOW && !pushed[nr] && !blockTaster2[nr] && tasterState[0] && tasterState[1])
		{
			pushed[nr] = true;
			timeoutTaster[nr] = millis();
			delay(20);
		}
		break;
	case 3:
		if (tasterState[0] == LOW && tasterState[2] == LOW && !pushed[nr] && !blockTaster2[nr] && tasterState[1])
		{
			pushed[nr] = true;
			timeoutTaster[nr] = millis();
			delay(20);
		}
		break;
	}

	if (pushed[nr] && (millis() - timeoutTaster[nr] < 2000) && tasterState[nr] == HIGH)
	{
		if (!blockTaster2[nr])
		{
			StaticJsonBuffer<400> jsonBuffer;
			JsonObject &root = jsonBuffer.createObject();
			root["type"] = "button";

			switch (nr)
			{
			case 0:
				root["left"] = "short";
				pressedTaster = 1;
				//Serial.println("LEFT: normaler Tastendruck");
				break;
			case 1:
				root["middle"] = "short";
				pressedTaster = 2;
				//Serial.println("MID: normaler Tastendruck");
				break;
			case 2:
				root["right"] = "short";
				pressedTaster = 3;
				//Serial.println("RIGHT: normaler Tastendruck");
				break;
			}

			String JS;
			root.printTo(JS);
			if (allowTasterSendToServer)
			{
				sendToServer(JS);
			}
			pushed[nr] = false;
			return 1;
		}
	}

	if (pushed[nr] && (millis() - timeoutTaster[nr] > 2000))
	{
		if (!blockTaster2[nr])
		{
			StaticJsonBuffer<400> jsonBuffer;
			JsonObject &root = jsonBuffer.createObject();
			root["type"] = "button";
			switch (nr)
			{
			case 0:
				root["left"] = "long";
				//Serial.println("LEFT: langer Tastendruck");
				break;
			case 1:
				root["middle"] = "long";
				//Serial.println("MID: langer Tastendruck");
				break;
			case 2:
				root["right"] = "long";
				//Serial.println("RIGHT: langer Tastendruck");
				break;
			case 3:
				if (allowTasterSendToServer)
				{
					allowTasterSendToServer = false;
					ignoreServer = true;
				}
				else
				{
					allowTasterSendToServer = true;
					ignoreServer = false;
					menuePointer = 0;
				}
				break;
			}
			String JS;
			root.printTo(JS);
			if (allowTasterSendToServer)
			{
				sendToServer(JS);
			}

			blockTaster[nr] = true;
			blockTaster2[nr] = true;
			pushed[nr] = false;
			return 2;
		}
	}
	if (nr == 3)
	{
		if (blockTaster[nr] && tasterState[0] == HIGH && tasterState[2] == HIGH)
		{
			blockTaster[nr] = false;
			blockTimeTaster[nr] = millis();
		}
	}
	else
	{
		if (blockTaster[nr] && tasterState[nr] == HIGH)
		{
			blockTaster[nr] = false;
			blockTimeTaster[nr] = millis();
		}
	}

	if (!blockTaster[nr] && (millis() - blockTimeTaster[nr] > 500))
	{
		blockTaster2[nr] = false;
	}
	return 0;
}

String utf8ascii(String s)
{
	String r = "";
	char c;
	for (unsigned int i = 0; i < s.length(); i++)
	{
		c = utf8ascii(s.charAt(i));
		if (c != 0)
			r += c;
	}
	return r;
}

void hardwareAnimatedUncheck(int typ, int x, int y)
{
	int wifiCheckTime = millis();
	int wifiCheckPoints = 0;
	while (millis() - wifiCheckTime < 2000)
	{
		while (wifiCheckPoints < 10)
		{
			matrix->clear();
			switch (typ)
			{
			case 0:
				matrix->setCursor(7, 6);
				matrix->print("WiFi");
				break;
			case 1:
				matrix->setCursor(1, 6);
				matrix->print("Server");
				break;
			case 2:
				matrix->setCursor(7, 6);
				matrix->print("Temp");
				break;
			case 4:
				matrix->setCursor(3, 6);
				matrix->print("Gest.");
				break;
			}

			switch (wifiCheckPoints)
			{
			case 9:
				matrix->drawPixel(x, y + 4, 0xF800);
			case 8:
				matrix->drawPixel(x - 1, y + 3, 0xF800);
			case 7:
				matrix->drawPixel(x - 2, y + 2, 0xF800);
			case 6:
				matrix->drawPixel(x - 3, y + 1, 0xF800);
			case 5:
				matrix->drawPixel(x - 4, y, 0xF800);
			case 4:
				matrix->drawPixel(x - 4, y + 4, 0xF800);
			case 3:
				matrix->drawPixel(x - 3, y + 3, 0xF800);
			case 2:
				matrix->drawPixel(x - 2, y + 2, 0xF800);
			case 1:
				matrix->drawPixel(x - 1, y + 1, 0xF800);
			case 0:
				matrix->drawPixel(x, y, 0xF800);
				break;
			}
			wifiCheckPoints++;
			matrix->show();
			delay(100);
		}
	}
}

void hardwareAnimatedCheck(int typ, int x, int y)
{
	int wifiCheckTime = millis();
	int wifiCheckPoints = 0;
	while (millis() - wifiCheckTime < 2000)
	{
		while (wifiCheckPoints < 7)
		{
			matrix->clear();
			switch (typ)
			{
			case 0:
				matrix->setCursor(7, 6);
				matrix->print("WiFi");
				break;
			case 1:
				matrix->setCursor(5, 6);
				matrix->print("Host");
				//Serial.println("host");
				break;
			case 2:
				matrix->setCursor(7, 6);
				matrix->print("Temp");
				break;
			case 3:
				matrix->setCursor(3, 6);
				matrix->print("Audio");
				break;
			case 4:
				matrix->setCursor(3, 6);
				matrix->print("Gest.");
				break;
			case 5:
				matrix->setCursor(7, 6);
				matrix->print("LDR");
				break;
			case 6:
				matrix->setCursor(7, 6);
				matrix->print("RTC");
				break;
			case 7:
				matrix->setCursor(3, 6);
				matrix->print("DHT11");
				break;
			}

			switch (wifiCheckPoints)
			{
			case 6:
				matrix->drawPixel(x, y, 0x07E0);    //对号
			case 5:
				matrix->drawPixel(x - 1, y + 1, 0x07E0);
			case 4:
				matrix->drawPixel(x - 2, y + 2, 0x07E0);
			case 3:
				matrix->drawPixel(x - 3, y + 3, 0x07E0);
			case 2:
				matrix->drawPixel(x - 4, y + 4, 0x07E0);
			case 1:
				matrix->drawPixel(x - 5, y + 3, 0x07E0);
			case 0:
				matrix->drawPixel(x - 6, y + 2, 0x07E0);
				break;
			}
			wifiCheckPoints++;
			matrix->show();
			delay(100);
		}
	}
}

void serverSearch(int rounds, int typ, int x, int y)    //服务器版本host
{
	matrix->clear();
	matrix->setTextColor(0xFFFF);
	matrix->setCursor(5, 6);
	matrix->print("Host");
	delay(100);
	//Serial.println("host2");

	if (typ == 0)
	{
		switch (rounds)    //没进入
		{
		case 3:
			matrix->drawPixel(x, y, 0x22ff);
			matrix->drawPixel(x + 1, y + 1, 0x22ff);
			matrix->drawPixel(x + 2, y + 2, 0x22ff);
			matrix->drawPixel(x + 3, y + 3, 0x22ff);
			matrix->drawPixel(x + 2, y + 4, 0x22ff);
			matrix->drawPixel(x + 1, y + 5, 0x22ff);
			matrix->drawPixel(x, y + 6, 0x22ff);
			//Serial.println("here");
		case 2:
			matrix->drawPixel(x - 1, y + 2, 0x22ff);
			matrix->drawPixel(x, y + 3, 0x22ff);
			matrix->drawPixel(x - 1, y + 4, 0x22ff);
		case 1:
			//matrix->drawPixel(x - 3, y + 3, 0x22ff);
		case 0:
			break;
		}
	}
	else if (typ == 1)
	{

		switch (rounds)
		{
		case 12:
			//matrix->drawPixel(x+3, y+2, 0x22ff);
			matrix->drawPixel(x + 3, y + 3, 0x22ff);
			//matrix->drawPixel(x+3, y+4, 0x22ff);
			matrix->drawPixel(x + 3, y + 5, 0x22ff);
			//matrix->drawPixel(x+3, y+6, 0x22ff);
		case 11:
			matrix->drawPixel(x + 2, y + 2, 0x22ff);
			matrix->drawPixel(x + 2, y + 3, 0x22ff);
			matrix->drawPixel(x + 2, y + 4, 0x22ff);
			matrix->drawPixel(x + 2, y + 5, 0x22ff);
			matrix->drawPixel(x + 2, y + 6, 0x22ff);
		case 10:
			matrix->drawPixel(x + 1, y + 3, 0x22ff);
			matrix->drawPixel(x + 1, y + 4, 0x22ff);
			matrix->drawPixel(x + 1, y + 5, 0x22ff);
		case 9:
			matrix->drawPixel(x, y + 4, 0x22ff);
		case 8:
			matrix->drawPixel(x - 1, y + 4, 0x22ff);
		case 7:
			matrix->drawPixel(x - 2, y + 4, 0x22ff);
		case 6:
			matrix->drawPixel(x - 3, y + 4, 0x22ff);
		case 5:
			matrix->drawPixel(x - 3, y + 5, 0x22ff);
		case 4:
			matrix->drawPixel(x - 3, y + 6, 0x22ff);
		case 3:
			matrix->drawPixel(x - 3, y + 7, 0x22ff);
		case 2:
			matrix->drawPixel(x - 4, y + 7, 0x22ff);
		case 1:
			matrix->drawPixel(x - 5, y + 7, 0x22ff);
		case 0:
			break;
		}
	}
	matrix->show();
}

void hardwareAnimatedSearch(int typ, int x, int y)
{
	for (int i = 0; i < 4; i++)
	{
		matrix->clear();
		matrix->setTextColor(0xFFFF);
		if (typ == 0)
		{
			matrix->setCursor(7, 6);
			matrix->print("WiFi");
		}
		else if (typ == 1)
		{
			matrix->setCursor(5, 6);
			matrix->print("Host");
			//Serial.println("here");
		}
		switch (i)
		{
		case 3:
			matrix->drawPixel(x, y, 0x22ff);
			matrix->drawPixel(x + 1, y + 1, 0x22ff);
			matrix->drawPixel(x + 2, y + 2, 0x22ff);
			matrix->drawPixel(x + 3, y + 3, 0x22ff);
			matrix->drawPixel(x + 2, y + 4, 0x22ff);
			matrix->drawPixel(x + 1, y + 5, 0x22ff);
			matrix->drawPixel(x, y + 6, 0x22ff);
		case 2:
			matrix->drawPixel(x - 1, y + 2, 0x22ff);
			matrix->drawPixel(x, y + 3, 0x22ff);
			matrix->drawPixel(x - 1, y + 4, 0x22ff);
		case 1:
			matrix->drawPixel(x - 3, y + 3, 0x22ff);
		case 0:
			break;
		}
		matrix->show();
		delay(100);
	}
}

void utf8ascii(char *s)
{
	int k = 0;
	char c;
	for (unsigned int i = 0; i < strlen(s); i++)
	{
		c = utf8ascii(s[i]);
		if (c != 0)
			s[k++] = c;
	}
	s[k] = 0;
}

void checkReset()
{
	if (drd.detect())
	{
		//Serial.println("** Double reset boot **");
		matrix->clear();
		matrix->setTextColor(matrix->Color(255, 0, 0));    //red
		matrix->setCursor(6, 6);
		matrix->print("RESET!");
		matrix->show();
		delay(1000);
		if (SPIFFS.begin())
		{
			delay(1000);
			SPIFFS.remove("/awtrix.json");
			SPIFFS.remove("/LiteConfig.json");
			SPIFFS.end();
			delay(1000);
		}
		wifiManager.resetSettings();
		ESP.reset();
	}

	int resetTimeShow = 0;
	int resetStartTime = millis();
	while (digitalRead(tasterPin[0]) && digitalRead(tasterPin[2]))
	{

		int showTime = (resetTime + (resetStartTime - millis())) / 1000;

		if (resetTimeShow != showTime)
		{
			resetTimeShow = showTime;
			matrix->clear();
			matrix->setTextColor(matrix->Color(255, 0, 0));
			matrix->setCursor(3, 6);
			matrix->print("RESET! " + (String)showTime);
			//Serial.println("RESET! " + (String)showTime);
			matrix->show();
			if (showTime < 1)
			{
				matrix->clear();
				matrix->setCursor(6, 6);
				matrix->setTextColor(matrix->Color(255, 0, 0));
				matrix->print("RESET!");
				matrix->show();
				delay(1000);
				if (SPIFFS.begin())
				{
					delay(1000);
					SPIFFS.remove("/awtrix.json");
					SPIFFS.remove("/LiteConfig.json");
					SPIFFS.end();
					delay(1000);
				}
				wifiManager.resetSettings();
				ESP.reset();
			}
		}
	}
}

String GetChipID()
{
	return String(ESP.getChipId());
}

int hexcolorToInt(char upper, char lower)
{
	int uVal = (int)upper;
	int lVal = (int)lower;
	uVal = uVal > 64 ? uVal - 55 : uVal - 48;
	uVal = uVal << 4;
	lVal = lVal > 64 ? lVal - 55 : lVal - 48;
	//  Serial.println(uVal+lVal);
	return uVal + lVal;
}

int GetRSSIasQuality(int rssi)
{
	int quality = 0;

	if (rssi <= -100)
	{
		quality = 0;
	}
	else if (rssi >= -50)
	{
		quality = 100;
	}
	else
	{
		quality = 2 * (rssi + 100);
	}
	return quality;
}

void updateMatrix(byte payload[], int length)
{
	if (!ignoreServer)
	{
		int y_offset = 5;
		if (firstStart)
		{
			//hardwareAnimatedCheck(1, 30, 2);
			firstStart = false;
		}

		connectionTimout = millis();

		switch (payload[0])
		{
		case 0:
		{
			//Command 0: DrawText

			//Prepare the coordinates
			uint16_t x_coordinate = int(payload[1] << 8) + int(payload[2]);
			uint16_t y_coordinate = int(payload[3] << 8) + int(payload[4]);

			matrix->setCursor(x_coordinate + 1, y_coordinate + y_offset);
			matrix->setTextColor(matrix->Color(payload[5], payload[6], payload[7]));
			String myText = "";
			for (int i = 8; i < length; i++)
			{
				char c = payload[i];
				myText += c;
			}
			//Serial.println(myText);
			matrix->print(utf8ascii(myText));
			break;
		}
		case 1:
		{
			//Command 1: DrawBMP

			//Prepare the coordinates
			uint16_t x_coordinate = int(payload[1] << 8) + int(payload[2]);
			uint16_t y_coordinate = int(payload[3] << 8) + int(payload[4]);

			int16_t width = payload[5];
			int16_t height = payload[6];

			unsigned short colorData[width * height];

			for (int i = 0; i < width * height * 2; i++)
			{
				colorData[i / 2] = (payload[i + 7] << 8) + payload[i + 1 + 7];
				i++;
			}

			for (int16_t j = 0; j < height; j++, y_coordinate++)
			{
				for (int16_t i = 0; i < width; i++)
				{
					matrix->drawPixel(x_coordinate + i, y_coordinate, (uint16_t)colorData[j * width + i]);
				}
			}
			break;
		}

		case 2:
		{
			//Command 2: DrawCircle

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1] << 8) + int(payload[2]);
			uint16_t y0_coordinate = int(payload[3] << 8) + int(payload[4]);
			uint16_t radius = payload[5];
			matrix->drawCircle(x0_coordinate, y0_coordinate, radius, matrix->Color(payload[6], payload[7], payload[8]));
			break;
		}
		case 3:
		{
			//Command 3: FillCircle

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1] << 8) + int(payload[2]);
			uint16_t y0_coordinate = int(payload[3] << 8) + int(payload[4]);
			uint16_t radius = payload[5];
			matrix->fillCircle(x0_coordinate, y0_coordinate, radius, matrix->Color(payload[6], payload[7], payload[8]));
			break;
		}
		case 4:
		{
			//Command 4: DrawPixel

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1] << 8) + int(payload[2]);
			uint16_t y0_coordinate = int(payload[3] << 8) + int(payload[4]);
			matrix->drawPixel(x0_coordinate, y0_coordinate, matrix->Color(payload[5], payload[6], payload[7]));
			break;
		}
		case 5:
		{
			//Command 5: DrawRect

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1] << 8) + int(payload[2]);
			uint16_t y0_coordinate = int(payload[3] << 8) + int(payload[4]);
			int16_t width = payload[5];
			int16_t height = payload[6];
			matrix->drawRect(x0_coordinate, y0_coordinate, width, height, matrix->Color(payload[7], payload[8], payload[9]));
			break;
		}
		case 6:
		{
			//Command 6: DrawLine

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1] << 8) + int(payload[2]);
			uint16_t y0_coordinate = int(payload[3] << 8) + int(payload[4]);
			uint16_t x1_coordinate = int(payload[5] << 8) + int(payload[6]);
			uint16_t y1_coordinate = int(payload[7] << 8) + int(payload[8]);
			matrix->drawLine(x0_coordinate, y0_coordinate, x1_coordinate, y1_coordinate, matrix->Color(payload[9], payload[10], payload[11]));
			break;
		}

		case 7:
		{
			//Command 7: FillMatrix

			matrix->fillScreen(matrix->Color(payload[1], payload[2], payload[3]));
			break;
		}

		case 8:
		{
			//Command 8: Show
			matrix->show();
			break;
		}
		case 9:
		{
			//Command 9: Clear
			matrix->clear();
			break;
		}
		case 10:
		{
			//deprecated
			//Command 10: Play
			myMP3.volume(payload[2]);
			delay(10);
			myMP3.play(payload[1]);
			
			break;
		}
		case 11:
		{
			//Command 11: reset
			ESP.reset();
			break;
		}
		case 12:
		{
			//Command 12: GetMatrixInfo
			StaticJsonBuffer<400> jsonBuffer;
			JsonObject &root = jsonBuffer.createObject();
			root["type"] = "MatrixInfo";
			root["version"] = version;
			root["wifirssi"] = String(WiFi.RSSI());
			root["wifiquality"] = GetRSSIasQuality(WiFi.RSSI());
			root["wifissid"] = WiFi.SSID();
			root["IP"] = WiFi.localIP().toString();
			if (photocell.getCurrentLux()>1)
			{
				root["LUX"] = photocell.getCurrentLux();
			}
			else
			{
				root["LUX"] = NULL;
			}

			BMESensor.refresh();
			Sensor.UpdateData();   //sht30更新数据
			if (tempState == 1)
			{
				root["Temp"] = BMESensor.temperature;
				root["Hum"] = BMESensor.humidity;
				root["hPa"] = BMESensor.pressure;
			}
			else if (tempState == 2)
			{
				root["Temp"] = htu.readTemperature();
				root["Hum"] = htu.readHumidity();
				root["hPa"] = 0;
			}
			else if (tempState == 3)
			{
				root["Temp"] = dht.readTemperature();
				root["Hum"] = dht.readHumidity();
				root["hPa"] = 0;
			}
			else if (tempState == 4)
			{
				root["Temp"] = Sensor.GetTemperature();
				//Serial.println("温度");
				//Serial.println(Sensor.GetTemperature());
				root["Hum"] = Sensor.GetRelHumidity();
				//Serial.println("湿度");
				//Serial.println(Sensor.GetRelHumidity());
				root["hPa"] = Sensor.GetAbsHumidity(SHT3x::Pa);
				//Serial.println("气压");
				//Serial.println(Sensor.GetAbsHumidity(SHT3x::Pa));
			}
			else
			{
				root["Temp"] = 0;
				root["Hum"] = 0;
				root["hPa"] = 0;
			}

			String JS;
			root.printTo(JS);
			sendToServer(JS);
			break;
		}
		case 13:
		{
			matrix->setBrightness(payload[1]);
			break;
		}
		case 14:
		{
			//tempState = (int)payload[1];
			//audioState = (int)payload[2];
			//gestureState = (int)payload[3];
			ldrState = int(payload[1] << 8) + int(payload[2]);
			matrixTempCorrection = (int)payload[3];
			matrix->clear();
			matrix->setCursor(6, 6);
			matrix->setTextColor(matrix->Color(0, 255, 50));
			matrix->print("SAVED!");
			matrix->show();
			delay(2000);
			if (saveConfig())
			{
				ESP.reset();
			}
			break;
		}
		case 15:
		{

			matrix->clear();
			matrix->setTextColor(matrix->Color(255, 0, 0));
			matrix->setCursor(6, 6);
			matrix->print("RESET!");
			matrix->show();
			delay(1000);
			if (SPIFFS.begin())
			{
				delay(1000);
				SPIFFS.remove("/awtrix.json");
				SPIFFS.remove("/LiteConfig.json");
				SPIFFS.end();
				delay(1000);
			}
			wifiManager.resetSettings();
			ESP.reset();
			break;
		}
		case 16:
		{
			sendToServer("ping");
			break;
		}
		case 17:
		{
			//Command 17: Volume
			myMP3.volume(payload[1]);

			break;
		}
		case 18:
		{
			//Command 18: Play
			myMP3.playMp3Folder(payload[1]);
			// Serial.println(payload[1]);
			break;
		}
		case 19:
		{
			//Command 18: Stop
			myMP3.stop();
			break;
		}
		case 20:
		{
			//change the connection...
			USBConnection = false;
			WIFIConnection = false;
			firstStart = true;
			break;
		}
		case 21:
		{
			//multicolor...
			uint16_t x_coordinate = int(payload[1] << 8) + int(payload[2]);
			uint16_t y_coordinate = int(payload[3] << 8) + int(payload[4]);
			matrix->setCursor(x_coordinate + 1, y_coordinate + y_offset);

			String myJSON = "";
			for (int i = 5; i < length; i++)
			{
				myJSON += (char)payload[i];
			}
			//Serial.println("myJSON: " + myJSON + " ENDE");
			DynamicJsonBuffer jsonBuffer;
			JsonArray &array = jsonBuffer.parseArray(myJSON);
			if (array.success())
			{
				//Serial.println("Array erfolgreich geöffnet... =)");
				for (int i = 0; i < (int)array.size(); i++)
				{
					String tempString = array[i]["t"];
					String colorString = array[i]["c"];
					JsonArray &color = jsonBuffer.parseArray(colorString);
					if (color.success())
					{
						//Serial.println("Color erfolgreich geöffnet... =)");
						String myText = "";
						int r = color[0];
						int g = color[1];
						int b = color[2];
						//Serial.println("Test: " + tempString + " / Color: " + r + "/" + g + "/" + b);
						matrix->setTextColor(matrix->Color(r, g, b));
						for (int y = 0; y < (int)tempString.length(); y++)
						{
							myText += (char)tempString[y];
						}
						matrix->print(utf8ascii(myText));
					}
				}
			}
			break;
		}
		case 22:
		{
			//Text
			//scrollSpeed
			//icon
			//color
			//multicolor (textData?)
			//moveIcon
			//repeatIcon
			//duration
			//repeat
			//rainbow
			//progress
			//progresscolor
			//progressBackgroundColor
			//soundfile

			String myJSON = "";
			for (int i = 1; i < length; i++)
			{
				myJSON += (char)payload[i];
			}
			DynamicJsonBuffer jsonBuffer;
			JsonObject &json = jsonBuffer.parseObject(myJSON);

			String tempString = json["text"];
			String colorString = json["color"];

			JsonArray &color = jsonBuffer.parseArray(colorString);
			int r = color[0];
			int g = color[1];
			int b = color[2];
			int scrollSpeed = (int)json["scrollSpeed"];

			Serial.println("Scrollspeed: " + (String)(scrollSpeed));

			int textlaenge;
			while (true)
			{
				matrix->setCursor(32, 6);
				matrix->print(utf8ascii(tempString));
				textlaenge = (int)matrix->getCursorX() - 32;
				for (int i = 31; i > (-textlaenge); i--)
				{
					int starzeit = millis();
					matrix->clear();
					matrix->setCursor(i, 6);
					matrix->setTextColor(matrix->Color(r, g, b));
					matrix->print(utf8ascii(tempString));
					matrix->show();
					client.loop();
					int endzeit = millis();
					if ((scrollSpeed + starzeit - endzeit) > 0)
					{
						delay(scrollSpeed + starzeit - endzeit);
					}
				}
				connectionTimout = millis();
				break;
			}
			break;
		}
		case 23:
		{
			//Command 23: DrawFilledRect

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1] << 8) + int(payload[2]);
			uint16_t y0_coordinate = int(payload[3] << 8) + int(payload[4]);
			int16_t width = payload[5];
			int16_t height = payload[6];
			matrix->fillRect(x0_coordinate, y0_coordinate, width, height, matrix->Color(payload[7], payload[8], payload[9]));
			break;
		}
		case 24:
		{
			myMP3.playFolder(payload[1], payload[2]);
			myMP3.loopFolder(payload[1]);
			break;
		}
		case 25:
		{
			myMP3.next();
			break;
		}
		case 26:
		{
			myMP3.previous();
			break;
		}
		case 27:
		{
			myMP3.stop();
			break;
		}
		case 30:
		{
			saveLiteConfig();
			break;
		}
		case 31:
		{
			apcConfigDef.liteMode = payload[1];
			saveConfig();
			ESP.reset();	
			break;
		}
		}
	}
}

void callback(char *topic, byte *payload, unsigned int length)
{
	WIFIConnection = true;
	updateMatrix(payload, length);
}

void reconnect()
{
	String clientId = "AWTRIXController-";
	clientId += String(random(0xffff), HEX);
	hardwareAnimatedSearch(1, 28, 0);
	if (client.connect(clientId.c_str()))
	{
		client.subscribe("awtrixmatrix/#");
		client.publish("matrixClient", "connected");
	}
}

void ICACHE_RAM_ATTR interruptRoutine()
{
	isr_flag = 1;
}

void handleGesture()
{
	String control;
	if (apds.isGestureAvailable())
	{
		switch (apds.readGesture())
		{
		case DIR_UP:
			control = "UP";
			break;
		case DIR_DOWN:
			control = "DOWN";
			break;
		case DIR_LEFT:
			control = "LEFT";
			break;
		case DIR_RIGHT:
			control = "RIGHT";
			break;
		case DIR_NEAR:
			control = "NEAR";
			break;
		case DIR_FAR:
			control = "FAR";
			break;
		default:
			control = "NONE";
		}
		StaticJsonBuffer<200> jsonBuffer;
		JsonObject &root = jsonBuffer.createObject();
		root["type"] = "gesture";
		root["gesture"] = control;
		String JS;
		root.printTo(JS);
		sendToServer(JS);
	}
}

uint32_t Wheel(byte WheelPos, int pos)
{
	if (WheelPos < 85)
	{
		return matrix->Color((WheelPos * 3) - pos, (255 - WheelPos * 3) - pos, 0);
	}
	else if (WheelPos < 170)
	{
		WheelPos -= 85;
		return matrix->Color((255 - WheelPos * 3) - pos, 0, (WheelPos * 3) - pos);
	}
	else
	{
		WheelPos -= 170;
		return matrix->Color(0, (WheelPos * 3) - pos, (255 - WheelPos * 3) - pos);
	}
}

void flashProgress(unsigned int progress, unsigned int total)
{
	matrix->setBrightness(80);
	long num = 32 * 8 * progress / total;
	for (unsigned char y = 0; y < 8; y++)
	{
		for (unsigned char x = 0; x < 32; x++)
		{
			if (num-- > 0)
				matrix->drawPixel(x, 8 - y - 1, Wheel((num * 16) & 255, 0));
		}
	}
	matrix->setCursor(1, 6);
	matrix->setTextColor(matrix->Color(200, 200, 200));
	matrix->print("FLASHING");
	matrix->show();
}

void saveConfigCallback()
{
	if (!USBConnection)
	{
		Serial.println("Should save config");
	}
	shouldSaveConfig = true;
}

void configModeCallback(WiFiManager *myWiFiManager)
{

	if (!USBConnection)
	{
		Serial.println("Entered config mode");
		Serial.println(WiFi.softAPIP());
		Serial.println(myWiFiManager->getConfigPortalSSID());
	}
	matrix->clear();
	matrix->setCursor(3, 6);
	matrix->setTextColor(matrix->Color(0, 255, 50));
	matrix->print("Hotspot");      //热点功能开启
	matrix->show();
}

void baseInit()
{
	delay(2000);
	Serial.setRxBufferSize(1024);
	Serial.begin(115200);
	mySoftwareSerial.begin(9600);

	if (SPIFFS.begin())
	{
		//if file not exists
		if (!(SPIFFS.exists("/awtrix.json")))
		{
			SPIFFS.open("/awtrix.json", "w+");
		}

		File configFile = SPIFFS.open("/awtrix.json", "r");
		if (configFile)
		{
			size_t size = configFile.size();
			// Allocate a buffer to store contents of the file.
			std::unique_ptr<char[]> buf(new char[size]);
			configFile.readBytes(buf.get(), size);
			DynamicJsonBuffer jsonBuffer;
			JsonObject &json = jsonBuffer.parseObject(buf.get());
			if (json.success())
			{

				strcpy(awtrix_server, json["awtrix_server"]);
				MatrixType2 = json["MatrixType"].as<bool>();
				apcConfigDef.liteMode = json["LiteMode"].as<bool>();
				apcConfigDef.offLine = json["OffLine"].as<bool>();
				matrixTempCorrection = json["matrixCorrection"].as<int>();

				if (json.containsKey("Port"))
				{
					strcpy(Port, json["Port"]);
				}
			}
			configFile.close();
		}

		if (!(SPIFFS.exists("/LiteConfig.json")))
		{
			SPIFFS.open("/LiteConfig.json", "w+");
		}
		File liteConfigFile = SPIFFS.open("/LiteConfig.json", "r");
		if (liteConfigFile)
		{
			size_t size = liteConfigFile.size();
			// Allocate a buffer to store contents of the file.
			std::unique_ptr<char[]> buf(new char[size]);
			liteConfigFile.readBytes(buf.get(), size);
			DynamicJsonBuffer jsonBuffer;
			JsonObject &json = jsonBuffer.parseObject(buf.get());
			if (json.success())
			{
				apcConfigDef.alarm_enable = json["alarm_enable"].as<bool>();
				apcConfigDef.volume = json["volume"].as<int>();
				apcConfigDef.brightness = json["brightness"].as<int>();
				strcpy(apcConfigDef.alarm_time, json["alarm_time"]);
				strcpy(apcConfigDef.cdd_date, json["cdd_date"]);
			}else
			{
				apcConfigDef.alarm_enable = false;
				apcConfigDef.volume = 15;
				apcConfigDef.brightness = 80;
				strcpy(apcConfigDef.alarm_time, "08:00");
				strcpy(apcConfigDef.cdd_date, "2022-12-31");
			}
			liteConfigFile.close();
		}
	}
	else
	{
		//error
	}
}
void matrixInit()
{
	Serial.println("Matrix Type");

	if (!MatrixType2)
	{
		matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
	}
	else
	{
		matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
	}

	switch (matrixTempCorrection)
	{
	case 0:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setCorrection(TypicalLEDStrip);
		break;
	case 1:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(Candle);
		break;
	case 2:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(Tungsten40W);
		break;
	case 3:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(Tungsten100W);
		break;
	case 4:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(Halogen);
		break;
	case 5:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(CarbonArc);
		break;
	case 6:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(HighNoonSun);
		break;
	case 7:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(DirectSunlight);
		break;
	case 8:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(OvercastSky);
		break;
	case 9:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(ClearBlueSky);
		break;
	case 10:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(WarmFluorescent);
		break;
	case 11:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(StandardFluorescent);
		break;
	case 12:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(CoolWhiteFluorescent);
		break;
	case 13:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(FullSpectrumFluorescent);
		break;
	case 14:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(GrowLightFluorescent);
		break;
	case 15:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(BlackLightFluorescent);
		break;
	case 16:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(MercuryVapor);
		break;
	case 17:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(SodiumVapor);
		break;
	case 18:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(MetalHalide);
		break;
	case 19:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(HighPressureSodium);
		break;
	case 20:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setTemperature(UncorrectedTemperature);
		break;
	default:
		FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setCorrection(TypicalLEDStrip);
		break;
	}

	matrix->begin();
	matrix->setTextWrap(false);
	matrix->setBrightness(80);
	matrix->setFont(&TomThumb);
}
static const char *serverConfigHtmlBegin ICACHE_RODATA_ATTR =
	"<html><head><title>AwtrixLite</title>																		\
<meta name='viewport' 																						\
content='width=device-width, initial-scale=1.0, minimum-scale=0.5, maximum-scale=2.0, user-scalable=yes' /> \
<style>																										\
ol{padding:0;margin:0;list-style:none;}																		\
ol li{padding:0;clear:both;margin:0 0 10px 0;}																\
label{float:left;width:150px;}</style>																		\
</head>																										\
<body>																										\
<form method='post'action='/config/post'>																	\
<fieldset id='Personal_information'>																		\
<ol>";

static const char *serverConfigHtmlEnd ICACHE_RODATA_ATTR =
	"<li><center><input type='submit'value='Submit'/></center></li>												\
</ol>																										\
</fieldset></form></body></html><div style='display:none'>";
void netInit()
{
	// 	if(!apcConfigDef.liteMode)
	// {
	wifiManager.setAPStaticIPConfig(IPAddress(172, 217, 28, 1), IPAddress(172, 217, 28, 1), IPAddress(255, 255, 255, 0));
	WiFiManagerParameter custom_awtrix_server("server", "AWTRIX Host", awtrix_server, 16);
	WiFiManagerParameter custom_port("Port", "Matrix Port", Port, 5);
	WiFiManagerParameter p_MatrixType2("MatrixType2", "MatrixType 2", "T", 2, "type=\"checkbox\" ", WFM_LABEL_BEFORE);
	WiFiManagerParameter p_LiteMode("LiteMode", "Offline Mode", "T", 2, "type=\"checkbox\" ", WFM_LABEL_BEFORE);
	WiFiManagerParameter p_OffLine("OffLine", "Deep Offline", "T", 2, "type=\"checkbox\" ", WFM_LABEL_BEFORE);
	//WiFiManagerParameter p_OffLine("OffLine", "Deep Offline", "T", 2, "type=\"checkbox\" ", WFM_LABEL_BEFORE);
	// Just a quick hint
	// WiFiManagerParameter p_hint("<small>Please configure your AWTRIX Host IP (without Port), and check MatrixType 2 if the arrangement of the pixels is different<br></small><br><br>");
	WiFiManagerParameter p_lineBreak_notext("<p></p>");
	// WiFiManagerParameter p_lineBreak_text("<p>Choose one of the following three:</p>");
	wifiManager.setSaveConfigCallback(saveConfigCallback);
	wifiManager.setAPCallback(configModeCallback);

	wifiManager.addParameter(&custom_awtrix_server);
	wifiManager.addParameter(&custom_port);
	wifiManager.addParameter(&p_lineBreak_notext);
	wifiManager.addParameter(&p_MatrixType2);
	wifiManager.addParameter(&p_lineBreak_notext);
	wifiManager.addParameter(&p_LiteMode);
	wifiManager.addParameter(&p_OffLine);
	//wifiManager.setCustomHeadElement("<style>html{ background-color: #607D8B;}</style>");

	hardwareAnimatedSearch(0, 24, 0);

	if (!apcConfigDef.liteMode && !wifiManager.autoConnect("像素时钟 By ZHANGZHUO", "xiangsushizhong"))
	{
		//reset and try again, or maybe put it to deep sleep
		ESP.reset();
		delay(5000);
	}

	//is needed for only one hotpsot!
	WiFi.mode(WIFI_STA);

	server.on("/", HTTP_GET, []() {
		server.sendHeader("Connection", "close");
		server.send(200, "text/html", serverIndex);
	});

	server.on("/config", HTTP_GET, []() {
		server.sendContent_P(serverConfigHtmlBegin);

		server.sendContent_P("<li><label>Alarm Time</label><input type='checkbox'name='alarm_enable'");
		if (apcConfigDef.alarm_enable)
		{
			server.sendContent_P("checked");
		}
		server.sendContent_P("/><input type='time'value='");
		server.sendContent(apcConfigDef.alarm_time);
		server.sendContent_P("'name='alarm_time'/></li>");

		server.sendContent_P("<li><label>CountDown Day</label><input type='date'value='");
		server.sendContent(apcConfigDef.cdd_date);
		server.sendContent_P("'name='cdd_date'/></li>");

		server.sendContent_P(serverConfigHtmlEnd);
		server.send_P(200, "text/html", "</div>");
	});

	server.on("/config/post", HTTP_POST, []() {
		server.sendHeader("Connection", "close");
		server.send(200, "text/html", "OK!");
		// Serial.println(server.arg("alarm_time"));
		// Serial.println(server.arg("alarm_enable"));
		// Serial.println(server.arg("cdd_date"));
		apcConfigDef.alarm_enable = server.arg("alarm_enable") == "on" ? true : false;
		strcpy(apcConfigDef.alarm_time, server.arg("alarm_time").c_str());
		strcpy(apcConfigDef.cdd_date, server.arg("cdd_date").c_str());
		saveLiteConfig();
	});

	server.on(
		"/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart(); }, []() {
      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);

        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
		  matrix->clear();
		  flashProgress((int)upload.currentSize,(int)upload.buf);
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
		  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");


        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield(); });

	server.begin();

	if (shouldSaveConfig)
	{
		strcpy(awtrix_server, custom_awtrix_server.getValue());
		MatrixType2 = (strncmp(p_MatrixType2.getValue(), "T", 1) == 0);
		apcConfigDef.liteMode = (strncmp(p_LiteMode.getValue(), "T", 1) == 0);
		apcConfigDef.offLine = (strncmp(p_OffLine.getValue(), "T", 1) == 0);
		strcpy(Port, custom_port.getValue());
		//USBConnection = (strncmp(p_USBConnection.getValue(), "T", 1) == 0);
		saveConfig();
		ESP.reset();
	}

	hardwareAnimatedCheck(0, 27, 2);

	delay(1000); //is needed for the dfplayer to startup
}
void hardwareInit()
{
	//Checking periphery
	Wire.begin(I2C_SDA, I2C_SCL);
	if (BMESensor.begin())
	{
		//temp OK
		tempState = 1;
		hardwareAnimatedCheck(2, 29, 2);
	}
	if (htu.begin())
	{
		tempState = 2;
		hardwareAnimatedCheck(2, 29, 2);
	}
	if (Sensor.Begin())
	{
		tempState = 4;
		//Serial.println("sht30就绪");
		hardwareAnimatedCheck(2, 29, 2);
	}

	dht.begin();
	if (dht.read())
	{
		tempState = 3;
		hardwareAnimatedCheck(7, 29, 2);
	}

	if (myMP3.begin(mySoftwareSerial))
	{ //Use softwareSerial to communicate with mp3.
	    myMP3.volume(5);
		myMP3.playFolder(2, 1);
		hardwareAnimatedCheck(3, 29, 2);
	}

	attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);
	apds.enableGestureSensor(true);
	if (apds.init())
	{
		hardwareAnimatedCheck(4, 29, 2);
		pinMode(APDS9960_INT, INPUT);
	}

	photocell.setPhotocellPositionOnGround(false);
	if (photocell.getCurrentLux() > 1)
	{
		hardwareAnimatedCheck(5, 29, 2);
	}

	if (rtc.begin())
	{
		if (!rtc.isrunning())
		{
			rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
		}
		hardwareAnimatedCheck(6, 29, 2);
	}
}

void setup()
{
	baseInit();
	matrixInit();
	checkReset();
	netInit();
	hardwareInit();

	matrix->clear();
	matrix->setCursor(7, 6);
	bufferpointer = 0;
	myTime = millis() - 500;
	myTime2 = millis() - 1000;
	myTime3 = millis() - 500;
	myCounter = 0;
	myCounter2 = 0;
	if (!apcConfigDef.liteMode)
	{
		for (int x = 32; x >= -90; x--)
		{
			matrix->clear();
			matrix->setCursor(x, 6);
			matrix->print("Host-IP: " + String(awtrix_server) + ":" + String(Port));
			matrix->setTextColor(matrix->Color(0, 255, 50));
			matrix->show();
			delay(60);
		}
		client.setServer(awtrix_server, atoi(Port));
		client.setCallback(callback);
	}
	else
	{
		for (int x = 32; x >= -100; x--)
		{
			matrix->clear();
			
			matrix->setCursor(x, 6);
			//delay(400);
			matrix->print("LiteMode IP: " + WiFi.localIP().toString());
			//Serial.println("LiteMode IP: " + WiFi.localIP().toString());
			matrix->setTextColor(matrix->Color(0, 255, 50));
			matrix->show();
			delay(40);
		}
		APC.systemInit(callback, &rtc, &apcConfigDef);
	}
	pinMode(D0, INPUT);
	pinMode(D0, INPUT_PULLUP);
	pinMode(D4, INPUT);
	pinMode(D4, INPUT_PULLUP);
	pinMode(D8, INPUT);
	ignoreServer = false;
	connectionTimout = millis();
}

void onLineModeLoop()
{
	if (firstStart && !ignoreServer)
	{
		if (millis() - myTime > 500)
		{
			serverSearch(myCounter, 0, 28, 0);
			myCounter++;
			if (myCounter == 4)
			{
				myCounter = 0;
			}
			myTime = millis();
		}
	}
	// not during the falsh process
	if (!updating)
	{
		//Wifi
		if (WIFIConnection || firstStart)
		{
			//Serial.println("wifi oder first...");
			if (!client.connected())
			{
				//Serial.println("nicht verbunden...");
				reconnect();
				if (WIFIConnection)
				{
					USBConnection = false;
					WIFIConnection = false;
					firstStart = true;
				}
			}
			else
			{
				client.loop();
			}
		}
	}
}

void loop()
{
	// APC.ramCheck("loop begin");
	server.handleClient();
	//is needed for the server search animation
	// Serial.println(photocell.getCurrentLux());

	if (apcConfigDef.liteMode)
	{
		APC.apcLoop();
	}
	else
	{
		onLineModeLoop();
	}

	if (!updating)
	{
		//check gesture sensor
		if (isr_flag == 1)
		{
			detachInterrupt(APDS9960_INT);
			handleGesture();
			isr_flag = 0;
			attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);
		}
		if (millis() - connectionTimout > 20000 && !apcConfigDef.liteMode)
		{
			USBConnection = false;
			WIFIConnection = false;
			firstStart = true;
		}
	}
	checkTaster(0);
	checkTaster(1);
	checkTaster(2);
	// APC.ramCheck("loop end");
}