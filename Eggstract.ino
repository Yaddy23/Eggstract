#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>

#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define FIREBASE_HOST "testeggstract-d27ef-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "Xx6Z3zvSViWrPMPsTy3OhbKwdD0icRqRhtyVFx5d"
// #define WIFI_SSID "Yad"
// #define WIFI_PASSWORD "Yad12345"

#define API_KEY "AIzaSyCv0-F2zacFaFeiTwtxfzhO90oEWaqwlpg"

FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

unsigned long startTime;
unsigned long endTime;

unsigned long sendDataPrevMillis = 0;
unsigned long printinlcd = 0;

float temp, humid;


//Incubator
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
#include <DHT.h>
#include <string.h>

#define DHTPIN D6  //humid
#define DHTTYPE DHT21
// Data wire is plugged into digital pin 2 on the Arduino
#define ONE_WIRE_BUS D7  // temp
// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);
// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);
DHT dht(DHTPIN, DHTTYPE);


const unsigned long EndOfIncubation = 1814400000;         // 21 days
const unsigned long EndRotationUntilSetDay = 1555200000;  // 18 days
const unsigned long TiltDuration = 10000;                 // 10 secs
const unsigned long StartTiltInSetTime = 7200000;         // 2 hours
const unsigned long Days = 86400000;                      // 1 Day 
const unsigned long restartNode = 3000000;                // 50 minutes

unsigned long referenceTime = 0;

int DaysCount = 0;

unsigned long previousMillis2 = 0; // for egg tilting
unsigned long previousMillis3 = 0; // for day counting
unsigned long currentMillis = 0;
String getCurrentMillis, getPreviousMillis2, getPreviousMillis3;

const int led = 13;

int start = 0;
WiFiManager wm;

void setup() {
  
  Serial.begin(115200);
  pinMode(D3, OUTPUT);  
  digitalWrite(D3, LOW);                                                                 
  pinMode(D4, OUTPUT);
  digitalWrite(D4, LOW);     
  pinMode(D5, OUTPUT);
  digitalWrite(D5, HIGH);     

  lcd.begin();
  lcd.backlight();
  dht.begin();
  sensors.begin();
  
  //wm.resetSettings();

  ESP.wdtDisable(); // Disable the watchdog timer to avoid a reset during setup
  ESP.wdtEnable(WDTO_8S); // Enable the watchdog timer with an 8 second timeout

  bool res;
  
  res = wm.autoConnect("EggstractNode","Eggstract");

  if(!res) {
    //ESP.restart();
  }else{

    Serial.println("Connected to Wifi: " + wm.getWiFiSSID());

    
    config.api_key = API_KEY;

    config.database_url = FIREBASE_HOST;

    if (Firebase.signUp(&config, &auth, "", "")) {
    // Serial.println("ok");
    signupOK = true;
    
    }else{
    // Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }

    config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

    Firebase.begin(&config, &auth);   
    Firebase.reconnectWiFi(true);  
    
    Firebase.RTDB.setString(&fbdo, "Eggstract/CameraIP", "192.161.1.11"); 
  }

}

void loop() {
  startTime = millis();

  sensors.requestTemperatures(); 
  temp = sensors.getTempCByIndex(0);
  humid = dht.readHumidity();

  if(startTime >= restartNode){
    ESP.restart();
  }

  ESP.wdtFeed();

  if (Firebase.RTDB.getString(&fbdo, "Eggstract/StartIncubate")) start = fbdo.stringData().toInt();  

  if (Firebase.RTDB.getString(&fbdo, "Eggstract/Days")) DaysCount = fbdo.stringData().toInt();
  
  testLCD();

  TempHumidifier();
  FirebaseSendData();  
  
  if(start != 0){

    if (Firebase.RTDB.getString(&fbdo, "Eggstract/CurrentMillis")) currentMillis = strtoul(fbdo.stringData().c_str(), NULL, 10); 
                        
    if (Firebase.RTDB.getString(&fbdo, "Eggstract/previousMillisEggTilt")) previousMillis2 = strtoul(fbdo.stringData().c_str(), NULL, 10); 

    if (Firebase.RTDB.getString(&fbdo, "Eggstract/previousMillisDayCount")) previousMillis3 = strtoul(fbdo.stringData().c_str(), NULL, 10); 

    Eggstracting();  
  }
  
}

void testLCD(){
  if(millis() - printinlcd > 5000 || printinlcd == 0){
    printinlcd = millis();

    lcd.clear();
    if(isnan(humid)) 
    {
      lcd.setCursor(0,0);
      lcd.print(F("FAILED TO READ"));
      lcd.setCursor(0,1);
      lcd.print(F("FROM DHT SENSOR"));
    }else{
      lcd.setCursor(0,0);
      lcd.print("Temp: ");
      lcd.print(temp);
      lcd.print((char)223);
      lcd.print("C");
      lcd.setCursor(0,1);
      lcd.print(F("Humid: "));
      lcd.print(humid);
      lcd.print("%");
    }
  }
}

void FirebaseSendData(){

  if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 2000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
        
    Firebase.RTDB.setFloat(&fbdo, "Eggstract/Temp", temp);

    Firebase.RTDB.setFloat(&fbdo, "Eggstract/Humid", humid);
  }
}

void Eggstracting(){
  
  endTime = millis();
  unsigned long elapsedTime = endTime - startTime;
  currentMillis += elapsedTime;
  
  Firebase.RTDB.setString(&fbdo, "Eggstract/CurrentMillis", currentMillis);
  
  if(currentMillis <= EndRotationUntilSetDay){

    if(currentMillis - previousMillis2 >= StartTiltInSetTime) { digitalWrite(D5, LOW); } //ON
    
    
    if(currentMillis - previousMillis2 >= StartTiltInSetTime + 10000){ //OFF AFTER 10 SECONDS
      digitalWrite(D5, HIGH); //OFF
    
      previousMillis2 = currentMillis;
      Firebase.RTDB.setString(&fbdo, "Eggstract/previousMillisEggTilt", previousMillis2);
      ESP.restart();
    }
      
  } 
  if(currentMillis >= EndOfIncubation){
        
    digitalWrite(D3, HIGH); // OFF
    digitalWrite(D4, HIGH); // OFF
    digitalWrite(D5, HIGH); // OFF
      
    ResetIncubator();
    
  }

  if(currentMillis - previousMillis3 >= Days){  
    DaysCount = DaysCount + 1;
    
    String getDays = String(DaysCount);
    Firebase.RTDB.setString(&fbdo, "Eggstract/Days", getDays);
    
    previousMillis3 = currentMillis;
    Firebase.RTDB.setString(&fbdo, "Eggstract/previousMillisDayCount", previousMillis3);
  }
}

void ResetIncubator(){

  Firebase.RTDB.setString(&fbdo, "Eggstract/Days", "0");
  
  Firebase.RTDB.setString(&fbdo, "Eggstract/StartIncubate", "0");

  Firebase.RTDB.setString(&fbdo, "Eggstract/CurrentMillis", "0");

  Firebase.RTDB.setString(&fbdo, "Eggstract/previousMillisDayCount", "0");

  Firebase.RTDB.setString(&fbdo, "Eggstract/previousMillisEggTilt", "0");

}

void TempHumidifier(){
  
  digitalWrite(D4, (temp >= 38) ? HIGH : LOW);
  
  if(DaysCount <= 18) digitalWrite(D3, (humid <= 55) ? HIGH : ((humid >= 60) ? LOW : HIGH)); // humid for day 1-18
  
  if(DaysCount >= 19) digitalWrite(D3, (humid <= 59) ? HIGH : ((humid >= 65) ? LOW : HIGH)); // humid for day 19-21    
}
