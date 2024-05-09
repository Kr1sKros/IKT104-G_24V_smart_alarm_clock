/*!
 * @file network_utilities.h
 * @brief A utility header file for all network related functionality.
 * @author Anders L.
 */

#ifndef NETWORK_UTILITIES_H
#define NETWORK_UTILITIES_H

#include "Socket.h"
#include "SocketAddress.h"
#include "mbed.h"

NetworkInterface *network = nullptr;
SocketAddress local_ip = nullptr;
TCPSocket socket;

void setup_default_network(){
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
}


#endif // NETWORK_UTILITIES_H