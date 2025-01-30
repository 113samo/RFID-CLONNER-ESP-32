#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// OLED display dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#include <MFRC522.h>

byte uidLength;                   // To store the length of the UID
byte activeMemoryUID[10];         // To store the UID (up to 10 bytes, which is enough for most cards)
String activeMemoryPICC = "";     // To store the PICC type
byte activeMemoryMemory[1024];  // Array to store the card's memory data
int activeMemoryMemorySize = 0; // Size of the data read

// Pins for software SPI
#define OLED_MOSI   17
#define OLED_CLK    16
#define OLED_DC     33
#define OLED_CS     32
#define OLED_RESET  25

// Initialize RFID reader
MFRC522 mfrc522(21, 22); // SDA, RST pins
MFRC522 rfid(21,22);

// Create display object using software SPI
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// Random variables
int readDelay = 0; // So you cant go to next menu mid scan
int rfidPrintX = 0; // So rfid print can be reused for writing
int readModeState; //  0 = just UID , 1 = memory and UID
int optionsState = 0; // 0 = RFID, 1 = library, 2 = return
int optionsStateRfid = 0; // 0 = read, 1 = write, 2 = clone, 3 = return :)
int saveMenuState =0; //    :)
int writeMenuState; // :))
int selectedSlot; //

// Button pins
const int button1Pin = 4;
const int button2Pin = 5;
int button1 = 0;
int button2 = 0;

enum MenuState {
  MENU = 1,
  OPTIONS = 2,
  RFID = 3,
  READMODE = 4,
  READ = 5,
  WRITE = 6,
  CLONE = 7,
  RFIDPRINT = 8,
  SAVEMENU = 9,
  WRITEMENU = 10,
};

MenuState currentState = MENU;  // Set initial state

// Define menu labels
const char *menuLabels[] = {"RFID", "Library", "Return"};
const char *rfidLabels[] = {"Read", "Write", "Clone", "Return"};
const char *readModeLabels[] = {"just UID", "UID and Memory", "Return"};
const char *saveMenuOptions[] = {"Slot 1", "Slot 2", "Slot 3", "None",};
const char *writeMenuOptions[] = {"Slot 1", "Slot 2", "Slot 3", "Return",};

struct SavedSlot {       // structure for save slot 1-3
  String piccType;       // To store the PICC type
  byte uid[10];          // Array to store the UID
  byte uidLength;        // Length of the UID
  byte memory[1024];     // Array to store the memory data
  int memorySize;        // Size of the memory data
  bool justUID;          // if true = just uid , false = uid and memory
};

SavedSlot savedSlots[3]; // Array of 3 saved slots

void setup() {
  Serial.begin(9600);

  // Configure buttons as input with internal pull-up resistors
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Stop execution if display initialization fails
  }

  SPI.begin();         // Initialize SPI bus
  mfrc522.PCD_Init();   // Initialize RFID reader
  rfid.PCD_Init();       // Init MFRC522

  // Initialize OLED display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  display.clearDisplay(); // Clear display after initialization
}

void buttonState() {
  if (digitalRead(button1Pin) == LOW) {
    Serial.println("Button 1 pressed: Switching to MENU");
    if (currentState == OPTIONS) {
       if (optionsState == 2) {
        optionsState = -1; 
        }
      optionsState++;
    }
    if (currentState == RFID) {
        if (optionsStateRfid == 3) {
          optionsStateRfid = -1;
          }
      optionsStateRfid++; 
    }
    if (currentState == READMODE) {
       if (readModeState == 2) {
        readModeState = -1; 
        }
      readModeState++;
    }
    if (currentState == SAVEMENU) {
       if (saveMenuState == 3) {
        saveMenuState = -1; 
        }
      saveMenuState++;
    }

    if (currentState == WRITEMENU) {
       if (writeMenuState == 3) {
        writeMenuState = -1; 
        }
      writeMenuState++;
    }
    delay(400);  // Debounce delay
  }

  if (digitalRead(button2Pin) == LOW) {
    if (currentState == MENU) {
       Serial.println("Button 2 pressed: Switching to OPTIONS");
      currentState = OPTIONS;  // Switch to OPTIONS state
      optionsState = 0;
    } else if (currentState == OPTIONS) {
      if (optionsState == 0) {
        currentState = RFID;  
      } else if (optionsState == 1) {
        
      } else if (optionsState == 2) {
          currentState = MENU;
      }
    }
    else if (currentState == RFID) {
      if (optionsStateRfid == 0) {
        currentState = READMODE;
        readModeState = 0;  
      } else if (optionsStateRfid == 1) {
          currentState = WRITEMENU;
          readModeState = 0;  
      } else if (optionsStateRfid == 2) {
        currentState = READMODE;
          readModeState = 0;
      } else if (optionsStateRfid == 3) {
          currentState = OPTIONS;
          optionsStateRfid = 0;
      }
    }

    else if (currentState == READMODE) {
      if (readModeState == 0) {
        currentState = READ;
      } else if (readModeState == 1) {
          currentState = READ;
      } else if (readModeState == 2) {
          currentState = RFID;
          readModeState = 0;
      }
    }

    else if (currentState == RFIDPRINT) {
      if (readDelay == 1){
        currentState = SAVEMENU;
        readDelay = 0;
      }
    }

    else if (currentState == SAVEMENU) {
      if (saveMenuState == 0) {
        selectedSlot = 0;
        save();
      } else if (saveMenuState == 1) {
          selectedSlot = 1;
          save();
      } else if (saveMenuState == 2) {
          selectedSlot = 2;
          save();
      } else if (saveMenuState == 3) {
          currentState = RFID;
      }
      saveMenuState = 0;
    }
    else if (currentState == WRITEMENU) {
      if (writeMenuState == 0) {
        selectedSlot = 0;
        currentState = WRITE;
      } else if (writeMenuState == 1) {
          selectedSlot = 1;
          currentState = WRITE;
      } else if (writeMenuState == 2) {
          selectedSlot = 2;
          currentState = WRITE;
      } else if (writeMenuState == 3) {
          currentState = OPTIONS;
      }
      writeMenuState = 0;
    }
    delay(400);  // Debounce delay
  }
}

void save(){
  savedSlots[selectedSlot].piccType = activeMemoryPICC;
  memcpy(savedSlots[selectedSlot].uid, activeMemoryUID, sizeof(activeMemoryUID));
  savedSlots[selectedSlot].uidLength = uidLength;
  if (readModeState == 0) {
    savedSlots[selectedSlot].justUID = true;
  } else {
    memcpy(savedSlots[selectedSlot].memory, activeMemoryMemory, activeMemoryMemorySize);
    savedSlots[selectedSlot].memorySize = activeMemoryMemorySize;
    savedSlots[selectedSlot].justUID = false;
  }
  currentState = OPTIONS;
}


// Function to draw a border
void drawBorder() {
  // Top and bottom borders
  display.drawLine(0, 0, SCREEN_WIDTH - 1, 0, SSD1306_WHITE);          // Top border
  display.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE); // Bottom border

  // Left and right borders
  display.drawLine(0, 0, 0, SCREEN_HEIGHT - 1, SSD1306_WHITE);         // Left border
  display.drawLine(SCREEN_WIDTH - 1, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE); // Right border
}

void optionsMenu() {
  display.clearDisplay();  // Clear the screen before redrawing
  drawBorder();
  for (int i = 0; i < 3; i++) {
    if (i == optionsState) {
      display.fillRect(4, i * 16 + 8, 120, 16, SSD1306_WHITE); // Inverted background
      display.setTextColor(SSD1306_BLACK);                    // Black text
      display.setCursor(6, i * 16 + 10);                       // Position for arrow
      display.print(F(">"));                                  // Draw arrow
    } else {
      display.setTextColor(SSD1306_WHITE);
      }
    display.setCursor(16, i * 16 + 10); // Position for menu label
    display.print(menuLabels[i]);     // Print the menu label
  }
  display.display();  // Render the updated screen
}

void rfidMenu() {
  display.clearDisplay();  // Clear the screen before redrawing
  drawBorder();

  for (int i = 0; i < 4; i++) {  // Loop for 4 options
  if (i == optionsStateRfid) {
    display.fillRect(4, i * 14 + 8, 120, 16, SSD1306_WHITE); // Inverted background for selected option
    display.setTextColor(SSD1306_BLACK);                    // Black text for selected option
    display.setCursor(6, i * 14 + 10);                       // Position for arrow
    display.print(F("~"));                                  // Draw arrow
  } else {
    display.setTextColor(SSD1306_WHITE);                    // White text for unselected options
  }
  
  display.setCursor(16, i * 14 + 10);  // Position for menu label
  display.print(rfidLabels[i]);        // Print the label from rfidLabels array
}

  display.display();  // Render the updated screen
  
}

void saveMenu() {
  display.clearDisplay();  // Clear the screen before redrawing
  drawBorder();

  for (int i = 0; i < 4; i++) {  // Loop for 4 options
    if (i == saveMenuState) {
      display.fillRect(4, i * 14 + 8, 120, 16, SSD1306_WHITE); // Inverted background for selected option
      display.setTextColor(SSD1306_BLACK);                    // Black text for selected option
      display.setCursor(6, i * 14 + 10);                       // Position for arrow
      display.print(F("~"));                                  // Draw arrow
    } else {
      display.setTextColor(SSD1306_WHITE);                    // White text for unselected options
    }
  
    display.setCursor(16, i * 14 + 10);  // Position for menu label
    display.print(saveMenuOptions[i]);    // Print the menu label
  }

  display.display();  // Render the updated screen
}

void writeMenu() {
  display.clearDisplay();  // Clear the screen before redrawing
  drawBorder();

  for (int i = 0; i < 4; i++) {  // Loop for 4 options
    if (i == writeMenuState) {
      display.fillRect(4, i * 14 + 8, 120, 16, SSD1306_WHITE); // Inverted background for selected option
      display.setTextColor(SSD1306_BLACK);                    // Black text for selected option
      display.setCursor(6, i * 14 + 10);                       // Position for arrow
      display.print(F("~"));                                  // Draw arrow
    } else {
      display.setTextColor(SSD1306_WHITE);                    // White text for unselected options
    }
  
    display.setCursor(16, i * 14 + 10);  // Position for menu label
    display.print(writeMenuOptions[i]);    // Print the menu label
  }

  display.display();  // Render the updated screen
}

void readMode() {
  display.clearDisplay();  // Clear the screen before redrawing
  drawBorder();
  for (int i = 0; i < 3; i++) {
    if (i == readModeState) {
      display.fillRect(4, i * 16 + 8, 120, 16, SSD1306_WHITE); // Inverted background
      display.setTextColor(SSD1306_BLACK);                    // Black text
      display.setCursor(6, i * 16 + 10);                       // Position for arrow
      display.print(F(">"));                                  // Draw arrow
    } else {
      display.setTextColor(SSD1306_WHITE);
      }
    display.setCursor(16, i * 16 + 10); // Position for menu label
    display.print(readModeLabels[i]);     // Print the menu label
  }
  display.display();  // Render the updated screen
}

void readUID() {
  // Check if a new RFID card is present
  if (!mfrc522.PICC_IsNewCardPresent()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Error: No card");
    display.display();
    delay(1000);
    return; // Exit the function if no card is detected
  }

  // Attempt to read the card's data
  if (!mfrc522.PICC_ReadCardSerial()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Error: Read failed");
    display.display();
    delay(1000);
    return; // Exit the function if the read fails
  }

  // Store UID into activeMemoryUID
  uidLength = mfrc522.uid.size; // Get UID size
  for (byte i = 0; i < uidLength; i++) {
    activeMemoryUID[i] = mfrc522.uid.uidByte[i];
  }

  // Get card type (PICC type)
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  activeMemoryPICC = String(mfrc522.PICC_GetTypeName(piccType)); // Store the type as a string

  // Halt the card to stop further communication
  mfrc522.PICC_HaltA();

  // Display the UID and PICC type on the OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Card UID: ");
  for (byte i = 0; i < uidLength; i++) {
    display.print(activeMemoryUID[i], HEX);
    if (i < uidLength - 1) display.print(":");
  }
  display.setCursor(0, 16);
  display.print("PICC Type: ");
  display.print(activeMemoryPICC);
  display.display();

  // Optional debugging to Serial Monitor
  Serial.print("Card UID: ");
  for (byte i = 0; i < uidLength; i++) {
    Serial.print(activeMemoryUID[i], HEX);
    if (i < uidLength - 1) Serial.print(":");
    }
}

void readUIDAndMemory() {
  // Check if a new RFID card is present
  if (!mfrc522.PICC_IsNewCardPresent()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Error: No card");
    display.display();
    delay(1000);
    return; // Exit if no card is detected
  }

  // Attempt to read the card's data
  if (!mfrc522.PICC_ReadCardSerial()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Error: Read failed");
    display.display();
    delay(1000);
    return; // Exit if the read fails
  }

  // Store UID into activeMemoryUID
  uidLength = mfrc522.uid.size; // Get UID size
  for (byte i = 0; i < uidLength; i++) {
    activeMemoryUID[i] = mfrc522.uid.uidByte[i];
  }

  // Get card type (PICC type)
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  activeMemoryPICC = String(mfrc522.PICC_GetTypeName(piccType)); // Store type as a string

  // Read memory blocks
  activeMemoryMemorySize = 0;
  byte buffer[18]; // Buffer to store read data (16 bytes + CRC)
  byte size = sizeof(buffer);

  for (byte block = 0; block < 64; block++) { // Adjust blocks based on card type
    byte status = mfrc522.MIFARE_Read(block, buffer, &size);
    if (status == MFRC522::STATUS_OK) {
      // Copy the block data into activeMemoryMemory
      for (byte i = 0; i < 16; i++) {
        activeMemoryMemory[activeMemoryMemorySize++] = buffer[i];
      }
    } else {
      break; // Stop if we encounter a read error
    }
  }

  // Halt the card to stop further communication
  mfrc522.PICC_HaltA();

  // Display the UID and memory status on the OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Card UID: ");
  for (byte i = 0; i < uidLength; i++) {
    display.print(activeMemoryUID[i], HEX);
    if (i < uidLength - 1) display.print(":");
  }
  display.setCursor(0, 16);
  display.print("PICC Type: ");
  display.print(activeMemoryPICC);
  display.setCursor(0, 32);
  display.print("Memory Blocks: ");
  display.print(activeMemoryMemorySize / 16);
  display.display();

  // Optional debugging to Serial Monitor
  Serial.print("Card UID: ");
  for (byte i = 0; i < uidLength; i++) {
    Serial.print(activeMemoryUID[i], HEX);
    if (i < uidLength - 1) Serial.print(":");
  }
  Serial.println();
  Serial.print("Memory Size: ");
  Serial.print(activeMemoryMemorySize);
  Serial.println(" bytes");
}

void writeToCardFromSlot(int selectedSlot) {
  if (selectedSlot < 0 || selectedSlot >= 3) {
    Serial.println("Invalid slot selected for writing.");
    return;
  }

  SavedSlot &slot = savedSlots[selectedSlot];

  if (slot.justUID) {
    // Write only the UID
    Serial.println("Writing UID to card...");
    MFRC522::StatusCode status = mfrc522.MIFARE_Ultralight_Write(0, slot.uid, slot.uidLength);
    if (status == MFRC522::STATUS_OK) {
      Serial.println("UID written successfully.");
    } else {
      Serial.print("Error writing UID: ");
      Serial.println(mfrc522.GetStatusCodeName(status));
    }
  } else {
    // Write both UID and memory
    Serial.println("Writing UID and memory to card...");

    // Write UID (first 4-7 bytes of the card)
    MFRC522::StatusCode status = mfrc522.MIFARE_Ultralight_Write(0, slot.uid, slot.uidLength);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("Error writing UID: ");
      Serial.println(mfrc522.GetStatusCodeName(status));
      return;
    }

    // Write memory blocks
    byte buffer[16]; // Each memory block is 16 bytes
    int dataIndex = 0;

    for (byte block = 0; block < 64 && dataIndex < slot.memorySize; block++) {
      memset(buffer, 0, sizeof(buffer)); // Clear buffer
      int bytesToWrite = min(slot.memorySize - dataIndex, 16); // Avoid overflow
      memcpy(buffer, &slot.memory[dataIndex], bytesToWrite);

      status = mfrc522.MIFARE_Write(block, buffer, sizeof(buffer));
      if (status != MFRC522::STATUS_OK) {
        Serial.print("Error writing to block ");
        Serial.print(block);
        Serial.print(": ");
        Serial.println(mfrc522.GetStatusCodeName(status));
        return;
      }

      dataIndex += bytesToWrite;
    }

    Serial.println("UID and memory written successfully.");
  }
}


void rfidPrint(){
  currentState = RFIDPRINT;
  display.setTextSize(0.6);              // Small text size
  display.setTextColor(SSD1306_WHITE); // White text color
  display.setCursor(10, 50);           // Set cursor to position (x=10, y=30)
  if(rfidPrintX = 1){
    display.print("Button 2 to return >");
  }else{
    display.print("Button 2 to save >");
  }
  display.display();
  rfidPrintX = 0;
}


void loop() {
  buttonState();

  switch (currentState) {
    case MENU:
       // Clear display
      display.clearDisplay();

  // Set text properties
      display.setTextSize(1);              // Small text size
      display.setTextColor(SSD1306_WHITE); // White text color
      display.setCursor(10, 30);           // Set cursor to position (x=10, y=30)

  // Print text
      display.print("Press Button");

  // Render the text on the display
      display.display();
      break;
    case OPTIONS:
      optionsMenu();
      break;
    case RFID:
      rfidMenu();
      break;
      case READMODE:
      readMode();
      break;
      case READ:
        if (readModeState == 0){
          readUID();
        }
        else{
          readUIDAndMemory();
        }
        rfidPrint();
        readDelay = 1;
      break;
      case SAVEMENU:
        saveMenu();
      break;
      case WRITEMENU:
        writeMenu();
      break;
      case WRITE:
        writeToCardFromSlot(selectedSlot);
        rfidPrintX = 1;
        rfidPrint();
        readDelay = 1;
      break;
    default:
      Serial.println("Unknown state");
      break;
  }
}