//*******************************************************************
//
// HMWHomebrew.cpp
//
// Homematic Wired Hombrew Hardware
// Arduino Uno als Homematic-Device
// HMW-HB-ARDUINO-UNO
// Thorsten Pferdekaemper (thorsten@pferdekaemper.com)
// nach einer Vorlage von
// Dirk Hoffmann (hoffmann@vmd-jena.de)
//
//-------------------------------------------------------------------
//Hardwarebeschreibung:
// =====================
//
// Kompatibel mit HMW-LC-Sw2-DR
// Pinsettings for Arduino Uno
//
// D0: RXD, normaler Serieller Port fuer Debug-Zwecke
// D1: TXD, normaler Serieller Port fuer Debug-Zwecke
// D2: RXD, RO des RS485-Treiber
// D3: TXD, DI des RS485-Treiber
// D4: Direction (DE/-RE) Driver/Receiver Enable vom RS485-Treiber
//
// D5: Taster 1   / Kanal 1
// D6: Taster 2   / Kanal 2
// D7: Schalter 1 / Kanal 3
// D8: Schalter 2 / Kanal 4
// D13: Status-LED

// Die Firmware funktioniert mit dem Standard-Uno Bootloader, im
// Gegensatz zur Homematic-Firmware

//*******************************************************************

// Do not remove the include below
#include "HMWHomebrew.h"

#include <EEPROM.h>
// TODO: Eigene SoftwareSerial
#include <SoftwareSerial.h>

// HM Wired Protokoll
#include "HMWRS485.h"
// default module methods
#include "HMWModule.h"

// Ein paar defines...
#define RS485_RXD 2
#define RS485_TXD 3
#define RS485_TXEN 4
#define I01 5
#define I02 6
#define O03 7
#define O04 8
#define LED 13

#define CHANNEL_IO_COUNT 4
#define CHANNEL_IO_BYTES 1  //CHANNEL_IO_COUNT / 8

// Das folgende Define kann benutzt werden, wenn ueber die
// Kanaele "geloopt" werden soll
// als Define, damit es zentral definiert werden kann, aber keinen (globalen) Speicherplatz braucht
#define CHANNEL_PORTS byte channelPorts[CHANNEL_IO_COUNT] = {I01, I02, O03, O04};


// --- EEPROM Variablen f�r Parametersets ---
// Die folgenden Defines sind die Adressen der entsprechenden
// "Variablen" im EEPROM
// EPA steht fuer "EEPROM Parameter Address"

// nach der eigestellten Logging Time sendet der Aktor seinen aktuellen Wert
// zur�ck an die Zentrale? mit einem "i" - event. Min 0.1 (1), Max 25.5 (255), default 2 (20) Sekunden
#define EPA_loggingTime 0x0001     // 1 byte

// Die Zentralenadresse wird wohl f�r die Logging "i" - Events benutzt.
#define EPA_centralAddress 0x0002  // 4 bytes

// Direktverkn�pfungen nicht erlaubt?
// TODO: Wird noch nicht benutzt. Wozu?
#define EPA_directLinkDeactivated 0x0006  // 2 bytes

// --- EEPROM Variablen f�r channels ---

// ist der Kanal Input oder Output (die ersten 12 Bits) (0 => Input => Default ???)
// TODO: in initDefault entsprechend setzen
// TODO: Obwohl der Parameter davor (directLinkDeactivated) als word (2 bytes)
//       definiert ist, ist die Adresse hier nur um 1 groesser. Da stimmt was nicht.
// #define EPA_channelBehaviour 0x0007  // 2 bytes ??

// ist der Kanal ein Taster oder Schalter und ist der Eingang gelocked?
// (die ersten 24 Bits) Ich vermute die Bits werden Paarweise ausgewertet
// Bit 1 pro Paar (1 => Taster => Default ???), Bit 2 pro Paar ( 1 => Locked => 0 Default) ???
// TODO: InitDefault muss dieses Byte auf 1 setzen
// TODO: Hier ist einiges unklar:
//    24 Bits aber nur ein Byte? Oder CHANNEL_IO_COUNT Bytes?
//    ...oder tatsaechlich nur ein Byte und die Bits gelten fuer jeweils 2 Ein-/Ausgaenge?
// #define EPA_channelInputTypeLocked 0x0009  // CHANNEL_IO_COUNT bytes ? Oder doch nur 1 byte?

// Sekunden nach der ein Tastendruck als Lang erkannt wird
// Pro kanal ein Byte Min: 0.4s, Max: 5s, 0,4 => Default => 0 ???
// TODO: in initDefault entsprechend setzen
#define EPA_channelLongPressTime 0x0010  // CHANNEL_IO_COUNT bytes (hier 2 fuer input?)

// Port Status, d.h. Port ist auf 0 oder 1
byte portStatus[CHANNEL_IO_BYTES];

unsigned int keyPressTimer[2];   // Wir haben zwei Inputs
// TODO: wird wirklich int gebraucht oder tut's auch byte?
unsigned int keyLongPressTime[2];
byte loggingTime;

SoftwareSerial rs485(RS485_RXD, RS485_TXD); // RX, TX
HMWRS485 hmwrs485(&rs485, RS485_TXEN, &Serial);
// device type: 0x11 = HMW-LC-Sw2-DR
// serial number
// address
// TODO: serial number und address sollte von woanders kommen
HMWModule hmwmodule(&hmwrs485, 0x11, "HHB2703111", 0x42380123);

// Read all inputs/outputs
// setzt Bits in portStatus[]
void readPins() {
  CHANNEL_PORTS
  for(byte i = 0; i < CHANNEL_IO_COUNT; i++){
	  // Pin lesen und Bit in portStatus setzen
	  // TODO: Check if this really works
	  bitWrite(portStatus[i/8],i%8,digitalRead(channelPorts[i]));
  }
}


void setModuleConfig() {
  byte setModuleConfig_byte;
  unsigned int setModuleConfig_word;
  unsigned long setModuleConfig_dword;

// loggingTime (Sekunden * 10)
   loggingTime = EEPROM.read(EPA_loggingTime);
   if(loggingTime < 1) loggingTime = 20;  // default ist 2 Sekunden

// Zentralenadresse
   setModuleConfig_dword = EEPROM.read(EPA_centralAddress +3);
   setModuleConfig_dword = (setModuleConfig_dword << 8) | EEPROM.read(EPA_centralAddress +2);
   setModuleConfig_dword = (setModuleConfig_dword << 8) | EEPROM.read(EPA_centralAddress +1);
   setModuleConfig_dword = (setModuleConfig_dword << 8) | EEPROM.read(EPA_centralAddress);
   if(setModuleConfig_dword == 0){
	   // 0x00000001, aber es steht eh 0 drin, also nur 1 Byte schreiben
	   EEPROM.write(EPA_centralAddress,1);
   }

// Zeit fuer "langer" Tastendruck pro Channel
   for(byte i = 0; i < 2; i++) {
	   keyLongPressTime[i] = EEPROM.read(EPA_channelLongPressTime + i);
   }

// set input/output
// Input Pins arbeiten mit PULLUP, d.h. muessen per Taster
// auf Masse gezogen werden
  pinMode(I01,INPUT_PULLUP);
  pinMode(I02,INPUT_PULLUP);
  pinMode(O03,OUTPUT);
  digitalWrite(O03,LOW);
  pinMode(O04,OUTPUT);
  digitalWrite(O04,LOW);
}



//The setup function is called once at startup of the sketch
void setup()
{
	pinMode(RS485_RXD, INPUT);
	pinMode(RS485_TXD, OUTPUT);
	pinMode(RS485_TXEN, OUTPUT);
	digitalWrite(RS485_TXEN, LOW);

	//   timer0 = 255
   Serial.begin(57600);
   rs485.begin(19200);
// config aus EEPROM lesen
   setModuleConfig();

// Test
// 0000111111100011
// channelParam_behaviour = &H0FE3

  hmwrs485.debug("huhu\n");
}

// The loop function is called in an endless loop
void loop()
{
// TODO: Alles in loop
// TODO: Long/short key

   static byte keyPress[2] = {0,0};
   static byte outstatus = 0;

// Daten empfangen (tut nichts, wenn keine Daten vorhanden)
   hmwrs485.receive();
 // Check
     if(hmwrs485.frameComplete) {
        if(hmwrs485.targetAddress == hmwrs485.txSenderAddress || hmwrs485.targetAddress == 0xFFFFFFFF){
          hmwrs485.parseFrame();
          hmwmodule.processEvents();
        }
     };

// Check Keys
// Hier werden alle Ein-/Ausgaenge gelesen
// Pins lesen und nach portStatus[] schreiben
  readPins();

// TODO: Das folgende ist nur fuer Testzwecke
// Alle 20 Sekunden eine Nachricht schicken
  // hmwTxTargetAdress(4)                   the target adress
  // hmwTxFrameControllByte                 the controll byte
  // hmwTxSenderAdress(4)                   the sender adress
  // hmwTxFrameDataLength                   the length of data to send
  // hmwTxFrameData(MAX_RX_FRAME_LENGTH)    the data array to send

  static unsigned long lasttime = 0;
  unsigned long time = millis();
  if(time-lasttime >= 20000){
    lasttime = time;

    // 2 buttons
    for(byte i = 0; i < 2; i++)
      // if(bitRead(portStatus[i/8],i%8))
      {
    	keyPress[i]++;
        hmwmodule.broadcastKeyEvent(i,keyPress[i]);
      };
    // 2 Ausgaenge
    for(byte i = 2; i < 4; i++){
      // if(bitRead(portStatus[i/8],i%8))
      if(outstatus)
        hmwmodule.broadcastInfoMessage(i, 0xC800);
      else
        hmwmodule.broadcastInfoMessage(i, 0x0000);
    }
    outstatus = !outstatus;
  }

// TODO: ueber den Bus schicken. Wann?

/* TODO...
 * for i = 1 to CANNEL_IO_COUNT
        if portStatus(i).0 = 0 then                                            ' Port ist Input oder Output

           if portStatus(i).1 = 0 then                                         ' Key am port ist gedr�ckt

              if keyPressTimer(i) = 0 then
                 keyPressTimer(i) = 2
              end if

              if keyPressTimer(i) = 100 then
                 set portStatus(i).2                                           ' set status long keypress detected
                 keyPressTimer(i) = 80                                         ' counter f�r repeated longpress
                                                                               ' Muss noch validiert werden
                 debug "longKey " ; i ; " - " ; keyPressTimer(i)
              end if

           elseif keyPressTimer(i) > 0 then
              if keyPressTimer(i) > 10 and portStatus(i).2 = 0then
                 debug "shortKey " ; i ; " - " ; keyPressTimer(i)
              end if
              reset portStatus(i).2
              keyPressTimer(i) = 0
           end if

        end if
     next */

}




/*
 * TODO... '**
 '* isr_timer0
 '*
 '* Config Timer0 = Timer , Prescale = 1024
 '* Timer 0 ist ein 8Bit Timer Bei Prescaler 1024 und 16 Mhz Taktfrequenz des AVR
 '* wird der Interrupt ca 61 mal pro Sekunde aufgerufen. Daher setzen wir den Timer
 '* zu begin der isr auf einen Startwert 100. Der Timer braucht dann f�r einen �berlauf
 '* nur noch 156 schritte wieter zu z�hlen. Somit ergibt sich dann ein Interrupt ca. alle 100 ms.
 '*
 '**
 isr_timer0:
    dim isr_timer0_i as byte
    timer0 = 100
'      timer0 = 250

    for isr_timer0_i = 1 to CANNEL_IO_COUNT
       if keyPressTimer(isr_timer0_i) > 1 then
          if keyPressTimer(isr_timer0_i) < 65536 then
             incr keyPressTimer(isr_timer0_i)
          end if
       end if
    next

 return */


