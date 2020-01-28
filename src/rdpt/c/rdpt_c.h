#ifndef _TUNNEL_H_
#define _TUNNEL_H_

#define ADDON_TUNNEL_ENABLE

#include <pthread.h>
#include "rdpt_prot.h"

#include "cpkl.h"

#define RDPT_C_SOCKBUFF_SIZE			(16 * 1024 * 1024)
#define RDPT_S_SOCKBUFF_SIZE			(16 * 1024 * 1024)

typedef struct _rdpt_chandesc {
	int			sock;
	char		*recvbuf;
} rdpt_chandesc_t;

typedef enum _rdpt_c_state {
	RDPT_C_STATE_RST = 0,
	RDPT_C_STATE_NEGO,
	RDPT_C_STATE_NEGO_ACK,
	RDPT_C_STATE_OK,
	RDPT_C_STATE_COUNT
} rdpt_c_state_e;

typedef struct _rdpt_protctx {
	unsigned		s2cpic_width, s2cpic_height;
	unsigned		left, right, top, bottom, x, y;	/* received from server */
	/* recevice socket pair */
	int				recv_sp_s, recv_sp_d;
	unsigned		recv_need_reset, next_h, padding_nh;

	rdpt_chandesc_t		chaninfo[RDPT_MAX_CHAN_NUM];
	volatile rdpt_c_state_e		state;
	pthread_mutex_t				send_lock;

	/* kbd send socket, one sock of the sockpait
	 * kbd_send =====> g_rdpt_sock
	 */
	int kbd_send;
	/* number of events in one RDP event pkt which send to s */
	int neperpkt;
	/* send interval: us */
	int send_intv;

	/* serv socket of rdpt, listen to the user socket */
	int serv_sock;

	struct {
		/**/
		unsigned	recv_from_s_fhlen;
		unsigned	recv_from_s_fhlen_proc;
		unsigned	send_to_s_fhlen;
		/**/
		unsigned	send_to_c_sdlen;
	} stat;

	rdpt_s_report_t last;
} rdpt_protctx_t;

extern int g_rdpt_sock;

void rdpt_send_proc();
void rdpt_recv(int x, int y, int cx, int cy, int width, int height, unsigned char * data);
void rdpt_init();

extern STREAM
rdp_init_data(int maxlen);
extern void
rdp_send_data(STREAM s, uint8 data_pdu_type);


#endif

