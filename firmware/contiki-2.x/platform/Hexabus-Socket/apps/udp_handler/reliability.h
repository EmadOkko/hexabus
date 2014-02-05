#ifndef _RELIABILITY_H_
#define _RELIABILTIY_H_

#include "hexabus_config.h"
#include "hexabus_types.h"
#include "hexabus_packet.h"
#include "process.h"

#define SEND_QUEUE_SIZE 8
#define RETRANS_LIMIT 3
#define RETRANS_TIMEOUT 0.5 * CLOCK_CONF_SECOND

PROCESS_NAME(reliability_send_process);

enum hxb_error_code enqueue_packet(struct hxb_queue_packet* packet);
enum hxb_error_code receive_packet(struct hxb_queue_packet* packet);
void reset_reliability_layer();

#endif