#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <TimeLib.h>
#include <EEPROM.h>

unsigned long timeRange;
String const PHONE = "+48792668255";  //For Multiple user
String smsPhone = "";  //For Multiple user
String msg;
String const googleMap = "https://maps.google.com/?q=";
String gpsCoords = "";

long currentSleepTime = 50;  //min
long time = 0;
int countSms = 0;

bool isCheckAllSms = false;
bool isRedUnreadSms = false;
bool isAllUnreadSmsCheck = false;
bool isSleepActivated = false;
bool isGpsRequested = false;

#define RX_PIN 11  // Arduino Pin connected to the TX of the GPS module
#define TX_PIN 10  // Arduino Pin connected to the RX of the GPS module

TinyGPSPlus gps;                           // the TinyGPS++ object
SoftwareSerial gpsSerial(RX_PIN, TX_PIN);  // the serial interface to the GPS module

//GSM Module RX pin to Arduino 4
//GSM Module TX pin to Arduino 3
#define rxPin 4
#define txPin 3
SoftwareSerial sim800(txPin, rxPin);

#define LED_PIN 13

void setup() {
  pinMode(LED_PIN, OUTPUT);  //Setting Pin 13 as output
  digitalWrite(LED_PIN, LOW);
  delay(200);

  isCheckAllSms = true;

  Serial.begin(9600);
  Serial.println("Initializing Serial... ");

  // gpsSerial.begin(9600);  // Default baud of NEO-6M GPS module is 9600
  // Serial.println("Initializing GPS module...");

  sim800.begin(9600);
  Serial.println("Initializing GSM module...");

  sim800.print("AT+CMGF=1\r");  //SMS text mode
  delay(1000);

  sim800.print("AT+CSQ\r");  //SMS text mode
  delay(1000);

  sim800.print("AT+COPS?\r");  //SMS text mode
  delay(1000);

  time = minute();
  isSleepActivated = false;
}

void loop() {

  delay(500);

  // if(isSleepActivated) {
  //   sleepMode(2);
  // }

  String commandSmsCheck = "";

  if (isCheckAllSms) {
    commandSmsCheck = "AT+CPMS?\r";
  } else if (isRedUnreadSms) {
    commandSmsCheck = "AT+CMGR=" + String(countSms) + "\r";
  } else {
    commandSmsCheck = "";
  }

  sim800.print(commandSmsCheck);
  delay(500);

  while (isGpsRequested) {
    gpsCoords = getGpsCoords();

    if (gpsCoords != "") {
      gpsSerial.end();
      sim800.begin(9600);
      Serial.println("Initializing GSM module...");
      delay(1000);
      isGpsRequested = false;
    }
  }

  while (sim800.available()) {
    parseData(sim800.readString());  //Calls the parseData function to parse SMS
  }

  doAction();  //Does necessary action according to SMS message

  while (Serial.available()) {
    sim800.println(Serial.readString());
  }
}

void parseData(String buff) {
  buff.trim();
  Serial.println("Start parsing data:");

  Serial.println("buff1:" + String(buff) + ":%");

  unsigned int index;
  unsigned int indexLast;

  //Remove sent "AT Command" from the response string.
  index = buff.indexOf("\r");
  buff.remove(0, index + 2);
  buff.trim();

  Serial.println("buff2:" + String(buff) + ":%");

  if (buff.indexOf("OK") >= 0) {

    index = buff.indexOf(":");
    String cmd = buff.substring(0, index);
    cmd.trim();

    buff.remove(0, index + 2);

    Serial.println("cmd3:" + String(cmd) + ":%");

    //Parse necessary message from SIM800L Serial buffer string
    if (cmd == "+CMT") {
      parseCMT_Command(buff);

    } else if (cmd.indexOf("+CMGR") >= 0) {
      parseCMGR_Command(buff);

    } else if (cmd.indexOf("+CPMS") >= 0) {
      parseCPMS_Command(buff);
    }

  } else if (buff.indexOf("CMTI") >= 0) {
    parseCMTI_Command(buff);

  } else {
    Serial.println("BAD REQUEST.");
    //TODO: RESTART / sleep 8sec and restart
  }
}

void parseCMT_Command(String buff) {
  Serial.println("CMT_MSG:");
  //get newly arrived memory location and store it in temp
  int index = buff.lastIndexOf(0x0D);              //Looking for position of CR(Carriage Return)
  msg = buff.substring(index + 2, buff.length());  //Writes message to variable "msg"
  msg.toLowerCase();

  Serial.println("msg:" + String(msg) + ":%");

  index = buff.indexOf(0x22);                     //Looking for position of first double quotes-> "
  smsPhone = buff.substring(index + 1, index + 14);  //Writes phone number to variable "PHONE"
  Serial.println(smsPhone);
}

void parseCMGR_Command(String buff) {
  Serial.println("CMGR_MSG:");

  Serial.println("buff4:" + String(buff) + ":%");

  //get newly arrived memory location and store it in temp
  int index = buff.lastIndexOf(0x22);              //Looking for position of CR(Carriage Return)
  int indexLast = buff.lastIndexOf(0x0a);          // looking for a new line mark
  msg = buff.substring(index + 2, indexLast - 1);  //Writes message to variable "msg"
  msg.toLowerCase();
  msg.trim();

  Serial.print("msg: ");  //Whole message gets changed to lower case
  Serial.println(msg);

  index = buff.indexOf(0x2C);                     //Looking for position of first double quotes-> "
  smsPhone = buff.substring(index + 2, index + 14);  //Writes phone number to variable "PHONE"

  Serial.print("smsPhone: ");
  Serial.println(smsPhone);

  if (countSms >= 0) {
    countSms = 0;

    sim800.print("AT+CMGDA=\"DEL READ\"\r");
    delay(500);

    sim800.print("AT+CMGDA=\"DEL SENT\"\r");
    delay(500);

    isCheckAllSms = false;
    isRedUnreadSms = false;
  }
}

void parseCPMS_Command(String buff) {
  Serial.println("CPMS:");

  Serial.println("buff5:" + String(buff) + ":%");

  int index = buff.lastIndexOf("SM_P") + 6;
  msg = buff.substring(index, index + 1);
  countSms = msg.toInt();

  Serial.println("COUNT:" + String(countSms) + ":%");

  isCheckAllSms = false;

  if (countSms > 0) {
    isRedUnreadSms = true;
  } else {
    isAllUnreadSmsCheck = true;
  }
}

void parseCMTI_Command(String buff) {
  Serial.println("CMTI:");

  Serial.println("buff6:" + String(buff) + ":%");

  int index = buff.lastIndexOf(",");
  msg = buff.substring(index + 1, index + 2);
  countSms = msg.toInt();

  Serial.println("COUNT:" + String(countSms) + ":%");

  isCheckAllSms = false;

  if (countSms > 0) {
    isRedUnreadSms = true;
  } else {
    isAllUnreadSmsCheck = true;
  }
}

void doAction() {

  Serial.println("Start doing acton:");

  if (msg == "alarm") {
    Reply("ALARM activated.");

  } else if (msg == "where r u" || msg == "where are you" || msg == "where are u" || msg == "where r you" || msg == "w r u" || msg == "wru") {
    gpsCoords = "";
    isGpsRequested = true;
    // Reply("GPS coords sended.");

  } else if (msg == "lock") {
    Reply("LOCK activated. Regular algorithm. Check every 50min.");

  } else if (msg == "sleep") {
    Reply("SLEEP every * mins.");

  } else if (gpsCoords != "") {
    Reply("GPS:");
    delay(2000);
    
    Reply(gpsCoords);
    delay(1000);
    gpsCoords = "";

  } else if (msg == "led on") {
    digitalWrite(LED_PIN, HIGH);
    Reply("LED is ON");

  } else if (msg == "led off") {
    digitalWrite(LED_PIN, LOW);
    Reply("LED is OFF");

  } else if (msg == "clear") {
    sim800.print("AT+CMGDA=\"DEL ALL\"\r");
    delay(1000);
    countSms--;
    Reply("Deleted all SMS.");

  } else if (minute() - time > 5) {  //5min
    Serial.println("SLEEP mode.");
    isSleepActivated = true;
  }

  smsPhone = "";  //Clears phone string
  msg = "";    //Clears message string
}

void Reply(String text) {
  Serial.println("Start reply:");
  sim800.print("AT+CMGF=1\r");
  delay(500);
  sim800.print("AT+CMGS=\"" + PHONE + "\"\r");
  delay(500);
  sim800.print(text);
  delay(100);
  sim800.write(0x1A);  //ascii code for ctrl+z, DEC->26, HEX->0x1A
  delay(500);
  Serial.println("SMS Sent Successfully.");
}

// watchdog interrupt
ISR(WDT_vect) {
  wdt_disable();  // disable watchdog
}  // end of WDT_vect

void sleepMode(long newTime) {
  Serial.println("Time to sleep.");
  //time in minutes
  if (newTime > 0) {
    currentSleepTime = newTime;
  }

  //374 cycles = 50min: 50min * 60 / 8
  //7 cycles for test: 1min
  int cycles = currentSleepTime * 60 / 8;

  for (int i = 0; i < cycles; i++) {
    // disable ADC
    ADCSRA = 0;

    // clear various "reset" flags
    MCUSR = 0;
    // allow changes, disable reset
    WDTCSR = bit(WDCE) | bit(WDE);
    // set interrupt mode and an interval
    WDTCSR = bit(WDIE) | bit(WDP3) | bit(WDP0);  // set WDIE, and 8 seconds delay
    wdt_reset();                                 // pat the dog

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    noInterrupts();  // timed sequence follows
    sleep_enable();

    // turn off brown-out enable in software
    MCUCR = bit(BODS) | bit(BODSE);
    MCUCR = bit(BODS);
    interrupts();  // guarantees next instruction executed
    sleep_cpu();
  }

  // cancel sleep as a precaution
  sleep_disable();
  Serial.println("Wake UP.");
  startUpSettings();
}

void startUpSettings() {
  pinMode(LED_PIN, OUTPUT);  //Setting Pin 13 as output
  digitalWrite(LED_PIN, LOW);
  delay(200);

  isCheckAllSms = true;

  Serial.begin(9600);
  Serial.println("Initializing Serial... ");

  sim800.begin(9600);
  Serial.println("Initializing GSM module...");

  sim800.print("AT+CMGF=1\r");  //SMS text mode
  delay(5000);

  time = minute();
  isSleepActivated = false;
}

String getGpsCoords() {
  sim800.end();
  gpsSerial.begin(9600);  // Default baud of NEO-6M GPS module is 9600
  Serial.println("Initializing GPS module...");
  delay(1000);

  long gpsTime = minute();
  String result = "";
  Serial.println("Get Gps Coords:");
  while (result == "") {
    while (gpsSerial.available()) {
      if (gps.encode(gpsSerial.read())) {
        if (gps.location.isValid()) {
          Serial.print(F("- latitude: "));
          Serial.println(gps.location.lat(), 6);

          Serial.print(F("- longitude: "));
          Serial.println(gps.location.lng(), 6);

          result = googleMap + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
          Serial.println(result);
          return result;
        } else if (minute() - gpsTime > 1) {
          break;
        } else {
          Serial.println(F("- location: INVALID"));
          return "";
        }
      }
      // if (millis() > 5000 && gps.charsProcessed() < 10)
      //   Serial.println(F("No GPS data received: check wiring"));
    }
  }
}

//send GPS coords after 15mins
void Alarm() {
}

//send GPS coords one time
void whereRU() {
}

//set sleep time
void sleepTime() {
}

//set regular algorithm: sleep 50min and check SMS
void lock() {
}
