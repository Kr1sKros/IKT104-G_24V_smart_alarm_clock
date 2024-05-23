#include "ThisThread.h"
#include "mbed.h"
#include "DFRobot_RGBLCD1602.h"
#include "HTS221Sensor.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
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
DFRobot_RGBLCD1602 lcd(&lcdI2C);

// Define the buttons in use
DigitalIn buttonPage(A0, PullUp);
InterruptIn buttonAdd(A1, PullUp);
InterruptIn buttonSubtract(A2, PullUp);
InterruptIn buttonEnter(A3, PullUp);

// Define buzzer
PwmOut buzzer(D13); //D13 eller PA_5

// Define Page controller object from graphics.h This object controls which page is displayed and page switching
PageController page_controller;
net_util nu;

// Global variables for weather API
float latitude;
float longitude;
char city_name[17] = {0}; // Max 16 characters + null terminator
bool checkCity = false;



//Define the alarm and alarm struct
struct alarm_t {
    int hours;
    int minutes;
    std::string alarm_state; //active, going, snooze, mute, deactive
};
alarm_t alarm;
alarm_t soundUntil; //Makes it so that the alarm plays for 10 minutes

//Define threads used by alarm
Thread check_alarm;
Thread play_melody;

//Define global variables
int current_time_setter = 0;
int set_hours = 0;
int set_minutes = 0;
int DefaultDisplaynumber;
bool input_happened = false;
bool unmute = false;
bool snoozeIsHit = false;

// Define Serial object for PC communication
BufferedSerial pc(USBTX, USBRX, 115200);

// Functions
void alarmOff();
void alarm_melody();
void AddTime();
void SubtractTime();
void EnterValue();

// Page definitions. Each page inherits from Page class that runs in a separate thread with the help of the page controller.
// Boot page definition. This page runs before the 'main program'
class Boot : public Page {
public:
    void display() override {
        lcd.clear();
        lcd.printf("Starting up...");

        nlohmann::json jsonData = nu.send_https_request("api.ipgeolocation.io", "/ipgeo?apiKey=6f5e9eb73e404705bf04d4c161c239e2", geolocation_cert);

        time_t currentEpochTime = jsonData["time_zone"]["current_time_unix"];
        std::string latStr = jsonData["latitude"].get<std::string>();
        std::string lonStr = jsonData["longitude"].get<std::string>();
        std::string cityStr = jsonData["city"].get<std::string>();
        std::string dateTimeStr = jsonData["time_zone"]["current_time"].get<std::string>();

        int fixedEpochTime = jsonData["time_zone"]["offset_with_dst"].get<int>();
        fixedEpochTime *= 3600;
        currentEpochTime += fixedEpochTime;
        set_time(currentEpochTime);
        char tempBuffer[20];
        snprintf(tempBuffer, sizeof(tempBuffer), "%ld", static_cast<long>(currentEpochTime));

        const char* currentEpochTimeStr = tempBuffer;
        latitude = std::stof(latStr);
        longitude = std::stof(lonStr);
        const char* city = cityStr.c_str();

        // Shows unix epoch time on boot
        lcd.clear();
        lcd.printf("Unix epoch time: ");
        lcd.setCursor(0, 1);
        lcd.printf(currentEpochTimeStr);
        ThisThread::sleep_for(2s);

        // Shows coordinates on boot
        lcd.clear();
        lcd.printf("Lat: %f", latitude);
        lcd.setCursor(0, 1);
        lcd.printf("Lon: %f", longitude);
        ThisThread::sleep_for(2s);

        // Shows nearest city on boot
        lcd.clear();
        lcd.printf("City: ");
        lcd.setCursor(0, 1);
        lcd.printf(city);
        ThisThread::sleep_for(2s);
        
        page_controller.display(0);
    }
};

// Page that shows weekday, date, month and time definition
class dateTime : public Page {
public:
    void display() override { //Enable(A1), Mute(A2), Disable(A3)
        DefaultDisplaynumber = page_controller.get_current_page();  
        bool entered_page = true;
        while(1) {
            lcd.clear();
            time_t seconds = time(NULL);
            char currentTime[32];
            std::strftime(currentTime, sizeof(currentTime), "%a %d %B %H:%M", localtime(&seconds));
            lcd.printf(currentTime);
            lcd.setCursor(0, 1);
            if(strcmp(alarm.alarm_state.c_str(), "going") == 0) {
                lcd.printf("Alarm (A) %i:%i", alarm.hours, alarm.minutes);
            }
            else if(strcmp(alarm.alarm_state.c_str(), "active") == 0) {
                lcd.printf("Alarm %i:%i", alarm.hours, alarm.minutes);
            }
            else if(strcmp(alarm.alarm_state.c_str(), "mute") == 0) {
                lcd.printf("Alarm (M) %i:%i", alarm.hours, alarm.minutes);
            }
            else if(strcmp(alarm.alarm_state.c_str(), "snooze") == 0) {
                lcd.printf("Alarm (S) %i:%i", alarm.hours, alarm.minutes);
            }
            if(input_happened || entered_page) {
                entered_page = false;
                input_happened = false;
            }
            else {
                ThisThread::sleep_for(1s); // Update the display every 5 seconds
            }
        }
    }
};
// Page that allows user to set alarm definition
class SetAlarm : public Page {
public:
    void display() override {  
        set_hours = 0;
        set_minutes = 0;
        bool entered_page = true;
        while(1) {
            if(input_happened || entered_page) {
                entered_page = false;
                input_happened = false;
                lcd.clear();
                if(current_time_setter == 0) {
                    lcd.printf("Sett Alarm %i:%i", set_hours%24,set_minutes%60);
                    lcd.setCursor(0,1);
                    lcd.printf("Enter hour");
                }
                else if(current_time_setter == 1) {
                    lcd.printf("Sett Alarm %i:%i", set_hours%24,set_minutes%60);
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


// Definition for the page displaying temperature and humidity
class TempHum : public Page {
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

// Include the new SearchCity class here
class SearchCity : public Page {
public:
    void display() override {
        lcd.clear();
        lcd.setCursor(0, 0);  // Set cursor to the first row
        lcd.printf("A1: Lat/Lon");
        lcd.setCursor(0, 1);
        lcd.printf("A2: City Name");

        while (1) {
            if (!buttonAdd) {
                lcd.clear();
                enterCoordinates();
                break;
            }
            if (!buttonSubtract) {
                lcd.clear();
                enterCityName();
                break;
            }

            ThisThread::sleep_for(100ms); // Add a small delay for debouncing
        }
    }

private:
    void enterCoordinates() {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("Enter Latitude:");

        char lat_str[11] = {0}; // Max 10 characters + null terminator
        int lat_index = 0;

        while (1) {
            if (pc.readable()) {
                char c;
                pc.read(&c, 1); // Read a character from the serial input
                if (c == '\r' || c == '\n') {
                    // Finish typing latitude on Enter key press
                    printf("\nLatitude entered: %.6f\n", latitude);

                    // Now ask for Longitude
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.printf("Enter Longitude:");
                    char lon_str[11] = {0}; // Max 10 characters + null terminator
                    int lon_index = 0;

                    while (1) {
                        if (pc.readable()) {
                            char c;
                            pc.read(&c, 1); // Read a character from the serial input
                            if (c == '\r' || c == '\n') {
                                // Finish typing longitude on Enter key press
                                lat_str[lat_index] = '\0'; // Null terminate the string
                                lon_str[lon_index] = '\0'; // Null terminate the string
                                latitude = atof(lat_str); // Convert to float
                                longitude = atof(lon_str); // Convert to float
                                printf("\nLongitude entered: %.6f\n", longitude);

                                checkCity = false;

                                // Display entered Latitude and Longitude
                                lcd.clear();
                                lcd.setCursor(0, 0);
                                lcd.printf("Lat: %.6f", latitude);
                                lcd.setCursor(0, 1);
                                lcd.printf("Lon: %.6f", longitude);
                                printf("Displaying Lat: %.6f, Lon: %.6f\n", latitude, longitude);
                                ThisThread::sleep_for(5s); // Display the coordinates for 5 seconds
                                return; // Exit the method
                            } else if (c == '\b' || c == 127) {
                                // Handle backspace
                                if (lon_index > 0) {
                                    lon_index--;
                                    lon_str[lon_index] = ' ';
                                }
                            } else if (lon_index < 10 && ((c >= '0' && c <= '9') || c == '.')) {
                                // Add character to longitude string if it's a digit or a dot
                                lon_str[lon_index++] = c;
                            }
                            // Echo the input to the display and console
                            lcd.setCursor(0, 1);
                            lcd.printf("%-10s", lon_str); // Clear the rest of the line
                            printf("\rLongitude: %-10s", lon_str);
                        }

                        ThisThread::sleep_for(100ms); // Add a small delay for debouncing
                    }
                } else if (c == '\b' || c == 127) {
                    // Handle backspace
                    if (lat_index > 0) {
                        lat_index--;
                        lat_str[lat_index] = ' ';
                    }
                } else if (lat_index < 10 && ((c >= '0' && c <= '9') || c == '.')) {
                    // Add character to latitude string if it's a digit or a dot
                    lat_str[lat_index++] = c;
                }
                // Echo the input to the display and console
                lcd.setCursor(0, 1);
                lcd.printf("%-10s", lat_str); // Clear the rest of the line
                printf("\rLatitude: %-10s", lat_str);
            }

            ThisThread::sleep_for(100ms); // Add a small delay for debouncing
        }
    }

    void enterCityName() {
        lcd.clear();
        lcd.setCursor(0, 0);  // Set cursor to the first row
        lcd.printf("Enter City name:");

        int city_index = 0;

        while (1) {
            if (pc.readable()) {
                char c;
                pc.read(&c, 1); // Read a character from the serial input
                if (c == '\r' || c == '\n') {
                    // Finish typing on Enter key press
                    city_name[city_index] = '\0'; // Null terminate the string
                    displayCityName(city_name);
                    break;
                } else if (c == '\b' || c == 127) {
                    // Handle backspace
                    if (city_index > 0) {
                        city_index--;
                        city_name[city_index] = ' ';
                    }
                } else if (city_index < 16 && c >= 32 && c <= 126) {
                    // Add character to city name if within printable ASCII range
                    city_name[city_index++] = c;
                }
                // Echo the input to the display and console
                lcd.setCursor(0, 1);
                lcd.printf("%-16s", city_name); // Clear the rest of the line
                printf("\rCity Name: %-16s", city_name);
            }

            ThisThread::sleep_for(100ms); // Add a small delay for debouncing
        }
    }

    void displayCityName(const char* city_name) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("City entered:");
        lcd.setCursor(0, 1);
        lcd.printf("%s", city_name);
        printf("\nDisplaying city name: %s\n", city_name); // Debugging output
        checkCity = true;
        ThisThread::sleep_for(5s); // Display the city name for 5 seconds
    }
};



class Weather : public Page {
public:
    void display() override {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("Fetching weather");
        lcd.setCursor(0, 1);
        lcd.printf("data...");

        char weatherRequest[256];

        if (!checkCity)
        {
            sprintf(weatherRequest, "/data/2.5/weather?lat=%f&lon=%f&appid=79c12506d304ebe5ae95df675b419522", latitude, longitude);
        }
        else 
        {
            sprintf(weatherRequest, "/data/2.5/weather?q=%s&appid=79c12506d304ebe5ae95df675b419522", city_name);
        }

        nlohmann::json jsonData = nu.send_https_request("api.openweathermap.org", weatherRequest, weather_cert);
        std::string weatherStr = jsonData["weather"][0]["main"].get<std::string>();
        const char *weather = weatherStr.c_str();
        int temperature = jsonData["main"]["temp"].get<int>() - 273;
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf(weather);
        lcd.setCursor(0, 1);
        lcd.printf("%d degrees", temperature);
    }
};



// Definition for page showing the top three news articles from CNN rss feed, http://rss.cnn.com/rss/cnn_latest.rss
class NewsPage : public Page {
public:
    void display() override {
        lcd.clear();
        lcd.setCursor(0, 0);  // Set cursor to the first row
        lcd.printf("Top News CNN:");
        lcd.setCursor(0, 1);  // Set cursor to the second row
        lcd.printf("fetching...");

        std::string news_header = text_utils::extractTitle(nu.send_get_request("rss.cnn.com", "/rss/cnn_latest.rss").c_str());

        // Scroll the news headlines on the second row
        text_utils::printScrolling(&lcd, news_header, false, 300, 1);
        this->display();
    }
};


// program main
int main() {
    // Add all pages to page controller. The pages will be displayed in the order that they are added.
    page_controller.add_page(static_cast<Page*>(new dateTime));
    page_controller.add_page(static_cast<Page*>(new SetAlarm));
    page_controller.add_page(static_cast<Page*>(new TempHum));
    page_controller.add_page(static_cast<Page*>(new SearchCity)); 
    page_controller.add_page(static_cast<Page*>(new Weather)); 
    page_controller.add_page(static_cast<Page*>(new NewsPage));

    // initialize hts sensor
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

    static_cast<Page*>(new Boot)->display();

    // Program mainloop
    while(1) {
        // Check if the button is pressed
        if(!buttonPage) {
            // Button is pressed, change the state and call the corresponding function
            page_controller.display_next();

            while (!buttonPage) {};
        }
        ThisThread::sleep_for(100ms); // Add a small delay to debounce the button
    }
}

//Functions
void AddTime() 
{
    input_happened = true;
    if(page_controller.get_current_page() == DefaultDisplaynumber+1) {
        if(current_time_setter == 0) set_hours++;
        else if (current_time_setter == 1) set_minutes++;
    }
    else if(page_controller.get_current_page() == DefaultDisplaynumber) {
        if(strcmp(alarm.alarm_state.c_str(), "going") == 0) {
            alarm.alarm_state = "snooze";
            snoozeIsHit = true;
        }
        else if(strcmp(alarm.alarm_state.c_str(), "deactive") == 0) 
            alarm.alarm_state = "active";
    }
}
void SubtractTime() 
{
    input_happened = true;
    if(page_controller.get_current_page() == DefaultDisplaynumber+1) {
        if(current_time_setter == 0) {
            if(set_hours-1 < 0) set_hours=23;
            else set_hours--;
        }
        else if (current_time_setter == 1) {
            if(set_minutes-1 < 0) set_minutes=59;
            else set_minutes--;
        }
    }
    else if(page_controller.get_current_page() == DefaultDisplaynumber) {
        if(strcmp(alarm.alarm_state.c_str(), "going") != 0)
            alarm.alarm_state = "mute";
    }
}
void EnterValue() 
{
    input_happened = true;
    if(page_controller.get_current_page() == DefaultDisplaynumber+1) {
        current_time_setter++;
        if (current_time_setter == 2) {
            alarm.minutes = set_minutes % 60;
            alarm.hours = set_hours % 24;
            alarm.alarm_state = "active";
        }
    }
    else if(page_controller.get_current_page() == DefaultDisplaynumber) {
        alarm.alarm_state = "deactive";
    }
}

void alarmOff() {   //Checks time in separate thread
    while (true) {  //gets current hour and minute
        time_t seconds = time(NULL);
        char buffer[32];
        strftime(buffer, 32, "%H", localtime(&seconds));
        int current_rtc_hour = std::stoi(buffer);
        strftime(buffer, 32, "%M", localtime(&seconds));
        int current_rtc_minute = std::stoi(buffer);

        if(strcmp(alarm.alarm_state.c_str(), "going") == 0) {  //Check if alarm has gone off for 10 min
            if(current_rtc_hour == soundUntil.hours && current_rtc_minute == soundUntil.minutes) {
                alarm.alarm_state = "active";
            }
        }     
        else if(strcmp(alarm.alarm_state.c_str(), "active") == 0) {        //Check if the alarm will go off
            if(current_rtc_hour == alarm.hours && current_rtc_minute == alarm.minutes) {     
                soundUntil.hours = alarm.hours; //Sets up the 10 min timer the sound will play to
                soundUntil.minutes = alarm.minutes + 10;
                if(soundUntil.minutes > 59) {
                    soundUntil.hours++;
                    soundUntil.minutes -= 60;
                }
                alarm.alarm_state = "going";
                page_controller.display(DefaultDisplaynumber);
            }
        }
        else if (strcmp(alarm.alarm_state.c_str(), "snooze") == 0) {   //Sets alarm to go off in 5 min
            if(snoozeIsHit == true) {   //Ensures this code runs only once
                snoozeIsHit = false; 
                soundUntil.hours = current_rtc_hour;
                soundUntil.minutes = current_rtc_minute + 5;
                if(current_rtc_minute > 59) {
                    current_rtc_hour++;
                    current_rtc_minute -= 60;
                }
            }   
            if(current_rtc_hour == soundUntil.hours && current_rtc_minute == soundUntil.minutes) { //Checks if 5 minutes has passed
                soundUntil.hours = current_rtc_hour;
                soundUntil.minutes = current_rtc_minute + 10;
                if(current_rtc_minute > 59) {
                    current_rtc_hour++;
                    current_rtc_minute -= 60;
                }
                alarm.alarm_state = "going";
                page_controller.display(DefaultDisplaynumber);
            }
        }
        else if(strcmp(alarm.alarm_state.c_str(), "mute") == 0) {
            if(current_rtc_hour == alarm.hours && current_rtc_minute == alarm.minutes) {     //Check if the alarm will go off
                unmute = true;
            }
            if(unmute) { //Waits for the minute to pass before activating the alarm
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

void alarm_melody() {        //checks if alarm in state 'going' play melody , runs in separate thread
    while(1) {
        if(strcmp(alarm.alarm_state.c_str(), "going") == 0) {
            const int notes[] =     {440, 494, 587, 494, 740, 1, 740, 660,  440, 494, 587, 494, 659, 1, 659, 587, 554, 494};
            const int durations[] = {250, 250, 250, 250, 500, 1, 500, 1000, 250, 250, 250, 250, 500, 1, 500, 500, 250, 500};
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
