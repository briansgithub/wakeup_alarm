#ifdef BUILD

//NodeMCU, Platform IO
#include <Arduino.h>

#include "RBDdimmer.h"

#define SEC (1000)
#define MIN (60*1000)
#define HRS (60*60*1000)

#define MIN_BRIGHT 22
#define MID_BRIGHT 55 
#define MAX_BRIGHT 94

#define TESTING
#ifdef TESTING 
     #define DUR_BRIGHTENING (20*SEC) 
     #define PAUSE_BEFORE_FLOODLIGHTS_ON (5*SEC)
     #define PAUSE_BEFORE_START_BLINKING (5*SEC)
     #define DUR_BLINKING (10*SEC)
     #define DUR_NOTBLINKING (10*SEC)
     #define DUR_UNTIL_LIGHTS_OFF (60*SEC)
     #define HH now.hour()
     #define MM now.minute()
     #define SS now.second()
#else 
     #define DUR_BRIGHTENING (3*MIN)
     #define PAUSE_BEFORE_FLOODLIGHTS_ON (2*MIN)
     #define PAUSE_BEFORE_START_BLINKING (5*MIN)
     #define DUR_BLINKING (4*MIN)
     #define DUR_NOTBLINKING (2*MIN)
     #define DUR_UNTIL_LIGHTS_OFF (2*HRS)
     //24-hr format. Exclude leading zeroes. 
     #define HH 9
     #define MM 20
     #define SS 0
#endif

/*
code for turning lights off
startTime = millis(); 
     if( millis() - startTime > _____ )
*/

//TODO: create a pin array instead of using macro

#define RELAY1_PIN 13 //GPIO 13 = D7
#define RELAY2_PIN 2  // 15 = D8, 2 = D4 //D4 used because D8 is LOW on boot (flashing the lights), while D4 is HIGH on boot
//#define RELAY3_PIN      //  =
//#define RELAY4_PIN      //  =
#define dimmer_pwm_pin 12 // 12 = D6
#define dimmer_zc_pin 14  // 14 = D5


dimmerLamp dimmer(dimmer_pwm_pin, dimmer_zc_pin);
uint8_t prevTime = 0; //used later for printing the time every 5 seconds

void setup()
{
    Serial.begin(115200);

#ifndef ESP8266
    while (!Serial)
        ; // wait for serial port to connect. Needed for native USB
#endif

    pinMode(RELAY1_PIN, OUTPUT);
    digitalWrite(RELAY1_PIN, HIGH);
    pinMode(RELAY2_PIN, OUTPUT);
    digitalWrite(RELAY2_PIN, HIGH);


#ifdef RELAY3_PIN
    pinMode(RELAY3_PIN, OUTPUT);
    digitalWrite(RELAY3_PIN, HIGH);
#endif
#ifdef RELAY4_PIN
    pinMode(RELAY4_PIN, OUTPUT);
    digitalWrite(RELAY4_PIN, HIGH);
#endif

    dimmer.begin(NORMAL_MODE, ON);
    dimmer.setPower(20); //abnormal blinking behavior when power<=20, even at 0. Dimmer connected to relay 1, so no current draw until relay1 is on.
}

void loop()
{


    if(1) //launch condition
    {
        int brightness = 0;
        unsigned long time_on = millis(); //used to turn off lights after 2 hours 
        
        digitalWrite(RELAY1_PIN, LOW);

        int minToMid_delay_dur = DUR_BRIGHTENING / (MID_BRIGHT - MIN_BRIGHT);
        minToMid_delay_dur *= 9; //from MIN to MID is given 9/10 of the time. MID to MAX is given 1/10 of the time.
        minToMid_delay_dur /= 10;

        brightness = MIN_BRIGHT;
        while (brightness < MID_BRIGHT)
        { 
            //65 is roughly brightest power setting. 94 is max usable setting. 20 is the min usable setting.
            //On Arduino Uno, int stores a 16-bit value
            //integer constants default to int, so some constant calculations may overflow (e.g. 60 * 1000 will yield a negative result).
            dimmer.setPower(brightness);

            Serial.print("power: "); 
            Serial.println(brightness);

            delay(minToMid_delay_dur);
            brightness++;
        }

        //perceived brightness scales logarithmically, so later power gets brighter faster
        // Mid to Max is given 1/10 of delay_dur time
        int midToMax_delay_dur = DUR_BRIGHTENING / (MID_BRIGHT - MIN_BRIGHT);
        midToMax_delay_dur /= 10;

        while ( brightness <= MAX_BRIGHT )
        { 
            dimmer.setPower(brightness);

            Serial.print("power: ");
            Serial.println(brightness);

            delay(midToMax_delay_dur);
            brightness++;
        }

        delay(PAUSE_BEFORE_FLOODLIGHTS_ON); 
        digitalWrite(RELAY2_PIN, LOW);
#ifdef RELAY3_PIN
        delay(3*MIN); // 3 min
        digitalWrite(RELAY3_PIN, LOW);
#endif
#ifdef RELAY4_PIN
        delay(2*MIN); // 2 min
        digitalWrite(RELAY4_PIN, LOW);
#endif

        delay(PAUSE_BEFORE_START_BLINKING);

        while ( (millis() - time_on) < DUR_UNTIL_LIGHTS_OFF)
        {
            unsigned long blinkStartTime = millis();
            while ( (millis()-blinkStartTime) < DUR_BLINKING)
            { 
                //digitalWrite(RELAY1_PIN, HIGH);
                digitalWrite(RELAY2_PIN, HIGH);
                delay(500);

                //digitalWrite(RELAY1_PIN, LOW);
                digitalWrite(RELAY2_PIN, LOW);

                delay(500);
            }

            //digitalWrite(RELAY1_PIN, LOW);
            digitalWrite(RELAY2_PIN, LOW);
            delay(DUR_NOTBLINKING); 
        }

        //turn off all outlets
        dimmer.setPower(20);
        digitalWrite(RELAY1_PIN, HIGH);
        digitalWrite(RELAY2_PIN, HIGH);
        #ifdef RELAY3_PIN
        digitalWrite(RELAY3_PIN, HIGH);
        #endif
        #ifdef RELAY4_PIN
        digitalWrite(RELAY4_PIN, HIGH);
        #endif

        #ifdef TESTING
        delay(24*HRS); //pause the test for 24 hours
        #endif 
    }

}

#endif