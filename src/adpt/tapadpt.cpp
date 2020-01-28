#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>


#include "secpair.h"

#include <fcntl.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <sys/select.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/if_tun.h>

#include "rdpt_func.c"

#define TAPADPT_AESKEY					{0x5a542de1, 0x49e2df34, 0xe4ab2dd3, 0x35a67819}

int tun_alloc(char *dev)
{
	struct ifreq ifr;
	int fd, err;

	if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
	{
		printf("open failed.\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	/* Flags: IFF_TUN   - TUN device (no Ethernet headers)
	 *        IFF_TAP   - TAP device
	 *
	 *        IFF_NO_PI - Do not provide packet information
	 */
	ifr.ifr_flags = IFF_TAP;

	if (*dev)
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0)
	{
		printf("ioctl failed. errno: %d\n", errno);
		close(fd);
		return err;
	}

	strcpy(dev, ifr.ifr_name);

	return fd;
}

unsigned ri_rand(unsigned begin, unsigned count)
{
	unsigned long long orig, orig_range;

	if (count <= ((unsigned)RAND_MAX + 1))
	{
		orig = rand();
		orig_range = (unsigned long long)RAND_MAX - 0 + 1;
	}
	else
	{
		orig = (((unsigned long long)ri_rand(0, 0xFF)) << 24) |
			   (((unsigned long long)ri_rand(0, 0xFF)) << 16) |
			   (((unsigned long long)ri_rand(0, 0xFF)) << 8) |
			   (((unsigned long long)ri_rand(0, 0xFF)) << 0);
		orig_range = 0x100000000LL;
	}

	orig *= count;
	orig /= orig_range;
	orig += begin;

	return (unsigned)orig;
}

typedef enum _tapadpt_type {
	TAPADPT_TYPE_OPEN = RDPT_SOCKDATA_USER,
	TAPADPT_TYPE_OPEN_REPLY,
	TAPADPT_TYPE_TD,			/* tunnel data */
	TAPADPT_TYPE_COUNT
} tapadpt_type_e;

typedef struct _tapadpt_open {
	char	tapip[16];
} tapadpt_open_t;

typedef struct _tapadpt_openreply {
	int		code;
} tapadpt_openreply_t;

typedef struct _adpt_ctx{
	const char	*name;
	char	tapip_serv[16], tapip_cli[16];
	volatile	int state;
	int		sock;
	int		tapfd;
	secpair	*sp;
	pthread_t	pt;
} adpt_ctx_t;

static unsigned adpt_rand()
{
	return ri_rand(0, 0xffffffff);
}

int tap_recv(adpt_ctx_t *ctx)
{
	int ret;
	char buf[4096 + 16], *p = buf + 16;

	ret = read(ctx->tapfd, p, 4096);
	if (ret <= 0)
	{
		printf("tap read return: %d.\n", ret);
		return -1;
	}

	ctx->sp->send(p, ret, TAPADPT_TYPE_TD);

	return 0;
}

/* from client */
static int adpt_recv(secpair_fh_t *spfh, void *param)
{
	adpt_ctx_t *ctx = (adpt_ctx_t *)param;
	char devname[32];
	int ret;

	switch (spfh->type)
	{
	case RDPT_SOCKDATA_TYPE_SOCKOPEN_REPLY:
	{
		rdpt_sockopen_reply_t *reply = (rdpt_sockopen_reply_t *)(spfh + 1);
		ctx->state = reply->ret;

		break;
	}
	case TAPADPT_TYPE_OPEN:
	{
		tapadpt_open_t *open = (tapadpt_open_t *)(spfh + 1);

		printf("recv tap open, tap ip: %s.\n", open->tapip);

		memset(devname, 0, sizeof(devname));
		ctx->tapfd = tun_alloc(devname);
		if (ctx->tapfd < 0)
		{
			/* tap open faild */
			printf("tap open faild.\n");
			ret = -1;
			goto _reply;
		}

		if (fork() == 0)
		{
			int ret = execl("/bin/bash", "bash", "tap_serv.sh",
				devname, open->tapip, NULL);
			if (ret < 0)
			{
				printf("execl faild, errno: %d\n", errno);
				ret = -2;
				goto _reply;
			}
		}
		wait(NULL);
		printf("script finished.\n");
		ret = 0;
_reply:
		/* reply */
		tapadpt_openreply_t *reply = (tapadpt_openreply_t *)open;
		reply->code = ret;
		ctx->sp->send(reply, sizeof(tapadpt_openreply_t), TAPADPT_TYPE_OPEN_REPLY);

		break;
	}
	case TAPADPT_TYPE_OPEN_REPLY:
	{
		tapadpt_openreply_t *reply = (tapadpt_openreply_t *)(spfh + 1);
		ctx->state = reply->code;
		if (ctx->state)
		{
			/* tap tunnel open faild */
			break;
		}

		memset(devname, 0, sizeof(devname));
		ctx->tapfd = tun_alloc(devname);
		if (ctx->tapfd < 0)
		{
			/* tap open faild */
			printf("tap open faild.\n");
			exit(0);
		}
		if (fork() == 0)
		{
			int ret = execl("/bin/bash", "bash", "tap_cli.sh",
				devname, ctx->tapip_cli, ctx->tapip_serv, NULL);
			if (ret < 0)
			{
				printf("execl faild, errno: %d\n", errno);
				ret = -1;
			}
		}
		wait(NULL);
		printf("script finished.\n");


		break;
	}
	case TAPADPT_TYPE_TD:
	{
		if (ctx->tapfd == 0)
		{
			/* recv tap tunnel data pkt before tap constructed, just ignore. */
			printf("recv tap tunnel data pkt, but tap is not open.\n");
			rdpt_hexdump(spfh + 1, spfh->pt_len, "");
			printf("\n");
		}
		else
			write(ctx->tapfd, spfh + 1, spfh->pt_len);
		break;
	}
	case RDPT_SOCKDATA_TYPE_SOCKINFO:
	{
		rdpt_sockinfo_t *info = (rdpt_sockinfo_t *)(spfh + 1);
		printf("recv sockinfo, stat: %d\n", info->state);
		break;
	}
	default:
	{
		/* unknown pkt type */
		printf("adpt_recv(), unknown pkt type: %d\n", spfh->type);
		break;
	}
	}
	return 0;
}

/* to client */
static int adpt_sp_send(secpair_fh_t *spfh, void *param)
{
	adpt_ctx_t *ctx = (adpt_ctx_t *)param;
	int ret;

	// printf("%s socket send %d.\n", ctx->name, spfh->dw_len * 4);

	/* send to client */
	ret = send(ctx->sock, spfh, spfh->dw_len * 4, 0);
	if (ret <= 0)
		printf("adpt_sp_send() send return: %d.\n", ret);
	else if (ret != spfh->dw_len * 4)
		printf("adpt_sp_send() send %d(%d needed).\n", ret, spfh->dw_len * 4);

	return 0;
}

static void * recv_loop(void *param)
{
	const unsigned key[] = TAPADPT_AESKEY;
	char *recvbuf = (char *)malloc(1024 * 1024);
	adpt_ctx_t *ctx = (adpt_ctx_t *)param;

	ctx->tapfd = 0;
	ctx->sp = new secpair(adpt_recv, adpt_sp_send, adpt_rand, (const char *)key, ctx);

	/* recv loop */
	int n, cur, left, len, sbp = 0;
	fd_set rfds, wfds;
	struct timeval tv;

_next:
	n = ctx->sock > ctx->tapfd ? ctx->sock : ctx->tapfd;
	n++;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(ctx->sock, &rfds);
	if (ctx->tapfd)
		FD_SET(ctx->tapfd, &rfds);

	/* default timeout */
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	switch (select(n, &rfds, &wfds, NULL, &tv))
	{
		case -1:
			printf("select: %s\n", strerror(errno));

		case 0:
		/* time out */
			goto _next;
	}

	if (FD_ISSET(ctx->sock, &rfds))
	{
		/* pkt recv from client */
		secpair_fh_t * spfh;

		len = recv(ctx->sock, recvbuf + sbp, 1024 * 1024 - sbp, 0);
		if (len <= 0)
		{
			shutdown(ctx->sock, SHUT_RDWR);
			close(ctx->tapfd);
			/* socket closed */
			goto _ret;
		}
		left = len + sbp;
		// printf("%s socket recv %d.\n", ctx->name, len);

		spfh = (secpair_fh_t *)recvbuf;
		cur = 0;
		while ((left >= sizeof(secpair_fh_t)) &&
			(left >= (spfh->dw_len * 4)))
		{
			ctx->sp->sp_recv(spfh);

			cur += spfh->dw_len * 4;
			left -= spfh->dw_len * 4;

			spfh += spfh->dw_len;
		}
		if (left)
		{
			memmove(recvbuf, recvbuf + cur, left);
			sbp = left;
		}
		else
			sbp = 0;
	}
	if (FD_ISSET(ctx->tapfd, &rfds))
	{
		/* pkt recv from tap */
		tap_recv(ctx);
	}

	goto _next;

_ret:
	printf("recv_loop stop, sock and tap closed.\n");

	free(recvbuf);
	return 0;
}

int serv_start(int argc, char *argv[])
{
	unsigned port;
	char devname[32];
	adpt_ctx_t *ctx[16] = {NULL};
	memset(devname, 0, sizeof(devname));

	/* sock serv */
	int listen_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0)
	{
		printf("tapadpt sock open faild.\n");
		exit(0);
	}

	/*  */
	sscanf(argv[2], "%d", &port);
	struct sockaddr_in srv_addr;
	srv_addr.sin_family = PF_INET;
	srv_addr.sin_port = htons(port);
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(listen_sock, (struct sockaddr *)(&srv_addr), sizeof(srv_addr)) < 0)
	{
		printf("bind failed, errno: %d.\n", errno);
		exit(0);
	}
	listen(listen_sock, 10);

	printf("listening...\n");

	while (1)
	{
		socklen_t len = sizeof(struct sockaddr_in);
		int client_sock = accept(listen_sock, (struct sockaddr *)(&srv_addr), &len);
		if (client_sock <= 0)
		{
			printf("accept faild, errno: %d\n", errno);
			continue;
		}

		printf("new tap client connected.\n");

		// if (fork() == 0)
		{
			ctx[0] = new adpt_ctx_t;
			ctx[0]->name = "serv";
			ctx[0]->sock = client_sock;
			recv_loop(ctx[0]);

			delete ctx[0];
			ctx[0] = NULL;
		}

		printf("tap client closed.\n");
	}
}

int open_sock_conn(char *url, adpt_ctx_t *ctx)
{
	int ret;
	struct sockaddr_in dstaddr;
	dstaddr.sin_family = AF_INET;

	ret = url2addr(url, &dstaddr);
	if (ret != RDPT_SOCKSTATE_SOCKOPEN_OK)
		return ret;

	/* connect */
    ret = connect(ctx->sock, (const struct sockaddr *)&(dstaddr), sizeof(dstaddr));
    if (ret < 0)
    {
        printf("connect failed.\n");
        return RDPT_SOCKSTATE_SOCKOPEN_CONNECT_FAILD;
    }
	else
	{
		ret = RDPT_SOCKSTATE_SOCKOPEN_OK;
        printf("connect to : %s.\n", url);
    }

	/* start to receive */
	pthread_create(&(ctx->pt), NULL, recv_loop, ctx);
	usleep(1000000);

	return ret;
}

/*
 * send sd open to the rdpt, with or without http proxy
 */
int sd_open_sock_conn
(
	adpt_ctx_t *ctx,
	char *sendbuf,
	const char *serv,
	const char *hp,
	const char *hpa
)
{
	/* send sd connect command */
	int timeout;
	rdpt_sockopen_t *subh = (rdpt_sockopen_t *)(sendbuf + 32);
	memset(subh, 0, sizeof(rdpt_sockopen_t));

	printf("rdpt sock connect begin...\n");

	if (hp)
		memcpy(subh->proxyurl, hp, strlen(hp));
	if (hpa)
		do_base64(hpa, strlen(hpa), subh->auth);

	memcpy(subh->desturl, serv, strlen(serv));

	/* send rdpt sd connect command */
	ctx->sp->send(subh,
		sizeof(rdpt_sockopen_t),
		RDPT_SOCKDATA_TYPE_SOCKOPEN);

	timeout = 200000;
	ctx->state = RDPT_SOCKSTATE_SOCKOPEN_TIMEOUT;
_timeout:
	if ((ctx->state) && timeout)
	{
		usleep(5000);
		timeout--;
		goto _timeout;
	}
	if (ctx->state == 0)
	{
		printf("rdpt sock connect established.\n");
	}
	else
	{
		printf("rdpt sock connect openfaild, state: %d.\n", ctx->state);
		return RDPT_SOCKSTATE_SOCKOPEN_TIMEOUT;
	}

	return RDPT_SOCKSTATE_SOCKOPEN_OK;
}


/*
 * tap_c	rdpt proxy	http proxy		tap_s
 * [1]		[2 2`]		[3 3`]			[4]
 * [1] <---> [4]
 * [1] <---> [2 2`] <---> [4]
 * [1] <---> [3 3`] <---> [4]			NOT support
 * [1] <---> [2 2`] <---> [3 3`] <---> [4]
 * "-rdpt=":"RDPT proxy", 'x.x.x.x:p'
 * "-hp="  :"http proxy", 'x.x.x.x:p' or 'proxy.zzz.com:p'
 * "-hpa=" :"http proxy auth", 'name:password'
 * "-serv=":"dest server addr", 'x.x.x.x:p'
 * "-lta=" :"local tap address", 'x.x.x.x'
 * "-rta=" :"remote tap address", 'x.x.x.x'
 */
int cli_start(int argc, char *argv[])
{
	int ret, timeout;
	unsigned port;
	char sendbuf[256];
	tapadpt_open_t *open = (tapadpt_open_t *)sendbuf;
	char *rdpt, *hp, *hpa, *serv, *lta, *rta;
	adpt_ctx_t ctx;

	memset(&ctx, 0, sizeof(adpt_ctx_t));
	ctx.name = "cli";

	/* param parse */
	rdpt = withparam(argv, argc, "-rdpt=");
	hp = withparam(argv, argc, "-hp=");
	hpa = withparam(argv, argc, "-hpa=");
	serv = withparam(argv, argc, "-serv=");
	lta = withparam(argv, argc, "-lta=");
	rta = withparam(argv, argc, "-rta=");

	if (serv == NULL)
	{
		printf("-serv is needed.\n");
		return 0;
	}

	if ((lta == NULL) || (rta == NULL))
	{
		printf("-lta and -rta is needed.\n");
		return 0;
	}

	memcpy(ctx.tapip_cli, lta, strlen(lta));
	memcpy(ctx.tapip_serv, rta, strlen(rta));

	ctx.sock = socket(PF_INET, SOCK_STREAM, 0);
	if (ctx.sock < 0)
	{
		printf("sock open faild.\n");
		return 0;
	}

	/**/
	if (rdpt)
	{
		ret = open_sock_conn(rdpt, &ctx);
		if (ret != RDPT_SOCKSTATE_SOCKOPEN_OK)
			goto _fail;
	}

	if (hp)
	{
		if (rdpt == NULL)
		{
			/* without rdpt proxy, just connect to httpproxy directly, NOT support now*/

			// open_sock_conn(hp, &ctx);
			printf("not support -hp without -rdpt.");
			goto _fail;
		}
		else
		{
			ret = sd_open_sock_conn(&ctx, sendbuf, serv, hp, hpa);
			if (ret != RDPT_SOCKSTATE_SOCKOPEN_OK)
				goto _fail;
		}
	}
	else
	{
		if (rdpt == NULL)
		{
			/* connect to server directly */
			ret = open_sock_conn(serv, &ctx);
			if (ret != RDPT_SOCKSTATE_SOCKOPEN_OK)
				goto _fail;
		}
		else
		{
			ret = sd_open_sock_conn(&ctx, sendbuf, serv, NULL, NULL);
			if (ret != RDPT_SOCKSTATE_SOCKOPEN_OK)
				goto _fail;
		}
	}

	/* auth */
	printf("auth start...\n");
	if (ctx.sp->startup())
		printf("auth faild.\n");
	else
		printf("auth seccess.\n");

	/* send tapopen */
	printf("tap open...\n");
	ctx.state = 1;
	memset(sendbuf, 0, sizeof(sendbuf));
	if (strlen(ctx.tapip_serv))
		memcpy(open->tapip, ctx.tapip_serv, strlen(ctx.tapip_serv));
	ctx.sp->send(open, sizeof(tapadpt_open_t), TAPADPT_TYPE_OPEN);

	timeout = 600;
_timeout_02:
	if ((ctx.state) && timeout)
	{
		usleep(5000);
		timeout--;
		goto _timeout_02;
	}
	if (ctx.state == 0)
	{
		printf("tap tunnel established.\n");
	}
	else
	{
		printf("tap tunnel openfaild, state: %d.\n", ctx.state);
		exit(0);
	}

	pthread_join(ctx.pt, NULL);

_fail:
	close(ctx.sock);
	return ret;
}


int main(int argc, char *argv[])
{
	srand((int)time(NULL));

#if 0
	char devname[32];
	memset(devname, 0, sizeof(devname));
	tun_alloc(devname);
	printf("%s\n", devname);
	if (fork() == 0)
	{
		int ret = execl("/bin/bash", "bash", "tapcfg.sh",
			devname, "172.12.1.10", "172.12.1.1", NULL);
		if (ret < 0)
		{
			printf("execl faild, errno: %d\n", errno);
		}
	}
	while (1);
#endif

	if (memcmp(argv[1], "-s", 2) == 0)
	{
		serv_start(argc, argv);
	}
	else if (memcmp(argv[1], "-c", 2) == 0)
	{
		/* argv[2]: dest ip */
		/* argv[3]: dest port */
		/* argv[4]: tap serv ip */
		/* argv[5]: tap local ip */
		/* client */
		cli_start(argc, argv);
	}


	return 0;
}
