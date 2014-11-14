#ifndef HEXABUS_TYPES_H_
#define HEXABUS_TYPES_H_

#ifdef __cplusplus
namespace hexabus {
#endif

#include <stdint.h>

// Boolean values
enum hxb_bool {
	HXB_FALSE = 0,
	HXB_TRUE = 1,
};

// Packet types
enum hxb_packet_type {
	HXB_PTYPE_ERROR          = 0x00, // An error occured -- check the error code field for more information
	HXB_PTYPE_INFO           = 0x01, // Endpoint provides information
	HXB_PTYPE_QUERY          = 0x02, // Endpoint is requested to provide information
	HXB_PTYPE_REPORT         = 0x03, // Endpoint provides information with seqence number of the causing QUERY
	HXB_PTYPE_WRITE          = 0x04, // Endpoint is requested to set its value
	HXB_PTYPE_EPINFO         = 0x09, // Endpoint metadata
	HXB_PTYPE_EPQUERY        = 0x0A, // Request endpoint metadata
	HXB_PTYPE_EPREPORT       = 0x0B, // Endpoint metadata with seqence number of the causing EPQUERY
	HXB_PTYPE_ACK            = 0x10, // Acknowledgement for INFO, EPINFO and PINFO packets
	HXB_PTYPE_PINFO          = 0x11, // Endpoint provides information with IP address of causing node
	HXB_PTYPE_TIMEINFO       = 0x12, // Current date and time to update the internal device clock
	HXB_PTYPE_EP_PROP_QUERY  = 0x13, // Endpoint is requested to provide information about its properties
	HXB_PTYPE_EP_PROP_REPORT = 0x14, // Endpoint provides property with seqence number of the causing QUERY
	HXB_PTYPE_EP_PROP_WRITE  = 0x15, // Endpoint is requested to write a property
};

// Data types
enum hxb_datatype {
	HXB_DTYPE_UNDEFINED = 0x00, // Undefined: Nonexistent data type
	HXB_DTYPE_BOOL      = 0x01, // Boolean. Value still represented by 8 bits, but may only be HXB_TRUE or HXB_FALSE
	HXB_DTYPE_UINT8     = 0x02, // Unsigned 8 bit integer
	HXB_DTYPE_UINT32    = 0x03, // Unsigned 32 bit integer
	HXB_DTYPE_DATETIME	= 0x04, // TODO: remove
	HXB_DTYPE_FLOAT     = 0x05, // 32bit floating point
	HXB_DTYPE_128STRING = 0x06, // 128char fixed length string
	HXB_DTYPE_TIMESTAMP = 0x07, // timestamp - used for measuring durations, time differences and so on - uint32; seconds
	HXB_DTYPE_65BYTES   = 0x08, // raw 65 byte array, e.g. state machine data.
	HXB_DTYPE_16BYTES   = 0x09, // raw 16 byte array, e.g. state machine ID.
};

enum hxb_flags {
	HXB_FLAG_NONE       	= 0x00, // No flags set
	HXB_FLAG_WANT_ACK   	= 0x01, // sending node requires acknowledgement
	HXB_FLAG_WANT_UL_ACK	= 0x02, // additonal flag for state machine changes
	HXB_FLAG_RELIABLE			= 0x04, // packet was sent through reliability layer
};

// Error codes
enum hxb_error_code {
	HXB_ERR_SUCCESS        = 0x00, // reserved: No error
	HXB_ERR_UNKNOWNEID     = 0x01, // A request for an endpoint which does not exist on the device was received
	HXB_ERR_WRITEREADONLY  = 0x02, // A WRITE was received for a readonly endpoint
	HXB_ERR_CRCFAILED      = 0x03, // A packet failed the CRC check -- TODO How can we find out what information was lost?
	HXB_ERR_DATATYPE       = 0x04, // A packet with a datatype that does not fit the endpoint was received
	HXB_ERR_INVALID_VALUE  = 0x05, // A value was encountered that cannot be interpreted
	HXB_ERR_RETRANSMISSION = 0x06, // A packet was received more than once

	// internal error values
	HXB_ERR_INTERNAL         = 0x80, // this is just a threshold. everything above is considered internal to a device and should never reach the network

	HXB_ERR_MALFORMED_PACKET  = 0x80,
	HXB_ERR_UNEXPECTED_PACKET = 0x81,
	HXB_ERR_NO_VALUE          = 0x82,
	HXB_ERR_OUT_OF_MEMORY     = 0x83
};

// Operators for comparison in state machine
enum hxb_stm_op {
	STM_EQ  = 0x00,
	STM_LEQ = 0x01,
	STM_GEQ = 0x02,
	STM_LT  = 0x03,
	STM_GT  = 0x04,
	STM_NEQ = 0x05,
};

// State machine runtime states
enum STM_state_t {
  STM_STATE_STOPPED = 0,
  STM_STATE_RUNNING = 1
};

struct hxb_datetime {
    uint8_t   hour;
    uint8_t   minute;
    uint8_t   second;
    uint8_t   day;
    uint8_t   month;
    uint16_t  year;
    uint8_t   weekday;  // numbers from 0 to 6, sunday as the first day of the week.
} __attribute__ ((packed));

// Struct for passing Hexabus values around
// One struct for all data types (except 128string, because that'd need too much memory), with a datatype flag indicating which
// of the values is used. Used for passing values to and from endpoint_access
struct hxb_value {
	uint8_t               datatype;   // Datatype that is used, or HXB_DTYPE_UNDEFINED
	union {
		uint8_t             v_bool;
		uint8_t             v_u8;
		uint32_t            v_u32;
		struct hxb_datetime v_datetime;
		float               v_float;
		char*               v_string;
		uint32_t            v_timestamp;
		char*               v_binary;
	};
} __attribute__((packed));

#ifdef __cplusplus
}
#endif

#endif
