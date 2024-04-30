#include <Wire.h>
#include <WiFi.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SD.h>
#include <String.h>
#include <Firebase_ESP_Client.h>
#include <TinyGPS++.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

int16_t ax, ay, az;
MPU6050 mpu;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
TinyGPSPlus gps;
struct MyData {  
  float X;
  float Y;
};
MyData data;
#define WIFI_SSID "PLDTHOMEFIBRzYWbH"
#define WIFI_PASSWORD "February#141725"                                                                 
#define API_KEY "AIzaSyBQaVLtSryZBpSV_cIeCzkgpfglvnftw20"
#define USER_EMAIL "erldhei@gmail.com"
#define USER_PASSWORD "arcsabaody42"
#define DATABASE_URL "https://prj-safestride-ff-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define Buzzer 2
#define GPS_BAUDRATE 9600


FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;
String gpsPath, mpuPath, batteryPath, idPath, valePath;
String xPath = "/X";
String yPath = "/Y";
String batPath= "/status";
String IDPath = "/ID";
String gpslatPath = "/Latitude";
String gpslongPath = "/Longitude";
String latPath ="/Latitude";
String longPath="/Longitude";
String datePath = "/Date";
String timePath = "/Time";
String formattedDate, formattedTime;

String parentGPS, parentMPU;


int vale = 0, battery, real=0, sec=0;
float x, y;
float latitude;
float longitude;
float deflat = 14.327476145509364;
float deflong = 120.94339924643688;
String serialid = "SS-01";
unsigned long mpuLastRunTime = 0;
unsigned long gpsLastRunTime = 0;
const unsigned long mpuInterval = 5000;  
const unsigned long gpsInterval = 1000;
const unsigned long gpsProcessingTimeLimit = 5000;

FirebaseJson json;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.pagasa.dost.gov.ph");

void initWiFi() 
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");

    while (WiFi.status() != WL_CONNECTED) 
    {
      Serial.print('.');
      delay(1000);
    }

  Serial.println(WiFi.localIP());
  Serial.println();
  timeClient.setTimeOffset(28800);
  timeClient.begin();
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(GPS_BAUDRATE);
  Wire.begin();
  initWiFi();
  config.api_key = API_KEY;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;

  Firebase.begin(&config, &auth);

  Serial.println("Getting User UID");

    while ((auth.token.uid) == "") 
    {
      Serial.print('.');
      delay(1000);
    }

  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  mpuPath = "/UsersData/" + uid + "/fallDetection";
  gpsPath = "/UsersData/" + uid + "/gpsReadings";
  batteryPath = "/UsersData/" + uid + "/Status";
  idPath = "/UsersData/" + uid + "/ID";
  valePath = "/UsersData/" + uid + "/valehistory";

  if (Firebase.RTDB.getFloat(&fbdo, valePath.c_str())) {
    vale = fbdo.floatData();
    vale = vale +  1;
    Serial.print("Retrieved current vale value from Firebase: ");
    Serial.println(vale);
  } else {
    Serial.println("Failed to retrieve current vale value from Firebase. Using default value (0).");
  }
  pinMode(Buzzer, OUTPUT);
  ledcSetup(0, 500, 8);
  mpu.initialize();
  offsets();
}

void offsets(){
    delay(1000);
  
  // Read accelerometer offsets
  mpu.setXAccelOffset(-432);
  mpu.setYAccelOffset(169);

}

void loop() {
  timeClient.update();
 
  Firebase.RTDB.setString(&fbdo,(idPath + "/" + IDPath).c_str(), serialid);
  timegetter();
if (millis() - mpuLastRunTime >= mpuInterval) {
    mpugetter();
    mpuLastRunTime = millis();
  }
  if (millis() - gpsLastRunTime >= gpsInterval) {
    readGPS();
    gpsLastRunTime = millis();
  }
  delay(1000);
}
void timegetter(){
  formattedTime = timeClient.getFormattedTime();
  int separatorIndex = formattedTime.indexOf(':');  // Find the first colon
  String hours = formattedTime.substring(0, separatorIndex);
  String minutes = formattedTime.substring(separatorIndex + 1, separatorIndex + 3);

  int hoursInt = hours.toInt();
  String ampm = (hoursInt < 12) ? "AM" : "PM";
  hoursInt = (hoursInt % 12 == 0) ? 12 : hoursInt % 12;

  formattedTime = String(hoursInt) + ":" + minutes + " " + ampm;
  formattedDate = timeClient.getFormattedDate();
  int spaceIndex = formattedDate.indexOf('T');
  formattedDate = formattedDate.substring(0, spaceIndex);
  int year = formattedDate.substring(0, 4).toInt();
  int month = formattedDate.substring(5, 7).toInt();
  int day = formattedDate.substring(8, 10).toInt();
  formattedDate = String(month) + "/" + String(day) + "/" + String(year % 100);
}
void mpugetter(){
  mpu.getAcceleration(&ax, &ay, &az);
  data.X = static_cast<float>(ax) / 16384.0; // X axis data
  data.Y = static_cast<float>(ay) / 16384.0; // Y axis data
  x = data.X;
  y = data.Y; 
  Serial.print("X: ");
  Serial.print(data.X);
  Serial.print(F(" ,"));
  Serial.print("Y: ");
  Serial.println(data.Y);

  if(data.X > 0.75 || data.X < -0.75 || data.Y > 0.75 || data.Y < -0.75){
    tone(Buzzer, 1000, 700);
    tone(Buzzer, 900, 100);
    passtoFire(x, y);
    delay(500);
    noTone(Buzzer);
  }
}
void passtoFire(float x, float y){
  String valeVal = String(vale);
    parentMPU = mpuPath + "/" + valeVal;
    Firebase.RTDB.setFloat(&fbdo, (mpuPath + "/" + valeVal + xPath).c_str(), x);
    Firebase.RTDB.setFloat(&fbdo, (mpuPath + "/" + valeVal + yPath).c_str(), y);
    if( latitude != 0.0 && longitude != 0.0){
        Firebase.RTDB.setFloat(&fbdo, (mpuPath + "/" + valeVal + latPath).c_str(), latitude);
        Firebase.RTDB.setFloat(&fbdo, (mpuPath + "/" + valeVal + longPath).c_str(), longitude);
    } 
    else{
        Firebase.RTDB.setFloat(&fbdo, (mpuPath + "/" + valeVal + latPath).c_str(), deflat);
        Firebase.RTDB.setFloat(&fbdo, (mpuPath + "/" + valeVal + longPath).c_str(), deflong);
    } 
    Firebase.RTDB.setString(&fbdo, (mpuPath + "/" + valeVal + timePath).c_str(), formattedTime);
    Firebase.RTDB.setString(&fbdo, (mpuPath + "/" + valeVal + datePath).c_str(), formattedDate);
    Firebase.RTDB.setFloat(&fbdo, valePath.c_str(), vale);
    vale++;
}
void readGPS()
{

  
  while (Serial2.available() > 0){
    if (gps.encode(Serial2.read())) {
      if (gps.location.isValid()) {
        latitude = gps.location.lat(), 10;
        longitude = gps.location.lng(), 10;
        Serial.println(latitude, 10);
        Serial.println(longitude, 10);
        Firebase.RTDB.setFloat(&fbdo, (gpsPath + "/" + gpslatPath).c_str(), latitude);
        Firebase.RTDB.setFloat(&fbdo, (gpsPath + "/" + gpslongPath).c_str(), longitude);
     
      } else {
  
        Firebase.RTDB.setFloat(&fbdo, (gpsPath + "/" + gpslongPath).c_str(), deflat);
        Firebase.RTDB.setFloat(&fbdo, (gpsPath + "/" + gpslongPath).c_str(), deflong);
            delay(2500);
      }
    }

   if (millis() - gpsLastRunTime >= gpsProcessingTimeLimit) {
      break;
    }
  }  
  
  delay(5000);

}