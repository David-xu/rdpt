#include <sys/types.h>
#include <sys/socket.h>		/* socket connect setsockopt */
#include <netinet/in.h>		/* sockaddr_in */
#include <netinet/tcp.h>	/* TCP_NODELAY */
#include <assert.h>

#include "rdesktop.h"

#ifdef ADDON_TUNNEL_ENABLE

#include <arpa/inet.h>
#include <netdb.h>
#include "rdpt_func.c"

static rdpt_protctx_t g_protctx;

/******************************************************************
 * rdpt frame lay
 ******************************************************************/

/* this is socket used by 'select' in the rdesktop main loop */
int g_rdpt_sock;

int inrecv_rect(int x, int y, int width, int height)
{
	x -= g_protctx.x;
	y -= g_protctx.y;

	if (((x >= g_protctx.left) && ((x + width) <= g_protctx.right)) &&
		((y >= g_protctx.top) && ((y + height) <= g_protctx.bottom)))
		return 1;

	return 0;
}

static int get_free_chan(rdpt_protctx_t *ctx)
{
	unsigned i;
	for (i = 0; i < RDPT_MAX_CHAN_NUM; i++)
	{
		if (ctx->chaninfo[i].sock == -1)
			return i;
	}

	return -1;
}

CODE_SECTION("send to s")

static int rdpt_sendq_init(void *param)
{
	int sockpair[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) < 0)
	{
		printf("build rdp socketpair faild.\n");
		return -1;
	}
	g_protctx.kbd_send = sockpair[0];
	g_rdpt_sock = sockpair[1];

	return 0;
}

static void kbd_inc_u8(rdpt_event_t *e, unsigned char v)
{
	if (v & 0x80)
		e->flag = RDP_KEYRELEASE;
	else
		e->flag = 0;

	if (v & 0x40)
		e->flag |= KBD_FLAG_EXT;

	e->type = RDP_INPUT_SCANCODE;
	e->param0 = g_incmap[v & 0x3f];
	e->param1 = 0;
}

/*  */
static void rdpt_send_frame(rdpt_fh_t *frame)
{
	rdpt_event_t e, sendbuf[RDPT_MAX_FRAME_LEN + 16];
	unsigned i, n = 0, len = frame->len_qw * 8;

	pthread_mutex_lock(&(g_protctx.send_lock));

	frame->crc = 0;
	frame->crc = crc32(frame, len);

	/* esc press */
	e.type = RDP_INPUT_SCANCODE;
	e.flag = RDP_KEYPRESS;
	e.param0 = 0x1;
	e.param1 = 0;
	sendbuf[n++] = e;

	/* encode data */
	for (i = 0; i < len; i++)
	{
		kbd_inc_u8(&e, ((unsigned char *)frame)[i]);
		sendbuf[n++] = e;
	}

	/* esc release */
	e.type = RDP_INPUT_SCANCODE;
	e.flag = RDP_KEYRELEASE;
	e.param0 = 0x1;
	e.param1 = 0;
	sendbuf[n++] = e;

	g_protctx.stat.send_to_s_fhlen += frame->len_qw * 8;
	send(g_protctx.kbd_send, sendbuf, sizeof(e) * n, 0);

	pthread_mutex_unlock(&(g_protctx.send_lock));
}

void rdpt_send_proc()
{
	int i, len;
	static rdpt_event_t ebuf[RDPT_MAXEVENT_PERPKT];

	assert(g_protctx.neperpkt <= RDPT_MAXEVENT_PERPKT);

	len = recv(g_rdpt_sock,
		((unsigned char *)ebuf),
		g_protctx.neperpkt * sizeof(rdpt_event_t),
		0);
	if (len <= 0)
	{
		close(g_rdpt_sock);
		printf("rdpt sock close.\n");
		g_rdpt_sock = 0;
		return;
	}

	assert((len % sizeof(rdpt_event_t)) == 0);
	len /= sizeof(rdpt_event_t);

	if (g_protctx.state == RDPT_C_STATE_RST)
	{
		/* don't send any data, the rdpt_s may has NOT been setup.\n */
		printf("state in rst, don't send any data, ignore %d bytes.\n", len);
		return;
	}

	STREAM s;
	s = rdp_init_data(4 + 12 * len);

	out_uint16_le(s, len);	/* number of events */
	out_uint16(s, 0);	/* pad */
	i = 0;
	while (len--)
	{
		out_uint32_le(s, time(NULL));
		out_uint16_le(s, ebuf[i].type);
		out_uint16_le(s, ebuf[i].flag);
		out_uint16_le(s, ebuf[i].param0);
		out_uint16_le(s, ebuf[i].param1);

		i++;
	}
	s_mark_end(s);

	rdp_send_data(s, RDP_DATA_PDU_INPUT);
	usleep(g_protctx.send_intv);
}

CODE_SECTION("recv from s")

static int rdpt_recvq_init(void *param)
{
	int sockpair[2];
	int optval;
	socklen_t optlen = sizeof(int);
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) < 0)
	{
		printf("build rdp socketpair faild.\n");
		return -1;
	}
	g_protctx.recv_sp_s = sockpair[0];
	g_protctx.recv_sp_d = sockpair[1];

	printf("recv sockpair:\n");
	getsockopt(g_protctx.recv_sp_s, SOL_SOCKET,
		SO_SNDBUF, &optval, &optlen);
	printf("send buf size: %d\n", optval);
	getsockopt(g_protctx.recv_sp_d, SOL_SOCKET,
		SO_RCVBUF, &optval, &optlen);
	printf("recv buf size: %d\n", optval);

	return 0;
}


static void rdpt_recv_reset(unsigned width, unsigned height)
{
	g_protctx.s2cpic_width = width;
	g_protctx.s2cpic_height = height;

	g_protctx.next_h = 0;
	g_protctx.padding_nh = 0;
	g_protctx.recv_need_reset = 1;
}

void rdpt_ctrlproc_c(rdpt_cf_t *cf)
{
	unsigned i;

	switch (cf->type)
	{
	case RDPT_CTRL_INFO:
	{
		if (g_protctx.state != RDPT_C_STATE_NEGO)
		{
			/* todo: */

		}
		else
		{
			g_protctx.left = cf->u.win_info.left;
			g_protctx.right = cf->u.win_info.right;
			g_protctx.top = cf->u.win_info.top;
			g_protctx.bottom = cf->u.win_info.bottom;
			g_protctx.x = cf->u.win_info.x;
			g_protctx.y = cf->u.win_info.y;

			printf("left: %d, right: %d, top: %d, bottom: %d, pos(%d, %d).\n",
				g_protctx.left, g_protctx.right,
				g_protctx.top, g_protctx.bottom,
				g_protctx.x, g_protctx.y);

			g_protctx.state = RDPT_C_STATE_NEGO_ACK;
		}
		break;
	}
	case RDPT_CTRL_SCLOSED:
	{
		//
		printf("rdpt_s closed, don't send data.\n");
		g_protctx.state = RDPT_C_STATE_RST;

		/* close all opened sd socket */
		for (i = 0; i < RDPT_MAX_CHAN_NUM; i++)
		{
			if (g_protctx.chaninfo[i].sock)
			{
				shutdown(g_protctx.chaninfo[i].sock, SHUT_RDWR);
				close(g_protctx.chaninfo[i].sock);
			}
		}
		memset(&(g_protctx.last), 0, sizeof(rdpt_s_report_t));

		break;
	}
	case RDPT_CTRL_S_REPORT:
	{
		rdpt_s_report_t *last = &(g_protctx.last);
		if (last->n_eku != cf->u.report.n_eku)
		{
			printf("err number of 'esc key up': %d\n", cf->u.report.n_eku - last->n_eku);
			last->n_eku = cf->u.report.n_eku;
		}

		if (last->n_ekd != cf->u.report.n_ekd)
		{
			printf("err number of 'esc key down': %d\n", cf->u.report.n_ekd - last->n_ekd);
			last->n_ekd = cf->u.report.n_ekd;
		}

		if (last->n_k != cf->u.report.n_k)
		{
			printf("err number of 'invalid key event': %d\n", cf->u.report.n_k - last->n_k);
			last->n_k = cf->u.report.n_k;
		}

		if (last->n_rce != cf->u.report.n_rce)
		{
			printf("err number of 'frame crc err': %d\n", cf->u.report.n_rce - last->n_rce);
			last->n_rce = cf->u.report.n_rce;
		}

		break;
	}
	default:
	{
		/* todo: */
		printf("known ctrl type: %d, discard.\n", cf->type);
		break;
	}
	}
}

static int recvproc_thread(void *param)
{
	unsigned total_proc = 0;
	int cur, len, sbp = 0;
	unsigned recv_crc, calc_crc;
	static char procbuf[16 * 1024 * 1024];

_retry:

	len = recv(g_protctx.recv_sp_d,
		procbuf + sbp,
		RDPT_S_SOCKBUFF_SIZE - sbp,
		0);
	if (len <= 0)
	{
		printf("recv sockpair closed.\n");
		return 0;
	}

	if (g_protctx.recv_need_reset)
	{
		if (sbp)
			memmove(procbuf,
				procbuf + sbp,
				len);
		sbp = 0;
		total_proc = 0;
		printf("recv reset.");
		g_protctx.recv_need_reset = 0;
	}

	g_protctx.stat.recv_from_s_fhlen_proc += len;

	// printf("recv_sp_d recv %d.\n", len);

	len += sbp;

	/* check crc */
	rdpt_fh_t *fh = (rdpt_fh_t *)procbuf;

	cur = 0;
	while (((fh->len_qw * 8) <= len) && (len != 0))
	{
#if 0
		printf("recvproc_thread: fh type: %d, len_qw: %d\n",
			fh->type, fh->len_qw);
#endif

		// CPKL_ASSERT(fh->len_qw != 0);

		recv_crc = fh->crc;
		fh->crc = 0;
		calc_crc = crc32(fh, fh->len_qw * 8);
		fh->crc = recv_crc;
		if (calc_crc != recv_crc)
		{
			/* todo: */
			unsigned line = total_proc / (g_protctx.s2cpic_width * 2);
			unsigned lineoff = total_proc % (g_protctx.s2cpic_width * 2);
			unsigned n_drop = g_protctx.s2cpic_width * 2 - lineoff;
			if (g_protctx.state != RDPT_C_STATE_RST)
			{
				printf("rdpt_recv() crc error(last left: %d, off: %d, recvbuf off: %d, line: %d(%d), lineoff: %d). 0x%08x != 0x%08x\n",
					sbp, total_proc, (unsigned)((char *)fh - procbuf),
					line, line % g_protctx.s2cpic_height, lineoff, calc_crc, recv_crc);

				cpkl_hexdump((char *)(fh) - lineoff,
					g_protctx.s2cpic_width * 2,
					"");
				printf("\n");
			}

			/* just drop this line */

			total_proc += n_drop;
			cur += n_drop;
			len -= n_drop;

			fh = CPKL_V2P(CPKL_P2V(fh) + n_drop);
			continue;
		}


		switch (fh->type)
		{
		case RDPT_FTYPE_PADDING:
		{
			/* just ignore */
			break;
		}
		case RDPT_FTYPE_CTRL:
		{
			rdpt_ctrlproc_c((rdpt_cf_t *)(fh + 1));

			break;
		}
		case RDPT_FTYPE_CD:
		{
#if 0
			printf("RDPT_FTYPE_CD, len: %d\n", fh->len_qw * 8);
			cpkl_hexdump(fh, fh->len_qw * 8, "");
			printf("\n");
#endif

			if (g_protctx.state != RDPT_C_STATE_OK)
			{
				printf("cd fwd faild, state err: %d\n", g_protctx.state);
			}
			else
			{
				rdpt_sockdata_t *sd = (rdpt_sockdata_t *)(fh + 1);
				CPKL_ASSERT((sd->dw_len * 4) <= ((fh->len_qw - 1) * 8));
				int chan = fh->chan, chansock;
				chansock = g_protctx.chaninfo[chan].sock;
				if (chansock > 0)
				{
#if 0
					printf("cd fwd, sd type: %d, sd len: %d\n",
						sd->type, sd->dw_len * 4);
#endif
					send(chansock, sd, sd->dw_len * 4, 0);
					g_protctx.stat.send_to_c_sdlen += sd->dw_len * 4;
				}
				else
				{
					printf("cd fwd faild, chan(%d) invalid sock: %d\n", chan, chansock);
				}
			}

			break;
		}
		default:
		{
			printf("recvproc_thread() unknown hf type: %d.\n", fh->type);
			break;
		}
		}

		total_proc += fh->len_qw * 8;
		cur += fh->len_qw * 8;
		len -= fh->len_qw * 8;

		fh += fh->len_qw;
	}

#if 0
	printf("total_proc: %d, cur: %d, len: %d, fh->len_qw * 8: %d\n",
		total_proc, cur, len, fh->len_qw * 8);
#endif

	if (len)
	{
		memmove(procbuf, procbuf + cur, len);
		sbp = len;
	}
	else
		sbp = 0;

	goto _retry;
}

// #define RDPT_RECV_DEBUG

void rdpt_recv(int x, int y, int cx, int cy, int width, int height, unsigned char * data)
{
	static char tmpbuf[16 * 1024 * 1024];

#ifdef RDPT_RECV_DEBUG
	int ignore = 0;
#endif

	if ((width == RDPT_S2C_INITWIN_WIDTH) &&
		(height == RDPT_S2C_INITWIN_HEIGHT))
	{
		// cpkl_hexdump(data, width * height * 2, "");
		// printf("\n");

		send(g_protctx.recv_sp_s, data, width * height * 2, 0);
		g_protctx.stat.recv_from_s_fhlen += width * height * 2;
#ifdef RDPT_RECV_DEBUG
		goto _ret;
#else
		return;
#endif
	}

	if (width != g_protctx.s2cpic_width)
	{
#ifdef RDPT_RECV_DEBUG
		printf("ignore == 1, g_protctx.s2cpic_width: %d\n", g_protctx.s2cpic_width);
		ignore = 1;
		goto _ret;
#else
		return;
#endif

	}
	else if (x != g_protctx.x)
	{
#ifdef RDPT_RECV_DEBUG
		ignore = 2;
		goto _ret;
#else
		return;
#endif

	}
	else if (!inrecv_rect(x, y, width, height))
	{
#ifdef RDPT_RECV_DEBUG
		ignore = 3;
		goto _ret;
#else
		return;
#endif
	}

	y -= g_protctx.y;
	int n_line = height, send_y = y, ret;

	while (n_line--)
	{
		/*  */
		if (g_protctx.next_h == send_y)
		{
			ret = send(g_protctx.recv_sp_s, data, width * 1 * 2, 0);
			if (ret <= 0)
			{
				printf("rdpt_recv() send faild, send_y: %d, line 1.\n", send_y);
			}
			else
				g_protctx.stat.recv_from_s_fhlen += width * 1 * 2;

			g_protctx.next_h = (g_protctx.next_h + 1) % g_protctx.s2cpic_height;

			if ((g_protctx.next_h == 0) && (g_protctx.padding_nh != 0))
			{
				ret = send(g_protctx.recv_sp_s, tmpbuf,
					g_protctx.s2cpic_width * g_protctx.padding_nh * 2, 0);
				if (ret <= 0)
				{
					printf("rdpt_recv() send faild, send_y: %d, line: %d.\n", 0, g_protctx.padding_nh);
				}
				else
					g_protctx.stat.recv_from_s_fhlen += g_protctx.s2cpic_width * g_protctx.padding_nh * 2;

				g_protctx.next_h = g_protctx.padding_nh;
				g_protctx.padding_nh = 0;
			}
		}
		else
		{
			// assert(send_y == g_protctx.padding_nh);
			/* just save this */
			memcpy(tmpbuf + g_protctx.padding_nh * g_protctx.s2cpic_width * 2,
				data, width * 1 * 2);

			g_protctx.padding_nh = g_protctx.padding_nh + 1;
		}
		data += width * 1 * 2;
		send_y++;
	}

#ifdef RDPT_RECV_DEBUG
_ret:
	printf("(%d)%d, %d, %d, %d, width: %d, height: %d\n",
		ignore, x, y, cx, cy, width, height);
#endif
}

/*
 * control command proc
 *****************************************************************/

CODE_SECTION("cli")

int rdpt_tunnel_nego()
{
	rdpt_recv_reset(RDPT_S2C_INITWIN_WIDTH, RDPT_S2C_INITWIN_HEIGHT);

	unsigned char sendbuf[256];
	rdpt_fh_t *fh = (rdpt_fh_t *)sendbuf;
	rdpt_cf_t *cf = (rdpt_cf_t *)(fh + 1);

	cf->type = RDPT_CTRL_INIT;
	cf->u.win_set.width = RDPT_S2C_INITWIN_WIDTH;
	cf->u.win_set.height = RDPT_S2C_INITWIN_HEIGHT;

	fh->type = RDPT_FTYPE_CTRL;
	fh->len_qw = RDPT_FRAMELEN_QW(sizeof(rdpt_fh_t) + sizeof(rdpt_cf_t));

	g_protctx.state = RDPT_C_STATE_NEGO;

	rdpt_send_frame(fh);

	unsigned retry = 20;
	/* waiting for reply */
	do
	{
		if (g_protctx.state != RDPT_C_STATE_NEGO_ACK)
		{
			usleep(100000);
		}
		else
			goto _OK;
	} while (retry--);
	/* timeout */
	printf("tunnel nego timeout");
	g_protctx.state = RDPT_C_STATE_RST;
	return -1;

	/*  */
_OK:
	/* we can modify the window size */

#if 1
	rdpt_recv_reset(g_protctx.right - g_protctx.left,
		g_protctx.bottom - g_protctx.top);
#else
	rdpt_recv_reset(512, 512);
#endif
	g_protctx.state = RDPT_C_STATE_OK;

	/* send confirm frame */
	cf->type = RDPT_CTRL_CNFM;
	cf->u.win_set.width = g_protctx.s2cpic_width;
	cf->u.win_set.height = g_protctx.s2cpic_height;

	fh->type = RDPT_FTYPE_CTRL;
	fh->len_qw = RDPT_FRAMELEN_QW(sizeof(rdpt_fh_t) + sizeof(rdpt_cf_t));
	rdpt_send_frame(fh);

	printf("tunnel nego done, win(%d x %d).\n",
		g_protctx.s2cpic_width, g_protctx.s2cpic_height);

	return 0;
}

static int ctrl_cli(void *param)
{
	#define CMDLINE_MAXLEN			256
	static char linebuf[CMDLINE_MAXLEN];
	char *subargv[16];
	unsigned len[16], n_arg;

_rep:
	cpkl_printf("\ntunnel_cli >");

	cpkl_pdf_memset(linebuf, 0, CMDLINE_MAXLEN);
	gets(linebuf);
	n_arg = cpkl_stdiv(linebuf,
		(int)cpkl_pdf_strlen(linebuf),
		16,
		subargv,
		len,
		1,
		" ",
		0);
	if (n_arg == 0)
		goto _rep;

	if (cpkl_pdf_memcmp(subargv[0], "init", 4) == 0)
	{
		rdpt_tunnel_nego();
	}
	else if (cpkl_pdf_memcmp(subargv[0], "setp",  4) == 0)
	{
		char *intv;
		unsigned old;
		intv = withparam(subargv, n_arg, "-intv=");
		if (intv)
		{
			old = g_protctx.send_intv;
			cpkl_pdf_sscanf(intv, "%d", &(g_protctx.send_intv));
			cpkl_printf("send_intv: %d --> %d.\n",
				old, g_protctx.send_intv);
		}

		char *npp;
		npp = withparam(subargv, n_arg, "-npp=");
		if (npp)
		{
			old = g_protctx.neperpkt;
			cpkl_pdf_sscanf(npp, "%d", &(g_protctx.neperpkt));
			cpkl_printf("number of event per pkt : %d --> %d.\n",
				old, g_protctx.neperpkt);
		}
	}
	else if (cpkl_pdf_memcmp(subargv[0], "stat",  4) == 0)
	{
		printf(
			"padding_nh: %d\n"
			"c <--> s:\n"
			"recv_from_s_fhlen: %d\n"
			"recv_from_s_fhlen_proc: %d\n"
			"send_to_s_fhlen: %d\n"
			"user <--> c:\n"
			"send_to_c_sdlen: %d\n"
			,
			g_protctx.padding_nh,
			g_protctx.stat.recv_from_s_fhlen,
			g_protctx.stat.recv_from_s_fhlen_proc,
			g_protctx.stat.send_to_s_fhlen,
			g_protctx.stat.send_to_c_sdlen
			);
		printf("report form s:\n"
			"err number of 'esc key up': %d\n"
			"err number of 'esc key down': %d\n"
			"err number of 'invalid key event': %d\n"
			"err number of 'frame crc err': %d\n",
			g_protctx.last.n_eku,
			g_protctx.last.n_ekd,
			g_protctx.last.n_k,
			g_protctx.last.n_rce);

		if (n_arg == 2)
		{
			if (cpkl_pdf_memcmp(subargv[1], "-r",  2) == 0)
				memset(&g_protctx.stat, 0, sizeof(g_protctx.stat));
		}
	}

	goto _rep;

	return 0;
}

CODE_SECTION("comm with user sockets")

void rdpt_chan_bind(unsigned chan)
{
	unsigned char sendbuf[256];
	rdpt_fh_t *fh = (rdpt_fh_t *)sendbuf;
	rdpt_cf_t *cf = (rdpt_cf_t *)(fh + 1);

	/* send channel bind */
	cf->type = RDPT_CTRL_CHANBIND;
	cf->u.chan = chan;

	fh->type = RDPT_FTYPE_CTRL;
	fh->len_qw = RDPT_FRAMELEN_QW(sizeof(rdpt_fh_t) + sizeof(rdpt_cf_t));

	rdpt_send_frame(fh);
}

void rdpt_chan_release(unsigned chan)
{
	unsigned char sendbuf[256];
	rdpt_fh_t *fh = (rdpt_fh_t *)sendbuf;
	rdpt_cf_t *cf = (rdpt_cf_t *)(fh + 1);

	/* send channel release */
	cf->type = RDPT_CTRL_CHANRELEASE;
	cf->u.chan = chan;

	fh->type = RDPT_FTYPE_CTRL;
	fh->len_qw = RDPT_FRAMELEN_QW(sizeof(rdpt_fh_t) + sizeof(rdpt_cf_t));

	rdpt_send_frame(fh);

	/* connect closed */
	g_protctx.chaninfo[chan].sock = -1;
}

static int sd_recv_thread(void *param)
{
	int cur, left, sbp, chan = CPKL_P2V(param);
	rdpt_sockdata_t *sd;
	unsigned char sendbuff[RDPT_MAX_FRAME_LEN];
	rdpt_fh_t *fh = (rdpt_fh_t *)sendbuff;

	sbp = 0;
_next:
	left = recv(g_protctx.chaninfo[chan].sock,
			g_protctx.chaninfo[chan].recvbuf + sbp,
			RDPT_C_SOCKBUFF_SIZE - sbp,
			0);
	if (left <= 0)
	{
		printf("recv return %d\n", left);
		goto _ret;
	}

	left += sbp;
	cur = 0;
	sd = (rdpt_sockdata_t *)g_protctx.chaninfo[chan].recvbuf;

	while ((left >= (sd->dw_len * 4)) && (left >= sizeof(rdpt_sockdata_t)))
	{
		/* fwd */
		fh->type = RDPT_FTYPE_CD;
		fh->chan = chan;
		fh->len_qw = RDPT_FRAMELEN_QW(sd->dw_len * 4 + sizeof(rdpt_fh_t));
		memcpy(fh + 1, sd, sd->dw_len * 4);
		// printf("chan %d send %d\n", fh->chan, fh->len_qw * 8);
		rdpt_send_frame(fh);

		cur += sd->dw_len * 4;
		left -= sd->dw_len * 4;
		sd += sd->dw_len;
	}

	if (left)
	{
		memmove(g_protctx.chaninfo[chan].recvbuf,
			g_protctx.chaninfo[chan].recvbuf + cur, left);
		sbp = left;
	}
	else
	{
		sbp = 0;
	}

	goto _next;

_ret:
	rdpt_chan_release(chan);

	printf("chan disconnected, chan: %d.\n", chan);

	return 0;

}

static int data_sock_serv(void *param)
{
	/* tcp server */
	g_protctx.serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (g_protctx.serv_sock < 0)
	{
		printf("rdpt serv sock open faild.\n");
		exit(0);
	}

	/*  */
	struct sockaddr_in srv_addr;
	srv_addr.sin_family = PF_INET;
	srv_addr.sin_port = htons(RDPT_SERV_PORT);
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(g_protctx.serv_sock, (struct sockaddr *)(&srv_addr), sizeof(srv_addr)) < 0)
	{
		printf("bind failed.\n");
		exit(0);
	}
	listen(g_protctx.serv_sock, 10);

	while (1)
	{
		/* get free channel */
		int chan = get_free_chan(&g_protctx);
		if (chan == -1)
		{
			usleep(10000);
			continue;
		}
		socklen_t len = sizeof(struct sockaddr_in);
		g_protctx.chaninfo[chan].sock = accept(g_protctx.serv_sock, (struct sockaddr *)(&srv_addr), &len);

		//
		rdpt_chan_bind(chan);

		printf("new chan connected, chan: %d.\n", chan);

		cpkl_tpinsert(sd_recv_thread, CPKL_V2P(chan), NULL);
	}

	return 0;
}

/*****************************************************************/

void rdpt_init()
{
	unsigned i;
	for (i = 0; i < RDPT_MAX_CHAN_NUM; i++)
	{
		g_protctx.chaninfo[i].sock = -1;
		g_protctx.chaninfo[i].recvbuf = (char *)malloc(RDPT_C_SOCKBUFF_SIZE);
	}

	g_protctx.send_intv = 5000;
	g_protctx.neperpkt = 64;

	cpkl_tmlkinit(100);

	pthread_mutex_init(&(g_protctx.send_lock), NULL);
	rdpt_sendq_init(NULL);
	rdpt_recvq_init(NULL);
	rdpt_recv_reset(RDPT_S2C_INITWIN_WIDTH, RDPT_S2C_INITWIN_HEIGHT);

	cpkl_tpstart(RDPT_MAX_CHAN_NUM + 4);
	cpkl_tpinsert(ctrl_cli, NULL, NULL);
	cpkl_tpinsert(data_sock_serv, NULL, NULL);
	cpkl_tpinsert(recvproc_thread, NULL, NULL);
}


#endif
