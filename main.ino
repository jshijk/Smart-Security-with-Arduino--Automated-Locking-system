#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9

// ===== YOUR CARD UID =====
String authorizedUID = "37 21 35 25";  // Change this to your card's UID

// ===== TIMING PATTERN =====
const int pattern[] = {300, 800, 300};  // Short-Long-Short pattern (in milliseconds)
const int patternLength = 3;
const int TOLERANCE = 150;  // Timing error allowance

// ===== HARDWARE PINS =====
const int RELAY_PIN = 7;     // Door lock relay

// ===== SYSTEM VARIABLES =====
unsigned long lastScanTime = 0;
int patternPosition = 0;
bool patternStarted = false;
unsigned long sessionStartTime = 0;
const unsigned long SESSION_TIMEOUT = 5000;
bool doorUnlocked = false;
unsigned long displayUpdateTime = 0;  // For countdown refresh rate

// LCD and RFID objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  
  // Setup pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Door starts locked
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  
  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // Welcome message
  lcd.setCursor(0, 0);
  lcd.print("Secure Door Lock");
  lcd.setCursor(0, 1);
  lcd.print("Pattern: S-L-S");
  
  delay(2000);
  lcd.clear();
  
  Serial.println("System Ready - Waiting for cards...");
}

void loop() {
  // Check for pattern timeout (only if door is locked and pattern in progress)
  if (patternStarted && !doorUnlocked) {
    checkTimeout();
  }
  
  // Update countdown display in real-time
  if (patternStarted && !doorUnlocked) {
    updateCountdownDisplay();
  } else {
    updateDisplay();
  }
  
  // Check for new card
  if ( !rfid.PICC_IsNewCardPresent() ) {
    return;
  }
  
  if ( !rfid.PICC_ReadCardSerial() ) {
    return;
  }
  
  // Get the card UID
  String cardID = getCardID();
  Serial.print("Card detected: ");
  Serial.println(cardID);
  
  // Check if card is authorized
  if (cardID != authorizedUID) {
    handleWrongCard();
    rfid.PICC_HaltA();
    return;
  }
  
  // If door is already unlocked, lock it (toggle off)
  if (doorUnlocked) {
    lockDoor();
    rfid.PICC_HaltA();
    return;
  }
  
  // Valid card and door is locked - handle pattern
  handlePattern();
  
  rfid.PICC_HaltA();
}

// ===== DISPLAY FUNCTIONS =====

void updateDisplay() {
  if (doorUnlocked) {
    lcd.setCursor(0, 0);
    lcd.print("DOOR UNLOCKED   ");
    lcd.setCursor(0, 1);
    lcd.print("Scan to LOCK    ");
  } else if (!patternStarted) {
    lcd.setCursor(0, 0);
    lcd.print("Scan ID 1/" + String(patternLength) + "    ");
    lcd.setCursor(0, 1);
    lcd.print("Pattern: ");
    for (int i = 0; i < patternLength; i++) {
      if (pattern[i] < 500) {
        lcd.print("S ");
      } else {
        lcd.print("L ");
      }
    }
    lcd.print("   ");
  }
}

void updateCountdownDisplay() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastScanTime;
  int expectedDelay = pattern[patternPosition - 1];
  
  // Calculate remaining time (don't go below 0)
  long remaining = expectedDelay - elapsed;
  if (remaining < 0) remaining = 0;
  
  // Update display every 100ms to avoid flickering
  if (now - displayUpdateTime < 100) {
    return;
  }
  displayUpdateTime = now;
  
  // Line 0: Show current scan position
  lcd.setCursor(0, 0);
  lcd.print("Scan ");
  lcd.print(patternPosition + 1);
  lcd.print("/");
  lcd.print(patternLength);
  lcd.print(" in:     ");
  
  // Line 1: Show countdown in milliseconds
  lcd.setCursor(0, 1);
  
  // Visual progress bar + countdown number
  int progress = map(elapsed, 0, expectedDelay, 0, 10);
  if (progress > 10) progress = 10;
  
  // Draw progress bar
  lcd.print("[");
  for (int i = 0; i < 10; i++) {
    if (i < progress) {
      lcd.print("=");
    } else {
      lcd.print(" ");
    }
  }
  lcd.print("]");
  
  // Show remaining ms at the end
  lcd.setCursor(12, 1);
  if (remaining < 100) lcd.print(" ");
  if (remaining < 10) lcd.print(" ");
  lcd.print(remaining);
  lcd.print("ms");
}

// ===== MAIN FUNCTIONS =====

void handlePattern() {
  unsigned long now = millis();
  
  if (!patternStarted) {
    // First scan - start new pattern
    patternStarted = true;
    patternPosition = 1;
    lastScanTime = now;
    sessionStartTime = now;
    displayUpdateTime = 0;
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan 1/");
    lcd.print(patternLength);
    lcd.print(" DONE!    ");
    lcd.setCursor(0, 1);
    lcd.print("Wait ");
    lcd.print(pattern[0]);
    lcd.print("ms...    ");
    
    Serial.println("Pattern started - 1/" + String(patternLength));
    delay(300);
    return;
  }
  
  // Calculate time since last scan
  unsigned long timeDiff = now - lastScanTime;
  int expectedDelay = pattern[patternPosition - 1];
  
  // Check if timing matches
  bool timingMatch = (timeDiff > expectedDelay - TOLERANCE && 
                      timeDiff < expectedDelay + TOLERANCE);
  
  if (timingMatch) {
    // Correct timing!
    patternPosition++;
    lastScanTime = now;
    displayUpdateTime = 0;
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GOOD! ");
    lcd.print(patternPosition);
    lcd.print("/");
    lcd.print(patternLength);
    
    Serial.print("Correct timing. Progress: ");
    Serial.print(patternPosition);
    Serial.print("/");
    Serial.println(patternLength);
    
    // Check if pattern complete
    if (patternPosition >= patternLength) {
      lcd.setCursor(0, 1);
      lcd.print("UNLOCKING...    ");
      delay(500);
      unlockDoor();
      resetPattern();
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Wait ");
      lcd.print(pattern[patternPosition - 1]);
      lcd.print("ms...    ");
      delay(300);
    }
  } else {
    // Wrong timing - pattern fails
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WRONG TIMING!   ");
    lcd.setCursor(0, 1);
    lcd.print("Expected: ");
    lcd.print(expectedDelay);
    lcd.print("ms  ");
    
    Serial.print("Wrong timing. Expected ~");
    Serial.print(expectedDelay);
    Serial.print("ms, got ");
    Serial.println(timeDiff);
    
    delay(2000);
    resetPattern();
  }
}

void checkTimeout() {
  if (millis() - lastScanTime > SESSION_TIMEOUT) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TIMEOUT!        ");
    lcd.setCursor(0, 1);
    lcd.print("Too slow!       ");
    
    Serial.println("Session timeout - pattern reset");
    delay(1500);
    resetPattern();
  }
}

void unlockDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  doorUnlocked = true;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ACCESS GRANTED! ");
  lcd.setCursor(0, 1);
  lcd.print("Scan ID to LOCK ");
  
  Serial.println("*** DOOR UNLOCKED - SCAN TO LOCK ***");
}

void lockDoor() {
  digitalWrite(RELAY_PIN, LOW);
  doorUnlocked = false;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DOOR LOCKED     ");
  lcd.setCursor(0, 1);
  lcd.print("Goodbye!        ");
  
  Serial.println("*** DOOR LOCKED ***");
  
  delay(1500);
  lcd.clear();
}

void handleWrongCard() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UNAUTHORIZED    ");
  lcd.setCursor(0, 1);
  lcd.print("Invalid Card    ");
  
  Serial.println("WARNING: Unauthorized card!");
  delay(2000);
  
  if (patternStarted) {
    resetPattern();
  }
}

void resetPattern() {
  patternStarted = false;
  patternPosition = 0;
  lastScanTime = 0;
  displayUpdateTime = 0;
  lcd.clear();
}

String getCardID() {
  String ID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      ID.concat("0");
    }
    ID.concat(String(rfid.uid.uidByte[i], HEX));
    if (i < rfid.uid.size - 1) {
      ID.concat(" ");
    }
  }
  ID.toUpperCase();
  return ID;
}
