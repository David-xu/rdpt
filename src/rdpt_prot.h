#ifndef _RDPT_PROT_H_
#define _RDPT_PROT_H_

#define CODE_SECTION(t)

#define RDPT_MAX_FRAME_BW				12
#define RDPT_MAX_FRAME_LEN				(0x1LL << RDPT_MAX_FRAME_BW)

#define RDPT_MAX_CHAN_BW				4
#define RDPT_MAX_CHAN_NUM				(0x1LL << RDPT_MAX_CHAN_BW)

#define RDPT_FRAMELEN_ALIGN				8
#define RDPT_FRAMELEN_QW(l)				((((l) + (RDPT_FRAMELEN_ALIGN) - 1) / (RDPT_FRAMELEN_ALIGN)))
#define RDPT_SOCKDATALEN_ALIGN			4
#define RDPT_SOCKDATALEN_DW(l)			((((l) + (RDPT_SOCKDATALEN_ALIGN) - 1) / (RDPT_SOCKDATALEN_ALIGN)))

#define RDPT_SERV_PORT					(8999)

#define RDPT_MAXEVENT_PERPKT			(4096)


/* rdpt protocol frame head
 *
 */
enum {
	RDPT_FTYPE_CTRL = 0,
	RDPT_FTYPE_CD,				/* channel data */
	RDPT_FTYPE_PADDING,
	RDPT_FTYPE_COUNT
};

enum {
	RDPT_CTRL_RST = 0,
	RDPT_CTRL_INIT,				/* c -> s */
	RDPT_CTRL_INFO,				/* s -> c */
	RDPT_CTRL_CNFM,				/* confirm */
	RDPT_CTRL_CHANBIND,			/* c -> s */
	RDPT_CTRL_CHANRELEASE,		/* c -> s */
	RDPT_CTRL_CHANSTATE,		/* s -> c */
	RDPT_CTRL_SCLOSED,			/* s -> c, s closed */
	RDPT_CTRL_S_REPORT,			/* s -> c */
	RDPT_CTRL_COUNT
};

typedef struct _rdpt_s_report {
	unsigned		ver;
	unsigned		n_eku;			/* number of invalid esc key up */
	unsigned		n_ekd;			/* number of invalid esc key down */
	unsigned		n_k;			/* number of invalid key */
	unsigned		n_rce;			/* number of receive frame crc error */
} rdpt_s_report_t;

typedef struct _rdpt_fh {		/* frame head */
	unsigned		type	: 2;
	unsigned		len_qw	: RDPT_MAX_FRAME_BW;	/* with this head, unit of quadword */
	unsigned		chan	: RDPT_MAX_CHAN_BW;
	unsigned				: (32 - 2 - RDPT_MAX_CHAN_BW - RDPT_MAX_FRAME_BW);
	unsigned		crc;
} rdpt_fh_t;

typedef struct _rdpt_cf {		/* control frame */
	unsigned		type	: 8;
	unsigned				: 24;

	union {
		struct {
			unsigned		left	: 16;
			unsigned		right	: 16;
			unsigned		top		: 16;
			unsigned		bottom	: 16;
			unsigned		x		: 16;
			unsigned		y		: 16;
		} win_info;
		struct {
			unsigned		width	: 16;
			unsigned		height	: 16;
		} win_set;

		rdpt_s_report_t		report;

		int			chan;
	} u;
} rdpt_cf_t;

#if 0
/* server to client, rdpt frame buddle, encode by pic display */
#define	RDPT_FBH_MAGIC		(0x1324bdca)
typedef struct _rdpt_fbh {		/* frame buddle */
	unsigned		magic;
	unsigned		crc;
	unsigned		len;	/* with this head */
	unsigned		n_f;
} rdpt_fbh_t;
#endif

/* just contain one frame */
#define	RDPT_S2C_INITWIN_WIDTH			(64)
#define	RDPT_S2C_INITWIN_HEIGHT			(1)

/* client to server, encode by kbd event */
typedef struct _rdpt_event{
	unsigned short			type;
	unsigned short			flag;
	unsigned short			param0;
	unsigned short			param1;
} rdpt_event_t;

/*********************************************************/
typedef enum _rdpt_sockdata_type {
	RDPT_SOCKDATA_TYPE_SENDFILE_HEAD = 0,
	RDPT_SOCKDATA_TYPE_SENDFILE_DATA,
	RDPT_SOCKDATA_TYPE_SENDFILE_FIN,
	RDPT_SOCKDATA_TYPE_RECVFILE_HEAD,
	RDPT_SOCKDATA_TYPE_RECVFILE_REP,
	RDPT_SOCKDATA_TYPE_RECVFILE_DATA,
	RDPT_SOCKDATA_TYPE_RECVFILE_FIN,
	RDPT_SOCKDATA_TYPE_SOCKOPEN = 16,
	RDPT_SOCKDATA_TYPE_SOCKOPEN_REPLY,			//
	RDPT_SOCKDATA_TYPE_SOCKINFO,
	RDPT_SOCKDATA_EXT = 20,
	RDPT_SOCKDATA_USER = 32,
	RDPT_SOCKDATA_TYPE_COUNT
} rdpt_sockdata_type_t;

typedef struct _rdpt_sockdata {
	unsigned				type	: 6;
	unsigned				dw_len	: RDPT_MAX_FRAME_BW;
	unsigned				pt_len	: RDPT_MAX_FRAME_BW;			/* plain text length */
	unsigned				flag	: 32 - 6 - RDPT_MAX_FRAME_BW * 2;
} rdpt_sockdata_t;

#define	RDPT_SOCKDATA_MAX_PAYLOADLEN		(RDPT_MAX_FRAME_LEN - sizeof(rdpt_fh_t) - sizeof(rdpt_sockdata_t))

typedef struct _rdpt_filedesc {
	unsigned			file_len;	/* file len */
	unsigned			name_len;
	char				name[1];	/* file name */
} rdpt_filedesc_t;

#define	RDPT_AUTHSTR_MAXLEN				48
#define RDPT_URLSTR_MAXLEN				24
typedef struct _rdpt_sockopen {
	char				proxyurl[RDPT_URLSTR_MAXLEN];	/* if proxyurl == empystring, just connect desturl */
	char				auth[RDPT_AUTHSTR_MAXLEN];
	char				desturl[RDPT_URLSTR_MAXLEN];
} rdpt_sockopen_t;

typedef enum _rdpt_sockstate {
	RDPT_SOCKSTATE_SOCKOPEN_OK = 0,
	RDPT_SOCKSTATE_SOCKOPEN_TIMEOUT,
	RDPT_SOCKSTATE_SOCKOPEN_INVALID_URL,
	RDPT_SOCKSTATE_SOCKOPEN_GETHOSTBYNAME_FAILD,
	RDPT_SOCKSTATE_SOCKOPEN_SOCK_FAILD,
	RDPT_SOCKSTATE_SOCKOPEN_CONNECT_FAILD,
	RDPT_SOCKSTATE_SOCKOPEN_PROXYBUILD_FAILD,
	RDPT_SOCKSTATE_SOCKCLOSED,
	RDPT_SOCKSTATE_COUNT
} rdpt_sockdata_flag_e;

typedef struct _rdpt_sockopen_reply {
	int					ret;
} rdpt_sockopen_reply_t;

typedef struct _rdpt_sockinfo {
	int					state;
} rdpt_sockinfo_t;

#endif

