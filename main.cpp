#include "mbed.h"
#include "DFRobot_RGBLCD1602.h"
#include <ctime>
#include <string>

// Define the pins for I2C communication
I2C lcdI2C(D14, D15); // SDA, SCL

// Create an LCD object
DFRobot_RGBLCD1602 lcd(&lcdI2C);  // Create an LCD display object

// Define the button pin
DigitalIn button(D7, PullUp);

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
    string text = fetchNewsHeader();
    int text_len = text.length();
    int scroll_len = 16; // Number of characters displayed at once
    
    // Display the initial portion of the text
    lcd.setCursor(0, 0);
    lcd.printf("Top News:");
    lcd.setCursor(0, 1);
    lcd.printf("%16s", &text);

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
