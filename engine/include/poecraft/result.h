#ifndef POECRAFT_RESULT_H
#define POECRAFT_RESULT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pc_result {
    PC_RESULT_OK = 0,
    PC_RESULT_INVALID_ARGUMENT = 1,
    PC_RESULT_IO_ERROR = 2,
    PC_RESULT_DATA_ERROR = 3,
    PC_RESULT_UNSUPPORTED_FEATURE = 4,
    PC_RESULT_INTERNAL_ERROR = 5
} pc_result;

typedef struct pc_error_info {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t code;
    char message[256];
} pc_error_info;

#ifdef __cplusplus
}
#endif

#endif
