#include "ThisThread.h"
#include "mbed.h"
#include "DFRobot_RGBLCD1602.h"
#include "HTS221Sensor.h"

#include <ctime>
#include <string>

#include "text_utilities.h"
#include "graphics.h"
#include "network_utilities.h"
#include "json.hpp"
#include "certificate.h"

// Define the pins for I2C communication
I2C lcdI2C(D14, D15); // SDA, SCL
DevI2C devI2cSensor(PB_11, PB_10); // Sensor I2C bus
HTS221Sensor hts(&devI2cSensor);

// Create an LCD object
DFRobot_RGBLCD1602 lcd(&lcdI2C);  // Create an LCD display object

// Define the button pin
DigitalIn buttonPage(A0, PullUp);
InterruptIn buttonAdd(A1, PullUp);
InterruptIn buttonSubtract(A2, PullUp);
InterruptIn buttonEnter(A3, PullUp);

// Define buzzer
PwmOut buzzer(D13); //D13 eller PA_5

// Define Page controller
PageController page_controller;
net_util nu;

//Define the alarm and alarm struct
struct alarm_t {
    int hours;
    int minutes;
    std::string alarm_state; //active, going, snooze, mute, deactive
};
alarm_t alarm;
alarm_t soundUntil; //Makes so the sound plays for 10 minuttes

//Define Thread
Thread check_alarm;
Thread play_melody;

//Define global variables
int current_time_setter = 0;
int sett_hours = 0;
int sett_minutes = 0;
bool input_happened = false;
bool unmute = false;
bool OnPageSetAlarm = false;
bool OnPageAlarmConfig = false;
bool snoozeIsHit = false;

void alarmOff();
void alarm_melody();
void AddTime() 
{
    input_happened = true;
    if(OnPageSetAlarm) {
        if(current_time_setter == 0) sett_hours++;
        else if (current_time_setter == 1) sett_minutes++;
    }
    else if(OnPageAlarmConfig) {
        if(alarm.alarm_state == "going") {
            alarm.alarm_state = "snooze";
            snoozeIsHit = true;
        }
        else alarm.alarm_state = "mute";
    }
}
void SubtractTime() 
{
    input_happened = true;
    if(OnPageSetAlarm) {
        if(current_time_setter == 0) sett_hours--;
        else if (current_time_setter == 1) sett_minutes--;
    }
    else if(OnPageAlarmConfig) {
        if(alarm.alarm_state == "deactive")
        alarm.alarm_state = "active";
    }
}
void EnterValue() 
{
    input_happened = true;
    if(OnPageSetAlarm) {
        current_time_setter++;
        if (current_time_setter == 2) {
            alarm.minutes = sett_minutes % 60;
            alarm.hours = sett_hours % 24;
            alarm.alarm_state = "active";
        }
    }
    else if(OnPageAlarmConfig) {
        alarm.alarm_state = "deactive";
    }
}


// Function prototypes


class Boot : public Page {
public:
    void display() override {
        lcd.clear();
        lcd.printf("Unix epoch time: ");
        lcd.setCursor(0, 1);
        lcd.printf("1234567890");
        ThisThread::sleep_for(2s);

        lcd.clear();
        lcd.printf("Lat: 58.3405");
        lcd.setCursor(0, 1);
        lcd.printf("Lon: 8.5934");
        ThisThread::sleep_for(2s);

        lcd.clear();
        lcd.printf("City: ");
        lcd.setCursor(0, 1);
        lcd.printf("Grimstad");
        ThisThread::sleep_for(2s);
        
        lcd.clear();
        lcd.printf("Sun 28 Apr 13:55");
    }
};

class SettAlarm : public Page {
public:
    void display() override {    
        OnPageSetAlarm = true;
        sett_hours = 0;
        sett_minutes = 0;
        printf("Page 1\n");
        bool entered_page = true;
        while(1) {
            if(input_happened || entered_page) {
                entered_page = false;
                input_happened = false;
                lcd.clear();
                if(current_time_setter == 0) {
                    lcd.printf("Sett Alarm %i:%i", sett_hours%24,sett_minutes%60);
                    lcd.setCursor(0,1);
                    lcd.printf("Enter hour");
                }
                else if(current_time_setter == 1) {
                    lcd.printf("Sett Alarm %i:%i", sett_hours%24,sett_minutes%60);
                    lcd.setCursor(0,1);
                    lcd.printf("Enter minute");
                }
                else if(current_time_setter == 2) {
                    lcd.printf("Sett Alarm %i:%i", alarm.hours%24,alarm.minutes%60);
                    lcd.setCursor(0,1);
                    lcd.printf("Alarm active");
                }
                if(current_time_setter > 2) {
                    current_time_setter = 0;
                    input_happened = true;
                }
            }
            ThisThread::sleep_for(300ms);
        }
    }
};

class AlarmConfig : public Page {       //active, going, snooze, mute, deactive
public:
    void display() override {
        OnPageAlarmConfig = true;
        bool entered_page = true;
        while(1) {
            if(input_happened || entered_page) {
                entered_page = false;
                input_happened = false;
                lcd.clear();
                lcd.printf("Alarm M(A1),En(A2),Dis(A3)");
                lcd.setCursor(0, 1);
                if(alarm.alarm_state == "going") {
                    lcd.clear();
                    lcd.printf("Alarm active:");
                    lcd.setCursor(0, 1);
                    lcd.printf("Press A1 to snooze");
                }
                else if(alarm.alarm_state == "active") {
                    lcd.printf("Alarm (A) %i:%i", alarm.hours, alarm.minutes);
                }
                else if(alarm.alarm_state == "mute") {
                    lcd.printf("Alarm %i:%i", alarm.hours, alarm.minutes);
                }
                else if(alarm.alarm_state == "snooze") {
                    lcd.printf("Alarm (S) %i:%i", alarm.hours, alarm.minutes);
                }
                else if(alarm.alarm_state == "mute") {
                    lcd.printf("Alarm %i:%i", alarm.hours, alarm.minutes);
                }
            }
        }
    }
};
class AnotherPage : public Page {
public:
    void display() override {
        float temperature, humidity;

        while (1) {
            lcd.clear();

            hts.get_temperature(&temperature);
            hts.get_humidity(&humidity);

            char buffer[32];
            int temp_int = (int)temperature;
            int temp_frac = (int)((temperature - temp_int) * 100);
            int hum_int = (int)humidity;
            int hum_frac = (int)((humidity - hum_int) * 100);

            // Display formatted temperature and humidity
            sprintf(buffer, "Temp: %d.%02d C", temp_int, temp_frac);
            lcd.setCursor(0, 0);
            lcd.printf(buffer);
            sprintf(buffer, "Hum: %d.%02d%%", hum_int, hum_frac);
            lcd.setCursor(0, 1);
            lcd.printf(buffer);

            ThisThread::sleep_for(1s);
        }
    }
};
class NewsPage : public Page {
public:
    void display() override {
        lcd.clear();
        lcd.setCursor(0, 0);  // Set cursor to the first row
        lcd.printf("Top News:");
        lcd.setCursor(0, 1);  // Set cursor to the second row
        lcd.printf("fetching...");

        std::string news_header = text_utils::extractTitle(nu.send_get_request("rss.cnn.com", "/rss/cnn_latest.rss").c_str());

        // Scroll the news headlines on the second row
        text_utils::printScrolling(&lcd, news_header, false, 300, 1);
    }
};

int main() {

    //std::string result = nu.send_get_request("rss.cnn.com", "/rss/cnn_latest.rss");
    
    //cout << extractTitle(result.c_str()) << endl;

    //TestPage tp;
    //AnotherPage ap;
    page_controller.add_page(static_cast<Page*>(new SettAlarm));
    page_controller.add_page(static_cast<Page*>(new AlarmConfig));
    page_controller.add_page(static_cast<Page*>(new AnotherPage));
    page_controller.add_page(static_cast<Page*>(new NewsPage));

    hts.init(nullptr);
    hts.enable();

    // Initialize the LCD
    lcd.init(); // Initialize the LCD
    lcd.clear();
    lcd.setRGB(255, 255, 255); // Set LCD backlight color to white

    check_alarm.start(callback(alarmOff));
    play_melody.start(callback(alarm_melody));

    buttonAdd.rise(&AddTime);
    buttonSubtract.rise(&SubtractTime);
    buttonEnter.rise(&EnterValue);

    
    // page_controller.display(0);
    
    static_cast<Page*>(new Boot)->display();

    while(1) {
        // Check if the button is pressed
        if(!buttonPage) {
            // Button is pressed, change the state and call the corresponding function
            if(OnPageSetAlarm) OnPageSetAlarm = false;
            if(OnPageAlarmConfig) OnPageAlarmConfig = false;
            page_controller.display_next();

            while (!buttonPage) {};
        }
        ThisThread::sleep_for(100ms); // Add a small delay to debounce the button
    }
}


void alarmOff() {
    while (true) {
        time_t seconds = time(NULL);
        char buffer[32];
        strftime(buffer, 32, "%H", localtime(&seconds));
        int current_rtc_hour = std::stoi(buffer);
        strftime(buffer, 32, "%M", localtime(&seconds));
        int current_rtc_minute = std::stoi(buffer);

        if(alarm.alarm_state == "going") {  //Check if alarm has gone off for 10 min
            if(current_rtc_hour == soundUntil.hours && current_rtc_minute == soundUntil.minutes) {
                alarm.alarm_state = "active";
                printf("it ran\n");
            }
        }     
        else if(alarm.alarm_state == "active") {
            if(current_rtc_hour == alarm.hours && current_rtc_minute == alarm.minutes) {     //Check if the alarm will go off
                soundUntil.hours = alarm.hours; //Sets up the time the sound will play to
                soundUntil.minutes = alarm.minutes + 10;
                if(soundUntil.minutes > 59) {
                    soundUntil.hours++;
                    soundUntil.minutes -= 60;
                }
                alarm.alarm_state = "going";
                OnPageSetAlarm = false;
                page_controller.display(1);
            }
        }
        else if (alarm.alarm_state == "snooze") {
            if(snoozeIsHit == true) {
                snoozeIsHit = false;
                time_t seconds = time(NULL);
                soundUntil.hours = current_rtc_hour;
                soundUntil.minutes = current_rtc_minute + 1;
                if(current_rtc_minute > 59) {
                    current_rtc_hour++;
                    current_rtc_minute -= 60;
                }
            }
            if(current_rtc_hour == soundUntil.hours && current_rtc_minute == soundUntil.minutes) {
                soundUntil.hours = current_rtc_hour;
                soundUntil.minutes = current_rtc_minute + 10;
                if(current_rtc_minute > 59) {
                    current_rtc_hour++;
                    current_rtc_minute -= 60;
                }
                alarm.alarm_state = "going";
                OnPageSetAlarm = false;
                page_controller.display(1);
            }
        }
        else if(alarm.alarm_state == "mute") {
            if(current_rtc_hour == alarm.hours && current_rtc_minute == alarm.minutes) {     //Check if the alarm will go off
                unmute = true;
            }
            if(unmute) {
                if(alarm.minutes == 59) {
                    if(current_rtc_hour == alarm.hours && current_rtc_minute == 0) {
                        alarm.alarm_state = "active";
                        input_happened = true;
                    }
                }
                else if(current_rtc_hour == alarm.hours && current_rtc_minute == alarm.minutes + 1) {
                    alarm.alarm_state = "active";
                    input_happened = true;
                }
            }
        }
        ThisThread::sleep_for(500ms);
    }
}

void alarm_melody() {
    while(1) {
        if(alarm.alarm_state == "going") {
            const int notes[] = {659, 622, 659, 622, 659, 494, 587, 523, 440};
            const int durations[] = {250, 250, 250, 250, 250, 250, 250, 250, 500};
            for (int i = 0; i < sizeof(notes) / sizeof(notes[0]); i++) {        //Soundloop
                if(alarm.alarm_state != "going") break;
                    buzzer.period(1.0 / notes[i]); 
                    buzzer = 0.5; 
            ThisThread::sleep_for(durations[i]);   
            }      
        }
        else {
            buzzer = 0.0; 
            ThisThread::sleep_for(100ms);   
        }
    }
}
