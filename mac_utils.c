#include "mac_utils.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#elif defined(__linux__) || defined(__unix__) || defined(__posix__)
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Function to get MAC address as a string (returns 0 on success, -1 on failure)
int get_mac_address(char *mac_str, size_t mac_str_size) {
    if (mac_str_size < 18) return -1; // Minimum size for "XX:XX:XX:XX:XX:XX\0"

#if defined(_WIN32) || defined(_WIN64)
    ULONG buffer_length = 0;
    DWORD result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, NULL, &buffer_length);
    if (result == ERROR_BUFFER_OVERFLOW) {
        IP_ADAPTER_ADDRESSES *adapters = (IP_ADAPTER_ADDRESSES *)MALLOC(buffer_length);
        if (adapters) {
            result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &buffer_length);
            if (result == NO_ERROR) {
                for (IP_ADAPTER_ADDRESSES *adapter = adapters; adapter; adapter = adapter->Next) {
                    if (adapter->PhysicalAddressLength == 6 && strcmp(adapter->AdapterName, "Loopback") != 0) { // Skip loopback
                        snprintf(mac_str, mac_str_size, "%02X:%02X:%02X:%02X:%02X:%02X",
                                 adapter->PhysicalAddress[0], adapter->PhysicalAddress[1], adapter->PhysicalAddress[2],
                                 adapter->PhysicalAddress[3], adapter->PhysicalAddress[4], adapter->PhysicalAddress[5]);
                        FREE(adapters);
                        return 0;
                    }
                }
            }
            FREE(adapters);
        }
    }
    return -1;

#elif defined(__APPLE__)
    // Use kIOMainPortDefault for macOS 12.0+, fall back to kIOMasterPortDefault for older versions
    #if defined(__MAC_12_0) && __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_12_0
        #define IO_PORT kIOMainPortDefault
    #else
        #define IO_PORT kIOMasterPortDefault
    #endif

    io_iterator_t intf_iterator;
    kern_return_t kern_result = KERN_FAILURE;

    CFMutableDictionaryRef matching_dict = IOServiceMatching(kIOEthernetInterfaceClass);
    if (matching_dict) {
        CFMutableDictionaryRef property_match_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (property_match_dict) {
            CFDictionarySetValue(property_match_dict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue);
            CFDictionarySetValue(matching_dict, CFSTR(kIOPropertyMatchKey), property_match_dict);
            CFRelease(property_match_dict);
        }
        kern_result = IOServiceGetMatchingServices(IO_PORT, matching_dict, &intf_iterator);
    }

    if (kern_result == KERN_SUCCESS) {
        io_object_t intf_service;
        while ((intf_service = IOIteratorNext(intf_iterator))) {
            io_object_t controller_service;
            kern_result = IORegistryEntryGetParentEntry(intf_service, kIOServicePlane, &controller_service);
            if (kern_result == KERN_SUCCESS) {
                CFDataRef mac_data = (CFDataRef)IORegistryEntryCreateCFProperty(controller_service, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
                if (mac_data) {
                    const UInt8 *mac_bytes = CFDataGetBytePtr(mac_data);
                    snprintf(mac_str, mac_str_size, "%02X:%02X:%02X:%02X:%02X:%02X",
                             mac_bytes[0], mac_bytes[1], mac_bytes[2], mac_bytes[3], mac_bytes[4], mac_bytes[5]);
                    CFRelease(mac_data);
                    IOObjectRelease(controller_service);
                    IOObjectRelease(intf_service);
                    IOObjectRelease(intf_iterator);
                    return 0;
                }
                IOObjectRelease(controller_service);
            }
            IOObjectRelease(intf_service);
        }
        IOObjectRelease(intf_iterator);
    }
    return -1;

#elif defined(__linux__) || defined(__unix__) || defined(__posix__)
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) return -1;

    // Try common interfaces (eth0 for RPi/Linux, en0 for others)
    const char *interfaces[] = {"eth0", "wlan0", "en0", NULL};
    for (int i = 0; interfaces[i]; i++) {
        strcpy(ifr.ifr_name, interfaces[i]);
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
            if (memcmp(mac, "\0\0\0\0\0\0", 6) != 0) { // Skip zero MAC (loopback/invalid)
                snprintf(mac_str, mac_str_size, "%02X:%02X:%02X:%02X:%02X:%02X",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                close(sock);
                return 0;
            }
        }
    }
    close(sock);
    return -1;
#else
    return -1; // Unsupported platform
#endif
}

// Function to get MAC address as a 6-byte number (returns non-negative on success, -1 on failure)
int64_t get_mac_address_as_long(void) {
    uint64_t mac_num = 0;

#if defined(_WIN32) || defined(_WIN64)
    ULONG buffer_length = 0;
    DWORD result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, NULL, &buffer_length);
    if (result == ERROR_BUFFER_OVERFLOW) {
        IP_ADAPTER_ADDRESSES *adapters = (IP_ADAPTER_ADDRESSES *)MALLOC(buffer_length);
        if (adapters) {
            result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &buffer_length);
            if (result == NO_ERROR) {
                for (IP_ADAPTER_ADDRESSES *adapter = adapters; adapter; adapter = adapter->Next) {
                    if (adapter->PhysicalAddressLength == 6 && strcmp(adapter->AdapterName, "Loopback") != 0) { // Skip loopback
                        mac_num = ((uint64_t)adapter->PhysicalAddress[0] << 40) |
                                  ((uint64_t)adapter->PhysicalAddress[1] << 32) |
                                  ((uint64_t)adapter->PhysicalAddress[2] << 24) |
                                  ((uint64_t)adapter->PhysicalAddress[3] << 16) |
                                  ((uint64_t)adapter->PhysicalAddress[4] << 8) |
                                  ((uint64_t)adapter->PhysicalAddress[5]);
                        FREE(adapters);
                        return (int64_t)mac_num;
                    }
                }
            }
            FREE(adapters);
        }
    }
    return -1;

#elif defined(__APPLE__)
    // Use kIOMainPortDefault for macOS 12.0+, fall back to kIOMasterPortDefault for older versions
    #if defined(__MAC_12_0) && __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_12_0
        #define IO_PORT kIOMainPortDefault
    #else
        #define IO_PORT kIOMasterPortDefault
    #endif

    io_iterator_t intf_iterator;
    kern_return_t kern_result = KERN_FAILURE;

    CFMutableDictionaryRef matching_dict = IOServiceMatching(kIOEthernetInterfaceClass);
    if (matching_dict) {
        CFMutableDictionaryRef property_match_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (property_match_dict) {
            CFDictionarySetValue(property_match_dict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue);
            CFDictionarySetValue(matching_dict, CFSTR(kIOPropertyMatchKey), property_match_dict);
            CFRelease(property_match_dict);
        }
        kern_result = IOServiceGetMatchingServices(IO_PORT, matching_dict, &intf_iterator);
    }

    if (kern_result == KERN_SUCCESS) {
        io_object_t intf_service;
        while ((intf_service = IOIteratorNext(intf_iterator))) {
            io_object_t controller_service;
            kern_result = IORegistryEntryGetParentEntry(intf_service, kIOServicePlane, &controller_service);
            if (kern_result == KERN_SUCCESS) {
                CFDataRef mac_data = (CFDataRef)IORegistryEntryCreateCFProperty(controller_service, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
                if (mac_data) {
                    const UInt8 *mac_bytes = CFDataGetBytePtr(mac_data);
                    mac_num = ((uint64_t)mac_bytes[0] << 40) |
                              ((uint64_t)mac_bytes[1] << 32) |
                              ((uint64_t)mac_bytes[2] << 24) |
                              ((uint64_t)mac_bytes[3] << 16) |
                              ((uint64_t)mac_bytes[4] << 8) |
                              ((uint64_t)mac_bytes[5]);
                    CFRelease(mac_data);
                    IOObjectRelease(controller_service);
                    IOObjectRelease(intf_service);
                    IOObjectRelease(intf_iterator);
                    return (int64_t)mac_num;
                }
                IOObjectRelease(controller_service);
            }
            IOObjectRelease(intf_service);
        }
        IOObjectRelease(intf_iterator);
    }
    return -1;

#elif defined(__linux__) || defined(__unix__) || defined(__posix__)
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) return -1;

    // Try common interfaces (eth0 for RPi/Linux, en0 for others)
    const char *interfaces[] = {"eth0", "wlan0", "en0", NULL};
    for (int i = 0; interfaces[i]; i++) {
        strcpy(ifr.ifr_name, interfaces[i]);
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
            if (memcmp(mac, "\0\0\0\0\0\0", 6) != 0) { // Skip zero MAC (loopback/invalid)
                mac_num = ((uint64_t)mac[0] << 40) |
                          ((uint64_t)mac[1] << 32) |
                          ((uint64_t)mac[2] << 24) |
                          ((uint64_t)mac[3] << 16) |
                          ((uint64_t)mac[4] << 8) |
                          ((uint64_t)mac[5]);
                close(sock);
                return (int64_t)mac_num;
            }
        }
    }
    close(sock);
    return -1;
#else
    return -1; // Unsupported platform
#endif
}
