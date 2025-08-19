#include "mac_utils.h"
#include <stdio.h>

int main() {
    char client_id[18]; // Enough for MAC string
    if (get_mac_address(client_id, sizeof(client_id)) == 0) {
        printf("Client ID (MAC string): %s\n", client_id);
    } else {
        printf("Failed to get MAC address as string.\n");
    }

    int64_t mac_long = get_mac_address_as_long();
    if (mac_long != -1) {
        printf("Client ID (MAC long/hex): %llx\n", (unsigned long long)mac_long);
        printf("Client ID (MAC long): %lld\n", (unsigned long long)mac_long);
    } else {
        printf("Failed to get MAC address as long.\n");
    }
    return 0;
}
