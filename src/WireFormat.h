#ifndef GUNGNIR_WIREFORMAT_H
#define GUNGNIR_WIREFORMAT_H


#include <cstdint>

typedef enum Status {
    STATUS_OK = 0,
    STATUS_OBJECT_DOESNT_EXIST = 2,
    STATUS_RETRY = 3,
    STATUS_MESSAGE_ERROR = 4,
    STATUS_INTERNAL_ERROR = 5
} Status;

class WireFormat {
public:
    /**
     * Each RPC request starts with this structure.
     */
    struct RequestCommon {
        uint16_t opcode;              /// Opcode of operation to be performed.
    } __attribute__((packed));

    /**
 * Each RPC response starts with this structure.
 */
    struct ResponseCommon {
        Status status;                // Indicates whether the operation
        // succeeded; if not, it explains why.
    } __attribute__((packed));


    struct RetryResponse {
        ResponseCommon common;
        uint32_t minDelayMicros;
        uint32_t maxDelayMicros;
        uint32_t messageLength;
    } __attribute__((packed));

};


#endif //GUNGNIR_WIREFORMAT_H