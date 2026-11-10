#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Firebase_ESP_Client.h>
#include <time.h>


// ====================== WIFI SETTINGS ======================
#define WIFI_SSID "****"        
#define WIFI_PASSWORD "****"    


// ====================== FIREBASE SETTINGS ==================
// API Key and Database URL required for ESP32 to communicate with Firebase RTDB
#define API_KEY "API KEY"  
#define DATABASE_URL "FIREBASE URL"


// Global objects for Firebase communication and configuration
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// ====================== NTP TIME SETTINGS ======================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;   // Set to UTC+8 (Malaysia/Singapore Time)
const int daylightOffset_sec = 0;      // No daylight savings in Malaysia


// ====================== HARDWARE PINS ======================
#define BUZZER 21
#define PIR 33
#define LED 13


// RC522 RFID Module Pins (SPI Communication)
#define SS_PIN 5
#define RST_PIN 22
MFRC522 mfrc522(SS_PIN, RST_PIN);


// LCD Initialization (I2C Address 0x27, 16 Columns, 2 Rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);


bool systemActive = false;


// ====================== SETUP: EXECUTED ONCE =============================
void setup() {
  Serial.begin(115200);


  // Initialize Peripheral Pins
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH); // Set High to keep Active-Low buzzer silent initially
  pinMode(PIR, INPUT);
  pinMode(LED, OUTPUT);


  // Initialize I2C LCD
  Wire.begin(4, 15); // Define Custom SDA (4) and SCL (15) for ESP32
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("   ATTENDLY   ");
  lcd.setCursor(0,1);
  lcd.print("  System Ready  ");


  // Initialize SPI Bus and RFID Reader
  SPI.begin(18, 19, 23, 5); // SCK, MISO, MOSI, SS
  mfrc522.PCD_Init();


  // Attempt to connect to the local Wi-Fi network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
 
  // Configure Firebase Credentials and auto-reconnect logic
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;


  // Sign up as an anonymous user for database access
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signup OK");
  } else {
    Serial.printf("Signup failed: %s\n", config.signer.signupError.message.c_str());
  }


  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);


  delay(2000);
  lcd.clear();


  // Sync System Time with NTP Server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


  Serial.print("Syncing time");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nTime synced!");


  // LOG SYSTEM START TIME: Records when the device was turned on for the current date
  if (getLocalTime(&timeinfo)) {
    char dateStr[11], timeStr[9];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);


    String path = "/classStart/" + String(dateStr) + "/startTime";
    Firebase.RTDB.getString(&fbdo, path);
   
    // Only log start time if it hasn't been logged today yet
    if (fbdo.dataType() == "null") {
      if (Firebase.RTDB.setString(&fbdo, path, String(timeStr))) {
        Serial.println("Success! First log for today: " + String(timeStr));
      }
    } else {
      Serial.print("Already exists. Today's start time is: ");
      Serial.println(fbdo.stringData());
    }
  }
}


// ====================== LOOP: MAIN LOGIC ==============================
void loop() {
  // STEP 1: Motion Detection Logic
  // The system only prompts for a card if someone is standing in front of it
  if(digitalRead(PIR) == HIGH){
    systemActive = true;
    digitalWrite(LED, HIGH);
    lcd.setCursor(0,0);
    lcd.print("Motion Detected ");
    lcd.setCursor(0,1);
    lcd.print("Scan your card  ");
  } else {
    systemActive = false;
    digitalWrite(LED, LOW);
    lcd.setCursor(0,0);
    lcd.print("No Motion       ");
    lcd.setCursor(0,1);
    lcd.print("System Idle     ");
    delay(500);
    return; // Skip the rest of the loop if no motion is detected
  }


  // STEP 2: Check for RFID Card
  if(!mfrc522.PICC_IsNewCardPresent()) return; // Exit if no card is near the reader
  if(!mfrc522.PICC_ReadCardSerial()) return;    // Exit if the card cannot be read


  beep(1); // Short beep to confirm physical contact with card


  // STEP 3: Process Scanned Card
  String scannedUID = uidToString(mfrc522.uid.uidByte);
  String studentPath = "/students/" + scannedUID;
 
  lcd.clear();
  lcd.print("Verifying...");


  // Query Firebase to check if this UID is registered in the "students" node
  if (Firebase.RTDB.getJSON(&fbdo, studentPath)) {
    if (fbdo.dataType() == "json") {
      FirebaseJsonData jsonData;
      FirebaseJson &json = fbdo.jsonObject();
     
      // Extract the "name" field from the student's data
      json.get(jsonData, "name");
      String studentName = jsonData.stringValue;


      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Attendance taken:");
      lcd.setCursor(0,1);
      lcd.print(studentName);


      digitalWrite(LED, HIGH);
      beep(2); // Double beep for success


      // STEP 4: Record attendance in the database
      sendAttendanceToFirebase(scannedUID, studentName);


    } else {
      // If the UID does not exist in the /students/ list
      handleInvalidCard();
    }
  } else {
    // If there is a connection error or query fails
    handleInvalidCard();
    Serial.println("DB Error: " + fbdo.errorReason());
  }


  delay(2000); // Display the student name for 2 seconds
  digitalWrite(LED, LOW);
  lcd.clear();
  mfrc522.PICC_HaltA(); // Stop encryption on the current card
}


// ====================== HELPER FUNCTIONS =========================


// Converts raw RFID bytes into a readable Hexadecimal String (e.g., "A1B2C3D4")
String uidToString(byte uid[4]){
  char buf[9];
  sprintf(buf, "%02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
  return String(buf);
}


// Packages the attendance data into a JSON and sends it to the /attendance/ date path
void sendAttendanceToFirebase(String uidStr, String studentName){
  struct tm nowTime;
  if (!getLocalTime(&nowTime)) return;


  char dateStr[11], timeStr[9];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &nowTime);
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &nowTime);


  // Path structure: /attendance/2026-02-11/UID_HERE
  String path = "/attendance/" + String(dateStr) + "/" + uidStr;


  FirebaseJson json;
  json.add("name", studentName);
  json.add("time", String(timeStr));
  json.add("status", "Present");


  Firebase.RTDB.setJSON(&fbdo, path, &json);
  Serial.println("Attendance logged for " + studentName);
}


// Displays error message and triggers a long buzzer warning
void handleInvalidCard() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Access Denied");
  lcd.setCursor(0,1);
  lcd.print("Invalid Card");
  digitalWrite(LED, LOW);
  longBeep();
}


// Standard beep function (active low)
void beep(int times){
  for(int i=0;i<times;i++){
    digitalWrite(BUZZER, LOW);
    delay(120);
    digitalWrite(BUZZER, HIGH);
    delay(150);
  }
}


// Sustained beep for errors
void longBeep(){
  digitalWrite(BUZZER, LOW);
  delay(400);
  digitalWrite(BUZZER, HIGH);
}

