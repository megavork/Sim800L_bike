#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <TimeLib.h>

unsigned long timeRange;
long currentSleepTime = 50;  //min
long time = 0;
int msgNumber = 1;
//sender phone number with country code
//const String PHONE = "+ZZxxxxxxxxxx";//For only 1 default number user
String PHONE = "+48792668255";  //For Multiple user
String msg;

//GSM Module RX pin to Arduino 4
//GSM Module TX pin to Arduino 3
#define rxPin 4
#define txPin 3
SoftwareSerial sim800(txPin, rxPin);

#define LED_PIN 13

void setup() {
  pinMode(LED_PIN, OUTPUT);  //Setting Pin 13 as output
  digitalWrite(LED_PIN, LOW);
  delay(1000);

  Serial.begin(9600);
  Serial.println("Initializing Serial... ");

  sim800.begin(9600);
  Serial.println("Initializing GSM module...");

  sim800.print("AT+CMGF=1\r");  //SMS text mode
  delay(1000);

  time = minute();

  // String commandSmsCheck = "AT+CMGR=";
  // commandSmsCheck.concat(msgNumber);
  // commandSmsCheck.concat("\r");

  // sim800.print(commandSmsCheck);  //SMS text mode
  // delay(300);
}

void loop() {
  flash();

  // String commandSmsCheck = "AT+CMGR=";
  // commandSmsCheck.concat(msgNumber);
  // commandSmsCheck.concat("\r");

  // sim800.print(commandSmsCheck);  //SMS text mode
  // delay(300);

  while (sim800.available()) {
    parseData(sim800.readString());  //Calls the parseData function to parse SMS
  }

  doAction();  //Does necessary action according to SMS message

  while (Serial.available()) {
    sim800.println(Serial.readString());
  }
}

void parseData(String buff) {
  Serial.print("buff1:");
  Serial.println(buff);
  Serial.println("!");

  unsigned int index;
  unsigned int indexLast;

  //Remove sent "AT Command" from the response string.
  index = buff.indexOf("\r");
  buff.remove(0, index + 2);
  buff.trim();

  Serial.print("buff2:");
  Serial.print(buff);
  Serial.println("!");

  if (buff != "OK") {

    index = buff.indexOf(":");
    String cmd = buff.substring(0, index);
    cmd.trim();

    buff.remove(0, index + 2);

    Serial.print("buff3:");
    Serial.print(cmd);
    Serial.println("!");

    //Parse necessary message from SIM800L Serial buffer string
    if (cmd == "+CMT") {
      Serial.println("CMT_MSG:");
      //get newly arrived memory location and store it in temp
      index = buff.lastIndexOf(0x0D);                  //Looking for position of CR(Carriage Return)
      msg = buff.substring(index + 2, buff.length());  //Writes message to variable "msg"
      msg.toLowerCase();

      Serial.print("msg: ");  //Whole message gets changed to lower case
      Serial.println(msg);

      index = buff.indexOf(0x22);                     //Looking for position of first double quotes-> "
      PHONE = buff.substring(index + 1, index + 14);  //Writes phone number to variable "PHONE"
      Serial.println(PHONE);

    } else if (cmd.indexOf("+CMGR") > 0) {
      Serial.println("CMGR_MSG:");

      Serial.print("buff4:");
      Serial.print(buff);
      Serial.println("!");

      //get newly arrived memory location and store it in temp
      index = buff.lastIndexOf(0x22);                  //Looking for position of CR(Carriage Return)
      indexLast = buff.lastIndexOf(0x0a);              // looking for a new line mark
      msg = buff.substring(index + 2, indexLast - 1);  //Writes message to variable "msg"
      msg.toLowerCase();
      msg.trim();

      Serial.print("msg: ");  //Whole message gets changed to lower case
      Serial.println(msg);

      index = buff.indexOf(0x2C);                     //Looking for position of first double quotes-> "
      PHONE = buff.substring(index + 2, index + 14);  //Writes phone number to variable "PHONE"
      Serial.print("PHONE: ");
      Serial.println(PHONE);

      if (msgNumber >= 1) {
        // msgNumber--;

        sim800.print("AT+CMGDA=\"DEL READ\"\r");
        delay(500);

        sim800.print("AT+CMGDA=\"DEL SENT\"\r");
        delay(500);
      }
    }
  } else {
     Serial.println("BAD REQUEST.");
    //TODO: RESTART / sleep 8sec and restart
  }
}

void doAction() {

  if (msg == "alarm") {
    Reply("ALARM activated.");

  } else if (msg == "where r u" || msg == "where are you" || msg == "where are u" || msg == "where r you" || msg == "w r u" || msg == "wru") {
    Reply("GPS coords sended.");

  } else if (msg == "lock") {
    Reply("LOCK activated. Regular algorithm. Check every 50min.");

  } else if (msg == "sleep ") {
    //TODO: NOT WORKING
    Reply("SLEEP every * mins.");

  } else if (msg == "led on") {
    digitalWrite(LED_PIN, HIGH);
    Reply("LED is ON");

  } else if (msg == "led off") {
    digitalWrite(LED_PIN, LOW);
    Reply("LED is OFF");

  } else if (minute() - time > 1) {  //1min

    Serial.println("SLEEP mode");
    // sleepMode(1);
  }

  PHONE = "";  //Clears phone string
  msg = "";    //Clears message string
}

void Reply(String text) {
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

void flash() {
  pinMode(LED_BUILTIN, OUTPUT);

  for (byte i = 0; i < 10; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(300);
    digitalWrite(LED_BUILTIN, LOW);
    delay(300);
  }

  pinMode(LED_BUILTIN, INPUT);

}  // end of flash

// watchdog interrupt
ISR(WDT_vect) {
  wdt_disable();  // disable watchdog
}  // end of WDT_vect

void sleepMode(long newTime) {
  Serial.println("Time to sleep.");
  //time in minutes
  if (newTime != 0) {
    currentSleepTime = newTime;
  }

  //374 cycles = 50min: 50min * 60 / 8
  //7 for test
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
