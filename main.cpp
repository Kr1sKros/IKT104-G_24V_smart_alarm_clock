#include "mbed.h"
#include "DFRobot_RGBLCD1602.h"
#include <ctime>

// Define the pins for I2C communication
I2C lcdI2C(D14, D15); // SDA, SCL

// Create an LCD object
DFRobot_RGBLCD1602 lcd(&lcdI2C);  // Create an LCD display object

// Define the button pin
DigitalIn button(D7, PullUp);

// Function prototypes
void showThing1();
void showThing2();
void showThing3();
void showThing4();
void showThing5();
void showThing6();

// Array of function pointers
void (*showFunctions[])() = {showThing1, showThing2, showThing3, showThing4, showThing5, showThing6};

void showUnix () {
    lcd.clear();
    lcd.printf("Unix epoch time: ");
    lcd.setCursor(0, 1);
    lcd.printf("1234567890");
    ThisThread::sleep_for(2s);
}

void showLatLon () {
    lcd.clear();
    lcd.printf("Lat: 58.3405");
    lcd.setCursor(0, 1);
    lcd.printf("Lon: 8.5934");
    ThisThread::sleep_for(2s);
}

void showCity () {
    lcd.clear();
    lcd.printf("City: ");
    lcd.setCursor(0, 1);
    lcd.printf("Grimstad");
    ThisThread::sleep_for(2s);
}

// Function implementations
void showThing1() {
    lcd.clear();
    lcd.printf("Sun 28 Apr 13:55");
}

void showThing2() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printf("Alarm: 07:30");
}

void showThing3() {
    lcd.clear();
    lcd.printf("Temp: 21.4");
    lcd.setCursor(0, 1);
    lcd.printf("Hum: 69%");
}

void showThing4() {
    lcd.clear();
    lcd.printf("Broken clouds");
    lcd.setCursor(0, 1);
    lcd.printf("7 degrees");
}

void showThing5() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printf("Enter city: ");
}

void showThing6() {
    lcd.clear();
    const char text[] = "Florida man challenged an alligator to a poker game, ended up in hospital - the alligator of course.";
    int text_len = strlen(text);
    int scroll_len = 16; // Number of characters displayed at once
    
    // Display the initial portion of the text
    lcd.setCursor(0, 0);
    lcd.printf("Top News:");
    lcd.setCursor(0, 1);
    lcd.printf("%.16s", text);

    // Delay before starting scrolling
    ThisThread::sleep_for(500ms);

    // Start scrolling
    for (int i = 0; i <= text_len - scroll_len; ++i) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("Top News:");
        lcd.setCursor(0, 1);
        lcd.printf("%.16s", &text[i]); // Display the next portion of the text
        ThisThread::sleep_for(250ms); // Adjust the delay as needed
    }
}


int main() {
    // Initialize the LCD
    lcd.init(); // Initialize the LCD
    lcd.setRGB(255, 255, 255); // Set LCD backlight color to white
    
    showUnix();
    showLatLon();
    showCity();

    int state = 0; // Initial state
    
    // Call the initial function
    showFunctions[state]();
    
    while(1) {
        // Check if the button is pressed
        if(!button) {
            // Button is pressed, change the state and call the corresponding function
            state++;
            if(state >= 6) {
                state = 0;
            }
            showFunctions[state]();
            while(!button) {} // Wait for button release
        }
        ThisThread::sleep_for(100ms); // Add a small delay to debounce the button
    }
}
