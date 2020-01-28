#include "cpkl.h"
#include "rdpt_prot.h"

/*
    CRC-32-IEEE 802.3
    x^{32} + x^{26} + x^{23} + x^{22} + x^{16} + x^{12} + x^{11} + x^{10} + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1
    1 0000 0100 1100 0001 0001 1101 1011 0111
      0    4    c    1    1    d    b    7
    0x04C11DB7 or 0xEDB88320 (0xDB710641)
*/

#define POLY        (0x04C11DB7UL)

unsigned crc32_raw(void *buf, unsigned len)
{
    unsigned reg = 0, bitoff = 0, carry = 0;
    unsigned char bitflag = 0x80, curbyte = 0;

    while ((bitoff >> 3) < len)
    {
        if ((bitoff & 0x7) == 0)
        {
            curbyte = ((unsigned char *)buf)[bitoff >> 3];
            bitflag = 0x80;
        }
        else
            bitflag >>= 1;

        carry = reg >> 31;
        reg <<= 1;
        reg |= ((curbyte & bitflag) ? 0x1 : 0x0);
        bitoff++;

        /* get one bit */
        if (carry)
        {
            reg ^= POLY;
        }
    }

    return reg;
}

unsigned crc32(void *buf, unsigned len)
{
    unsigned char r_buff[8] = {0};
    unsigned r = crc32_raw(buf, len);
    r_buff[0] = (unsigned char)(r >> 24);
    r_buff[1] = (unsigned char)(r >> 16);
    r_buff[2] = (unsigned char)(r >> 8);
    r_buff[3] = (unsigned char)(r >> 0);
    return crc32_raw(r_buff, sizeof(r_buff));
}

unsigned do_base64(const char *buf, unsigned len, char *output)
{
	static char enc[64], init = 0;
	unsigned i, j, tmp, ret, n_pad;
	if (init == 0)
	{
		for (i = 0; i < 26; i++)
		{
			enc[i] = 'A' + i;
			enc[i + 26] = 'a' + i;
		}
		for (i = 0; i < 10; i++)
			enc[i + 52] = '0' + i;

		enc[62] = '+';
		enc[63] = '/';
		init = 1;
	}

	ret = 0;
	n_pad = 0;
	for (i = 0; i < len;)
	{
		tmp = 0;
		for (j = 0; j < 3; j++)
		{
			tmp <<= 8;
			if (i < len)
			{
				tmp |= buf[i++];
			}
			else
			{
				n_pad++;
			}
		}
		output[ret++] = enc[(tmp >> 18) & 0x3f];
		output[ret++] = enc[(tmp >> 12) & 0x3f];
		if (n_pad > 1)
			output[ret++] = '=';
		else
			output[ret++] = enc[(tmp >> 6) & 0x3f];
		if (n_pad)
			output[ret++] = '=';
		else
			output[ret++] = enc[(tmp >> 0) & 0x3f];
	}
	CPKL_ASSERT(i == len);

	return ret;
}

#define CMDLINE_MAXLEN			256

typedef struct _thread_param {
	int recv_sock;
} thread_param_t;

typedef enum {
	RDPT_TPSTATE_IDLE = 0,
	RDPT_TPSTATE_RECV_BEGIN,
	RDPT_TPSTATE_RECV_DATA,
	RDPT_TPSTATE_COUNT
} rdpt_tpstate_e;

rdpt_tpstate_e g_tpstate = RDPT_TPSTATE_IDLE;

struct {
	unsigned	len, recv_len;
	unsigned	name_len;
	char		*buf;
	char		filename[256];
} rdpt_recv_ctx;

struct {
	unsigned	recv_from_s_sdlen;
} stat;

static int one_sockdata_proc(rdpt_sockdata_t *h)
{
	rdpt_filedesc_t *subh = (rdpt_filedesc_t *)(h + 1);

	// cpkl_printf("one_sockdata_proc(), type: %d, len: %d\n", h->type, h->dw_len * 4);
	// cpkl_hexdump(h, rpl->len - sizeof(rpl_struct_t), "");
	// cpkl_printf("\n");


	switch (h->type)
	{
	case RDPT_SOCKDATA_TYPE_RECVFILE_REP:
	{
		if (g_tpstate != RDPT_TPSTATE_RECV_BEGIN)
		{
			return -1;
		}

		if ((subh->name_len == 0) && (subh->file_len == 0))
		{
			/* file not exist */
			cpkl_printf("recv file not exist.\n");
			free(rdpt_recv_ctx.buf);
			g_tpstate = RDPT_TPSTATE_IDLE;
			return -1;
		}

		memset(&rdpt_recv_ctx, 0, sizeof(rdpt_recv_ctx));
		rdpt_recv_ctx.name_len = subh->name_len;
		memcpy(rdpt_recv_ctx.filename, subh->name, rdpt_recv_ctx.name_len);
		rdpt_recv_ctx.len = subh->file_len;
		rdpt_recv_ctx.buf = (char *)malloc(rdpt_recv_ctx.len);

		g_tpstate = RDPT_TPSTATE_RECV_DATA;

		break;
	}
	case RDPT_SOCKDATA_TYPE_RECVFILE_DATA:
	{
		if (g_tpstate != RDPT_TPSTATE_RECV_DATA)
		{
			return -1;
		}

		memcpy(rdpt_recv_ctx.buf + rdpt_recv_ctx.recv_len, h + 1, h->dw_len * 4 - sizeof(rdpt_sockdata_t));
		rdpt_recv_ctx.recv_len += h->dw_len * 4 - sizeof(rdpt_sockdata_t);

		// printf("\rrecvfile %d bytes...", rdpt_recv_ctx.recv_len);

		break;
	}
	case RDPT_SOCKDATA_TYPE_RECVFILE_FIN:
	{
		if (g_tpstate != RDPT_TPSTATE_RECV_DATA)
		{
			return -1;
		}

		unsigned crc, crc_recv = *((unsigned *)(h + 1));
		/* check len */
		if (rdpt_recv_ctx.recv_len < rdpt_recv_ctx.len)
		{
			cpkl_printf("file recv len error: %d (should be %d)\n", rdpt_recv_ctx.recv_len, rdpt_recv_ctx.len);
			free(rdpt_recv_ctx.buf);
			g_tpstate = RDPT_TPSTATE_IDLE;
			return -1;
		}

		/* check crc */
		crc = crc32(rdpt_recv_ctx.buf, rdpt_recv_ctx.len);
		if (crc != crc_recv)
		{
			cpkl_printf("crc check faild.\n");
			free(rdpt_recv_ctx.buf);
			g_tpstate = RDPT_TPSTATE_IDLE;
			return -1;
		}

		FILE *fp = fopen(rdpt_recv_ctx.filename, "wb");
		if (fp == NULL)
		{
			cpkl_printf("file open faild: %s.\n", rdpt_recv_ctx.filename);
			free(rdpt_recv_ctx.buf);
			g_tpstate = RDPT_TPSTATE_IDLE;
			return -1;
		}

		/* write file */
		fwrite(rdpt_recv_ctx.buf, 1, rdpt_recv_ctx.len, fp);

		cpkl_printf("file %s recv success.\n", rdpt_recv_ctx.filename);

		fclose(fp);
		free(rdpt_recv_ctx.buf);
		g_tpstate = RDPT_TPSTATE_IDLE;

		break;
	}
	case RDPT_SOCKDATA_TYPE_SOCKOPEN_REPLY:
	{
		rdpt_sockopen_reply_t *reply = (rdpt_sockopen_reply_t *)(h + 1);

		cpkl_printf("sockopen reply, ret(%d)\n", reply->ret);

		break;
	}
	case RDPT_SOCKDATA_TYPE_SOCKINFO:
	{
		rdpt_sockinfo_t *info = (rdpt_sockinfo_t *)(h + 1);

		switch (info->state)
		{
		case RDPT_SOCKSTATE_SOCKCLOSED:
		{
			/* remote socket closed */
			cpkl_printf("remote socket closed.\n");
			break;
		}
		default:
		{
			break;
		}
		}

		break;
	}
	default:
	{
		break;
	}
	}

	return 0;
}

static int cmdrecv_thread(void *param)
{
	thread_param_t *p = (thread_param_t *)param;
	static char buf[1024 * 1024];
	unsigned left = 0;
	int cur, len;
	rdpt_sockdata_t *sockdata;

	while (1)
	{
		len = recv(p->recv_sock, buf + left, sizeof(buf) - left, 0);
		if (len <= 0)
		{
			/* socket closed */
			printf("socket closed.\n");
			exit(0);
		}
		// cpkl_printf("ctrl recv %d\n", len);

		left += len;
		cur = 0;
		sockdata = (rdpt_sockdata_t *)buf;
		while ((left != 0) &&
			(left >= sizeof(rdpt_sockdata_t)) &&
			(left >= (sockdata->dw_len * 4)))
		{
			one_sockdata_proc(sockdata);

			stat.recv_from_s_sdlen += sockdata->dw_len * 4;

			cur += sockdata->dw_len * 4;
			left -= sockdata->dw_len * 4;
			sockdata += sockdata->dw_len;
		}

		if (left)
			memmove(buf, buf + cur, left);
	}
}

static int rdpt_sendfile(char *name, rdpt_sockdata_t *cmd, int sock)
{
	int len, cur, id;
	FILE *fp = fopen(name, "rb");
	if (fp == NULL)
	{
		cpkl_printf("file read failed: %s\n", name);
		return -1;
	}
	else
		cpkl_printf("file to translate: %s\n", name);

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	cpkl_printf("len: %d\n", len);

	u8 *filebuf = (u8 *)cpkl_malloc(len);
	fseek(fp, 0, SEEK_SET);
	fread(filebuf, 1, len, fp);
	u32 crc = crc32(filebuf, len);
	printf("crc: 0x%08x\n", crc);

	rdpt_filedesc_t *subh;

	cmd->type = RDPT_SOCKDATA_TYPE_SENDFILE_HEAD;
	cmd->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + sizeof(rdpt_filedesc_t) + cpkl_pdf_strlen(name));
	cmd->flag = 0;
	/* file length */
	subh = (rdpt_filedesc_t *)(cmd + 1);
	subh->file_len = len;
	subh->name_len = cpkl_pdf_strlen(name);
	cpkl_pdf_memcpy(subh->name, name, cpkl_pdf_strlen(name));
	subh->name[cpkl_pdf_strlen(name)] = 0;

	send(sock, (const char *)cmd, cmd->dw_len * 4, 0);

	/* send file body */
	id = 0;
	do {
		cur = len < RDPT_SOCKDATA_MAX_PAYLOADLEN ? len : RDPT_SOCKDATA_MAX_PAYLOADLEN;

		cmd->type = RDPT_SOCKDATA_TYPE_SENDFILE_DATA;
		cmd->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + cur);
		cmd->flag = 0;
		cpkl_pdf_memcpy(cmd + 1, filebuf + RDPT_SOCKDATA_MAX_PAYLOADLEN * id, cur);

		send(sock, (const char *)cmd, cmd->dw_len * 4, 0);

		id++;
		len -= cur;
	} while (len);

	/* send finish */
	cmd->type = RDPT_SOCKDATA_TYPE_SENDFILE_FIN;
	cmd->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + sizeof(unsigned));
	cmd->flag = 0;
	*((unsigned *)(cmd + 1)) = crc;

	send(sock, (const char *)cmd, cmd->dw_len * 4, 0);

	printf("sendfile finishd.\n");

	return 0;
}

static int rdpt_recvfile(char *name, rdpt_sockdata_t *cmd, int sock)
{
	rdpt_filedesc_t *subh;

	cmd->type = RDPT_SOCKDATA_TYPE_RECVFILE_HEAD;
	cmd->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + sizeof(rdpt_filedesc_t) + cpkl_pdf_strlen(name));
	cmd->flag = 0;

	subh = (rdpt_filedesc_t *)(cmd + 1);
	subh->file_len = 0;
	subh->name_len = cpkl_pdf_strlen(name);
	cpkl_pdf_memcpy(subh->name, name, cpkl_pdf_strlen(name));
	subh->name[cpkl_pdf_strlen(name)] = 0;

	send(sock, (const char *)cmd, cmd->dw_len * 4, 0);

	/* begin to recv */
	g_tpstate = RDPT_TPSTATE_RECV_BEGIN;

	return 0;
}

static int rdpt_opensock(char *name, rdpt_sockdata_t *cmd, int sock)
{
	rdpt_sockopen_t *subh = (rdpt_sockopen_t *)(cmd + 1);
	const char *proxyurl = "proxy.h3c.com:8080";
	const char *auth = "xxxxx:xxxxxxxx";		// user:password
	const char *url = "23.105.202.197:26550";

	memset(subh, 0, sizeof(rdpt_sockopen_t));

	cmd->type = RDPT_SOCKDATA_TYPE_SOCKOPEN;
	cmd->dw_len = RDPT_SOCKDATALEN_DW(sizeof(rdpt_sockdata_t) + sizeof(rdpt_sockopen_t));
	cmd->flag = 0;

	memcpy(subh->proxyurl, proxyurl, cpkl_pdf_strlen(proxyurl));
	do_base64(auth, cpkl_pdf_strlen(auth), subh->auth);
	memcpy(subh->desturl, url, cpkl_pdf_strlen(url));

	cpkl_printf("send sockopen...\n");

	send(sock, (const char *)cmd, cmd->dw_len * 4, 0);

	return 0;
}

void test()
{

}

int main(int argc, char *argv)
{
	static char linebuf[CMDLINE_MAXLEN];
	char *subargv[16];
	u32 len[16], n_arg;
	int sock, ret;
	static char send_buff[4096];
	rdpt_sockdata_t *cmd = (rdpt_sockdata_t *)send_buff;

	WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

	// test();

	cpkl_tpstart(2);

	/* connect to socket */
	/* socket client */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
	{
		exit(0);
	}

    struct sockaddr_in dstaddr;
    dstaddr.sin_family = AF_INET;
    dstaddr.sin_port = htons(RDPT_SERV_PORT);
    dstaddr.sin_addr.s_addr = inet_addr("192.168.2.10");
    // printf("0x%08x\n", htonl(dstaddr.sin_addr.s_addr));

    ret = connect(sock, (const struct sockaddr *)&(dstaddr), sizeof(dstaddr));
    if (ret < 0)
    {
        printf("connect failed.\n");
        exit(0);
    }
	else {
        printf("connect success.\n");
    }

	thread_param_t thp = {sock};
	cpkl_tpinsert(cmdrecv_thread, &thp, NULL);

	cpkl_printf("\n\n");
	while (1)
	{
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
			continue;

		if (cpkl_pdf_memcmp(subargv[0], "sendfile", 8) == 0)
		{
			rdpt_sendfile(subargv[1], cmd, sock);
		}
		else if (cpkl_pdf_memcmp(subargv[0], "recvfile", 8) == 0)
		{
			rdpt_recvfile(subargv[1], cmd, sock);
		}
		else if (cpkl_pdf_memcmp(subargv[0], "opensock", 8) == 0)
		{
			rdpt_opensock(subargv[1], cmd, sock);
		}
		else if (cpkl_pdf_memcmp(subargv[0], "stat",  4) == 0)
		{
			printf("recv_from_s_sdlen: %d\n",
				stat.recv_from_s_sdlen);

			if (n_arg == 2)
			{
				if (cpkl_pdf_memcmp(subargv[1], "-r",  2) == 0)
					memset(&stat, 0, sizeof(stat));
			}
		}
	}

	return 0;
}
