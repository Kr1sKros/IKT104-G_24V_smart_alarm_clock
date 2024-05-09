#include "mbed.h"
#include "HTS221Sensor.h"
#include "DFRobot_RGBLCD1602.h"
#include "DevI2C.h"
#include <ctime>
#include <string>


// Define the pins for separate I2C communication
DevI2C devI2cSensor(PB_11, PB_10); // Sensor I2C bus
DevI2C devI2cDisplay(D14, D15);    // Display I2C bus

// Create sensor and LCD objects
HTS221Sensor hts(&devI2cSensor);
DFRobot_RGBLCD1602 lcd(&devI2cDisplay);

// Define the button pin
InterruptIn button(D7, PullUp);

// Volatile variables used in the interrupt context
volatile bool buttonPressed = false;
volatile uint64_t lastInterruptTime = 0;

// Button interrupt handler
void buttonPressHandler() {
    uint64_t interruptTime = Kernel::get_ms_count();
    if (interruptTime - lastInterruptTime > 200) { // 200 ms debounce threshold
        buttonPressed = true;
    }
    lastInterruptTime = interruptTime;
}


std::string extractTitle(const char* xml) {
    const char* startTag = "<title><![CDATA[";
    const char* endTag = "]]></title>";

    // Find the position of the first occurrence of the start tag
    const char* startPos = strstr(xml, startTag);
    if (!startPos) {
        return ""; // Title start tag not found
    }

    // Find the position of the end tag
    const char* endPos = strstr(startPos, endTag);
    if (!endPos) {
        return ""; // Title end tag not found
    }

    // Find the position of the second occurrence of the start tag
    startPos = strstr(endPos, startTag);
    if (!startPos) {
        return ""; // Second title start tag not found
    }
    startPos += strlen(startTag); // Move startPos to the beginning of the title text

    // Find the position of the end tag
    endPos = strstr(startPos, endTag);
    if (!endPos) {
        return ""; // Title end tag not found
    }

    // Extract the title text between startPos and endPos
    return std::string(startPos, endPos - startPos);
}
std::string fetchNewsHeader() {

    lcd.printf("Top News: ");
    lcd.setCursor(0, 1);
    lcd.printf("Loading... ");
    // Get pointer to default network interface
    NetworkInterface *network = nullptr;

    do {
        printf("Get pointer to default network interface...\n");
        network = NetworkInterface::get_default_instance();

        if (!network) {
            printf("Failed to get default network interface\n");
        }

        ThisThread::sleep_for(1000ms);
    } while (network == nullptr);

    nsapi_size_or_error_t result;

    do {
        printf("Connecting to the network...\n");
        result = network->connect();

        if (result != NSAPI_ERROR_OK) {
            printf("Failed to connect to network: %d\n", result);
        }
    } while (result != NSAPI_ERROR_OK);

    SocketAddress address;

    do {
        printf("Get local IP address...\n");
        result = network->get_ip_address(&address);

        if (result != NSAPI_ERROR_OK) {
            printf("Failed to get local IP address: %d\n", result);
        }
    } while (result != NSAPI_ERROR_OK);

    printf("Connected to WLAN and got IP address %s\n", address.get_ip_address());

    TCPSocket socket;

    // Configure timeout on socket receive
    // (returns NSAPI_ERROR_WOULD_BLOCK on timeout)
    socket.set_timeout(500);

    result = socket.open(network);

    if (result != NSAPI_ERROR_OK) {
        printf("Failed to open TCPSocket: %d\n", result);
        return "";
    }

    const char host[] = "rss.cnn.com";
    result = network->gethostbyname(host, &address);

    if (result != NSAPI_ERROR_OK) {
        printf("Failed to get IP address of host %s: %d\n", host, result);
        return "";
    }

    printf("IP address of server %s is %s\n", host, address.get_ip_address());

    // Set server TCP port number
    address.set_port(80);

    // Connect to server at the given address
    result = socket.connect(address);

    // Check result
    if (result != NSAPI_ERROR_OK) {
        printf("Failed to connect to server at %s: %d\n", host, result);
        return "";
    }

    printf("Successfully connected to server %s\n", host);

    // Create HTTP request
    const char http_request[] = "GET /rss/edition.rss HTTP/1.1\r\n"
                                "Host: rss.cnn.com\r\n"
                                "Connection: close\r\n"
                                "\r\n";

    // The request might not be fully sent in one go,
    // so keep track of how much we have sent
    nsapi_size_t bytes_to_send = strlen(http_request);
    nsapi_size_or_error_t sent_bytes = 0;

    printf("\nSending message: \n%s", http_request);

    // Loop as long as there are more data to send
    while (bytes_to_send) {
        // Try to send the remaining data.
        // send() returns how many bytes were actually sent
        sent_bytes = socket.send(http_request + sent_bytes, bytes_to_send);

        if (sent_bytes < 0) {
            // Negative return values from send() are errors
            printf("Failed to send HTTP request: %d\n", sent_bytes);
            return "";
        } else {
            printf("Sent %d bytes\n", sent_bytes);
        }

        bytes_to_send -= sent_bytes;
    }

    printf("Complete message sent\n");

    // Define expected maximum size of HTTP response
    static constexpr size_t HTTP_RESPONSE_BUF_SIZE = 4000;

    // The response needs to be stored in memory. The memory object is called a
    // buffer. If you make this buffer static it will be placed in bss (memory
    // for global and static variables) and won't use the main thread stack
    static char http_response[HTTP_RESPONSE_BUF_SIZE + 1]; // Plus 1 for '\0'

    // Nullify response buffer
    memset(http_response, 0, sizeof(http_response));

    nsapi_size_t remaining_bytes = HTTP_RESPONSE_BUF_SIZE;
    nsapi_size_or_error_t received_bytes = 0;

    // Loop as long as there are more data to read,
    // we might not read all in one call to recv()
    while (remaining_bytes > 0) {
        nsapi_size_or_error_t result =
            socket.recv(http_response + received_bytes, remaining_bytes);

        // Negative return values from recv() are errors
        if (result < 0) {
            if (result != NSAPI_ERROR_WOULD_BLOCK) {
                // Error other than timeout
                received_bytes = result;
                break;
            } else {
                // Timeout, wait and try again
                ThisThread::sleep_for(100ms);
                continue;
            }
        } else if (result == 0) {
            // Connection closed
            break;
        }

        printf("Received %d bytes\n", result);

        received_bytes += result;
        remaining_bytes -= result;
    }

    if (received_bytes < 0) {
        printf("Failed to read the HTTP response: %d\n", received_bytes);
        return "";
    }

    printf("\nReceived %d bytes with HTTP status code: %.*s\n", received_bytes,
           strstr(http_response, "\n") - http_response, http_response);

    // Take a look at the complete response, but before doing
    // so make sure we have a null-terminated string
    http_response[received_bytes] = '\0';
    std::string title = extractTitle(http_response);

    socket.close();
    network->disconnect();

    return title;
}

void showThing1();
void showThing2();
void showThing3(float temperature, float humidity);  // Updated to include parameters
void showThing4();
void showThing5();
void showThing6();


// Array of function pointers for different displays
// This array can only include functions with the same type of parameters.
void (*showFunctions[])(void) = {showThing1, showThing2, showThing4, showThing5, showThing6};

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

void showThing1() {
    lcd.clear();
    lcd.printf("Sun 28 Apr 13:55");
}

void showThing2() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printf("Alarm: 07:30");
}

void showThing3(float temperature, float humidity) {
    lcd.clear();

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

    ThisThread::sleep_for(2s); // Maintain display for readability
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

void showThing6(const std::string& text) {
    int text_len = text.length();
    int scroll_len = 16; // Number of characters displayed at once
    
    // Display the initial portion of the text
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printf("Top News:");
    lcd.setCursor(0, 1);
    lcd.printf("%16s", text.c_str());

    // Delay before starting scrolling
    ThisThread::sleep_for(500ms);

    // Start scrolling
    for (int i = 0; i <= text_len - scroll_len; ++i) {
        lcd.setCursor(0, 1);
        lcd.printf("%.16s", text.c_str() + i); // Display the next portion of the text
        ThisThread::sleep_for(250ms); // Adjust the delay as needed
    }
}


int main() {
    // Initialize sensor
    hts.init(nullptr);
    hts.enable();

    // Initialize LCD
    lcd.init();
    lcd.setRGB(255, 255, 255); // Set backlight to white for testing

    // Attach the interrupt handler to the button press
    button.fall(callback(buttonPressHandler));

    showUnix();
    showLatLon();
    showCity();

    std::string newsHeader; // Variable to hold fetched news

    // Main loop variables
    int state = 0;
    float temperature, humidity;

    while (1) {
        // Read sensor values continuously
        hts.get_temperature(&temperature);
        hts.get_humidity(&humidity);

        // Fetch news header
        newsHeader = fetchNewsHeader();  // Fetch the news header

        // Check if the button has been pressed
        if (buttonPressed) {
            state = (state + 1) % 6; // Update the state based on button press
            buttonPressed = false;   // Reset the button pressed flag
        }

        // Display data based on the current state
        switch (state) {
            case 0:
                showThing1();
                break;
            case 1:
                showThing2();
                break;
            case 2:
                showThing3(temperature, humidity);
                break;
            case 3:
                showThing4();
                break;
            case 4:
                showThing5();
                break;
            case 5:
                showThing6(newsHeader); // Pass the fetched news to showThing6
                break;
            default:
                break;
        }

        ThisThread::sleep_for(100ms); // Add a small delay to reduce CPU load
    }
}
