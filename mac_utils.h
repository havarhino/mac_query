#ifndef MAC_UTILS_H
#define MAC_UTILS_H

#include <stdint.h>
#include <stddef.h>

// Get MAC address as a string (e.g., "B8:27:EB:12:34:56")
// Returns 0 on success, -1 on failure
int get_mac_address(char *mac_str, size_t mac_str_size);

// Get MAC address as a 6-byte number in a 64-bit integer
// Returns non-negative on success, -1 on failure
int64_t get_mac_address_as_long(void);

#endif // MAC_UTILS_H
