#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9
String UID = "37 21 35 25";  // Keep this format
byte lock = 1;

const int RELAY_PIN = 7;

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Start with relay off (door locked)

  lcd.init();
  lcd.backlight();

  SPI.begin();
  rfid.PCD_Init();
  
  Serial.println("RFID Door Lock System Ready");
}

void loop() {
  lcd.setCursor(4, 0);
  lcd.print("Welcome!");
  lcd.setCursor(1, 1);
  lcd.print("Put your card");

  if ( ! rfid.PICC_IsNewCardPresent())
    return;
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scanning");
  Serial.print("NUID tag is: ");
  
  String ID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    lcd.print(".");
    
    // Format the UID byte correctly
    if (rfid.uid.uidByte[i] < 0x10) {
      ID.concat("0");  // Add leading zero without space for first byte
      ID.concat(String(rfid.uid.uidByte[i], HEX));
    } else {
      ID.concat(String(rfid.uid.uidByte[i], HEX));
    }
    
    // Add space between bytes (except after the last byte)
    if (i < rfid.uid.size - 1) {
      ID.concat(" ");
    }
    
    // Print to Serial Monitor
    if (rfid.uid.uidByte[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) {
      Serial.print(" ");
    }
    
    delay(300);
  }
  
  Serial.println();
  ID.toUpperCase();
  
  Serial.print("Formatted ID: '");
  Serial.print(ID);
  Serial.println("'");
  Serial.print("Target UID: '");
  Serial.print(UID);
  Serial.println("'");
  
  if (ID == UID) {
    if (lock == 1) {
      // Door is locked, open it
      digitalWrite(RELAY_PIN, HIGH);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Door is open");
      lcd.setCursor(0, 1);
      lcd.print("Welcome!");
      Serial.println("Door OPENED");
      delay(1500);
      lcd.clear();
      lock = 0;
    } 
    else if (lock == 0) {
      // Door is open, lock it
      digitalWrite(RELAY_PIN, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Door is locked");
      Serial.println("Door LOCKED");
      delay(1500);
      lcd.clear();
      lock = 1;
    }
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wrong card!");
    lcd.setCursor(0, 1);
    lcd.print("Access Denied");
    Serial.println("Access Denied - Wrong Card");
    delay(1500);
    lcd.clear();
  }
  
  // Halt PICC
  rfid.PICC_HaltA();
  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}