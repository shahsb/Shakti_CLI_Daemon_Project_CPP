#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <cstdint>

// Message types for IPC
enum class MessageType {
    INSERT,
    DELETE,
    PRINT_ALL,
    DELETE_ALL,
    FIND,
    RESPONSE_SUCCESS,
    RESPONSE_ERROR,
    RESPONSE_DATA
};

// IPC message structure
struct IPCMessage {
    MessageType type;
    int32_t number;  // For INSERT, DELETE, FIND operations
    int64_t timestamp; // For timestamps
    char error_msg[256]; // For error messages
};

// Operation results
struct NumberEntry {
    int32_t number;
    int64_t timestamp;
    
    NumberEntry(int32_t n, int64_t t) : number(n), timestamp(t) {}
    
    // For sorting
    bool operator<(const NumberEntry& other) const {
        return number < other.number;
    }
};

#endif