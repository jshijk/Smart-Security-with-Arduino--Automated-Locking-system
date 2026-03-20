#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9

// ===== YOUR CARD UID =====
String authorizedUID = "37 21 35 25";

// ===== HARDWARE PINS =====
const int RELAY_PIN = 7;

// ===== SWIPE SETTINGS =====
const unsigned long SWIPE_TIMEOUT = 1500;      // Max time for full swipe
const unsigned long DETECTION_WINDOW = 50;       // Check every 50ms
const int MIN_SWIPE_POINTS = 3;                // Need at least 3 readings
const int DIRECTION_THRESHOLD = 30;              // % difference to determine direction

// ===== UNLOCK PATTERN =====
// 0 = Left, 1 = Right, 2 = Up, 3 = Down
const int unlockPattern[] = {1, 3};      // Right → Down = UNLOCK
const int lockPattern[] = {0, 2};        // Left → Up = LOCK
const int patternLength = 2;

// ===== SYSTEM VARIABLES =====
bool doorUnlocked = false;
bool swipeInProgress = false;
unsigned long swipeStartTime = 0;
unsigned long lastDetectionTime = 0;

// Swipe detection variables
int readCount = 0;
unsigned long firstReadTime = 0;
unsigned long lastReadTime = 0;
bool cardWasPresent = false;

// For simple "swipe" detection using multiple quick reads
int consecutiveReads = 0;
unsigned long firstConsecutiveTime = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  lcd.init();
  lcd.backlight();
  SPI.begin();
  rfid.PCD_Init();
  
  // Welcome
  lcd.setCursor(0, 0);
  lcd.print("Swipe Direction");
  lcd.setCursor(0, 1);
  lcd.print("Lock System");
  delay(2500);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Right-Down=OPEN");
  lcd.setCursor(0, 1);
  lcd.print("Left-Up=LOCK");
  delay(2500);
  lcd.clear();
  
  Serial.println("Ready - Swipe patterns:");
  Serial.println("  RIGHT then DOWN = UNLOCK");
  Serial.println("  LEFT then UP = LOCK");
}

void loop() {
  updateDisplay();
  
  // Check for card presence
  bool cardPresent = rfid.PICC_IsNewCardPresent();
  
  if (cardPresent) {
    if (!rfid.PICC_ReadCardSerial()) {
      return;
    }
    
    String cardID = getCardID();
    
    // Check authorization
    if (cardID != authorizedUID) {
      handleWrongCard();
      rfid.PICC_HaltA();
      return;
    }
    
    // Valid card detected
    handleCardDetection();
    rfid.PICC_HaltA();
  } else {
    // No card present
    if (cardWasPresent && swipeInProgress) {
      // Card was removed - end of swipe
      endSwipe();
    }
    cardWasPresent = false;
  }
}

void handleCardDetection() {
  unsigned long now = millis();
  
  if (!swipeInProgress) {
    // Start new swipe
    startSwipe();
  } else {
    // Continue swipe - check timing
    if (now - swipeStartTime > SWIPE_TIMEOUT) {
      // Swipe took too long
      swipeTimeout();
      return;
    }
    
    // Count this as part of swipe
    consecutiveReads++;
    lastReadTime = now;
    
    // Visual feedback during swipe
    if (consecutiveReads == 1) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Swipe detected...");
    }
  }
  
  cardWasPresent = true;
}

void startSwipe() {
  swipeInProgress = true;
  swipeStartTime = millis();
  firstReadTime = millis();
  lastReadTime = millis();
  consecutiveReads = 1;
  firstConsecutiveTime = millis();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reading swipe...");
  lcd.setCursor(0, 1);
  lcd.print("Keep moving!");
  
  Serial.println("Swipe started");
}

void endSwipe() {
  unsigned long swipeDuration = lastReadTime - firstReadTime;
  
  Serial.print("Swipe ended. Duration: ");
  Serial.print(swipeDuration);
  Serial.print("ms, Reads: ");
  Serial.println(consecutiveReads);
  
  // Determine swipe direction based on timing characteristics
  int direction = analyzeSwipe(swipeDuration, consecutiveReads);
  
  if (direction >= 0) {
    handleDirection(direction);
  } else {
    // Could not determine direction
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Swipe unclear!");
    lcd.setCursor(0, 1);
    lcd.print("Try again");
    delay(1500);
  }
  
  resetSwipe();
}

int analyzeSwipe(unsigned long duration, int reads) {
  // Simple heuristic: fast reads = quick swipe
  // We use duration and read density to guess direction
  // In reality, RFID can't detect true spatial direction
  // So we simulate with timing patterns:
  
  // Short quick swipe = LEFT (fast removal)
  // Longer steady swipe = RIGHT (slow pass)
  // Medium with pause = UP or DOWN (lift and return)
  
  if (reads < MIN_SWIPE_POINTS) {
    return -1; // Too few readings
  }
  
  unsigned long avgInterval = duration / (reads - 1);
  
  Serial.print("Avg interval: ");
  Serial.println(avgInterval);
  
  // Simulate direction based on speed pattern
  // This is a "fake" direction for demo - in real hardware you'd need
  // multiple antennas or accelerometer on card
  
  // For this implementation, we use timing as "direction":
  // Very fast (<100ms total) = LEFT (0)
  // Fast (100-300ms) = RIGHT (1)  
  // Medium (300-600ms) = UP (2)
  // Slow (>600ms) = DOWN (3)
  
  if (duration < 150) {
    Serial.println("Detected: LEFT (fast)");
    return 0; // Left
  } else if (duration < 350) {
    Serial.println("Detected: RIGHT (medium)");
    return 1; // Right
  } else if (duration < 700) {
    Serial.println("Detected: UP (steady)");
    return 2; // Up
  } else {
    Serial.println("Detected: DOWN (slow)");
    return 3; // Down
  }
}

void handleDirection(int direction) {
  static int patternBuffer[2];
  static int bufferIndex = 0;
  static unsigned long lastDirectionTime = 0;
  unsigned long now = millis();
  
  // Reset buffer if too much time passed
  if (now - lastDirectionTime > 2000) {
    bufferIndex = 0;
  }
  
  // Store direction
  patternBuffer[bufferIndex] = direction;
  bufferIndex++;
  lastDirectionTime = now;
  
  // Show what was detected
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dir: ");
  lcd.print(directionToString(direction));
  lcd.setCursor(0, 1);
  
  if (bufferIndex == 1) {
    lcd.print("First swipe OK");
    delay(800);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Swipe second...");
    lcd.setCursor(0, 1);
    if (doorUnlocked) {
      lcd.print("Need: Left-Up");
    } else {
      lcd.print("Need: Right-Down");
    }
  } else if (bufferIndex >= 2) {
    // Check pattern
    lcd.print("Checking...");
    delay(500);
    
    bool isUnlock = (patternBuffer[0] == unlockPattern[0] && 
                     patternBuffer[1] == unlockPattern[1]);
    bool isLock = (patternBuffer[0] == lockPattern[0] && 
                   patternBuffer[1] == lockPattern[1]);
    
    bufferIndex = 0; // Reset
    
    if (!doorUnlocked && isUnlock) {
      unlockDoor();
    } else if (doorUnlocked && isLock) {
      lockDoor();
    } else {
      // Wrong pattern
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wrong pattern!");
      lcd.setCursor(0, 1);
      lcd.print("Try again");
      delay(1500);
    }
  }
}

String directionToString(int dir) {
  switch(dir) {
    case 0: return "LEFT ";
    case 1: return "RIGHT";
    case 2: return "UP   ";
    case 3: return "DOWN ";
    default: return "???  ";
  }
}

void swipeTimeout() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Too slow!");
  lcd.setCursor(0, 1);
  lcd.print("Swipe faster");
  delay(1500);
  resetSwipe();
}

void resetSwipe() {
  swipeInProgress = false;
  consecutiveReads = 0;
  cardWasPresent = false;
  lcd.clear();
}

void updateDisplay() {
  if (swipeInProgress) return;
  
  if (doorUnlocked) {
    lcd.setCursor(0, 0);
    lcd.print("UNLOCKED        ");
    lcd.setCursor(0, 1);
    lcd.print("Swipe Left-Up   ");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("LOCKED          ");
    lcd.setCursor(0, 1);
    lcd.print("Swipe Right-Down");
  }
}

void unlockDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  doorUnlocked = true;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ACCESS GRANTED! ");
  lcd.setCursor(0, 1);
  lcd.print("Door OPEN       ");
  delay(2000);
  lcd.clear();
  
  Serial.println("*** DOOR UNLOCKED ***");
}

void lockDoor() {
  digitalWrite(RELAY_PIN, LOW);
  doorUnlocked = false;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LOCKED!         ");
  lcd.setCursor(0, 1);
  lcd.print("Goodbye         ");
  delay(1500);
  lcd.clear();
  
  Serial.println("*** DOOR LOCKED ***");
}

void handleWrongCard() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WRONG CARD      ");
  lcd.setCursor(0, 1);
  lcd.print("Access Denied   ");
  delay(2000);
  lcd.clear();
}

String getCardID() {
  String ID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) ID.concat("0");
    ID.concat(String(rfid.uid.uidByte[i], HEX));
    if (i < rfid.uid.size - 1) ID.concat(" ");
  }
  ID.toUpperCase();
  return ID;
}
