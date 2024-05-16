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
DigitalIn button(D7, PullUp);

// Function prototypes

PageController page_controller;
net_util nu;

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

class TestPage : public Page {
public:
    void display() override {
        lcd.clear();
        lcd.printf("We are very much testing");
        lcd.setCursor(0, 1);
        lcd.printf("%i", page_controller.get_current_page());
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
    page_controller.add_page(static_cast<Page*>(new TestPage));
    page_controller.add_page(static_cast<Page*>(new AnotherPage));
    page_controller.add_page(static_cast<Page*>(new NewsPage));

    hts.init(nullptr);
    hts.enable();

    // Initialize the LCD
    lcd.init(); // Initialize the LCD
    lcd.clear();
    lcd.setRGB(255, 255, 255); // Set LCD backlight color to white
    
    // page_controller.display(0);
    
    static_cast<Page*>(new Boot)->display();

    while(1) {
        // Check if the button is pressed
        if(!button) {
            // Button is pressed, change the state and call the corresponding function
            page_controller.display_next();

            while (!button) {};
        }
        ThisThread::sleep_for(100ms); // Add a small delay to debounce the button
    }
}
