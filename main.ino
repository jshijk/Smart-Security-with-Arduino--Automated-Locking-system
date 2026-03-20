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
const int BUZZER_PIN = 6;     // Buzzer pin

// ===== BUZZER SETTINGS =====
const int BUZZER_FREQUENCY = 1000;  // 1kHz tone
const int ERROR_BEEP_COUNT = 3;       // Number of error beeps

// ===== SYSTEM VARIABLES =====
unsigned long lastScanTime = 0;
int patternPosition = 0;
bool patternStarted = false;
unsigned long sessionStartTime = 0;
const unsigned long SESSION_TIMEOUT = 5000;
bool doorUnlocked = false;

// LCD and RFID objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  
  // Setup pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Door starts locked
  digitalWrite(BUZZER_PIN, LOW); // Buzzer off
  
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
  
  testBuzzer();
  
  delay(2000);
  lcd.clear();
  
  Serial.println("System Ready - Waiting for cards...");
}

void loop() {
  // Check for pattern timeout (only if door is locked and pattern in progress)
  if (patternStarted && !doorUnlocked) {
    checkTimeout();
  }
  
  // Display status screen
  updateDisplay();
  
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
  
  // Short beep to acknowledge card read
  beep(100);
  
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

// ===== BUZZER FUNCTIONS =====

void beep(int duration) {
  tone(BUZZER_PIN, BUZZER_FREQUENCY);
  delay(duration);
  noTone(BUZZER_PIN);
}

void beepPattern(int count, int beepDuration, int pauseDuration) {
  for (int i = 0; i < count; i++) {
    tone(BUZZER_PIN, BUZZER_FREQUENCY);
    delay(beepDuration);
    noTone(BUZZER_PIN);
    
    if (i < count - 1) {
      delay(pauseDuration);
    }
  }
}

void testBuzzer() {
  beepPattern(2, 200, 100);
}

void successChime() {
  tone(BUZZER_PIN, 523);  // C5
  delay(150);
  tone(BUZZER_PIN, 659);  // E5
  delay(150);
  tone(BUZZER_PIN, 784);  // G5
  delay(300);
  noTone(BUZZER_PIN);
}

void errorSound() {
  beepPattern(ERROR_BEEP_COUNT, 200, 150);
}

// ===== DISPLAY FUNCTIONS =====

void updateDisplay() {
  if (doorUnlocked) {
    // Door is unlocked - show lock prompt
    lcd.setCursor(0, 0);
    lcd.print("DOOR UNLOCKED   ");
    lcd.setCursor(0, 1);
    lcd.print("Scan to LOCK    ");
  } else if (!patternStarted) {
    // Waiting for first scan
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
    lcd.print("   "); // Clear remaining chars
  } else {
    // Pattern in progress - show countdown
    lcd.setCursor(0, 0);
    lcd.print("Scan ID ");
    lcd.print(patternPosition + 1);
    lcd.print("/");
    lcd.print(patternLength);
    lcd.print("    "); // Clear remaining chars
    
    lcd.setCursor(0, 1);
    // Show which timing is expected next
    lcd.print("Wait ");
    if (pattern[patternPosition - 1] < 500) {
      lcd.print("SHORT ");
    } else {
      lcd.print("LONG  ");
    }
    lcd.print("ms    "); // Clear remaining chars
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
    sessionStartTime = now;
    
    // Immediate feedback
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan ID 1/" + String(patternLength));
    lcd.setCursor(0, 1);
    lcd.print("Next: ");
    if (pattern[0] < 500) {
      lcd.print("SHORT delay");
    } else {
      lcd.print("LONG delay");
    }
    
    Serial.println("Pattern started - 1/" + String(patternLength));
    delay(500); // Brief pause to show the message
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
    
    // Short happy beep for correct timing
    beep(80);
    
    // Show progress
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
      // Show what's next
      lcd.setCursor(0, 1);
      lcd.print("Next: ");
      if (pattern[patternPosition - 1] < 500) {
        lcd.print("SHORT");
      } else {
        lcd.print("LONG ");
      }
      delay(300);
    }
  } else {
    // Wrong timing - pattern fails
    errorSound();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WRONG TIMING!   ");
    lcd.setCursor(0, 1);
    lcd.print("Expected: ");
    if (expectedDelay < 500) {
      lcd.print("SHORT");
    } else {
      lcd.print("LONG ");
    }
    
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
    beep(500);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TIMEOUT!        ");
    lcd.setCursor(0, 1);
    lcd.print("Start over 1/" + String(patternLength));
    
    Serial.println("Session timeout - pattern reset");
    delay(1500);
    resetPattern();
  }
}

void unlockDoor() {
  successChime();
  
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
  
  beep(200);
  
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
  errorSound();
  
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
