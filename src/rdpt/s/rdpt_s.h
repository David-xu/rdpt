#ifndef _RDPT_S_H_
#define _RDPT_S_H_

// #define RDPT_DEBUG_ON

// #define RDPT_VC_DEBUG

#ifdef RDPT_DEBUG_ON

HANDLE g_stdout;

#define RDPT_DEBUG(fmt, ...) do {				\
	static char __buf[4096];					\
	unsigned long __len;						\
	__len = sprintf(__buf, fmt, ##__VA_ARGS__);	\
	WriteFile(g_stdout, __buf, __len, &__len, 0);\
} while (0)

#else
#define RDPT_DEBUG(fmt, ...)
#endif

#define RDPT_ASSERT(cond)												\
	do {																\
		if (!(cond))													\
		{																\
			RDPT_DEBUG("\n%s:%d, %s", __FILE__, __LINE__, #cond);		\
			while (1);													\
		}																\
	} while (0)

#define RDPT_S_SENDTIMER_ID			2
#define RDPT_S_REPORTTIMER_ID		3

#define CPKL_ALIGN(l, align)	((((l) + (align) - 1) / (align)) * (align))

typedef enum _rdpt_recvstate{
	RDPT_STATE_IDLE = 0,
	RDPT_STATE_DATA,
	RDPT_STATE_COUNT
} rdpt_recvstate_e;

#define CYCBUF_MAXBUF			256
typedef struct _frame_cycblkbuf {
	volatile unsigned	b, e;
	unsigned char	buf[CYCBUF_MAXBUF][RDPT_MAX_FRAME_LEN];
	unsigned		len[CYCBUF_MAXBUF];
} frame_cycblkbuf_t;

typedef struct _frame_cycbuf {
	volatile unsigned long long		b, e;
	char			*buf;
	HANDLE			ins_lock;
} frame_cycbuf_t;

typedef enum _rdpt_recvfile_state {
	RFS_IDLE = 0,
	RFS_DATA,
	RFS_COUNT
} rdpt_recvfile_state_t;

typedef struct _rdpt_chanctx {
	unsigned				inuse	: 1;
	unsigned				state	: 7;
	unsigned				chan	: RDPT_MAX_CHAN_NUM;

	struct {
		char		name[256];
		unsigned	len, recv_len;
		char		*buf;
		rdpt_recvfile_state_t	state;
	} recvfile_ctx;

	struct {
		int						sock;
		HANDLE					h_rt;	/* recv thread handle */
	} sockdesc;

} rdpt_chanctx_t;

typedef enum _rdpt_s_state {
	RDPT_S_STATE_RST = 0,
	RDPT_S_STATE_OK,
	RDPT_S_STATE_COUNT
} rdpt_s_state_e;

typedef struct _rdpt_ctx {
	HWND hwnd;

	unsigned		eg_width, eg_height;
	unsigned		lw_b, pic_sz;	/* line width in bytes, pic size in bytes */

	unsigned		send_quota;

	frame_cycblkbuf_t	recvframe;
	frame_cycbuf_t		sendframe;

	RECT		client_rect;
	POINT		client_pos;
	unsigned	client_width, client_height;

	int			kbd_decmap[256];

	rdpt_s_state_e		state;
	rdpt_chanctx_t		chanlist[RDPT_MAX_CHAN_NUM];

	HANDLE				timer_q;
#if 1
	UINT_PTR			send_timer;
#else
	HANDLE				send_timer;
#endif
	rdpt_s_report_t		report;
} rdpt_ctx_t;

int buf_insert(frame_cycbuf_t *cycbuf, rdpt_fh_t *fh);
int eg_setwin(int width, int height);
int channel_ctx_reset(rdpt_chanctx_t *ctx);
int channel_send(rdpt_chanctx_t *ctx, void *buf, unsigned length);
int channel_recv(rdpt_chanctx_t *ctx, rdpt_sockdata_t *sd);

#ifndef WIN32
/**/
/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr {
        unsigned short sa_family;              /* address family */
        char    sa_data[14];            /* up to 14 bytes of direct address */
};

//
// IPv4 Internet address
// This is an 'on-wire' format structure.
//
typedef struct in_addr {
        union {
                struct { unsigned char s_b1,s_b2,s_b3,s_b4; } S_un_b;
                struct { unsigned short s_w1,s_w2; } S_un_w;
                unsigned long S_addr;
        } S_un;
#define s_addr  S_un.S_addr /* can be used for most tcp & ip code */
#define s_host  S_un.S_un_b.s_b2    // host on imp
#define s_net   S_un.S_un_b.s_b1    // network
#define s_imp   S_un.S_un_w.s_w2    // imp
#define s_impno S_un.S_un_b.s_b4    // imp #
#define s_lh    S_un.S_un_b.s_b3    // logical host
} IN_ADDR, *PIN_ADDR, FAR *LPIN_ADDR;


/*
 * Socket address, internet style.
 */
struct sockaddr_in {
        short   sin_family;
        unsigned short sin_port;
        struct  in_addr sin_addr;
        char    sin_zero[8];
};

/*
 * Structures returned by network data base library, taken from the
 * BSD file netdb.h.  All addresses are supplied in host order, and
 * returned in network order (suitable for use in system calls).
 */

struct  hostent {
        char    FAR * h_name;           /* official name of host */
        char    FAR * FAR * h_aliases;  /* alias list */
        short   h_addrtype;             /* host address type */
        short   h_length;               /* length of address */
        char    FAR * FAR * h_addr_list; /* list of addresses */
#define h_addr  h_addr_list[0]          /* address, for backward compat */
};

struct hostent * gethostbyname(const char * name);

#endif

#endif
