#ifndef POECRAFT_API_H
#define POECRAFT_API_H

#include <stdint.h>

#include "poecraft/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PC_ABI_VERSION 1u

typedef struct pc_data* pc_data_handle;
typedef struct pc_session* pc_session_handle;
typedef struct pc_action_context* pc_action_context_handle;

uint32_t pc_abi_version(void);

#ifdef __cplusplus
}
#endif

#endif
