/*!
 * @file network_utilities.h
 * @brief A utility header file for all network related functionality.
 * @author Anders L.
 */

#ifndef NETWORK_UTILITIES_H
#define NETWORK_UTILITIES_H

#include "mbed.h"
#include "nsapi_types.h"
#include <cstdio>

#include <iostream>
#include <string>

class net_util {
public:
    NetworkInterface *network = nullptr;
    TCPSocket socket;

    net_util(){
        if (this->network != nullptr){
            this->network->disconnect();
            this->network = nullptr;

            printf("network is not nullptr");
        }

        do {
            printf("Get pointer to default network interface...\n");
            this->network = NetworkInterface::get_default_instance();

            if (!this->network) {
                printf("Failed to get default network interface\n");
            }

            ThisThread::sleep_for(1000ms);
        } while (this->network == nullptr);

        nsapi_size_or_error_t result;

        do {
            printf("Connecting to the network...\n");
            result = network->connect();

            if (result != NSAPI_ERROR_OK) {
                printf("Failed to connect to network: %d\n", result);
            }
        } while (result != NSAPI_ERROR_OK);

        cout << "succesfully connected to default network\n";
        /*
        do {
        printf("Get local IP address...\n");
        result = network->get_ip_address(&this->local_ip);

        if (result != NSAPI_ERROR_OK) {
            printf("Failed to get local IP address: %d\n", result);
        }
        } while (result != NSAPI_ERROR_OK);

        printf("Connected to WLAN and got IP address %s\n", this->local_ip.get_ip_address());
        */
    }

    std::string send_get_request(const char* hostname, const char* resource){
        const char http_request_template[] = "GET %s HTTP/1.1\r\n"
                                            "Host: %s\r\n"
                                            "Connection: close\r\n"
                                            "\r\n";

        char http_request[1024]; // Assuming maximum length of HTTP request

        std::snprintf(http_request, sizeof(http_request), http_request_template, resource, hostname);
        
        this->socket.close();
        this->socket.set_timeout(500);

        nsapi_size_or_error_t result = this->socket.open(this->network);

        if (result != NSAPI_ERROR_OK) {
            printf("Failed to open TCPSocket: %d\n", result);
            return "";
        }

        SocketAddress server_address;

        do {
            result = this->network->gethostbyname(hostname, &server_address);
            if (result != NSAPI_ERROR_OK) {
                printf("Failed to get IP address of host %s: %d\n", hostname, result);
            }
            // return "";
        } while(result != NSAPI_ERROR_OK);

        

        server_address.set_port(80);

        result = this->socket.connect(server_address);

        if (result != NSAPI_ERROR_OK) {
            printf("Failed to connect to server at %s: %d\n", hostname, result);
            return "";
        }

        nsapi_size_t bytes_to_send = strlen(http_request);
        nsapi_size_or_error_t sent_bytes = 0;

        printf("\nSending message: \n%s", http_request);

        // Loop as long as there are more data to send
        while (bytes_to_send) {
            // Try to send the remaining data.
            // send() returns how many bytes were actually sent
            sent_bytes = socket.send(http_request, bytes_to_send);

            if (sent_bytes < 0) {
                // Negative return values from send() are errors
                printf("Failed to send HTTP request: %d\n", sent_bytes);
                return "";
            } 

            bytes_to_send -= sent_bytes;
        }

        std::cout << "Finished sending message. moving over to other matters 😈\n";

        static constexpr size_t HTTP_RESPONSE_BUF_SIZE = 4000;

        static char http_response[HTTP_RESPONSE_BUF_SIZE + 1];

        // Nullify response buffer
        memset(http_response, 0, sizeof(http_response));

        nsapi_size_t remaining_bytes = HTTP_RESPONSE_BUF_SIZE;
        nsapi_size_or_error_t received_bytes = 0;

        while (remaining_bytes > 0) {
            nsapi_size_or_error_t result =
                socket.recv(http_response + received_bytes, remaining_bytes);

            if (result < 0) {
                if (result != NSAPI_ERROR_WOULD_BLOCK) {
                    received_bytes = result;
                    break;
                } else {
                    ThisThread::sleep_for(100ms);
                    continue;
                }
            } else if (result == 0) {
                break;
            }
            received_bytes += result;
            remaining_bytes -= result;
        }

        if (received_bytes < 0) {
            printf("Failed to read the HTTP response: %d\n", received_bytes);
            return "";
        }
        
        http_response[received_bytes] = '\0';


        cout << "message received \n";

        std::string response_string(http_response);
        
        return response_string;
    }

    
};




#endif // NETWORK_UTILITIES_H