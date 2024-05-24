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
#include <json.hpp>

// this is the master class that contains all the functions for making different requests
class net_util {
public:
    NetworkInterface *network = nullptr;
    TCPSocket socket;

    // connects to network inside the constructor. Pretty neat.
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
    }

    // standardized method for sending get requests using unsecured http. Used to fetch cnn news feed.
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

        static constexpr size_t HTTP_RESPONSE_BUF_SIZE = 4000 * 4;

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
        
        // cout << response_string << endl;

        return response_string;
    }

    // standardized method for sending requests using https
    // This function works for all https_responses, as long as we have a correct hostname, resource and a valid certificate of the webpage
    nlohmann::json send_https_request(const char* hostname, const char* resource, const char* cert)
    {
        TLSSocket *https_socket = new TLSSocket;
        const char https_request_template[] = "GET %s HTTP/1.1\r\n"
                                            "Host: %s\r\n"
                                            "Connection: close\r\n"
                                            "\r\n";
        
        static char https_request[1024];
        std::snprintf(https_request, sizeof(https_request), https_request_template, resource, hostname);
        this->socket.close();

        https_socket->set_timeout(500);
        nsapi_size_or_error_t result = https_socket->open(network);

        if (result != NSAPI_ERROR_OK)
        {
            printf("Failed to open TLS socket, error code: %d\n", result);
            return NULL;
        }
        nsapi_connection_status_t connectionStatus = this->network->get_connection_status();
        if (connectionStatus != NSAPI_STATUS_GLOBAL_UP)
        {
            std::cout << "Feil" << std::endl;
            return NULL;
        }

        SocketAddress server_address;
        do {
            result = this->network->gethostbyname(hostname, &server_address);

            if (result != NSAPI_ERROR_OK)
            {
                printf("Failed to get the IP address of the host, error code: %d\n", result);
            }
        }while (result != NSAPI_ERROR_OK);

        std::cout << server_address.get_ip_address() << std::endl;

        server_address.set_port(443);

        result = https_socket->set_root_ca_cert(cert);

        if (result != NSAPI_ERROR_OK)
        {
            printf("Failed to get the root certificate of the website, error code: %d\n", result);
            return NULL;
        }

        https_socket->set_hostname(hostname);
        
        result = https_socket->connect(server_address); 

        if (result != NSAPI_ERROR_OK)
        {
            printf("Failed to connect to the server %s, error code: %d\n", hostname, result);
            return NULL;
        }

        nsapi_size_t BytestoSend = strlen(https_request);
        nsapi_size_t sentBytes = 0;

        while (BytestoSend > 0)
        {
            sentBytes = https_socket->send(https_request + sentBytes, BytestoSend);
            if (sentBytes < 0) break;
            else printf("Sent %d bytes\n", sentBytes);

            if (sentBytes < 0)
            {
                printf("Failed to sent https request: %d", sentBytes);
            }

            BytestoSend -= sentBytes;
        }

        printf("Request sent successfully, handling response...\n\n");

        //char response[2000] = {0};
        static char response[2000] = {0};
        int counter = 0;
        char chunk[100] = {0};
        nsapi_size_t remainingBytes = 100;
        nsapi_size_or_error_t receivedBytes = 0;
        
        memset(response, 0, 2000);

        do // Handling response
        {
            result = https_socket->recv(chunk, 100);
            for (int i = 0; i < 100; i++)
            {
                if (chunk[i] == '\0') break;
                response[counter] = chunk[i];
                chunk[i] = 0;
                counter++;
            }
        } while(result != 0);

        https_socket->close();
        delete https_socket;

        // Removing characters that may exist behind the last curly bracket in the response
        int safetyCounter = 1000;
        while (response[strlen(response) - 1] != '}')
        {
            response[strlen(response) - 1] = '\0';
            safetyCounter--;
            if (safetyCounter == 0)
            {
                return 1;
            }
        }

        std::cout << response << std::endl;

        char *ptr = strchr(response, '{');
        nlohmann::json jsonData = nlohmann::json::parse(ptr);
        return jsonData; // Returns this data as a jsonobject, so that its easy to use in main.cpp
    }
};


#endif // NETWORK_UTILITIES_H
