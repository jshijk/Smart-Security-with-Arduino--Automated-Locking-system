# Smart Security with Arduino: Automated Locking system
This is out main branch for our code

This Arduino-based RFID door lock system uses an LCD display, an MFRC522 RFID reader, and a relay module to control a door locking mechanism. 
The program begins by initializing all required components: the LCD screen, the RFID reader, SPI communication, and the relay pin (set as an output).

How the System Works:
When powered on, the LCD displays a welcome message prompting the user to scan an RFID card.
The system continuously checks whether a new card is present. Once a card is detected, it reads the card’s Unique Identifier (UID).

The UID is converted into a formatted string and compared with a pre-defined authorized UID stored in the code.

If the scanned UID matches the authorized UID:
-The relay toggles its state.
-If the door was locked, it becomes unlocked.
-If the door was unlocked, it becomes locked.
-The LCD displays the current door status ("Door is open" or "Door is locked").

If the scanned UID does not match:
-The LCD displays “Wrong card!”
-The relay state does not change.
-The variable lock keeps track of the current door state.

Important Parts of the Code

UID Variable:
-Stores the authorized RFID card number. Only this card can control the door.

RFID Reader (MFRC522):
-Detects and reads the UID from RFID cards.

Relay Module:
-Acts as a switch to control the door lock mechanism.

LCD Display:
-Provides user feedback such as welcome messages, scanning progress, door status, or access denial.

Lock Variable (lock):
-Tracks whether the door is currently locked (1) or unlocked (0).

How to Run the System

1.) Connect the components properly:
-MFRC522 to Arduino using SPI pins.
-Relay module to pin 7.
-LCD I2C to SDA and SCL pins.

2.)Install required libraries:
-LiquidCrystal_I2C
-MFRC522

3.) Upload the code to the Arduino using the Arduino IDE.
4.) Power the system using USB or an external power supply.
5.) Place an RFID card near the reader to test access.

Possible Results When Running

1.) Authorized Card Scanned
-Door unlocks (if previously locked).
-Door locks (if previously unlocked).
-LCD shows updated door status.

2.) Unauthorized Card Scanned
-LCD displays “Wrong card!”
-Door remains in its current state.

3.) No Card Present
-LCD continues to display the welcome message.
-System waits for input.

4.) Hardware Issue (e.g., disconnected reader)
-Card will not be detected.
-System will remain on the welcome screen.
