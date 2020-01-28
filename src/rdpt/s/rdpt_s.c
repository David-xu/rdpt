//+---------------------------------------------------------------------------
//
//  HELLO_WIN.C - Windows GUI 'Hello World!' Example
//
//+---------------------------------------------------------------------------

// #include <winsock2.h>
#include <Windows.h>
#include <stdio.h>
#include <time.h>
#include "rdpt_prot.h"
#include "rdpt_s.h"

#include "rdpt_func.c"

#ifdef WIN32
#pragma comment (lib, "ws2_32.lib")
#endif

rdpt_ctx_t	g_rdpt_ctx;

#ifdef RDPT_VC_DEBUG
int rdpt_fhproc_s(rdpt_fh_t *fh);
int eg_setwin(int width, int height);
int buf_insert(frame_cycbuf_t *cycbuf, rdpt_fh_t *fh);
void testfunc()
{
	unsigned char buf[256];
	memset(buf, 0, sizeof(buf));
	rdpt_fh_t *fh = (rdpt_fh_t *)buf;
	rdpt_cf_t *cf = (rdpt_cf_t *)(fh + 1);

	rdpt_sockdata_t *sd = (rdpt_sockdata_t *)(fh + 1);
	rdpt_filedesc_t *fd = (rdpt_filedesc_t *)(sd + 1);

#if 1
	static unsigned count = 2;
	memset(buf, 0x22, sizeof(buf));
	fh->len_qw = count++;
	buf_insert(&(g_rdpt_ctx.sendframe), fh);
#else
	cf->type = RDPT_CTRL_INIT;
	cf->u.win_set.width = RDPT_S2C_INITWIN_WIDTH;
	cf->u.win_set.height = RDPT_S2C_INITWIN_HEIGHT;

	fh->type = RDPT_FTYPE_CTRL;
	fh->len_qw = RDPT_FRAMELEN_QW(sizeof(rdpt_fh_t) + sd->dw_len * 4);
	fh->chan = 0;
#endif

	rdpt_fhproc_s(fh);
}

void testfunc_01()
{
	unsigned char buf[256];
	memset(buf, 0, sizeof(buf));
	rdpt_fh_t *fh = (rdpt_fh_t *)buf;
	rdpt_cf_t *cf = (rdpt_cf_t *)(fh + 1);

	cf->type = RDPT_CTRL_INIT;
	cf->u.win_set.width = RDPT_S2C_INITWIN_WIDTH;
	cf->u.win_set.height = RDPT_S2C_INITWIN_HEIGHT;

	fh->type = RDPT_FTYPE_CTRL;
	fh->len_qw = RDPT_FRAMELEN_QW(sizeof(rdpt_fh_t) + sizeof(rdpt_cf_t));
	fh->chan = 0;
	rdpt_fhproc_s(fh);
}

#endif

CODE_SECTION("rdpt frame recv from c")

static int kbd_dec(unsigned flag, unsigned scancode, unsigned char *ret)
{
	unsigned char v;
	if (g_rdpt_ctx.kbd_decmap[scancode] == -1)
		return -1;
	if (g_rdpt_ctx.kbd_decmap[scancode] >= 0x40)
		return -2;

	v = (unsigned char)g_rdpt_ctx.kbd_decmap[scancode];
	if (flag & 0x80)
		v |= 0x80;
	if (flag & 0x01)
		v |= 0x40;

	*ret = v;
	return 0;
}

static int g_rdpt_state = RDPT_STATE_IDLE;
static unsigned g_rdpt_len = 0;
static unsigned char g_rdpt_igbuff[4096];

int blkbuf_eq(frame_cycblkbuf_t *cycbuf, void *buf, unsigned len)
{
	rdpt_fh_t *fh = (rdpt_fh_t *)buf;
	if (len > RDPT_MAX_FRAME_LEN)
		return -1;

	if (len == 0)
		len = len;

	if (((cycbuf->b + 1) % CYCBUF_MAXBUF) == cycbuf->e)
	{
		RDPT_DEBUG("cycle buffer full.\n");
		return -3;
	}

	memcpy(cycbuf->buf[cycbuf->b], buf, len);
	cycbuf->len[cycbuf->b] = len;
	cycbuf->b = (cycbuf->b + 1) % CYCBUF_MAXBUF;

	return 0;
}


LRESULT CALLBACK kbd_hook(
  int nCode,
  WPARAM wParam,
  LPARAM lParam
)
{
	KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

	switch (p->scanCode)
	{
	/* esc */
	case 0x1:
	{
		/* key up */
		if (p->flags & 0x80)
		{
			if (g_rdpt_state == RDPT_STATE_DATA)
			{
				/* frame finish */
				if (g_rdpt_len)
				{
					blkbuf_eq(&(g_rdpt_ctx.recvframe),
						g_rdpt_igbuff,
						g_rdpt_len);
				}
			}
			else
			{
				/* some error */
				RDPT_DEBUG("state err(%d): esc key up\n", g_rdpt_state);

				g_rdpt_ctx.report.n_eku++;
			}
			g_rdpt_state = RDPT_STATE_IDLE;
			g_rdpt_len = 0;
		}
		/* key down */
		else
		{
			if (g_rdpt_state != RDPT_STATE_IDLE)
			{
				/* some error */
				RDPT_DEBUG("state err(%d): esc key down\n", g_rdpt_state);

				g_rdpt_ctx.report.n_ekd++;
			}
			g_rdpt_state = RDPT_STATE_DATA;
			g_rdpt_len = 0;
		}
		break;
	}
#ifdef RDPT_VC_DEBUG
	case 0x1c:
	{
		/* key down */
		if (p->flags & 0x80)
		{

		}
		else
		{
			testfunc();
		}

		break;
	}
#endif
	default:
	{
		unsigned char v;
		if ((kbd_dec(p->flags, p->scanCode, &v) == 0) &&
			(g_rdpt_state == RDPT_STATE_DATA))
		{
			g_rdpt_igbuff[g_rdpt_len++] = v;
		}
		else
		{
			/* just ignore this kbd event */
			g_rdpt_ctx.report.n_k++;
		}
		break;
	}
	}

	// return CallNextHookEx(NULL, nCode, wParam, lParam);
	return 1;	//
}

void rdpt_ctrlproc_s(rdpt_cf_t *cf)
{
	unsigned sendbuf[64];
	rdpt_fh_t *send_h = (rdpt_fh_t *)sendbuf;
	rdpt_cf_t *send_cf = (rdpt_cf_t *)(send_h + 1);

	switch (cf->type)
	{
	case RDPT_CTRL_INIT:
	{
		eg_setwin(cf->u.win_set.width, cf->u.win_set.height);

		send_cf->type = RDPT_CTRL_INFO;
		send_cf->u.win_info.left = g_rdpt_ctx.client_rect.left;
		send_cf->u.win_info.right = g_rdpt_ctx.client_rect.right;
		send_cf->u.win_info.top = g_rdpt_ctx.client_rect.top;
		send_cf->u.win_info.bottom = g_rdpt_ctx.client_rect.bottom;
		send_cf->u.win_info.x = g_rdpt_ctx.client_pos.x;
		send_cf->u.win_info.y = g_rdpt_ctx.client_pos.y;

		RDPT_DEBUG("nego info send: (%d x %d) at (%d, %d)\n",
			g_rdpt_ctx.client_rect.right - g_rdpt_ctx.client_rect.left,
			g_rdpt_ctx.client_rect.bottom - g_rdpt_ctx.client_rect.top,
			send_cf->u.win_info.x, send_cf->u.win_info.y);

		send_h->type = RDPT_FTYPE_CTRL;
		send_h->len_qw = RDPT_FRAMELEN_QW(sizeof(rdpt_fh_t) + sizeof(rdpt_cf_t));

		buf_insert(&(g_rdpt_ctx.sendframe), send_h);
		break;
	}
	case RDPT_CTRL_CNFM:
	{
		eg_setwin(cf->u.win_set.width, cf->u.win_set.height);

		g_rdpt_ctx.state = RDPT_S_STATE_OK;

		RDPT_DEBUG("nego done: (%d x %d)\n",
			g_rdpt_ctx.eg_width, g_rdpt_ctx.eg_height);

		break;
	}
	case RDPT_CTRL_CHANBIND:
	{
		unsigned chan = cf->u.chan;

		channel_ctx_reset(&(g_rdpt_ctx.chanlist[chan]));

		g_rdpt_ctx.chanlist[chan].inuse = 1;
		break;
	}
	case RDPT_CTRL_CHANRELEASE:
	{
		unsigned chan = cf->u.chan;

		channel_ctx_reset(&(g_rdpt_ctx.chanlist[chan]));

		break;
	}
	default:
	{
		/* todo: */
		break;
	}
	}
}

int rdpt_fhproc_s(rdpt_fh_t *fh)
{
	/* check crc */
	unsigned crc_calc, crc_recv = fh->crc;
	fh->crc = 0;
	crc_calc = crc32(fh, fh->len_qw * RDPT_FRAMELEN_ALIGN);
#ifndef RDPT_VC_DEBUG
	if (crc_recv != crc_calc)
	{
		RDPT_DEBUG("frame crc error, discard.\n");
		g_rdpt_ctx.report.n_rce++;
		return -1;
	}
#endif

#if 0
	RDPT_DEBUG("rdpt_fhproc_s(), type: %d, chan: %d, len: %d.\n",
		fh->type, fh->chan, fh->len_qw * 8);
#endif
	switch (fh->type)
	{
	case RDPT_FTYPE_CTRL:
	{
		rdpt_cf_t *cf = (rdpt_cf_t *)(fh + 1);

		rdpt_ctrlproc_s(cf);

		break;
	}
	case RDPT_FTYPE_CD:
	{
		unsigned chan = fh->chan;

		channel_recv(&(g_rdpt_ctx.chanlist[chan]), (rdpt_sockdata_t *)(fh + 1));

		break;
	}
	}

	return 0;
}

static DWORD WINAPI recv_from_c(LPVOID param)
{
	while (1)
	{
		/* proc new frame */
		while (g_rdpt_ctx.recvframe.b !=
			g_rdpt_ctx.recvframe.e)
		{
			rdpt_fhproc_s((rdpt_fh_t *)(g_rdpt_ctx.recvframe.buf[g_rdpt_ctx.recvframe.e]));

			g_rdpt_ctx.recvframe.e = (g_rdpt_ctx.recvframe.e + 1) % CYCBUF_MAXBUF;
		}
		Sleep(1);
	}
}

CODE_SECTION("rdpt frame send to c")

int buf_insert(frame_cycbuf_t *cycbuf, rdpt_fh_t *fh)
{
	size_t left, len = fh->len_qw * 8;
	unsigned long long mid;
	unsigned b_off;

	// RDPT_DEBUG("buf_insert(), %d bytes;\n", len);
	if (((cycbuf->b + len) - cycbuf->e) > g_rdpt_ctx.pic_sz)
		return -1;

	//
	fh->crc = 0;
	fh->crc = crc32(fh, len);

	WaitForSingleObject(cycbuf->ins_lock, INFINITE);
	if (((cycbuf->b + len) - cycbuf->e) > g_rdpt_ctx.pic_sz)
	{
		ReleaseMutex(cycbuf->ins_lock);
		return -1;
	}

	mid = ((cycbuf->b / g_rdpt_ctx.pic_sz) + 1) * g_rdpt_ctx.pic_sz;

	b_off = cycbuf->b % g_rdpt_ctx.pic_sz;

	memcpy(cycbuf->buf + b_off, fh,
		(size_t)(mid - cycbuf->b) < len ? (size_t)(mid - cycbuf->b) : len);

	if ((mid - cycbuf->b) < len)
	{
		left = (size_t)(len - (mid - cycbuf->b));
		memcpy(cycbuf->buf, ((char *)fh) + len - left, left);
	}
	cycbuf->b += len;

	ReleaseMutex(cycbuf->ins_lock);
	return 0;
}

/* return cycbuf' begin position valud */
void buf_padding(frame_cycbuf_t *cycbuf)
{
	rdpt_fh_t *fh;
	unsigned off, left;

	if (cycbuf->b % g_rdpt_ctx.lw_b)
	{
		/* padding */
		off = cycbuf->b % g_rdpt_ctx.pic_sz;
		left = g_rdpt_ctx.lw_b - (off % g_rdpt_ctx.lw_b);
		fh = (rdpt_fh_t *)(cycbuf->buf + off);
		memset(fh, 0, left);
		fh->type = RDPT_FTYPE_PADDING;
		fh->len_qw = RDPT_FRAMELEN_QW(left);
		fh->crc = crc32(fh, fh->len_qw * 8);

		cycbuf->b += fh->len_qw * 8;
		RDPT_ASSERT((cycbuf->b % g_rdpt_ctx.lw_b) == 0);
	}
}

int eg_emitpic
(
	HDC hdc,
	void *buf,
	unsigned width,
	unsigned height,
	unsigned y_pos
)
{
	SYSTEMTIME tm;

	GetSystemTime(&tm);
	RDPT_DEBUG("(%6d:%3d)eg_emitpic() y_pos: %d, height: %d\n",
		tm.wSecond, tm.wMilliseconds,
		y_pos, height);

	char bmibuf[sizeof(BITMAPINFO) + 3 * sizeof(unsigned)];
	BITMAPINFO *bmi = (BITMAPINFO *)bmibuf;

	bmi = (BITMAPINFO *)bmibuf;
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biBitCount = 16;
	bmi->bmiHeader.biWidth = width;
	bmi->bmiHeader.biHeight = height;
	bmi->bmiHeader.biSizeImage = width * height * 2;
	bmi->bmiHeader.biCompression = BI_BITFIELDS;
	bmi->bmiHeader.biPlanes = 1;
	((unsigned *)(bmi->bmiColors))[0] = 0x0000f800;
	((unsigned *)(bmi->bmiColors))[1] = 0x000007e0;
	((unsigned *)(bmi->bmiColors))[2] = 0x0000001f;

	/*  */
	unsigned lw_b = width * 2;
	char *p1, *p2;
	static char tmpbuf[16 * 1024];
	for (unsigned i = 0; i < (height / 2); i++)
	{
		/* i: line number */
		p1 = (char *)buf + i * lw_b;
		p2 = (char *)buf + (height - i - 1) * lw_b;
		memcpy(tmpbuf, p1, lw_b);
		memcpy(p1, p2, lw_b);
		memcpy(p2, tmpbuf, lw_b);
	}

	SetDIBitsToDevice(hdc,
		0, y_pos, width, height,
		0, 0, 0, height,
		buf,
		bmi,
		DIB_RGB_COLORS);

	return 0;
}

int paint_proc
(
	HWND hwnd,
	unsigned width,
	unsigned height,
	unsigned y_pos)
{

	/* invalid */
	const RECT rect = {0, y_pos, width, y_pos + height};
	InvalidateRect(hwnd, &rect, FALSE);

	/* paint */
	HDC         hdc;
	PAINTSTRUCT PS;
	hdc = BeginPaint(hwnd, &PS);
	if (hdc == NULL)
	{
		RDPT_DEBUG("BeginPaint() faild.\n");
		return 0;
	}

	/*  */
	eg_emitpic(hdc,
		g_rdpt_ctx.sendframe.buf + y_pos * g_rdpt_ctx.lw_b,
		width, height, y_pos);

	EndPaint(hwnd, &PS);

	return 0;
}

int eg_setwin(int width, int height)
{
	g_rdpt_ctx.send_quota = height;
	g_rdpt_ctx.eg_width = width;
	g_rdpt_ctx.eg_height = height;
	g_rdpt_ctx.lw_b = width * 2;
	g_rdpt_ctx.pic_sz = g_rdpt_ctx.lw_b * height;

#ifndef RDPT_VC_DEBUG
	g_rdpt_ctx.sendframe.b = g_rdpt_ctx.sendframe.e = 0;
#else
	if (g_rdpt_ctx.eg_height > 4)
	{
		g_rdpt_ctx.sendframe.b = g_rdpt_ctx.sendframe.e =
		g_rdpt_ctx.pic_sz - g_rdpt_ctx.lw_b * 2;
	}
	else
	{
		g_rdpt_ctx.sendframe.b = g_rdpt_ctx.sendframe.e = 0;
	}
#endif

	RDPT_DEBUG("reset send buffer: (%d x %d)\n", width, height);

	return 0;
}

#if 0
static VOID CALLBACK frame_send2c
(
	PVOID		lpParameter,
	BOOLEAN		TimerOrWaitFired
)
#endif

static void CALLBACK frame_send2c(
  HWND     hwnd,
  UINT     uMsg,
  UINT_PTR idEvent,
  DWORD    dwTime
)
{
	unsigned long long hmid, hb, he;
	unsigned hb_off, he_off;
	unsigned n_line;

	// rdpt_ctx_t *ctx = (rdpt_ctx_t *)lpParameter;
	rdpt_ctx_t *ctx = &g_rdpt_ctx;
	ctx->send_quota += 20;

	if (ctx->send_quota > ctx->eg_height)
		ctx->send_quota = ctx->eg_height;

	if (ctx->sendframe.b == ctx->sendframe.e)
		return;

	RDPT_ASSERT((ctx->sendframe.e % ctx->lw_b) == 0);
	he = ctx->sendframe.e / ctx->lw_b;

	/* get number of lines to send this time */
	WaitForSingleObject(ctx->sendframe.ins_lock, INFINITE);
_retry:
	hb = ctx->sendframe.b / ctx->lw_b;
	n_line = (unsigned)(hb - he);

	if (n_line == 0)
	{
		/* only one line, need padding */
		buf_padding(&(ctx->sendframe));
		goto _retry;
	}
	ReleaseMutex(ctx->sendframe.ins_lock);

	if (n_line > ctx->send_quota)
		n_line = ctx->send_quota;

	ctx->send_quota -= n_line;

	hb = he + n_line;
	RDPT_ASSERT(hb != he);

	hmid = ((he / ctx->eg_height) + 1) * ctx->eg_height;

	hb_off = hb % ctx->eg_height;
	he_off = he % ctx->eg_height;

	paint_proc(ctx->hwnd,
		ctx->eg_width,
		(unsigned)(hmid > hb ? (hb - he) : (hmid - he)),
		he_off);

	if (hmid < hb)
	{
		paint_proc(ctx->hwnd,
			ctx->eg_width,
			(unsigned)(hb - hmid),
			0);
	}

	ctx->sendframe.e = hb * ctx->lw_b;
}

CODE_SECTION("SD frame proc")

static int proxy_connect(rdpt_chanctx_t *ctx, char *auth, char *url)
{
	const unsigned buflen = 8 * 1024;
	char *buf = (char *)malloc(8 * 1024);
	int len, ret = 0;
	/* send http proxy connect request */
	len = sprintf_s(buf, buflen,
		"CONNECT %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Proxy-Connection: Keep-Alive\r\n"
		"Proxy-Authorization: Basic %s\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		url, url, auth);
	send(ctx->sockdesc.sock, buf, len, 0);

	/* receive respond */
	memset(buf, 0, buflen);
	len = recv(ctx->sockdesc.sock, buf, buflen, 0);
	if (len <= 0)
	{
		ret = -1;
		goto _ret;
	}

	/* check ok
	 * HTTP/1.1 200 Connection Estab
	*/
	char *argv[3];
	unsigned sublen[3];
	stdiv(buf, len, 3, argv, sublen, 3, " \n\t", 0);
	if (memcmp(argv[1], "200", 3) != 0)
	{
		ret = -2;
		goto _ret;
	}

_ret:
	free(buf);
	return ret;
}

static DWORD WINAPI sockrecv_thread(LPVOID param)
{
	#define SOCKRECV_BUFSIZE				(8 * 1024)
	rdpt_chanctx_t *ctx = (rdpt_chanctx_t *)param;
	char *p, *raw_buf = (char *)malloc(SOCKRECV_BUFSIZE + 32);
	int len;
	unsigned cur, left, sbp = 0;
	rdpt_sockdata_t * sd;

	p = raw_buf + 32;

_next:
	len = recv(ctx->sockdesc.sock, p + sbp, SOCKRECV_BUFSIZE - sbp, 0);
	if (len <= 0)
	{
		/* socket closed */
		goto _ret;
	}
	left = len + sbp;

	cur = 0;
	sd = (rdpt_sockdata_t *)p;
	while ((left >= (sd->dw_len * 4)) && (left >= sizeof(rdpt_sockdata_t)))
	{
		channel_send(ctx, sd, sd->dw_len * 4);

		if (sd->dw_len * 4 >= 4096)
		{
			RDPT_DEBUG("sockrecv_thread(): recv sd len: %d\n", sd->dw_len * 4);
		}

		cur += sd->dw_len * 4;
		left -= sd->dw_len * 4;
		sd += sd->dw_len;
	}

	if (left)
	{
		sbp = left;
		memmove(p, p + cur, left);
	}
	else
		sbp = 0;

	goto _next;

_ret:
	free(raw_buf);
	ctx->sockdesc.h_rt = 0;

	return 0;
}

static void proxysockclose(rdpt_chanctx_t *ctx)
{
	if (ctx->sockdesc.sock)
	{
		shutdown(ctx->sockdesc.sock, 2);		/*SD_BOTH:2*/
		closesocket(ctx->sockdesc.sock);

		ctx->sockdesc.sock = 0;

		RDPT_DEBUG("proxy socked closed.\n");
	}
}

/* channel */
int channel_ctx_reset(rdpt_chanctx_t *ctx)
{
	unsigned char chan = ctx->chan;

	proxysockclose(ctx);

	memset(ctx, 0, sizeof(rdpt_chanctx_t));
	ctx->chan = chan;

	return 0;
}

int channel_send(rdpt_chanctx_t *ctx, void *buf, unsigned length)
{
	rdpt_fh_t *send_fh = (rdpt_fh_t *)buf;
	send_fh--;

	send_fh->type = RDPT_FTYPE_CD;
	send_fh->chan = ctx->chan;
	send_fh->len_qw = RDPT_FRAMELEN_QW(length + sizeof(rdpt_fh_t));
#if 0
	return buf_insert(&(g_rdpt_ctx.sendframe), send_fh);
#else
_retry:
	if (buf_insert(&(g_rdpt_ctx.sendframe), send_fh))
	{
		Sleep(1);
		goto _retry;
	}
	return 0;
#endif
}

int channel_recv(rdpt_chanctx_t *ctx, rdpt_sockdata_t *sd)
{
	char tmp_sendbuf[RDPT_MAX_FRAME_LEN + 32];

	switch (sd->type)
	{
	case RDPT_SOCKDATA_TYPE_SENDFILE_HEAD:
	{
		if (ctx->recvfile_ctx.state != RFS_IDLE)
		{
			return -1;
		}
		rdpt_filedesc_t *subh = (rdpt_filedesc_t *)(sd + 1);

		memset(&(ctx->recvfile_ctx), 0, sizeof(ctx->recvfile_ctx));
		memcpy(ctx->recvfile_ctx.name, subh->name, subh->name_len);
		ctx->recvfile_ctx.len = subh->file_len;
		ctx->recvfile_ctx.recv_len = 0;
		ctx->recvfile_ctx.buf = (char *)malloc(ctx->recvfile_ctx.len);

		RDPT_DEBUG("receive file %s begin...\n", ctx->recvfile_ctx.name);
		ctx->recvfile_ctx.state = RFS_DATA;

		break;
	}
	case RDPT_SOCKDATA_TYPE_SENDFILE_DATA:
	{
		if (ctx->recvfile_ctx.state != RFS_DATA)
		{
			return -1;
		}
		memcpy(ctx->recvfile_ctx.buf + ctx->recvfile_ctx.recv_len,
			sd + 1, sd->dw_len * 4 - sizeof(rdpt_sockdata_t));

		ctx->recvfile_ctx.recv_len += sd->dw_len * 4 - sizeof(rdpt_sockdata_t);
		RDPT_DEBUG("\rreceive file %d", ctx->recvfile_ctx.recv_len);

		break;
	}
	case RDPT_SOCKDATA_TYPE_SENDFILE_FIN:
	{
		if (ctx->recvfile_ctx.state != RFS_DATA)
		{
			return -1;
		}

		unsigned crc, recv_crc = *((unsigned *)(sd + 1));

		/* check len */
		if (ctx->recvfile_ctx.recv_len < ctx->recvfile_ctx.len)
		{
			RDPT_DEBUG("received no enough file, %d(should %d)\n",
				ctx->recvfile_ctx.recv_len,
				ctx->recvfile_ctx.len);
			free(ctx->recvfile_ctx.buf);
			ctx->recvfile_ctx.state = RFS_IDLE;
			return -1;
		}

		/* check crc */
		crc = crc32(ctx->recvfile_ctx.buf, ctx->recvfile_ctx.len);
		if (crc != recv_crc)
		{
			RDPT_DEBUG("crc check faild. 0x%08x != 0x%08x\n", crc, recv_crc);
			free(ctx->recvfile_ctx.buf);
			ctx->recvfile_ctx.state = RFS_IDLE;
			return -1;
		}

		FILE *fp = fopen(ctx->recvfile_ctx.name, "wb");
		if (fp == NULL)
		{
			RDPT_DEBUG("open file: %s faild.\n",
				ctx->recvfile_ctx.name);
			free(ctx->recvfile_ctx.buf);
			ctx->recvfile_ctx.state = RFS_IDLE;
			return -1;
		}

		/* write file */
		fwrite(ctx->recvfile_ctx.buf, 1, ctx->recvfile_ctx.len, fp);

		fclose(fp);
		free(ctx->recvfile_ctx.buf);
		ctx->recvfile_ctx.state = RFS_IDLE;

		RDPT_DEBUG("file receive finished.\n");
		break;
	}
	case RDPT_SOCKDATA_TYPE_RECVFILE_HEAD:
	{
		unsigned len;
		rdpt_filedesc_t *subh = (rdpt_filedesc_t *)(sd + 1);
		char *filebuf = NULL;

		memset(tmp_sendbuf, 0, sizeof(tmp_sendbuf));
		rdpt_sockdata_t *send_h = (rdpt_sockdata_t *)(tmp_sendbuf + 32);
		rdpt_filedesc_t *send_subh = (rdpt_filedesc_t *)(send_h + 1);
		memset(tmp_sendbuf, 0, sizeof(tmp_sendbuf));
		memcpy(send_subh->name, subh->name, subh->name_len);

		FILE *fp = fopen(send_subh->name, "rb");
		if (fp == NULL)
		{
			send_subh->file_len = send_subh->name_len = 0;
		}
		else
		{
			/* calc file length */
			fseek(fp, 0, SEEK_END);
			len = ftell(fp);

			/* read file */
			filebuf = (char *)malloc(len);
			fseek(fp, 0, SEEK_SET);
			fread(filebuf, 1, len, fp);
			/* no need file, just close */
			fclose(fp);

			send_subh->file_len = len;
			send_subh->name_len = subh->name_len;
		}
		send_h->type = RDPT_SOCKDATA_TYPE_RECVFILE_REP;
		send_h->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + sizeof(rdpt_filedesc_t) + subh->name_len);
		send_h->flag = 0;
		channel_send(ctx, send_h, send_h->dw_len * 4);

		if (fp == NULL)
		{
			RDPT_DEBUG("file poll faild, file %s, not exist.\n", send_subh->name);
			/* can't open dest file, just return */
			return -1;
		}
		RDPT_DEBUG("file poll begin(%s:%d)...\n", send_subh->name, send_subh->file_len);

		/* send loop */
		unsigned crc = crc32(filebuf, len);
		unsigned i, cur;
		for (i = 0; i < len;)
		{
			cur = len - i;
			if (cur > RDPT_SOCKDATA_MAX_PAYLOADLEN)
				cur = RDPT_SOCKDATA_MAX_PAYLOADLEN;
			send_h->type = RDPT_SOCKDATA_TYPE_RECVFILE_DATA;
			send_h->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + cur);
			send_h->flag = 0;
			memcpy(send_h + 1, filebuf + i, cur);

			channel_send(ctx, send_h, send_h->dw_len * 4);

			i += cur;
		}

		/* send fin */
		send_h->type = RDPT_SOCKDATA_TYPE_RECVFILE_FIN;
		send_h->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + sizeof(unsigned));
		send_h->flag = 0;
		*((unsigned *)(send_h + 1)) = crc;
		channel_send(ctx, send_h, send_h->dw_len * 4);

		free(filebuf);
		RDPT_DEBUG("file poll finished.\n", send_subh->name);
		break;
	}
	case RDPT_SOCKDATA_TYPE_SOCKOPEN:
	{
		int ret, withproxy = 0;
		rdpt_sockopen_t *soparam = (rdpt_sockopen_t *)(sd + 1);

		memset(tmp_sendbuf, 0, sizeof(tmp_sendbuf));
		rdpt_sockdata_t *send_h = (rdpt_sockdata_t *)(tmp_sendbuf + 32);
		rdpt_sockopen_reply_t *reply = (rdpt_sockopen_reply_t *)(send_h + 1);

		/* do sock connect */
		ret = socket(2, 1, 0);		/* AF_INET: 2, SOCK_STREAM: 1 */
		if (ret < 0)
		{
			RDPT_DEBUG("sock open faild.\n");
			ctx->sockdesc.sock = 0;

			reply->ret = RDPT_SOCKSTATE_SOCKOPEN_SOCK_FAILD;
			goto _sendreply;
		}
		else
		{
			ctx->sockdesc.sock = ret;
		}

		/* if proxy exist */
		struct sockaddr_in dstaddr;
		dstaddr.sin_family = 2;	/* AF_INET:2 */

		/* direct socket connect */
		if (strlen(soparam->proxyurl) == 0)
		{
			RDPT_DEBUG("dest url: %s\n", soparam->desturl);
			ret = url2addr(soparam->desturl, &dstaddr);
		}
		/* connect with http proxy */
		else
		{
			RDPT_DEBUG("proxy url: %s\n", soparam->proxyurl);
			withproxy = 1;
			ret = url2addr(soparam->proxyurl, &dstaddr);
		}

		if (ret != 0)
		{
			reply->ret = ret;
			closesocket(ctx->sockdesc.sock);
			goto _sendreply;
		}

		/* connect to dest socket */
	    ret = connect(ctx->sockdesc.sock,
			(const struct sockaddr *)&(dstaddr),
			sizeof(dstaddr));
	    if (ret < 0)
	    {
			reply->ret = RDPT_SOCKSTATE_SOCKOPEN_CONNECT_FAILD;
			closesocket(ctx->sockdesc.sock);
			goto _sendreply;
	    }

		if (withproxy)
		{
			RDPT_DEBUG("http proxy connectting...\n");
			/* construct proxy tunnel */
			if (proxy_connect(ctx, soparam->auth, soparam->desturl))
			{
				reply->ret = RDPT_SOCKSTATE_SOCKOPEN_PROXYBUILD_FAILD;
				proxysockclose(ctx);
				goto _sendreply;
			}

			RDPT_DEBUG("http proxy connection established.\n");
		}
		else
			RDPT_DEBUG("socket connection established.\n");

		/* create recv thread */
		ctx->sockdesc.h_rt = CreateThread(NULL, 0, sockrecv_thread, (LPVOID)ctx, 0, 0);

		reply->ret = RDPT_SOCKSTATE_SOCKOPEN_OK;

_sendreply:
		send_h->type = RDPT_SOCKDATA_TYPE_SOCKOPEN_REPLY;
		send_h->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + sizeof(rdpt_sockopen_reply_t));
		send_h->flag = 0;
		channel_send(ctx, send_h, send_h->dw_len * 4);

		break;
	}
	default:
	{
		/* try to fwd to ctx socket */
		if (ctx->sockdesc.sock)
		{
			send(ctx->sockdesc.sock, (const char *)sd, sd->dw_len * 4, 0);
		}
		else
		{
			//
			memset(tmp_sendbuf, 0, sizeof(tmp_sendbuf));
			rdpt_sockdata_t *send_h = (rdpt_sockdata_t *)(tmp_sendbuf + 32);
			rdpt_sockinfo_t *info = (rdpt_sockinfo_t *)(send_h + 1);
			send_h->type = RDPT_SOCKDATA_TYPE_SOCKINFO;
			send_h->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + sizeof(rdpt_sockinfo_t));
			send_h->flag = 0;
			info->state = RDPT_SOCKSTATE_SOCKCLOSED;
			channel_send(ctx, send_h, send_h->dw_len * 4);
		}

		break;
	}
	}

	return 0;
}

CODE_SECTION("tunnel init")

int tunnel_init()
{
	unsigned i;
	//
	for (i = 0; i < RDPT_MAX_CHAN_NUM; i++)
	{
		g_rdpt_ctx.chanlist[i].inuse = 0;
		g_rdpt_ctx.chanlist[i].chan = i;
	}

	/* init kbdmap */
	for (i = 0; i < (sizeof(g_rdpt_ctx.kbd_decmap) / sizeof(g_rdpt_ctx.kbd_decmap[0])); i++)
	{
		g_rdpt_ctx.kbd_decmap[i] = -1;
	}
	for (i = 0; i < (sizeof(g_incmap) / sizeof(g_incmap[0])); i++)
	{
		g_rdpt_ctx.kbd_decmap[g_incmap[i]] = i;
	}
	CreateThread(NULL, 0, recv_from_c, (LPVOID)NULL, 0, 0);

	g_rdpt_ctx.send_quota = 0;
	g_rdpt_ctx.recvframe.b = g_rdpt_ctx.recvframe.e = 0;

	g_rdpt_ctx.sendframe.buf = (char *)malloc(16 * 1024 * 1024);
	g_rdpt_ctx.sendframe.ins_lock = CreateMutex(NULL, FALSE, NULL);

	g_rdpt_ctx.state = RDPT_S_STATE_RST;

#ifndef RDPT_VC_DEBUG
	eg_setwin(RDPT_S2C_INITWIN_WIDTH, RDPT_S2C_INITWIN_HEIGHT);
#else
	eg_setwin(128, 128);
#endif

	/* socket init */
#ifdef WIN32
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);
#else
	char buf[1024];
    WSAStartup(0x0202, buf);
#endif

	return 0;
}

int tunnel_uninit()
{
	unsigned i;
	char sendbuf[64];

	/* stop send timer */
	KillTimer(g_rdpt_ctx.hwnd, RDPT_S_SENDTIMER_ID);

	for (i = 0; i < RDPT_MAX_CHAN_NUM; i++)
	{
		channel_ctx_reset(g_rdpt_ctx.chanlist + i);
	}

	/* send sclosed ctrl frame to c */
	rdpt_fh_t *fh = (rdpt_fh_t *)sendbuf;
	rdpt_cf_t *cf = (rdpt_cf_t *)(fh + 1);
	memset(sendbuf, 0, sizeof(sendbuf));
	cf->type = RDPT_CTRL_SCLOSED;
	fh->type = RDPT_FTYPE_CTRL;
	fh->len_qw = RDPT_FRAMELEN_QW(sizeof(rdpt_fh_t) + sizeof(rdpt_cf_t));

_retry:
	if (buf_insert(&(g_rdpt_ctx.sendframe), fh))
	{
		Sleep(1);
		goto _retry;
	}

_draining:
	frame_send2c(g_rdpt_ctx.hwnd, 0, 0, 0);
	/* wait for the send buff to be drained */
	if (g_rdpt_ctx.sendframe.b != g_rdpt_ctx.sendframe.e)
	{
		Sleep(1000);
		goto _draining;
	}

	Sleep(200);

	return 0;
}

static void CALLBACK s_report(
  HWND     hwnd,
  UINT     uMsg,
  UINT_PTR idEvent,
  DWORD    dwTime
)
{
	if (g_rdpt_ctx.state != RDPT_S_STATE_OK)
		return;

	char sendbuf[256];
	memset(sendbuf, 0, sizeof(sendbuf));

	rdpt_fh_t *fh = (rdpt_fh_t *)sendbuf;
	rdpt_cf_t *cf = (rdpt_cf_t *)(fh + 1);

	cf->type = RDPT_CTRL_S_REPORT;
	cf->u.report = g_rdpt_ctx.report;

	fh->type = RDPT_FTYPE_CTRL;
	fh->len_qw = RDPT_FRAMELEN_QW(sizeof(rdpt_fh_t) + sizeof(rdpt_cf_t));

	buf_insert(&(g_rdpt_ctx.sendframe), fh);
}

//+---------------------------------------------------------------------------
//
//  Function:   WndProc
//
//  Synopsis:   very unusual type of function - gets called by system to
//              process windows messages.
//
//  Arguments:  same as always.
//     lParam:
//       low word: param0
//      high word: param1
//----------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	unsigned short m_x = lParam & 0xffff;
	unsigned short m_y = (lParam >> 16) & 0xffff;
	unsigned char scancode = (lParam >> 16) & 0xff;

    switch (message) {

        // ----------------------- first and last
        case WM_CREATE:
            break;
		case WM_MOVE:
		case WM_SIZE:
			GetClientRect(hwnd, &(g_rdpt_ctx.client_rect));
			g_rdpt_ctx.client_width = g_rdpt_ctx.client_rect.right - g_rdpt_ctx.client_rect.left;
			g_rdpt_ctx.client_height = g_rdpt_ctx.client_rect.bottom - g_rdpt_ctx.client_rect.top;

			g_rdpt_ctx.client_pos.x = 0;
			g_rdpt_ctx.client_pos.y = 0;
			ClientToScreen(hwnd, &(g_rdpt_ctx.client_pos));
			RDPT_DEBUG("client_rect: left(%d) right(%d) top(%d) bottom(%d), pos(%d, %d).\n",
				g_rdpt_ctx.client_rect.left,
				g_rdpt_ctx.client_rect.right,
				g_rdpt_ctx.client_rect.top,
				g_rdpt_ctx.client_rect.bottom,
				g_rdpt_ctx.client_pos.x, g_rdpt_ctx.client_pos.y);

			return DefWindowProc(hwnd, message, wParam, lParam);
        case WM_DESTROY:
            PostQuitMessage(0);
            break;

#if 0
        case WM_PAINT:
        {
			if (paint_proc(hwnd))
				return DefWindowProc(hwnd, message, wParam, lParam);

			break;
        }
#endif
		case WM_CLOSE:
		{
			tunnel_uninit();
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
		case WM_KILLFOCUS:
		{
			PostQuitMessage(0);
			break;
		}

        // ----------------------- let windows do all other stuff
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

//+---------------------------------------------------------------------------
//
//  Function:   WinMain
//
//  Synopsis:   standard entrypoint for GUI Win32 apps
//
//----------------------------------------------------------------------------
#define APPNAME "HELLO_WIN"
int APIENTRY WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
    MSG msg;
    WNDCLASS wc;
	const char szAppName[] = APPNAME; // The name of this application
	const char szTitle[]   = APPNAME; // The title bar text
	const char *pWindowText;

#ifdef RDPT_DEBUG_ON
	AllocConsole();
	g_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	RDPT_DEBUG("rdpt test.\n");
#endif

#if 1
	/* receive kbdhook register */
	if (SetWindowsHookEx(WH_KEYBOARD_LL, kbd_hook, hInstance, 0) == NULL)
	{
		RDPT_DEBUG("kbd hoot reg faild: 0x%08x.\n", GetLastError());
		while (1);
	}
#endif

	if (tunnel_init())
	{
		RDPT_DEBUG("tunnel init faild.\n");
		while (1);
	}

    pWindowText = lpCmdLine[0] ? lpCmdLine : "Hello Windows!";

    // Fill in window class structure with parameters that describe
    // the main window.

    ZeroMemory(&wc, sizeof wc);
    wc.hInstance     = hInstance;
    wc.lpszClassName = szAppName;
    wc.lpfnWndProc   = (WNDPROC)WndProc;
    wc.style         = CS_DBLCLKS|CS_VREDRAW|CS_HREDRAW;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    if (FALSE == RegisterClass(&wc))
        return 0;

    // create the browser
    g_rdpt_ctx.hwnd = CreateWindow(
        szAppName,
        szTitle,
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        0, // CW_USEDEFAULT,
        0, // CW_USEDEFAULT,
        800,//CW_USEDEFAULT,
        600,//CW_USEDEFAULT,
        0,
        0,
        hInstance,
        0);

    if (NULL == g_rdpt_ctx.hwnd)
        return 0;

#if 1
	SetTimer(g_rdpt_ctx.hwnd, RDPT_S_SENDTIMER_ID, 10, frame_send2c);
	SetTimer(g_rdpt_ctx.hwnd, RDPT_S_REPORTTIMER_ID, 1000, s_report);
#else
	g_rdpt_ctx.timer_q = CreateTimerQueue();
	CreateTimerQueueTimer(&(g_rdpt_ctx.send_timer),
		g_rdpt_ctx.timer_q, frame_send2c,
		&g_rdpt_ctx, 10, 10, 0);
#endif

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

#if 0
	DeleteTimerQueue(g_rdpt_ctx.timer_q);
#endif

#ifdef RDPT_DEBUG_ON
	FreeConsole();
#endif

    return msg.wParam;
}

