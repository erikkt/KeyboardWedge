#include <MFRC522.h>
#include <require_cpp11.h>
#include <MFRC522Extended.h>
#include <deprecated.h>
#include <SPI.h>
#include <BleKeyboard.h>



/*
 * --------------------------------------------------------------------------------------------------------------------
 * Code for a Bluetooth capable controller and an MFRC522 to read a password from
 *   an NFC card and "type" it out to the host computer.
 * --------------------------------------------------------------------------------------------------------------------
 * This code was written to run on an ESP32. The following connections need to be made between the
 * ESP32 and the MFRC522 board.
 * 
 *       ESP32         MFRC522 PIN
 *        5                SDA
 *        27               RST
 *        19               MISO
 *        18               SCK
 *        23               MOSI
 *        3V               3.3V
 *       Gnd               GND
 * 
 * This project has been placed into the public domain.  Anyone may make one of these wedges.
 * 
 * This Code will read any of the supported card types and will then search through the data on the card looking for
 * the header string "ZPKW" from that location until the following null will be output via bluetooth keyboard as though the
 * wedge was a keyboard followed by the Enter key.
 * 
 * Currently the following NFC card types are supported:
 * 
 */


// Create the UIDPassword Array
char * UIDPassword[][2] = { 
  { "1153BC4E97", "ArrayPassword1"}, 
  { "D3E14208", "ArrayPassword2"}, 
  { "11041A3D320A5481", "ArrayPassword3"},
  { NULL, NULL }
};

// Set the end character
uint8_t endChar = '$';

// Set the header string
byte header[] = "ZPKW";

// Define the ESP32 pins, these are configurable.
#define RST_PIN   27
#define SS_PIN    5
#define LEDPIN   2

BleKeyboard bleKeyboard("ESP32 Wedge");
//bleKeyboard.setDelay(60);
// Create MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);   
MFRC522::MIFARE_Key key;


int getUIDPassword(MFRC522::Uid uid) {
  uint8_t entry = 0;
  uint8_t pwLength;

  char hexDigits[] = "0123456789ABCDEF"; 
  // Convert the uid into a Hex String
  char UIDString[(uid.size * 2) + 1 ];
  uint8_t uidsize = uid.size;
  for (uint8_t ch = 0 ; ch < uidsize ; ch ++)
  {
    UIDString[(ch * 2)] = hexDigits[uid.uidByte[ch]/16];
    UIDString[(ch *2) + 1] = hexDigits[uid.uidByte[ch]%16];
  }
  UIDString[uid.size * 2] = '\0';
  
  while ( UIDPassword[entry][0] != NULL ) 
  {
    
    if (strcmp(UIDString, UIDPassword[entry][0]) == 0)
    {
      Serial.print("Array password found\nPassword: ");
      pwLength = strlen(UIDPassword[entry][1]);
      //Keyboard.begin();
      for (uint8_t i = 0; i < pwLength ; i++)  
      {
        bleKeyboard.write(UIDPassword[entry][1][i]);
        Serial.print(UIDPassword[entry][1][i]);
      }
      Serial.println();
      blink();
      bleKeyboard.write(KEY_RETURN);
      delay(1000);
      return(true);
    }
    entry++;
  }
  Serial.println("No array password found!");
  return(false);  
}

int getPassword(uint8_t blocks, uint8_t blockLength, bool auth) {
  
  //some variables we need
  uint8_t block;
  uint8_t bufferSize;
  bool password;
  MFRC522::StatusCode status;
  
  // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
  for (uint8_t i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  
  bufferSize = 18;
  //bufferSize = blockLength + 2;
  uint8_t buffer[bufferSize];
  password = false;
  
  
  for (byte count = 0; count < 3; count++)
  {
    
    for (block = 1; block <= blocks; block++) 
    {
      // Authenticate
      if (auth)
      {
        status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)); //line 834 of MFRC522.cpp file
        if (status != MFRC522::STATUS_OK)
        {
          Serial.println("Auth failure");
          Serial.println(mfrc522.GetStatusCodeName((MFRC522::StatusCode)status));
          return(false);
        }
      }
      // Read block     
      status = mfrc522.MIFARE_Read(block, buffer, &bufferSize);
      
      if (status != MFRC522::STATUS_OK) 
      {
        Serial.println("Read failure");
        Serial.println(mfrc522.GetStatusCodeName((MFRC522::StatusCode)status));
        return(false);
      }  

      
      // Check data and output the password
      for (uint8_t i = 0; i < blockLength; i++)  
      {
        //l = i+3;
        if (buffer[i] == header[0] && buffer[i+1] == header[1] && buffer[i+2] == header[2] && buffer[i+3] == header[3])
        {
          // We found the password header
          Serial.println("Password header found!");
          Serial.print("Password: ");
          // Now get ready to start typing
          password = true;
          i+=3;       
        }
        else if (buffer[i] == endChar && password == true)
        {
          // We found the end character at the end of the password, type return.
          
          password = false;
          bleKeyboard.write(KEY_RETURN);
          Serial.println("\nEnd character found");
          blink();
          delay(1000);
          return(true);
        }
        else
        {
          // Otherwise we either ignore the character (we are not in the password block) or type it (we are)
          if (password == true)
          { 
            // We are in the password block so send the password keystroke.
            Serial.write(buffer[i]);
            bleKeyboard.write(buffer[i]);
            delay(5);
          }
        }
      }
    }
  }
  Serial.println("No tag stored password found!");
  return(false);
}

int getUID(MFRC522::Uid uid) {
  uint8_t pwLength;
  char hexDigits[] = "0123456789ABCDEF"; 

  uint8_t uidsize = uid.size;
  Serial.print("uid: ");  
  
  // Convert the uid into a Hex String
  for (uint8_t ch = 0; ch < uidsize ; ch ++)
  {
    Serial.print(hexDigits[uid.uidByte[ch]/16]);
    Serial.print(hexDigits[uid.uidByte[ch]%16]);
    bleKeyboard.write(hexDigits[uid.uidByte[ch]/16]);
    bleKeyboard.write(hexDigits[uid.uidByte[ch]%16]);
  }
  Serial.println();
  bleKeyboard.write(KEY_RETURN);
  blink();
  delay(1000);
  return(true);
}
void blink() {
  for (uint8_t b = 0; b < 6 ; b ++)
  {
    digitalWrite(LEDPIN, LOW);
    delay(80);
    digitalWrite(LEDPIN, HIGH);
    delay(80);
  }
}
void setup() {

  // Initialiase the SPI bus
  SPI.begin();
  Serial.begin(115200);
  bleKeyboard.begin();
  
  // Initialise the MFRC522 Card Reader
  mfrc522.PCD_Init();
  mfrc522.PCD_WriteRegister(MFRC522::RFCfgReg, MFRC522::RxGain_avg);
  pinMode(LEDPIN, OUTPUT);
}

void loop() {

  if(bleKeyboard.isConnected()) {
    digitalWrite(LEDPIN, HIGH);
  }
  else 
  {
    digitalWrite(LEDPIN, LOW); 
  }

  //Define some variables;
  byte blocks;
  byte blocklength;
  bool auth = true;
  MFRC522::PICC_Type cardName; 
 
  // Look for new cards, and select one if present
  if ( mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial() ) {
    // Get the card type 
    cardName = mfrc522.PICC_GetType(mfrc522.uid.sak);
    
    Serial.println(mfrc522.PICC_GetTypeName((MFRC522::PICC_Type)cardName));
    // Select the Card info
    switch (cardName)
    {
      case MFRC522::PICC_TYPE_ISO_14443_4:
        // Not yet defined
        return;
        break;
      case MFRC522::PICC_TYPE_ISO_18092:
        // Not yet defined
        return;
        break;
      case MFRC522::PICC_TYPE_MIFARE_MINI:
        // The Classic Mifare Mini has 20 blocks of 16 bytes each.
        blocks=19;
        blocklength=16;
        break;
      case MFRC522::PICC_TYPE_MIFARE_UL:
        // The Mifare Ultralight has 16 blocks of 4 bytes each.
        blocks=15;
        blocklength=4;
        // If this is an NTAG21[356] it will show up here, the UID will be 7 characters
        // and the first UID byte will be 04h
        if (mfrc522.uid.uidByte[0] == 04 && mfrc522.uid.size == 7)
        {

          // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
          for (uint8_t i = 0; i < 6; i++) {
            key.keyByte[i] = 0xFF;
          }
          uint8_t bufferSize = 18;
          uint8_t buffer[bufferSize];
          uint8_t block=3;
          MFRC522::StatusCode status;

          // Read block
          status = mfrc522.MIFARE_Read(block, buffer, &bufferSize);
          if (status != MFRC522::STATUS_OK) 
          {
            return;
          }

          switch (buffer[2])
          {
            case 0x12:
              // NTAG213
              Serial.println("Type: NTAG213");
              blocks=43;
              break;
            case 0x3E:
              // NTAG215
              Serial.println("Type: NTAG215");
              blocks=133;
              break;
            case 0x6D:
              // NTAG216
              // Turning authentication off
              auth = false;
              Serial.println("Type: NTAG216");
              blocks=229;
              break;
            default:
              //Unknown NXP tag
              Serial.println("Type: Unknown");
              return;
          }
        }
        break;
//      case MFRC522::PICC_TYPE_MIFARE_UL_C:
//        // The Mifare Ultralight C has 48 blocks of 4 bytes each.
//        blocks=47;
//        blocklength=4;
//        break;
      case MFRC522::PICC_TYPE_MIFARE_PLUS:
        // No information
        return;
        break;
      case MFRC522::PICC_TYPE_MIFARE_1K:
        // The Classic Mifare 1K has 64 blocks of 16 bytes each.
        blocks=63;
        blocklength=16;
        break;
      case MFRC522::PICC_TYPE_MIFARE_4K:
        // The Classic Mifare 4K has 256 blocks of 16 bytes each.
        blocks= 255;
        blocklength=16;  
        break;
      default:
        // We don't know how to handle this card type yet.
        return;
    }
    // See if the uid is in our password array
    if ( ! getUIDPassword(mfrc522.uid))
    {
      if ( ! getPassword(blocks, blocklength, auth))
      {
        getUID(mfrc522.uid);
      }
    }
  }
  bleKeyboard.end();
  // Close the card reader 
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
