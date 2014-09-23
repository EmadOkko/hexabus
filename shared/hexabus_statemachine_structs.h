#ifndef HEXABUS_STATEMACHINE_STRUCTS_H_
#define HEXABUS_STATEMACHINE_STRUCTS_H_

#include "hexabus_types.h"

#ifdef __cplusplus
namespace hexabus {
#endif

// these structs implement a simple table layout

// op for datetime: bits 0..6 denote dependency on hour, minute, second, ...; bit 7 sets whether to check for >= or <.
// date/time transitions need to be stored separately. They are also executed seperately, each time before the "normal" transitions are executed

struct condition {
  uint8_t  sourceIP[16];    // IP
  uint32_t sourceEID;       // EID we expect data from
  uint8_t  op;              // predicate function
  struct   hxb_value value; // Date to compare with
} __attribute__ ((packed));

struct transition {
  uint8_t  fromState;      // current state
  uint8_t  cond;           // index of condition that must be matched
  uint32_t eid;            // id of endpoint which should do something
  uint8_t  goodState;      // new state if everything went fine
  uint8_t  badState;       // new state if some went wrong
  struct   hxb_value value;  // Data for the endpoint
} __attribute__ ((packed));

// flags for date/time conditions
#define HXB_SM_HOUR    0x01
#define HXB_SM_MINUTE  0x02
#define HXB_SM_SECOND  0x04
#define HXB_SM_DAY     0x08
#define HXB_SM_MONTH   0x10
#define HXB_SM_YEAR    0x20
#define HXB_SM_WEEKDAY 0x40
#define HXB_SM_DATETIME_OP_GEQ 0x80

// operator for timestamp comparison
#define HXB_SM_TIMESTAMP_OP 0x80

// "true" condition "index"
#define TRUE_COND_INDEX 255

#define EE_STATEMACHINE_CHUNK_SIZE 64

#ifdef __cplusplus
}
#endif

#endif

