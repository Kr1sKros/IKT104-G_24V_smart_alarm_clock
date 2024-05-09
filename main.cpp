#include "ThisThread.h"
#include "mbed.h"
#include "DFRobot_RGBLCD1602.h"
#include <ctime>
#include <string>

#include "newsfeed.h"
#include "graphics.h"

// Define the pins for I2C communication
I2C lcdI2C(D14, D15); // SDA, SCL

// Create an LCD object
DFRobot_RGBLCD1602 lcd(&lcdI2C);  // Create an LCD display object

// Define the button pin
DigitalIn button(D7, PullUp);

// Function prototypes

PageController page_controller;

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
        lcd.clear();
        lcd.printf("this is a another test");
        lcd.setCursor(0, 1);
        lcd.printf("%i", page_controller.get_current_page());
    }
};
class NewsPage : public Page {
    void display() override {
        lcd.clear();
        lcd.printf("Top news");
        lcd.setCursor(0, 1);
        lcd.printf("fetching...");
        
        std::string news_header = fetchNewsHeader();
        std::cout << news_header << endl;
        lcd.clear();
        lcd.printf("Top news");
        lcd.setCursor(0, 1);
        lcd.printf("%16s", news_header.c_str());
    }
};

int main() {

    //TestPage tp;
    //AnotherPage ap;
    page_controller.add_page(static_cast<Page*>(new TestPage));
    page_controller.add_page(static_cast<Page*>(new AnotherPage));
    page_controller.add_page(static_cast<Page*>(new NewsPage));

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
