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
bool waitingForRemoval = false;  // NEW: Track if we need card removed
unsigned long cardRemovedTime = 0;
const unsigned long REMOVAL_TIMEOUT = 2000;  // Max time to remove card
const unsigned long SESSION_TIMEOUT = 5000;
bool doorUnlocked = false;

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
  // Check for timeouts
  if (patternStarted && !doorUnlocked) {
    checkTimeout();
  }
  
  // Check if card is still present (for removal detection)
  bool cardPresent = rfid.PICC_IsNewCardPresent() || checkCardStillThere();
  
  // Handle removal detection
  if (waitingForRemoval && !cardPresent) {
    // Card was removed!
    handleCardRemoved();
    return;
  }
  
  // Update display based on state
  updateDisplay();
  
  // If waiting for removal, don't accept new scans yet
  if (waitingForRemoval) {
    return;
  }
  
  // Check for new card scan
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) {
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

// Check if same card is still on reader (without re-reading)
bool checkCardStillThere() {
  // Try to re-select the card without full read
  byte bufferATQA[2];
  byte bufferSize = sizeof(bufferATQA);
  
  // Check if card is still present using WUPA command
  MFRC522::StatusCode result = rfid.PICC_WakeupA(bufferATQA, &bufferSize);
  
  if (result == MFRC522::STATUS_OK) {
    rfid.PICC_HaltA();  // Put it back to halt state
    return true;
  }
  return false;
}

// ===== DISPLAY FUNCTIONS =====

void updateDisplay() {
  if (doorUnlocked) {
    lcd.setCursor(0, 0);
    lcd.print("DOOR UNLOCKED   ");
    lcd.setCursor(0, 1);
    lcd.print("Scan to LOCK    ");
  } else if (!patternStarted) {
    // Initial state - waiting for first scan
    lcd.setCursor(0, 0);
    lcd.print("Please scan the ");
    lcd.setCursor(0, 1);
    lcd.print("card to start   ");
  } else if (waitingForRemoval) {
    // Waiting for user to remove card
    lcd.setCursor(0, 0);
    lcd.print("Please remove   ");
    lcd.setCursor(0, 1);
    lcd.print("the ID          ");
  } else {
    // Waiting for next scan
    lcd.setCursor(0, 0);
    lcd.print("Scan ");
    lcd.print(patternPosition + 1);
    lcd.print("/");
    lcd.print(patternLength);
    lcd.print("        ");
    lcd.setCursor(0, 1);
    lcd.print("Please scan card");
  }
}

// ===== MAIN FUNCTIONS =====

void handlePattern() {
  unsigned long now = millis();
  
  if (!patternStarted) {
    // First scan - start new pattern
    patternStarted = true;
    patternPosition = 1;
    lastScanTime = now;
    waitingForRemoval = true;  // Now wait for removal
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan 1/");
    lcd.print(patternLength);
    lcd.print(" done!   ");
    lcd.setCursor(0, 1);
    lcd.print("Remove card...  ");
    
    Serial.println("Scan 1/" + String(patternLength) + " - Waiting for removal");
    return;
  }
  
  // Calculate time since last scan (when card was removed)
  unsigned long timeDiff = now - cardRemovedTime;
  int expectedDelay = pattern[patternPosition - 1];
  
  // Check if timing matches
  bool timingMatch = (timeDiff > expectedDelay - TOLERANCE && 
                      timeDiff < expectedDelay + TOLERANCE);
  
  if (timingMatch) {
    // Correct timing!
    patternPosition++;
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GOOD! ");
    lcd.print(patternPosition);
    lcd.print("/");
    lcd.print(patternLength);
    lcd.print("       ");
    
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
      // Need to remove card again
      waitingForRemoval = true;
      lcd.setCursor(0, 1);
      lcd.print("Remove card...  ");
    }
  } else {
    // Wrong timing - pattern fails
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WRONG TIMING!   ");
    lcd.setCursor(0, 1);
    lcd.print("Expected: ");
    lcd.print(expectedDelay);
    lcd.print("ms ");
    
    Serial.print("Wrong timing. Expected ~");
    Serial.print(expectedDelay);
    Serial.print("ms, got ");
    Serial.println(timeDiff);
    
    delay(2000);
    resetPattern();
  }
}

void handleCardRemoved() {
  unsigned long now = millis();
  cardRemovedTime = now;
  waitingForRemoval = false;
  
  // If this was the first scan, just record the time and wait for next scan
  if (patternPosition == 1 && lastScanTime == 0) {
    lastScanTime = now;  // Actually this shouldn't happen, but safety check
  }
  
  lastScanTime = now;  // Update to removal time for next interval calculation
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Card removed!   ");
  lcd.setCursor(0, 1);
  
  if (patternPosition >= patternLength) {
    lcd.print("Done!           ");
  } else {
    lcd.print("Wait for scan ");
    lcd.print(patternPosition + 1);
  }
  
  Serial.print("Card removed at position ");
  Serial.print(patternPosition);
  Serial.print("/");
  Serial.println(patternLength);
  
  delay(200);  // Brief debounce
}

void checkTimeout() {
  unsigned long now = millis();
  
  // Check removal timeout
  if (waitingForRemoval && (now - lastScanTime > REMOVAL_TIMEOUT)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Remove card!    ");
    lcd.setCursor(0, 1);
    lcd.print("Timeout soon... ");
    
    if (now - lastScanTime > SESSION_TIMEOUT) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TIMEOUT!        ");
      lcd.setCursor(0, 1);
      lcd.print("Start over      ");
      
      Serial.println("Session timeout - pattern reset");
      delay(1500);
      resetPattern();
    }
    return;
  }
  
  // Check session timeout between scans
  if (!waitingForRemoval && patternStarted && (now - lastScanTime > SESSION_TIMEOUT)) {
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
  lcd.print("Scan to LOCK    ");
  
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
  waitingForRemoval = false;
  cardRemovedTime = 0;
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
