/*!
 * @file newsfeed.h
 * @brief A header file with a function capable of retrieving latest news from cnn using their public rss feed.
 * @author Anders L.
 */


#ifndef NEWSFEED_H
#define NEWSFEED_H

#include <string>
#include "mbed.h"

std::string fetchNewsHeader() {
    // Get pointer to default network interface
    NetworkInterface *network = nullptr;

    
    // Get pointer to default network interface
    // NetworkInterface *network = nullptr;

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
    const char http_request[] = "GET /rss/cnn_latest.rss HTTP/1.1\r\n"
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
    // std::string title = extractTitle(http_response);

    const char* startTag = "<title><![CDATA[";
    const char* endTag = "]]></title>";

    // Find the position of the first occurrence of the start tag
    const char* startPos = strstr(http_response, startTag);
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

    socket.close();
    network->disconnect();

    return std::string(startPos, endPos - startPos);
}

#endif // NEWSFEED_H