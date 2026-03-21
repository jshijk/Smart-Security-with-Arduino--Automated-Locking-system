#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9

String authorizedUID = "37 21 35 25";
const char PATTERN[] = "..-";

const unsigned long TAP_WINDOW = 400;
const unsigned long SYMBOL_GAP = 1000;
const unsigned long PATTERN_END = 1500;

const int RELAY_PIN = 7;
const int BUZZER_PIN = 6;  // Buzzer pin

bool doorUnlocked = false;
bool readingPattern = false;
bool inSymbol = false;

int tapCount = 0;
unsigned long lastTapTime = 0;
unsigned long symbolEndTime = 0;

char morseBuffer[4];
int symbolCount = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  lcd.init();
  lcd.backlight();
  SPI.begin();
  rfid.PCD_Init();
  
  // Startup beep
  beep(200, 2);
  
  lcd.setCursor(0, 0);
  lcd.print("Tap: ..-");
  lcd.setCursor(0, 1);
  lcd.print("1tap=. 2tap=-");
  delay(3000);
  lcd.clear();
}

void loop() {
  bool cardDetected = rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial();
  
  if (cardDetected) {
    if (getCardID() != authorizedUID) {
      rfid.PICC_HaltA();
      return;
    }
    rfid.PICC_HaltA();
    handleTap();
  } else {
    checkGaps();
  }
  
  updateDisplay();
}

void handleTap() {
  unsigned long now = millis();
  
  if (!readingPattern) {
    readingPattern = true;
    symbolCount = 0;
    morseBuffer[0] = '\0';
    tapCount = 1;
    lastTapTime = now;
    inSymbol = true;
    
    // Short beep on first tap
    beep(100, 1);
    
    lcd.clear();
    lcd.print("Tap 1...");
    return;
  }
  
  if (!inSymbol) {
    inSymbol = true;
    tapCount = 1;
    lastTapTime = now;
    
    // Short beep on new symbol
    beep(100, 1);
    
    lcd.setCursor(0, 0);
    lcd.print("Tap 1...");
    return;
  }
  
  if (now - lastTapTime < TAP_WINDOW) {
    tapCount++;
    lastTapTime = now;
    
    lcd.setCursor(0, 0);
    lcd.print("Tap ");
    lcd.print(tapCount);
    lcd.print("...");
    
    if (tapCount > 2) {
      // Error beep for too many taps
      errorBeep();
      
      lcd.setCursor(0, 1);
      lcd.print("Too many! Reset");
      delay(1500);
      resetPattern();
    }
  }
}

void checkGaps() {
  if (!readingPattern) return;
  
  unsigned long now = millis();
  
  if (inSymbol && (now - lastTapTime > TAP_WINDOW)) {
    inSymbol = false;
    symbolEndTime = now;
    
    if (symbolCount >= 3) return;
    
    if (tapCount == 1) {
      morseBuffer[symbolCount] = '.';
      lcd.setCursor(0, 1);
      lcd.print("= DOT (.)       ");
    } else if (tapCount == 2) {
      morseBuffer[symbolCount] = '-';
      lcd.setCursor(0, 1);
      lcd.print("= DASH (-)      ");
    }
    
    symbolCount++;
    morseBuffer[symbolCount] = '\0';
    
    lcd.setCursor(0, 0);
    lcd.print("Code: ");
    lcd.print(morseBuffer);
    lcd.print("        ");
    
    tapCount = 0;
  }
  
  if (!inSymbol && (now - symbolEndTime > PATTERN_END)) {
    checkPattern();
  }
}

void checkPattern() {
  readingPattern = false;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Got: ");
  lcd.print(morseBuffer);
  
  if (strcmp(morseBuffer, PATTERN) == 0) {
    // Correct pattern - toggle lock
    if (doorUnlocked) {
      lockDoor();
    } else {
      unlockDoor();
    }
  } else {
    // Wrong pattern - error beep
    errorBeep();
    
    lcd.setCursor(0, 1);
    lcd.print("Wrong! Use ..-");
    delay(2000);
  }
  
  resetPattern();
}

void resetPattern() {
  symbolCount = 0;
  morseBuffer[0] = '\0';
  tapCount = 0;
  inSymbol = false;
  readingPattern = false;
}

void unlockDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  doorUnlocked = true;
  
  // Success beep - door opening
  successBeep();
  
  lcd.setCursor(0, 1);
  lcd.print("UNLOCKED!");
  delay(2000);
}

void lockDoor() {
  digitalWrite(RELAY_PIN, LOW);
  doorUnlocked = false;
  
  // Lock beep - door closing
  lockBeep();
  
  lcd.setCursor(0, 1);
  lcd.print("LOCKED!");
  delay(1500);
}

// ===== BUZZER FUNCTIONS =====

void beep(int duration, int count) {
  for (int i = 0; i < count; i++) {
    tone(BUZZER_PIN, 1000);
    delay(duration);
    noTone(BUZZER_PIN);
    if (i < count - 1) delay(100);
  }
}

void successBeep() {
  // Happy ascending tone for unlock
  tone(BUZZER_PIN, 523);  // C5
  delay(150);
  tone(BUZZER_PIN, 659);  // E5
  delay(150);
  tone(BUZZER_PIN, 784);  // G5
  delay(300);
  noTone(BUZZER_PIN);
}

void lockBeep() {
  // Descending tone for lock
  tone(BUZZER_PIN, 784);  // G5
  delay(150);
  tone(BUZZER_PIN, 659);  // E5
  delay(150);
  tone(BUZZER_PIN, 523);  // C5
  delay(300);
  noTone(BUZZER_PIN);
}

void errorBeep() {
  // Low error beep for wrong pattern
  tone(BUZZER_PIN, 200);  // Low tone
  delay(300);
  noTone(BUZZER_PIN);
  delay(100);
  tone(BUZZER_PIN, 200);
  delay(300);
  noTone(BUZZER_PIN);
}

void updateDisplay() {
  if (readingPattern) return;
  
  lcd.setCursor(0, 0);
  lcd.print("Use: ..-");
  lcd.setCursor(0, 1);
  if (doorUnlocked) {
    lcd.print("UNLOCKED        ");
  } else {
    lcd.print("LOCKED          ");
  }
}

String getCardID() {
  String ID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) ID += "0";
    ID += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) ID += " ";
  }
  ID.toUpperCase();
  return ID;
}
