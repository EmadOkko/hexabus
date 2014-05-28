#include "reliability.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hexabus_config.h"
#include "process.h"
#include "net/packetqueue.h"
#include "sequence_numbers.h"
#include "udp_handler.h"
#include "state_machine.h"

#define LOG_LEVEL UDP_HANDLER_DEBUG
#include "syslog.h"

#define MAX(a,b) (seqnumIsLessEqual((a),(b))?(b):(a))

enum hxb_send_state_t {
	SINIT		= 0,
	SREADY		= 1,
	SWAIT_ACK	= 2,
	SFAILED		= 3,
};

enum hxb_recv_state_t {
	RINIT 		= 0,
	RREADY 		= 1,
	RFAILED 	= 2,
};

PROCESS(reliability_send_process, "Reliability sending state machine");
AUTOSTART_PROCESSES(&reliability_send_process);

PACKETQUEUE(send_queue, SEND_QUEUE_SIZE);

struct reliability_state {
	enum hxb_send_state_t send_state;
	enum hxb_recv_state_t recv_state;
	uint8_t fail;
	bool ack;
	uint8_t retrans;
	uint16_t want_ack_for;
	struct etimer timeout_timer;
	bool recved_packet;
	uint16_t rseq_num;
	struct hxb_queue_packet* P;
};

struct reliability_state rstates[16];

static uint8_t fail;
static struct hxb_queue_packet* R;

bool seqnumIsLessEqual(uint16_t first, uint16_t second) {
	int16_t distance = (int16_t)(first - second);
	return distance <= 0;
}

enum hxb_error_code packet_generator(union hxb_packet_any* buffer, void* data) {
	
	uint8_t hash = hash_ip(&((struct hxb_queue_packet*)data)->ip);

	if(rstates[hash].want_ack_for == 0) {
		rstates[hash].want_ack_for = next_sequence_number(&((struct hxb_queue_packet*)data)->ip);
	}

	memcpy(buffer, &((struct hxb_queue_packet*)data)->packet, sizeof(union hxb_packet_any));
	buffer->header.sequence_number = rstates[hash].want_ack_for;

	return HXB_ERR_SUCCESS;
}

enum hxb_error_code send_packet(struct hxb_queue_packet* data) {
	return udp_handler_send_generated(&((struct hxb_queue_packet*)data)->ip, ((struct hxb_queue_packet*)data)->port, &packet_generator, (void*)data);
}

enum hxb_error_code enqueue_packet(struct hxb_queue_packet* packet) {

	struct hxb_queue_packet* queue_entry = malloc(sizeof(struct hxb_queue_packet));

	if(packet != NULL) {
		memcpy(queue_entry, packet, sizeof(struct hxb_queue_packet));

		if(packetqueue_enqueue_packetbuf(&send_queue, 0, (void*) queue_entry)) {
			return HXB_ERR_OUT_OF_MEMORY;
		}
	} else {
		return HXB_ERR_NO_VALUE;
	}

	return HXB_ERR_SUCCESS;
}

bool allows_implicit_ack(union hxb_packet_any* packet) {
	switch(packet->header.type) {
		case HXB_PTYPE_QUERY:
		case HXB_PTYPE_EPQUERY:
		case HXB_PTYPE_WRITE:
		case HXB_PTYPE_ACK:
			return true;
			break;
		default:
			return false;
	}
}

bool is_ack_for(union hxb_packet_any* packet, uint16_t seq_num) {
	switch(packet->header.type) {
		case HXB_PTYPE_REPORT:
		case HXB_PTYPE_EPREPORT:
		case HXB_PTYPE_ACK:
			return (seq_num == packet->header.sequence_number);
			break;
		default:
			return false;
	}
}

static void run_send_state_machine(uint8_t rs) {
	switch(rstates[rs].send_state) {
		case SINIT:
			rstates[rs].want_ack_for = 0;
			rstates[rs].retrans = 0;
			rstates[rs].send_state = SREADY;
			rstates[rs].P = NULL;
			break;
		case SREADY:
			if (packetqueue_len(&send_queue) > 0) {
				if(hash_ip(&((struct hxb_queue_packet *)packetqueue_first(&send_queue))->ip) == rs) {
					rstates[rs].P = (struct hxb_queue_packet *)packetqueue_first(&send_queue);
					packetqueue_dequeue(&send_queue);
	
					if(!send_packet((struct hxb_queue_packet *)rstates[rs].P)) {
						rstates[rs].retrans = 0;
						rstates[rs].ack = false;
						rstates[rs].send_state = SWAIT_ACK;
						etimer_set(&(rstates[rs].timeout_timer), RETRANS_TIMEOUT);
					} else {
						syslog(LOG_ERR, "Could not send reliable packet.");
					}
				}
			}
			break;
		case SWAIT_ACK:
			if(rstates[rs].ack) {
				rstates[rs].want_ack_for = 0;
				free(rstates[rs].P);
				rstates[rs].P = NULL;
				rstates[rs].send_state = SREADY;
			} else if(etimer_expired(&(rstates[rs].timeout_timer))) {
				if((rstates[rs].retrans)++ < RETRANS_LIMIT) {
					send_packet((struct hxb_queue_packet *)rstates[rs].P);
					etimer_set(&(rstates[rs].timeout_timer), RETRANS_TIMEOUT);
				} else {
					fail = 1;
					rstates[rs].send_state = SFAILED;
					syslog(LOG_WARN, "Reached maximum retransmissions, attempting recovery...");
				}
			}
			break;
		case SFAILED:
			if(fail == 2) {
				syslog(LOG_WARN, "Resetting...");
				process_post(PROCESS_BROADCAST, udp_handler_event, UDP_HANDLER_UP);
				sm_restart();
			}
			break;
	}
}

static void run_recv_state_machine(uint8_t rs) {
	switch(rstates[rs].recv_state) {
		case RINIT:
			rstates[rs].rseq_num = 0;
			rstates[rs].ack = false;
			rstates[rs].recved_packet = false;
			rstates[rs].recv_state = RREADY;
			break;
		case RREADY:
			if(fail) {
				rstates[rs].recv_state = RFAILED;
			} else if(rstates[rs].recved_packet){
				syslog(LOG_INFO, "Processing packet.");
				if((R->packet.header.flags & HXB_FLAG_WANT_ACK) && !allows_implicit_ack(&(R->packet))) {
					udp_handler_send_ack(&(R->ip), R->port, uip_ntohs(R->packet.header.sequence_number));

					if(seqnumIsLessEqual(uip_ntohs(R->packet.header.sequence_number), rstates[rs].rseq_num)) {
						//TODO handle reordering here
						rstates[rs].recv_state = RREADY;
						break;
					}
				}

				rstates[rs].rseq_num = MAX(rstates[rs].rseq_num, uip_ntohs(R->packet.header.sequence_number));

				//FIXME reports are not processed
				if(rstates[rs].want_ack_for != 0 && is_ack_for(&(R->packet), rstates[rs].want_ack_for)) {
					rstates[rs].ack = true;
					rstates[rs].recv_state = RREADY;
				} else {
					udp_handler_handle_incoming(R);
					rstates[rs].recv_state = RREADY;
				}
			}
			break;
		case RFAILED:
			if(uip_ipaddr_cmp(&(R->ip), &udp_master_addr)) {
				fail = 2;
				rstates[rs].recv_state = RREADY;
			}
			break;
	}
}

enum hxb_error_code receive_packet(struct hxb_queue_packet* packet) {

	uint8_t hash = hash_ip(&(packet->ip));

	R = packet;
	rstates[hash].recved_packet = true;
	run_recv_state_machine(hash); 
	rstates[hash].recved_packet = false;

	if(fail)
		return HXB_ERR_INTERNAL;
	return HXB_ERR_SUCCESS;
}

void init_reliability_layer() {
	for (int i=0; i<16; ++i) {
		rstates[i].recv_state = RINIT;
		rstates[i].send_state = SINIT;
	}
	packetqueue_init(&send_queue);
	fail = 0;
}

void reset_reliability_layer() {
	while(packetqueue_len(&send_queue) > 0) {
		free(packetqueue_first(&send_queue));
		packetqueue_dequeue(&send_queue);
	}
	init_reliability_layer();
}

PROCESS_THREAD(reliability_send_process, ev, data)
{
	PROCESS_EXITHANDLER(goto exit);

	PROCESS_BEGIN();

	init_reliability_layer();

	while (1) {
		if (ev == udp_handler_event && *(udp_handler_event_t*) data == UDP_HANDLER_UP) {
			reset_reliability_layer();
		}

		for (int i=0; i<16; ++i)
		{
			run_send_state_machine(i);
			run_recv_state_machine(i);
		}
		PROCESS_PAUSE();
	}

exit: ;

	PROCESS_END();
}