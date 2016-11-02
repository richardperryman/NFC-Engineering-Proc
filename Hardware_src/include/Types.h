#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <stdint.h>

/**
 * @file Types.h
 * 
 * This header handles all type imports and type definitions.
 */

enum debug_level_t {
    CRITICAL = 50,
    ERROR = 40,
    WARNING = 30,
    INFO = 20
};

// Missing opcodes for startup sequence stuff
// Not sure if I'll leave this as 16-bit hex numbers or convert to byte array
enum serial_opcodes_t {
    REQUESTAUTH = 0x0001,
    ACKNOWLEDGE = 0x0002,
    DATA = 0x0003,
    ERROR = 0xFFFF
};

#endif // TYPES_H
