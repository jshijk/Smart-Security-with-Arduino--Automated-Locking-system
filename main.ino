#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9

// ===== YOUR CARD UID =====
String authorizedUID = "37 21 25 25";  // Change this to your card's UID

// ===== TIMING PATTERN =====
const int pattern[] = {300, 800, 300};  // Short-Long-Short pattern
const int patternLength = 3;
const int TOLERANCE = 150;  // Timing error allowance

// ===== HARDWARE PINS =====
const int RELAY_PIN = 7;     // Door lock relay
const int BUZZER_PIN = 6;     // Buzzer pin (change if needed)

// ===== BUZZER SETTINGS =====
const int BUZZER_FREQUENCY = 1000;  // 1kHz tone (adjust for your buzzer)
const int OPEN_BUZZ_DURATION = 3000;  // Buzzer stays on while door open
const int SUCCESS_BEEP_COUNT = 2;     // Number of success confirmation beeps
const int ERROR_BEEP_COUNT = 3;       // Number of error beeps

// ===== SYSTEM VARIABLES =====
unsigned long lastScanTime = 0;
int patternPosition = 0;
bool patternStarted = false;
unsigned long sessionStartTime = 0;
const unsigned long SESSION_TIMEOUT = 5000;

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
  
  // Welcome message with buzzer test
  lcd.setCursor(0, 0);
  lcd.print("Secure Door Lock");
  lcd.setCursor(0, 1);
  lcd.print("Pattern: S-L-S");
  
  // Test buzzer
  testBuzzer();
  
  delay(2000);
  lcd.clear();
  
  Serial.println("System Ready - Waiting for cards...");
}

void loop() {
  // Check for pattern timeout
  if (patternStarted) {
    checkTimeout();
  }
  
  // Display main screen
  if (!patternStarted) {
    displayMainScreen();
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
  
  // Short beep to acknowledge card read
  beep(100);  // Quick 100ms beep
  
  // Check if card is authorized
  if (cardID != authorizedUID) {
    handleWrongCard();
    rfid.PICC_HaltA();
    return;
  }
  
  // Valid card - handle pattern
  handlePattern();
  
  rfid.PICC_HaltA();
}

// ===== BUZZER FUNCTIONS =====

void beep(int duration) {
  // Simple beep for specified milliseconds
  tone(BUZZER_PIN, BUZZER_FREQUENCY);
  delay(duration);
  noTone(BUZZER_PIN);
}

void beepPattern(int count, int beepDuration, int pauseDuration) {
  // Beep multiple times with pauses in between
  for (int i = 0; i < count; i++) {
    tone(BUZZER_PIN, BUZZER_FREQUENCY);
    delay(beepDuration);
    noTone(BUZZER_PIN);
    
    if (i < count - 1) {  // Don't pause after last beep
      delay(pauseDuration);
    }
  }
}

void testBuzzer() {
  // Quick test sequence on startup
  beepPattern(2, 200, 100);  // Two short beeps
}

void successChime() {
  // Happy ascending tone pattern
  tone(BUZZER_PIN, 523);  // C5
  delay(150);
  tone(BUZZER_PIN, 659);  // E5
  delay(150);
  tone(BUZZER_PIN, 784);  // G5
  delay(300);
  noTone(BUZZER_PIN);
}

void errorSound() {
  // Error sound - low beeps
  beepPattern(ERROR_BEEP_COUNT, 200, 150);
}

void doorOpenBuzzer() {
  // Buzzer stays on while door is open
  tone(BUZZER_PIN, BUZZER_FREQUENCY);
}

void doorCloseBuzzer() {
  noTone(BUZZER_PIN);
}

// ===== MAIN FUNCTIONS =====

void displayMainScreen() {
  lcd.setCursor(0, 0);
  lcd.print("Scan your card  ");
  lcd.setCursor(0, 1);
  
  // Show pattern hint
  lcd.print("Pattern: ");
  for (int i = 0; i < patternLength; i++) {
    if (pattern[i] < 500) {
      lcd.print("S ");  // S for Short
    } else {
      lcd.print("L ");  // L for Long
    }
  }
}

void handlePattern() {
  unsigned long now = millis();
  
  if (!patternStarted) {
    // First scan - start new pattern
    patternStarted = true;
    patternPosition = 1;
    lastScanTime = now;
    sessionStartTime = now;
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pattern started");
    lcd.setCursor(0, 1);
    lcd.print("Scan 1/");
    lcd.print(patternLength);
    
    Serial.println("Pattern started - Scan 1");
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
    beep(80);  // Quick confirmation beep
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Good timing!");
    lcd.setCursor(0, 1);
    lcd.print("Scan ");
    lcd.print(patternPosition);
    lcd.print("/");
    lcd.print(patternLength);
    
    Serial.print("Correct timing. Scan ");
    Serial.print(patternPosition);
    Serial.print("/");
    Serial.println(patternLength);
    
    // Check if pattern complete
    if (patternPosition >= patternLength) {
      grantAccess();
      resetPattern();
    }
  } else {
    // Wrong timing - pattern fails
    errorSound();  // Play error sound
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wrong timing!");
    lcd.setCursor(0, 1);
    lcd.print("Start over");
    
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
    // Pattern timeout - play warning sound
    beep(500);  // Long single beep
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Session timeout");
    lcd.setCursor(0, 1);
    lcd.print("Try again");
    
    Serial.println("Session timeout - no card scanned");
    delay(1500);
    resetPattern();
  }
}

void grantAccess() {
  // Play success chime
  successChime();
  
  // Unlock the door
  digitalWrite(RELAY_PIN, HIGH);
  
  // Start door open buzzer (continuous)
  doorOpenBuzzer();
  
  // Show success message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ACCESS GRANTED!");
  lcd.setCursor(0, 1);
  lcd.print("Door unlocked");
  
  Serial.println("*** ACCESS GRANTED - DOOR UNLOCKED ***");
  
  // Keep door unlocked for 3 seconds with buzzer on
  delay(OPEN_BUZZ_DURATION);
  
  // Lock the door and stop buzzer
  digitalWrite(RELAY_PIN, LOW);
  doorCloseBuzzer();
  
  // Quick beep to confirm door locked
  beep(100);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Door locked");
  delay(1000);
}

void handleWrongCard() {
  // Play error sound
  errorSound();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UNAUTHORIZED");
  lcd.setCursor(0, 1);
  lcd.print("Card not valid");
  
  Serial.println("WARNING: Unauthorized card detected!");
  delay(2000);
  
  // If pattern was in progress, reset it
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