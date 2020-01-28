#include "cpkl.h"

CODE_SECTION("====================")
CODE_SECTION("Some Alg")
CODE_SECTION("====================")

static u32 cpkl_alg_getbw(const void *pv, u32 len)
{
	u8 *src = (u8 *)pv;
	u32 i;
	for (i = 0; i < len; i++)
	{
		if (src[i] != 0)
		{
			return ((len - i - 1) * sizeof(u8) * 8 + cpkl_alg_getbw32(src[i]));
		}
	}

	return 0;
}

/*
 * shift one bit
 * return FSB
 */
static u8 cpkl_alg_bs(u8 *buf, u32 len)
{
	u8 ret = (buf[0] & 0x80) ? 0x1 : 0x0;
	u32 i;

	for (i = 0; i < (len - 1); i++)
	{
		buf[i] <<= 1;
		if (buf[i + 1] & 0x80)
			buf[i] |= 0x1;
	}
	buf[i] <<= 1;

	return ret;
}

/*
 * buffer shift cat
 *
 * sl: shiftlen (bit)
 */
static inline void cpkl_alg_bsc(u8 *dst, u32 dstlen, u8 *src, u32 srclen, u32 sl)
{
	u8 tmp;
	while (sl--)
	{
		tmp = cpkl_alg_bs(src, srclen);

		cpkl_alg_bs(dst, dstlen);

		dst[dstlen - 1] |= tmp;
	}
}

/*
 * Galois Field Division
 *  'pv' / 'dvsr'
 *      pv :
 *   pvlen : length of pv (number of byte)
 *    dvsr :
 * dvsrlen : length of dvsr (number of byte)
 *     rmd : result string
 */
static void cpkl_alg_getrmd(const void *pv, u32 pvlen, const void *dvsr, u32 dvsrlen, u8 *rmd)
{
#define	CPKL_CONFIG_MAXDVLEN			(16)
	static u8 dvbuf[CPKL_CONFIG_MAXDVLEN];
	u32 i, dvsrwidth, srcwidth, counter;
	u8 tmp;
	dvsrwidth = cpkl_alg_getbw(dvsr, dvsrlen);
	srcwidth = cpkl_alg_getbw(pv, pvlen);

	i = pvlen * 8 - srcwidth;
	while (i--)
		cpkl_alg_bs((u8 *)pv, pvlen);

	cpkl_pdf_memset(dvbuf, 0, CPKL_CONFIG_MAXDVLEN);

	counter = srcwidth;

	while (counter--)
	{
		tmp = cpkl_alg_bs((u8 *)pv, pvlen);

		cpkl_alg_bs(dvbuf, CPKL_CONFIG_MAXDVLEN);

		dvbuf[CPKL_CONFIG_MAXDVLEN - 1] |= tmp;

		tmp = dvbuf[(CPKL_CONFIG_MAXDVLEN * 8 - dvsrwidth) / 8];
		tmp <<= (CPKL_CONFIG_MAXDVLEN * 8 - dvsrwidth) % 8;

		if (tmp & 0x80)
			for (i = 0; i < dvsrlen; i++)
				dvbuf[CPKL_CONFIG_MAXDVLEN - dvsrlen + i] ^= ((u8 *)dvsr)[i];
	}

	/**/
	tmp = (dvsrwidth - 1 + 7) / 8;
	for (i = 0; i < tmp; i++)
	{
		rmd[i] = dvbuf[CPKL_CONFIG_MAXDVLEN - tmp + i];
	}
}

/*
 * same as ethernet FSC calculation algorithm.
 * e.g. FCS of:
 * FF FF FF FF FF FF 00 00 00 00 00 4A 00 00 00 00
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 * 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 * 00 00 00 00 00 00 00 00 00 00 00 00
 * is 51 DA 8A 29
 *
 */
u32 cpkl_alg_crc32(const void *pv, u32 size)
{
	static const u32 crc_table[] =
	{
		0x4DBDF21C, 0x500AE278, 0x76D3D2D4, 0x6B64C2B0,
		0x3B61B38C, 0x26D6A3E8, 0x000F9344, 0x1DB88320,
		0xA005713C, 0xBDB26158, 0x9B6B51F4, 0x86DC4190,
		0xD6D930AC, 0xCB6E20C8, 0xEDB71064, 0xF0000000
	};
	u32 n, crc = 0;
	const u8 *data = (const u8 *)pv;

	for (n = 0; n < size; n++)
	{
		/* lower nibble */
		crc = (crc >> 4) ^ crc_table[(crc ^ (data[n] >> 0)) & 0x0F];

		/* upper nibble */
		crc = (crc >> 4) ^ crc_table[(crc ^ (data[n] >> 4)) & 0x0F];
	}

	return ((crc >> 24) | (((crc >> 16) & 0xFF) << 8) |
			(((crc >> 8) & 0xFF) << 16) | (crc << 24));
}

/*
 * This is the CRC-32C table
 * poly = 0x1EDC6F41
 */

static const u32 crc32c_table[256] = {
0x00000000L,  0x1edc6f41L,  0x3db8de82L,  0x2364b1c3L,
0x7b71bd04L,  0x65add245L,  0x46c96386L,  0x58150cc7L,
0xf6e37a08L,  0xe83f1549L,  0xcb5ba48aL,  0xd587cbcbL,
0x8d92c70cL,  0x934ea84dL,  0xb02a198eL,  0xaef676cfL,
0xf31a9b51L,  0xedc6f410L,  0xcea245d3L,  0xd07e2a92L,
0x886b2655L,  0x96b74914L,  0xb5d3f8d7L,  0xab0f9796L,
0x05f9e159L,  0x1b258e18L,  0x38413fdbL,  0x269d509aL,
0x7e885c5dL,  0x6054331cL,  0x433082dfL,  0x5deced9eL,
0xf8e959e3L,  0xe63536a2L,  0xc5518761L,  0xdb8de820L,
0x8398e4e7L,  0x9d448ba6L,  0xbe203a65L,  0xa0fc5524L,
0x0e0a23ebL,  0x10d64caaL,  0x33b2fd69L,  0x2d6e9228L,
0x757b9eefL,  0x6ba7f1aeL,  0x48c3406dL,  0x561f2f2cL,
0x0bf3c2b2L,  0x152fadf3L,  0x364b1c30L,  0x28977371L,
0x70827fb6L,  0x6e5e10f7L,  0x4d3aa134L,  0x53e6ce75L,
0xfd10b8baL,  0xe3ccd7fbL,  0xc0a86638L,  0xde740979L,
0x866105beL,  0x98bd6affL,  0xbbd9db3cL,  0xa505b47dL,
0xef0edc87L,  0xf1d2b3c6L,  0xd2b60205L,  0xcc6a6d44L,
0x947f6183L,  0x8aa30ec2L,  0xa9c7bf01L,  0xb71bd040L,
0x19eda68fL,  0x0731c9ceL,  0x2455780dL,  0x3a89174cL,
0x629c1b8bL,  0x7c4074caL,  0x5f24c509L,  0x41f8aa48L,
0x1c1447d6L,  0x02c82897L,  0x21ac9954L,  0x3f70f615L,
0x6765fad2L,  0x79b99593L,  0x5add2450L,  0x44014b11L,
0xeaf73ddeL,  0xf42b529fL,  0xd74fe35cL,  0xc9938c1dL,
0x918680daL,  0x8f5aef9bL,  0xac3e5e58L,  0xb2e23119L,
0x17e78564L,  0x093bea25L,  0x2a5f5be6L,  0x348334a7L,
0x6c963860L,  0x724a5721L,  0x512ee6e2L,  0x4ff289a3L,
0xe104ff6cL,  0xffd8902dL,  0xdcbc21eeL,  0xc2604eafL,
0x9a754268L,  0x84a92d29L,  0xa7cd9ceaL,  0xb911f3abL,
0xe4fd1e35L,  0xfa217174L,  0xd945c0b7L,  0xc799aff6L,
0x9f8ca331L,  0x8150cc70L,  0xa2347db3L,  0xbce812f2L,
0x121e643dL,  0x0cc20b7cL,  0x2fa6babfL,  0x317ad5feL,
0x696fd939L,  0x77b3b678L,  0x54d707bbL,  0x4a0b68faL,
0xc0c1d64fL,  0xde1db90eL,  0xfd7908cdL,  0xe3a5678cL,
0xbbb06b4bL,  0xa56c040aL,  0x8608b5c9L,  0x98d4da88L,
0x3622ac47L,  0x28fec306L,  0x0b9a72c5L,  0x15461d84L,
0x4d531143L,  0x538f7e02L,  0x70ebcfc1L,  0x6e37a080L,
0x33db4d1eL,  0x2d07225fL,  0x0e63939cL,  0x10bffcddL,
0x48aaf01aL,  0x56769f5bL,  0x75122e98L,  0x6bce41d9L,
0xc5383716L,  0xdbe45857L,  0xf880e994L,  0xe65c86d5L,
0xbe498a12L,  0xa095e553L,  0x83f15490L,  0x9d2d3bd1L,
0x38288facL,  0x26f4e0edL,  0x0590512eL,  0x1b4c3e6fL,
0x435932a8L,  0x5d855de9L,  0x7ee1ec2aL,  0x603d836bL,
0xcecbf5a4L,  0xd0179ae5L,  0xf3732b26L,  0xedaf4467L,
0xb5ba48a0L,  0xab6627e1L,  0x88029622L,  0x96def963L,
0xcb3214fdL,  0xd5ee7bbcL,  0xf68aca7fL,  0xe856a53eL,
0xb043a9f9L,  0xae9fc6b8L,  0x8dfb777bL,  0x9327183aL,
0x3dd16ef5L,  0x230d01b4L,  0x0069b077L,  0x1eb5df36L,
0x46a0d3f1L,  0x587cbcb0L,  0x7b180d73L,  0x65c46232L,
0x2fcf0ac8L,  0x31136589L,  0x1277d44aL,  0x0cabbb0bL,
0x54beb7ccL,  0x4a62d88dL,  0x6906694eL,  0x77da060fL,
0xd92c70c0L,  0xc7f01f81L,  0xe494ae42L,  0xfa48c103L,
0xa25dcdc4L,  0xbc81a285L,  0x9fe51346L,  0x81397c07L,
0xdcd59199L,  0xc209fed8L,  0xe16d4f1bL,  0xffb1205aL,
0xa7a42c9dL,  0xb97843dcL,  0x9a1cf21fL,  0x84c09d5eL,
0x2a36eb91L,  0x34ea84d0L,  0x178e3513L,  0x09525a52L,
0x51475695L,  0x4f9b39d4L,  0x6cff8817L,  0x7223e756L,
0xd726532bL,  0xc9fa3c6aL,  0xea9e8da9L,  0xf442e2e8L,
0xac57ee2fL,  0xb28b816eL,  0x91ef30adL,  0x8f335fecL,
0x21c52923L,  0x3f194662L,  0x1c7df7a1L,  0x02a198e0L,
0x5ab49427L,  0x4468fb66L,  0x670c4aa5L,  0x79d025e4L,
0x243cc87aL,  0x3ae0a73bL,  0x198416f8L,  0x075879b9L,
0x5f4d757eL,  0x41911a3fL,  0x62f5abfcL,  0x7c29c4bdL,
0xd2dfb272L,  0xcc03dd33L,  0xef676cf0L,  0xf1bb03b1L,
0xa9ae0f76L,  0xb7726037L,  0x9416d1f4L,  0x8acabeb5L
};

/*
 * Steps through buffer one byte at at time, calculates reflected
 * crc using table.
 */
u32 cpkl_alg_crc32c(const void* pv, u32 size)
{
	u32 crc = 0xffffffff;
	const u8* data = (const u8 *)pv;

	while (size--)
	{
		crc = crc32c_table[(crc >> 24)  ^ *data++] ^ (crc << 8);
	}

	return crc;
}

/*
 * G(x):
 * x^64 + x^62 + x^57 + x^55 + x^54 + x^53 + x^52 +
 * x^47 + x^46 + x^45 + x^40 + x^39 + x^38 + x^37 +
 * x^35 + x^33 + x^32 + x^31 + x^29 + x^27 + x^24 +
 * x^23 + x^22 + x^21 + x^19 + x^17 + x^13 + x^12 +
 * x^10 + x^9  + x^7  + x^4  + x^1  + x^0
 * 0x1 42F0E1EB A9EA3693
 */
static u8 _cpkl_dvsr[] = {0x01, 0x42, 0xF0, 0xE1, 0xEB, 0xA9, 0xEA, 0x36, 0x93};
u64 cpkl_alg_crc64(const void *pv, u32 size)
{
	u64 rst;
	u8 *src = (u8 *)cpkl_malloc(size + 8);
	cpkl_pdf_memcpy(src, pv, size);

	cpkl_alg_getrmd(src, size + 8, _cpkl_dvsr, sizeof(_cpkl_dvsr), (u8 *)&rst);

	cpkl_free(src);

	return rst;
}

u64 cpkl_alg_crc64ck(const void *pv, u32 size)
{
	u64 rst;
	u8 *src = (u8 *)cpkl_malloc(size);
	cpkl_pdf_memcpy(src, pv, size);

	cpkl_alg_getrmd(src, size, _cpkl_dvsr, sizeof(_cpkl_dvsr), (u8 *)&rst);

	cpkl_free(src);

	if (rst == 0)
		return 1;

	return 0;
}

/*
 * return 16bit result
 */
u16 cpkl_alg_foldxor(const void *key, u32 size)
{
	u16 rslt = 0, us;
	const u16 *p = (const u16*)key;
	u32 x;

	for (x = 0; x < (size >> 1); x++)
	{
		us = *p++;
		rslt ^= us;
	}

	if (size & 1)
	{
		rslt ^= (*(const u8 *)p << 8);
	}

	return CPKL_HTONS(rslt);
}


/* binary search */

void *cpkl_alg_bsch(void *base, unsigned num, unsigned width, void *dst, int (*comp)(const void *, const void *))
{
	u32 idx_l = 0, idx_r = num;

	if (num == 0)
		return NULL;

	while ((idx_l + 1) != idx_r)
	{
		u32 idx_m = (idx_l + idx_r) / 2;

		if (comp(dst, CPKL_V2P(CPKL_P2V(base) + idx_m * width)) == 0)
		{
			return CPKL_V2P(CPKL_P2V(base) + idx_m * width);
		}
		else if (comp(dst, CPKL_V2P(CPKL_P2V(base) + idx_m * width)) < 0)
		{
			idx_r = idx_m;
		}
		else
		{
			idx_l = idx_m;
		}
	}

    if (comp(dst, CPKL_V2P(CPKL_P2V(base) + idx_l * width)) == 0)
    {
        return CPKL_V2P(CPKL_P2V(base) + idx_l * width);
    }

	return NULL;

}

/*
 * search and compare with the divflag array, cut the string into several sub string
 * the substring just without the divflag
 */
int cpkl_stdiv
(
	char *buf,			/* input */
	int buflen,			/* input */
	int n_argv,			/* input: sizeof argv */
	char *argv[],		/* output */
	u32 len[],			/* output */
	int n_divflag,		/* input */
	char *divflag,		/* input */
	u32 ctrl			/* input */
)
{
	int i, j, ret = 0, state = 0;		/* 0: begin, 1: not div char 2: div char */

	for (i = 0; i < n_argv; i++)
	{
		argv[i] = NULL;
		len[i] = 0;
	}

	for (i = 0; i < buflen;)
	{
		for (j = 0; j < n_divflag; j++)
		{
			if (buf[i] == divflag[j])
			{
				/* ok, we find one div charactor */

				switch (state)
				{
				case 0:
				case 1:
					state = 2;
					break;
				case 2:
					if (ctrl & CPKL_STDIVCTRL_EMPTYSUBSTR)
					{
						/* need to save this empty flag */

						if (ret == n_argv)
							return ret;

						ret++;
					}
					break;
				default:
					CPKL_ASSERT(0);
				}

				goto cpkl_stdiv_nextch;
			}
		}

		/* reach here, the buf[i] is NOT the div charactor */
		switch (state)
		{
		case 0:
		case 2:
			if (ret == n_argv)
				return ret;

			argv[ret] = &(buf[i]);
			len[ret] = 1;
			ret++;

			state = 1;

			break;
		case 1:
			(len[ret - 1])++;
			break;
		default:
			CPKL_ASSERT(0);
		}

cpkl_stdiv_nextch:

        i++;
	}

    return ret;
}

/* byte swap,
 * [0][1][2][3][4]  --->  [4][3][2][1][0]
 */
void cpkl_bswap(void *p, u32 size)
{
	u8 *buff = (u8 *)p, i, tmp;
	for (i = 0; i < (size >> 1); i++)
	{
		tmp = buff[i];
		buff[i] = buff[size - i - 1];
		buff[size - i - 1] = tmp;
	}
}

void cpkl_hexdump(void *buf, u32 len, char *prefix)
{
#define CPKL_HEXDUMP_LINEWIDTH			(16)
	u8 *p = (u8 *)buf;
	u32 i;

	cpkl_printf("\n%s"
				"       ",
				prefix);
	for (i = 0; i < CPKL_HEXDUMP_LINEWIDTH; i++)
	{
		if ((i % 4) == 0)
			cpkl_printf("%02x ", i);
		else
			cpkl_printf("   ");
	}

	for (i = 0; i < len; i++)
	{
		if ((i % CPKL_HEXDUMP_LINEWIDTH) == 0)
			cpkl_printf("\n%s"
						"0x%03X: ",
						prefix,
						i);
		cpkl_printf("%02X ", *p++);
	}
}

CODE_SECTION("====================")
CODE_SECTION("print with time stamp")
CODE_SECTION("====================")
#ifdef CPKL_CONFIG_PRINT_TMSTAMP
int cpkl_printf(const char *fmt, ...)
{
	static cpkl_custmtx_t lock;
	static u32 initdone = 0;
	va_list args;
	char *tmpbuf;
	int len, i;

	if (initdone == 0)
	{
		cpkl_mtxcreate(&lock);
		initdone = 1;
	}

	tmpbuf = (char *)cpkl_malloc(64 * 1024);

	va_start(args, fmt);
	len = vsnprintf(tmpbuf, 64 * 1024, fmt, args);
	va_end(args);

	cpkl_mtxlock(&lock);
	for (i = 0; i < len; i++)
	{
		cpkl_pdf_printf("%c", tmpbuf[i]);
		if (tmpbuf[i] == '\n')
			cpkl_pdf_printf("[%010lld.%06lld]:", cpkl_tmsstamp() / 1000000, cpkl_tmsstamp() % 1000000);
	}
	cpkl_mtxunlock(&lock);

	cpkl_free(tmpbuf);
	return 0;
}
#endif

CODE_SECTION("====================")
CODE_SECTION("Time statistic")
CODE_SECTION("====================")

/*
 * return: the us of current time
 */
u64 cpkl_tmsstamp(void)
{
	u64	stamp = 0;
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
    LARGE_INTEGER num, curtime;
    QueryPerformanceFrequency(&num);
    QueryPerformanceCounter(&curtime);
    stamp = curtime.QuadPart * 1000000 / num.QuadPart;
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	struct timeval		curtime;
	gettimeofday(&curtime, NULL);
	stamp = curtime.tv_sec * 1000000 + curtime.tv_usec;
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
	stamp = cpu_clock(0);	/* get ns */
	stamp /= 1000;
#endif

	return stamp;
}

#ifdef CPKL_CONFIG_TMS

#ifndef CPKL_CONFIG_TMSNUM
#define	CPKL_CONFIG_TMSNUM				(64)
#endif

#define CPKL_TMSCOMMLEN					(32)

typedef struct _cpkl_tmstat {
	char		comm[CPKL_TMSCOMMLEN];
	u64	start, sum;
	int			swch, hasinit;
} cpkl_tmstat_t;

static cpkl_tmstat_t _cpkl_tmsar[CPKL_CONFIG_TMSNUM];

static void cpkl_tmsrepone(int tmsidx)
{
	char fmtstr[128];

	CPKL_ASSERT(tmsidx < CPKL_CONFIG_TMSNUM);

	if (_cpkl_tmsar[tmsidx].hasinit == 0)
		return;

	if (_cpkl_tmsar[tmsidx].swch)
	{
		cpkl_tms(tmsidx, CPKL_TMS_OFF);
		cpkl_printf("\ntms %2d just take off.", tmsidx);
	}

	cpkl_pdf_sprintf(fmtstr, 128, "\n\"%%%ds\" tms[%%2d] : %%10llu(us)", CPKL_TMSCOMMLEN);
    cpkl_printf(fmtstr,
				_cpkl_tmsar[tmsidx].comm,
				tmsidx,
				_cpkl_tmsar[tmsidx].sum);
}

void cpkl_tms(int tmsidx, int swch)
{
	CPKL_ASSERT(tmsidx < CPKL_CONFIG_TMSNUM);

	if (_cpkl_tmsar[tmsidx].hasinit == 0)
	{
		cpkl_printf("\ntms %2d need to reset first.", tmsidx);
		return;
	}

	if (swch == CPKL_TMS_ON)
	{
		CPKL_ASSERT(_cpkl_tmsar[tmsidx].swch == CPKL_TMS_OFF);

		_cpkl_tmsar[tmsidx].swch = CPKL_TMS_ON;
		_cpkl_tmsar[tmsidx].start = cpkl_tmsstamp();
	}
	else if (swch == CPKL_TMS_OFF)
	{
		CPKL_ASSERT(_cpkl_tmsar[tmsidx].swch == CPKL_TMS_ON);

		_cpkl_tmsar[tmsidx].sum += cpkl_tmsstamp() - _cpkl_tmsar[tmsidx].start;
		_cpkl_tmsar[tmsidx].swch = CPKL_TMS_OFF;
	}
	else
	{
		cpkl_printf("\nTimer Err. switch:%d", swch);
	}
}

void cpkl_tmsreset(int tmsidx, char *comm)
{
	CPKL_ASSERT(tmsidx < CPKL_CONFIG_TMSNUM);

	cpkl_pdf_memset(&(_cpkl_tmsar[tmsidx]), 0, sizeof(cpkl_tmstat_t));

	if (comm)
	{
		u32 cl = (u32)cpkl_pdf_strlen(comm);
		if (cl >= CPKL_TMSCOMMLEN)
		{
			cl = CPKL_TMSCOMMLEN - 1;
		}
		cpkl_pdf_memcpy(&(_cpkl_tmsar[tmsidx].comm), comm, cl);
	}

	_cpkl_tmsar[tmsidx].hasinit = 1;
}

void cpkl_tmreport(int tmsidx)
{
	CPKL_ASSERT(tmsidx < CPKL_CONFIG_TMSNUM);

	if (tmsidx == CPKL_TMSREPORTALL)
	{
		int i;
		for (i = 0; i < CPKL_CONFIG_TMSNUM; i++)
		{
			cpkl_tmsrepone(i);
		}
	}
	else
	{
		cpkl_tmsrepone(tmsidx);
	}
}

#endif

CODE_SECTION("====================")
CODE_SECTION("Random Infrastructure")
CODE_SECTION("====================")

#ifdef CPKL_CONFIG_RI
void cpkl_ri_seed(void)
{
	cpkl_pdf_srand((int)cpkl_pdf_time(NULL));
}

u32 cpkl_ri_rand(u32 begin, u32 count)
{
	u64 orig, orig_range;

	if (count <= ((u32)RAND_MAX + 1))
	{
		orig = cpkl_pdf_rand();
		orig_range = (u64)RAND_MAX - 0 + 1;
	}
	else
	{
		orig = (((u64)cpkl_ri_rand(0, 0xFF)) << 24) |
			   (((u64)cpkl_ri_rand(0, 0xFF)) << 16) |
			   (((u64)cpkl_ri_rand(0, 0xFF)) << 8) |
			   (((u64)cpkl_ri_rand(0, 0xFF)) << 0);
		orig_range = 0x100000000LL;
	}

	orig *= count;
	orig /= orig_range;
	orig += begin;

	CPKL_ASSERT((orig >= begin) && (orig < (begin + count)));

	return (u32)orig;
}

u32 *cpkl_ri_rdgen(u32 *distri, u32 n_distri)
{
	u32 i, j, r, sum, total = 0, *curdis, *ret;

	CPKL_ASSERT(distri != NULL);

	curdis = (u32 *)cpkl_malloc(n_distri * sizeof(u32));
	if (curdis == NULL)
		return NULL;

	for (i = 0; i < n_distri; i++)
	{
		total += distri[i];
		curdis[i] = distri[i];
	}
	ret = (u32 *)cpkl_malloc(total * sizeof(u32));
	if (ret == NULL)
	{
		cpkl_free(curdis);
		return NULL;
	}

	for (i = 0; i < total; i++)
	{
		r = cpkl_ri_rand(0, total - i);
		sum = 0;

		for (j = 0; j < n_distri; j++)
		{
			sum += curdis[j];
			if (r < sum)
			{
				(curdis[j])--;
				ret[i] = j;
				break;
			}
		}
	}

	cpkl_free(curdis);
	return ret;
}

#ifdef CPKL_CONFIG_DEBUG
void cpkl_ri_test(void)
{
	u32 *ret, tmp[16] = {3, 1, 2, 2};

	cpkl_ri_seed();
	ret = cpkl_ri_rdgen(tmp, 4);
	cpkl_free(ret);

}
#endif

#endif

CODE_SECTION("====================")
CODE_SECTION("Custom Signal")
CODE_SECTION("====================")

/*
 * after create, the signal is without sig
 */
int _cpkl_sigcreate(cpkl_custsig_t *sig, u32 initsig, u32 maxsig, char *filename, const char *funcname, u32 line)
{
	int ret = 0;

	sig->maxsig = maxsig;

#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
	sig->sig = CreateSemaphore(NULL, initsig, maxsig, NULL);
	if (sig->sig == 0)
		ret = -1;
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	if (sem_init(&(sig->u_sem), 0, initsig) != 0)
	{
		ret = -1;
#ifdef CPKL_CONFIG_DEBUG
		cpkl_printf("\nsem_init() faild, errno : %d"
					"\nfile: %s, func: %s, line: %d",
					errno,
					filename, funcname, line);
#endif
	}
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
	sema_init(&(sig->k_sem), initsig);
#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif


#ifdef CPKL_CONFIG_DEBUG
	sig->tmsum = 0;
	sig->times = 0;
#endif

    return ret;
}

void _cpkl_sigdsty(cpkl_custsig_t *sig, char *filename, const char *funcname, u32 line)
{
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
	CloseHandle(sig->sig);
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	sem_destroy(&(sig->u_sem));
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
	/* todo: kernel semaphore destroy */
#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif

#ifdef CPKL_CONFIG_DEBUG
	/* tmstat */
	cpkl_printf("\nhsp_sigdsty(), func: %s, line: %d, "
				"sum: %lld(us), "
				"times: %lld",
				funcname, line,
				sig->tmsum,
				sig->times);
#endif
}

/* send signal*/
void _cpkl_sigsend(cpkl_custsig_t *sig, char *filename, const char *funcname, u32 line)
{
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
	ReleaseSemaphore(sig->sig, 1, NULL);
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	if (sem_post(&(sig->u_sem)) == -1)
	{
#ifdef CPKL_CONFIG_DEBUG
		cpkl_printf("\nsemaphore up operate failed. error:%d"
					"\nfile: %s, func: %s, line: %d",
					errno,
					filename, funcname, line);
#endif
	}
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
	up(&(sig->k_sem));
#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif
}

/* wait for signal */
int _cpkl_sigwait(cpkl_custsig_t *sig, const char *filename, const char *funcname, u32 line)
{
#ifdef CPKL_CONFIG_DEBUG
	/* tmstat in sig block */
	u64 tmbg = cpkl_tmsstamp();
#endif
	int ret = 0;

#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
    WaitForSingleObject(sig->sig, INFINITE);
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	if (sem_wait(&(sig->u_sem)) == -1)
	{
#ifdef CPKL_CONFIG_DEBUG
		cpkl_printf("\nsemaphore down operate failed. error:%d"
					"\nfile: %s, func: %s, line: %d",
					errno,
					filename, funcname, line);
#endif
	}
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
	down(&(sig->k_sem));
#endif

#ifdef CPKL_CONFIG_DEBUG
	sig->tmsum += (cpkl_tmsstamp() - tmbg);
	(sig->times)++;
#endif

    return ret;
}

CODE_SECTION("====================")
CODE_SECTION("Custom Mutex")
CODE_SECTION("====================")

int _cpkl_mtxcreate(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line)
{
	int ret = 0;

#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
#ifdef CPKL_MUTEX_CS
	InitializeCriticalSection(&(mtx->cs));
#else
	mtx->mtx = CreateMutex(NULL, FALSE, NULL);

	if (mtx->mtx == NULL)
	{
		ret = -1;
	}
#endif
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	if (pthread_mutex_init(&(mtx->mtx), NULL) != 0)
	{
		ret = -1;
		cpkl_printf("\npthread_mutex_init() faild, errno : %d"
					"\nfile: %s, func: %s, line: %d",
					errno,
					filename, funcname, line);
	}
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif

	mtx->times = mtx->tmsum = 0;

	return ret;
}

void _cpkl_mtxdsty(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line)
{
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
#ifdef CPKL_MUTEX_CS
	DeleteCriticalSection(&(mtx->cs));
#else
	CloseHandle(mtx->mtx);
#endif
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	pthread_mutex_destroy(&(mtx->mtx));
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif

#ifdef CPKL_CONFIG_DEBUG
	/* tmstat */
	cpkl_printf("\ncpkl_mtxdsty(), func: %s, line: %d, "
				"sum: %lld(us), "
				"times: %lld",
				funcname, line,
				mtx->tmsum,
				mtx->times);
#endif
}

int _cpkl_mtxlock(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line)
{
#ifdef CPKL_CONFIG_DEBUG
	/* tmstat in sig block */
	u64 tmbg = cpkl_tmsstamp();
#endif
	int ret = 0;

#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
#ifdef CPKL_MUTEX_CS
	EnterCriticalSection(&(mtx->cs));
#else
	WaitForSingleObject(mtx->mtx, INFINITE);
#endif
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	if (pthread_mutex_lock(&(mtx->mtx)) != 0)
	{
		ret = -1;
#ifdef CPKL_CONFIG_DEBUG
		cpkl_printf("\nmutex lock operate failed. error:%d"
					"\nfile: %s, func: %s, line: %d",
					errno,
					filename, funcname, line);
#endif
	}
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif

#ifdef CPKL_CONFIG_DEBUG
	mtx->tmsum += (cpkl_tmsstamp() - tmbg);
	(mtx->times)++;
#endif

	return ret;
}

int _cpkl_mtxtrylock(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line)
{
	int ret = 0;

#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
#ifdef CPKL_MUTEX_CS
	EnterCriticalSection(&(mtx->cs));
#else
	ret = WaitForSingleObject(mtx->mtx, 0);
#endif
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	ret = pthread_mutex_trylock(&(mtx->mtx));
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif


#ifdef CPKL_CONFIG_DEBUG
	(mtx->times)++;
#endif

	return ret;
}

void _cpkl_mtxunlock(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line)
{
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
#ifdef CPKL_MUTEX_CS
	LeaveCriticalSection(&(mtx->cs));
#else
	ReleaseMutex(mtx->mtx);
#endif
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	if (pthread_mutex_unlock(&(mtx->mtx)) != 0)
	{
#ifdef CPKL_CONFIG_DEBUG
		cpkl_printf("\nmutex unlock operate failed. error:%d"
					"\nfile: %s, func: %s, line: %d",
					errno,
					filename, funcname, line);
#endif
	}
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD

#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif
}

CODE_SECTION("====================")
CODE_SECTION("error log system")
CODE_SECTION("====================")

cpkl_errlog_t* cpkl_elnew(cpkl_els_t *els, void *buf, u32 len)
{
	cpkl_errlog_t *newel = (cpkl_errlog_t *)cpkl_malloc(sizeof(cpkl_errlog_t) + len);

	if (newel == NULL)
		return NULL;

	cpkl_pdf_memcpy(newel->buf, buf, len);

	cpkl_mtxlock(&(els->lock));

	cpkl_pdf_time(&(newel->stamp));
	cpkl_listadd_tail(&(newel->ln), &(els->head));
	(els->n_el)++;

	cpkl_mtxunlock(&(els->lock));

	return newel;
}

cpkl_errlog_t* cpkl_eldel(cpkl_els_t *els)
{
	cpkl_errlog_t *ret = NULL;

	cpkl_mtxlock(&(els->lock));

	if (els->n_el == 0)
		goto _ret;

	ret = CPKL_GETCONTAINER(els->head.next, cpkl_errlog_t, ln);

	cpkl_listdel(els->head.next);
	(els->n_el)--;

_ret:
	cpkl_mtxlock(&(els->lock));

	return ret;
}

CPKL_FCTNEW_DEFINE(cpkl_els_t)
{
	if (param == NULL)
		return NULL;

	cpkl_elsfcp_t *fcp = (cpkl_elsfcp_t *)param;

	cpkl_els_t *newels = (cpkl_els_t *)cpkl_malloc(sizeof(cpkl_els_t));
	if (newels == NULL)
		return NULL;

	cpkl_initlisthead(&(newels->head));

	cpkl_mtxcreate(&(newels->lock));

	return newels;
}

CPKL_FCTDEL_DEFINE(cpkl_els_t)
{
	cpkl_els_t *els = (cpkl_els_t *)obj;

	cpkl_errlog_t *p, *n;
	CPKL_LISTENTRYWALK_SAVE(p, n, cpkl_errlog_t, &(els->head), ln)
	{
		cpkl_free(p);
	}

	cpkl_mtxdsty(&(els->lock));

	cpkl_free(els);
}

CODE_SECTION("====================")
CODE_SECTION("BST Implementation")
CODE_SECTION("====================")

#if CPKL_CONFIG_BSTTYPE == CPKL_CONFIG_BSTTYPE_AVL

CODE_SECTION("BST AVL")

/* inbalance factor
 * hl - hr
 */
static int cpkl_avl_inbf(cpkl_avln_t *node)
{
	CPKL_ASSERT(node != NULL);

	u32 lch = (node->lc != NULL) ? (node->lc->subth) : 0;
	u32 rch = (node->rc != NULL) ? (node->rc->subth) : 0;

	return (lch - rch);
}

/*
 * refresh subtree height
 * return : 1, need to reballance
 *			0, no need
 */
static u32 cpkl_avl_rfh(cpkl_avln_t *node)
{
	CPKL_ASSERT(node != NULL);

	int oldh = node->subth;
	int lch = (node->lc != NULL) ? (node->lc->subth) : 0;
	int rch = (node->rc != NULL) ? (node->rc->subth) : 0;

	node->subth = (lch > rch) ? (lch + 1) : (rch + 1);

	return ((((lch - rch) > 1) || ((lch - rch) < -1)) ||
			(oldh != node->subth));
}

/* avl tree auto balance
 * <-_
 *    \
 *    |    this is ths left rotate
 *    /
 *
 */
static void cpkl_avl_lr(cpkl_avln_t **opn)
{
	/* father node field address */
	cpkl_avln_t *r = *opn;
	cpkl_avln_t *rc = r->rc;

	rc->f = r->f;

	r->f = rc;
	r->rc = rc->lc;
	if (r->rc)
		r->rc->f = r;
	cpkl_avl_rfh(r);

	rc->lc = r;
	cpkl_avl_rfh(rc);

	*opn = rc;
}

/*
 *   _->
 *  /
 *  |      this is the right rotate
 *  \
 */
static void cpkl_avl_rr(cpkl_avln_t **opn)
{
	cpkl_avln_t *r = *opn;
	cpkl_avln_t *lc = r->lc;

	lc->f = r->f;

	r->f = lc;
	r->lc = lc->rc;
	if (r->lc)
		r->lc->f = r;
	cpkl_avl_rfh(r);

	lc->rc = r;
	cpkl_avl_rfh(lc);

	*opn = lc;
}

/*
 * auto ballance opeartion
 * start from the AVL node refer by 'cn'
 */
static void cpkl_avl_ab(cpkl_avln_t **root, cpkl_avln_t *cn)
{
	cpkl_avln_t **cnp, *wln = cn;

	/* if we insert the root node, no need to change anything either */
	while (wln)
	{
		/* refresh the father node's heigth */
		if (cpkl_avl_rfh(wln) == 0)
		{
			break;
		}

		/* rotation may be occupyed, we need to store the father node's now */
		cn = wln;
		wln = wln->f;

		if (cpkl_avl_inbf(cn) < -1)
		{
			if (cpkl_avl_inbf(cn->rc) > 0)
			{
				cpkl_avl_rr(&(cn->rc));
			}

			if (cn->f == NULL)
				cnp = root;
			else
				cnp = (cn->f->lc == cn) ? &(cn->f->lc) : &(cn->f->rc);

			cpkl_avl_lr(cnp);
		}
		else if (cpkl_avl_inbf(cn) > 1)
		{
			if (cpkl_avl_inbf(cn->lc) < 0)
			{
				cpkl_avl_lr(&(cn->lc));
			}

			if (cn->f == NULL)
				cnp = root;
			else
				cnp = (cn->f->lc == cn) ? &(cn->f->lc) : &(cn->f->rc);

			cpkl_avl_rr(cnp);
		}
	}
}

/*
 * this is the AVL node cut function, without auto ballance
 * treeinfo stored in rmnode should NOT erase
 */
static void cpkl_avl_ndcut(cpkl_avln_t **root, cpkl_avln_t *rmnode)
{
	/* new child after cut */
	cpkl_avln_t *nc = NULL;

	CPKL_ASSERT((root != NULL) && (rmnode != NULL));

	if ((rmnode->lc == NULL) && (rmnode->rc == NULL))
	{
		nc = NULL;
	}
	else if (rmnode->lc == NULL)		/* that means rmnode->rc != NULL */
	{
		nc = rmnode->rc;
	}
	else if (rmnode->rc == NULL)		/* that means rmnode->lc != NULL */
	{
		nc = rmnode->lc;
	}
	else
	{
		CPKL_ASSERT(0);
	}

    if (nc)
	    nc->f = rmnode->f;

	if (rmnode->f != NULL)
	{
		root = (rmnode->f->lc == rmnode) ? &(rmnode->f->lc) : &(rmnode->f->rc);
	}

	*root = nc;

}

/*
 * return :  0, success
 			-1, invalid insert node
 */
int cpkl_bst_insert(cpkl_bstn_t **root, cpkl_bstn_t *newnode, cpkl_bstncmp cmpf)
{
	CPKL_ASSERT((root != NULL) && (newnode != NULL));

	cpkl_bstn_t **cnp = root;		/* cur node pointer */
	cpkl_bstn_t *cn = NULL;			/* cur node */

	while (*cnp)
	{
		cn = *cnp;
		switch(cmpf(newnode, cn))
		{
		case CPKL_BSTCMP_1LT2:
		{
			cnp = &(cn->lc);
            break;
		}
		case CPKL_BSTCMP_1BT2:
		{
			cnp = &(cn->rc);
            break;
		}
		default:
			return -1;
		}
	}
	/* we have found the right position, let's save it */
	*cnp = newnode;
	newnode->f = cn;

	/* before auto ballance, the newnode is a leafnode */
	newnode->lc = newnode->rc = NULL;
	newnode->subth = 1;

	cpkl_avl_ab(root, cn);

	return 0;
}


/*
 *
 */
void cpkl_bst_remove(cpkl_bstn_t **root, cpkl_bstn_t *rmnode)
{
	cpkl_bstn_t *rbpos;		/* reballance posision */

	if ((rmnode->lc != NULL) && (rmnode->rc != NULL))
	{
		cpkl_bstn_t *rmpos;

		/* find the position, substitude the 'rmnode' with the info in rmpos
		 * and cut the rmpos
		 */
		if (rmnode->lc->subth > rmnode->rc->subth)
		{
			/* find the most right child node
			   in the left subtree */
			rmpos = rmnode->lc;
			while (rmpos->rc != NULL)
				rmpos = rmpos->rc;
		}
		else
		{
			/* find the most left child node
			   in the right subtree */
			rmpos = rmnode->rc;
			while (rmpos->lc != NULL)
				rmpos = rmpos->lc;
		}

		rbpos = rmpos->f;
        if (rbpos == rmnode)
        {
            rbpos = rmpos;
        }

		/* first we cut the rmpos node */
		cpkl_avl_ndcut(root, rmpos);

		/* then substitude the rmnode with rmpos
		 * it means rmpos sit on the rmnode's position
		 */
		rmpos->f = rmnode->f;
		rmpos->lc = rmnode->lc;
		rmpos->rc = rmnode->rc;
		rmpos->subth = rmnode->subth;
		if (rmpos->lc)
			rmpos->lc->f = rmpos;
		if (rmpos->rc)
			rmpos->rc->f = rmpos;

		/* fix bug, fatal error, hard to catch. 2016-02-26 */
		/*
		if (rmnode->f != NULL)
		{
			root = (rmnode->f->lc == rmnode) ? &(rmnode->f->lc) : &(rmnode->f->rc);
		}
		*root = rmpos;
		*/
		if (rmpos->f != NULL)
		{
			if (rmpos->f->lc == rmnode)
				rmpos->f->lc = rmpos;
			else
				rmpos->f->rc = rmpos;
		}
		else
			*root = rmpos;
	}
	else
	{
		rbpos = rmnode->f;

		/* now we just cut the rmnode directly */
		cpkl_avl_ndcut(root, rmnode);
	}

	/* auto ballance */
	cpkl_avl_ab(root, rbpos);
}

cpkl_bstn_t* cpkl_bst_lkup(cpkl_bstn_t *root, cpkl_bstn_t* dest, cpkl_bstncmp cmpf)
{
	int cmpret = 0;
	/* use the 'root'var as the walker pointer directlly */
	while (root)
	{
		cmpret = cmpf(dest, root);
		switch (cmpret)
		{
		case CPKL_BSTCMP_1LT2:
			root = root->lc;
			break;
		case CPKL_BSTCMP_1BT2:
			root = root->rc;
            break;
		case CPKL_BSTCMP_1IN2:
		case CPKL_BSTCMP_1EQ2:
			return root;
        default:
			return NULL;
		}
	}

	return NULL;
}

int cpkl_bst_walk(cpkl_bstn_t *root, u32 walktype, cpkl_bstwkop op, void *param)
{
	if (root == NULL)
		return 0;

	if (root->subth == 1)
	{
		return op(root, param);
	}

	switch (walktype)
	{
	case CPKL_BSTWALKTYPE_LMR:
	case CPKL_BSTWALKTYPE_LRM:
	{
		if (cpkl_bst_walk(root->lc, walktype, op, param) == -1)
			return -1;
		if (walktype == CPKL_BSTWALKTYPE_LMR)
		{
			if (op(root, param) == -1)
				return -1;
			if (cpkl_bst_walk(root->rc, walktype, op, param) == -1)
				return -1;
		}
		else
		{
			if (cpkl_bst_walk(root->rc, walktype, op, param) == -1)
				return -1;
			if (op(root, param) == -1)
				return -1;
		}

		break;
	}
	case CPKL_BSTWALKTYPE_MLR:
	case CPKL_BSTWALKTYPE_MRL:
	{
		if (op(root, param) == -1)
			return -1;

		if (walktype == CPKL_BSTWALKTYPE_MLR)
		{
			if (cpkl_bst_walk(root->lc, walktype, op, param) == -1)
				return -1;
			if (cpkl_bst_walk(root->rc, walktype, op, param) == -1)
				return -1;
		}
		else
		{
			if (cpkl_bst_walk(root->rc, walktype, op, param) == -1)
				return -1;
			if (cpkl_bst_walk(root->lc, walktype, op, param) == -1)
				return -1;
		}

		break;
	}
	case CPKL_BSTWALKTYPE_RLM:
	case CPKL_BSTWALKTYPE_RML:
	{
		if (cpkl_bst_walk(root->rc, walktype, op, param) == -1)
			return -1;

		if (walktype == CPKL_BSTWALKTYPE_RLM)
		{
			if (cpkl_bst_walk(root->lc, walktype, op, param) == -1)
				return -1;
			if (op(root, param) == -1)
				return -1;
		}
		else
		{
			if (op(root, param) == -1)
				return -1;
			if (cpkl_bst_walk(root->lc, walktype, op, param) == -1)
				return -1;
		}

		break;
	}
	default:
		return -1;
	}

    return 0;
}

void cpkl_bst_ndmv(cpkl_bstn_t **root, cpkl_bstn_t *from, cpkl_bstn_t *to)
{
	if (from->f)
	{
		if (from->f->lc == from)
			from->f->lc = to;
		else
			from->f->rc = to;
	}
	else
	{
		*root = to;
	}

	if (from->lc)
		from->lc->f = to;
	if (from->rc)
		from->rc->f = to;

	to->f = from->f;
	to->lc = from->lc;
	to->rc = from->rc;
}

#ifdef CPKL_CONFIG_DEBUG
/* height check */
static int cpkl_avlhc(cpkl_avln_t *root)
{
	u32 lh, rh;
	if (root == NULL)
		return 0;

	if (root->lc)
	{
		if (cpkl_avlhc(root->lc))
		{
			return -1;
		}

		lh = root->lc->subth;
	}
	else
	{
		lh = 0;
	}

	if (root->rc)
	{
		if (cpkl_avlhc(root->rc))
		{
			return -1;
		}

		rh = root->rc->subth;
	}
	else
	{
		rh = 0;
	}

	if (root->subth != ((lh > rh) ? (lh + 1) : (rh + 1)))
	{
		CPKL_ASSERT(0);
		return -1;
	}

	return 0;
}


/* ballance check */
static int cpkl_avlbc(cpkl_avln_t *root)
{
	int lh, rh;
	if (root == NULL)
		return 0;

	if (root->lc)
	{
		if (cpkl_avlbc(root->lc))
		{
			return -1;
		}

		lh = (int)(root->lc->subth);
	}
	else
	{
		lh = 0;
	}

	if (root->rc)
	{
		if (cpkl_avlbc(root->rc))
		{
			return -1;
		}

		rh = (int)(root->rc->subth);
	}
	else
	{
		rh = 0;
	}

	if (!(((lh - rh) > -2) && ((lh - rh) < 2)))
	{
		CPKL_ASSERT(0);
		return -1;
	}

	return 0;
}

/* data ralationship check */
static int cpkl_avldrc(cpkl_avln_t *root, cpkl_bstncmp cmpf)
{
	if (root == NULL)
		return 0;

	if (root->lc)
	{
		if (root->lc->f != root)
		{
			cpkl_printf("\nroot->lc->f != root");
			goto cpkl_avldrc_faild;
		}

		if (!((cmpf(root->lc, root) == CPKL_BSTCMP_1LT2) ||
			  (cmpf(root->lc, root) == CPKL_BSTCMP_1EQ2)))
		{
			cpkl_printf("\n(!(cmpf(root->lc, root) < 0))");
			goto cpkl_avldrc_faild;
		}

		if (cpkl_avldrc(root->lc, cmpf))
		{
			goto cpkl_avldrc_faild;
		}
	}

	if (root->rc)
	{
		if (root->rc->f != root)
		{
			cpkl_printf("\nroot->rc->f != root");
			goto cpkl_avldrc_faild;
		}

		if (!((cmpf(root->rc, root) == CPKL_BSTCMP_1BT2) ||
			  (cmpf(root->rc, root) == CPKL_BSTCMP_1EQ2)))
		{
			cpkl_printf("\n(!(cmpf(root->rc, root) > 0))");
			goto cpkl_avldrc_faild;
		}

		if (cpkl_avldrc(root->rc, cmpf))
		{
			goto cpkl_avldrc_faild;
		}
	}
	return 0;

cpkl_avldrc_faild:
	return -1;
}

/* * avl valid check * */
int cpkl_avlvldck(cpkl_bstn_t *root, cpkl_bstncmp cmpf)
{
	/* height check */
	if (cpkl_avlhc(root))
		return -1;

	/* ballance check */
	if (cpkl_avlbc(root))
		return -1;

	/* data check */
	if (cpkl_avldrc(root, cmpf))
		return -1;

	return 0;
}

#endif

#elif CPKL_CONFIG_BSTTYPE == CPKL_CONFIG_BSTTYPE_RBTREE

CODE_SECTION("BST RBTree")

#error "Now we don't support the RBTree as the BST, try to use the AVL"
#else
#error "Binary Search Tree algtype not support, check the MARCO 'CPKL_CONFIG_BSTTYPE' definition."
#endif

/* add some test code for the BST */
#ifdef CPKL_CONFIG_DEBUG

typedef struct _cpkl_bsttestn {
	u32 val;
	cpkl_bstn_t bstn;
} cpkl_bsttestn_t;

static int cpkl_bsttestcmp(cpkl_bstn_t *n1, cpkl_bstn_t *n2)
{
	cpkl_bsttestn_t *p1 = CPKL_GETCONTAINER(n1, cpkl_bsttestn_t, bstn);
	cpkl_bsttestn_t *p2 = CPKL_GETCONTAINER(n2, cpkl_bsttestn_t, bstn);

	if (p1->val < p2->val)
		return CPKL_BSTCMP_1LT2;
	else if (p1->val > p2->val)
		return CPKL_BSTCMP_1BT2;
	else
		return CPKL_BSTCMP_1EQ2;
}


void cpkl_bsttest(void)
{
	const u32 testtimes = 1024 * 128;
	u32 i, tmp, *src, *dist;
	cpkl_bsttestn_t *bstar;
	cpkl_bstn_t *root = NULL;

	bstar = (cpkl_bsttestn_t *)cpkl_malloc(sizeof(cpkl_bsttestn_t) * testtimes);

	if (bstar == NULL)
		return;

	src = (u32 *)cpkl_malloc(testtimes * sizeof(u32));
	if (src == NULL)
	{
		cpkl_free(bstar);
		return;
	}
	for (i = 0; i < testtimes; i++)
		src[i] = 1;

	dist = cpkl_ri_rdgen(src, testtimes);
	if (dist == NULL)
	{
		cpkl_free(bstar);
		cpkl_free(src);
		return;
	}
	for (i = 0; i < testtimes; i++)
	{
		tmp = dist[i];
		bstar[i].val = tmp << 4;
	}

	/* insert */
	cpkl_tmsreset(0, "bst test insert");
	cpkl_tms(0, CPKL_TMS_ON);
	for (i = 0; i < testtimes; i++)
	{
		CPKL_ASSERT(cpkl_bst_insert(&root, &(bstar[i].bstn), cpkl_bsttestcmp) == 0);
	}
	cpkl_tms(0, CPKL_TMS_OFF);

	/* lookup */
	cpkl_tmsreset(1, "bst test lookup");
	cpkl_tms(1, CPKL_TMS_ON);
	for (i = 0; i < testtimes; i++)
	{
		cpkl_bstn_t *p = cpkl_bst_lkup(root, &(bstar[i].bstn), cpkl_bsttestcmp);
		cpkl_bsttestn_t *tn = CPKL_GETCONTAINER(p, cpkl_bsttestn_t, bstn);
		CPKL_ASSERT(tn->val == bstar[i].val);
	}
	cpkl_tms(1, CPKL_TMS_OFF);

	/* remove */
	cpkl_tmsreset(2, "bst test remove");
	cpkl_tms(2, CPKL_TMS_ON);
	for (i = 0; i < testtimes; i++)
	{
		cpkl_bst_remove(&root, &(bstar[i].bstn));;
	}
	cpkl_tms(2, CPKL_TMS_OFF);

	CPKL_ASSERT(root == NULL);

	cpkl_tmreport(CPKL_TMSREPORTALL);

	/*  */
	cpkl_free(bstar);
	cpkl_free(src);
	cpkl_free(dist);

	cpkl_mmcheck();
}

#endif

CODE_SECTION("====================")
CODE_SECTION("Memory monitor")
CODE_SECTION("====================")


#ifdef CPKL_CONFIG_MEMMONITOR

typedef struct _cpkl_mmblkinf {
	cpkl_bstn_t	bstn;
	const char	*filename, *funcname;
	u32	line;
	u32	s_real, s_occupy;
} cpkl_mmblkinf_t;

/**/
static u32				cpkl_mmstatinit = 0;

/* this is the memory monitor BST root */
static cpkl_bstn_t		*cpkl_mmroot = NULL;

/* in the multi thread env, we need to protect the cpkl_malloc and cpkl_free */
static cpkl_custmtx_t	cpkl_mmlocker;

/* memory statinfo */
static cpkl_mmstat_t	cpkl_mmstat = {0};

/* range compire */
static int cpkl_mmrgcmp(cpkl_bstn_t *n1, cpkl_bstn_t *n2)
{
	cpkl_mmblkinf_t *p1 = CPKL_GETCONTAINER(n1, cpkl_mmblkinf_t, bstn);
	cpkl_mmblkinf_t *p2 = CPKL_GETCONTAINER(n2, cpkl_mmblkinf_t, bstn);

	if (CPKL_P2V(p1) < CPKL_P2V(p2))
		return CPKL_BSTCMP_1LT2;
	else if (CPKL_P2V(p1) > CPKL_P2V(p2))
		return CPKL_BSTCMP_1BT2;
	else
		return CPKL_BSTCMP_1EQ2;
}

static int cpkl_mmchkwalk(cpkl_bstn_t *n1, void *param)
{
	cpkl_mmblkinf_t *p1 = CPKL_GETCONTAINER(n1, cpkl_mmblkinf_t, bstn);

	cpkl_printf("\nfile: %s, function: %s, line: %d, size: %d, occupy: %d",
				p1->filename, p1->funcname, p1->line,
				p1->s_real, p1->s_occupy);

	return 0;
}

static void cpkl_mminit(void)
{
	cpkl_mtxcreate(&cpkl_mmlocker);
	cpkl_mmstatinit = 1;
}

void *_cpkl_malloc
(
	u32 size,
	const char *filename,
	const char *funcname,
	u32 line
)
{
	if (cpkl_mmstatinit == 0)
		cpkl_mminit();

	/*  */
	u32 occupy = 0x1;
	while (occupy < size)
		occupy <<= 1;

	cpkl_mmblkinf_t *p = (cpkl_mmblkinf_t *)cpkl_pdf_malloc(sizeof(cpkl_mmblkinf_t) + size);

	if (p == NULL)
	{
		return NULL;
	}

	/* we need to get locker */
	cpkl_mtxlock(&cpkl_mmlocker);

	cpkl_mmstat.real += size;
	cpkl_mmstat.occupy += occupy;
	if (cpkl_mmstat.real_max < cpkl_mmstat.real)
	{
		cpkl_mmstat.real_max = cpkl_mmstat.real;
		cpkl_mmstat.occupy_max = cpkl_mmstat.occupy;
		cpkl_mmstat.max_filename = filename;
		cpkl_mmstat.max_funcname = funcname;
		cpkl_mmstat.max_line = line;
	}

	/* insert the node into BST, it will NOT failed. */
	CPKL_ASSERT(cpkl_bst_insert(&cpkl_mmroot, &(p->bstn), cpkl_mmrgcmp) == 0);

	/* release locker */
	cpkl_mtxunlock(&cpkl_mmlocker);

	p->filename = filename;
	p->funcname = funcname;
	p->line = line;
	p->s_real = size;
	p->s_occupy = occupy;

	p++;

	/* init to zero */
	cpkl_pdf_memset(p, 0, size);

	return (void *)p;
}

void _cpkl_free
(
	void *p,
	const char *filename,
	const char *funcname,
	u32 line
)
{
	if (cpkl_mmstatinit == 0)
		cpkl_mminit();

	CPKL_ASSERT(p != NULL);

	cpkl_mmblkinf_t *_p = (cpkl_mmblkinf_t *)p - 1;
	cpkl_bstn_t *dstbst;

	/* we need to get locker */
	cpkl_mtxlock(&cpkl_mmlocker);

	dstbst = cpkl_bst_lkup(cpkl_mmroot, &(_p->bstn), cpkl_mmrgcmp);
	if (dstbst == NULL)
	{
	    cpkl_printf("\ncpkl_free() double free fault, file: %s, function: %s, line: %d.",
				    filename, funcname, line);

		/* release locker */
		cpkl_mtxunlock(&cpkl_mmlocker);

	    return;
	}

	cpkl_bst_remove(&cpkl_mmroot, &(_p->bstn));

	cpkl_mmstat.real -= _p->s_real;
	cpkl_mmstat.occupy -= _p->s_occupy;

	/* release locker */
	cpkl_mtxunlock(&cpkl_mmlocker);

	cpkl_pdf_free(_p);
}

void cpkl_mmcheck(void)
{
	if (cpkl_mmstatinit == 0)
		cpkl_mminit();

	/* we need to get locker */
	cpkl_mtxlock(&cpkl_mmlocker);

	if (cpkl_mmroot == NULL)
	{
		CPKL_ASSERT((cpkl_mmstat.real == 0) && (cpkl_mmstat.occupy == 0));
		cpkl_printf("\nNo memory leak existed.");
	}
	else
	{
		cpkl_printf("\nMemory leak detected : %lldM\t%lldK\t%lldB",
					cpkl_mmstat.real >> 20,
					(cpkl_mmstat.real >> 10) & ((0x1 << 10) - 1),
					cpkl_mmstat.real & ((0x1 << 10) - 1));

		/* walk all if some memleak exist */
		CPKL_ASSERT(cpkl_bst_walk(cpkl_mmroot, CPKL_BSTWALKTYPE_LMR, cpkl_mmchkwalk, NULL) == 0);
	}

	cpkl_printf("\nMax memory allocate file: %s  function: %s  line: %d"
				"\nMax memory space   used: %lldM\t%lldK"
				"\nMax memory space occupy: %lldM\t%lldK",
				cpkl_mmstat.max_filename, cpkl_mmstat.max_funcname, cpkl_mmstat.max_line,
				cpkl_mmstat.real_max >> 20, (cpkl_mmstat.real_max >> 10) & ((0x1 << 10) - 1),
				cpkl_mmstat.occupy_max >> 20, (cpkl_mmstat.occupy_max >> 10) & ((0x1 << 10) - 1));
	/* release locker */
	cpkl_mtxunlock(&cpkl_mmlocker);
#ifdef CPKL_CONFIG_DEBUG
	/* tmstat */
	cpkl_printf("\ncpkl_mmcheck(), sigblocktime: %lld(us), times: %lld",
				cpkl_mmlocker.tmsum, cpkl_mmlocker.times);
#endif

	cpkl_printf("\n\n\n");
}

void cpkl_mmgetstat(cpkl_mmstat_t *stat)
{
	if (cpkl_mmstatinit == 0)
		cpkl_mminit();

	if (stat == NULL)
		return;

	*stat = cpkl_mmstat;
}

#endif

CODE_SECTION("====================")
CODE_SECTION("Slab Heap")
CODE_SECTION("====================")

static u32 cpkl_shgetfreeidx(cpkl_sh_t *sh)
{
	u32 i, n_shs = sh->n_afs + sh->n_hfs + sh->n_nfs;
	cpkl_shs_t *p;
	u8 *map;

	if (n_shs == 0)
		return 0;

	/*  */
	while ((map = (u8 *)cpkl_malloc(n_shs)) == NULL);

	CPKL_LISTENTRYWALK(p, cpkl_shs_t, &(sh->afh), curslb)
	{
		CPKL_ASSERT(map[p->shs_idx] == 0);
		map[p->shs_idx] = 1;
	}

	CPKL_LISTENTRYWALK(p, cpkl_shs_t, &(sh->hfh), curslb)
	{
		CPKL_ASSERT(map[p->shs_idx] == 0);
		map[p->shs_idx] = 1;
	}

	CPKL_LISTENTRYWALK(p, cpkl_shs_t, &(sh->nfh), curslb)
	{
		CPKL_ASSERT(map[p->shs_idx] == 0);
		map[p->shs_idx] = 1;
	}

	for (i = 0; i < n_shs; i++)
	{
		if (map[i] == 0)
			break;
	}

	cpkl_free(map);

	return i;
}

/* slab stack new slab */
static void cpkl_shinitslab(cpkl_sh_t *sh, cpkl_shs_t *shs)
{
	cpkl_nfblock_t *fp;

	/* sequency the shs, use the shs->slab_idx */
	shs->shs_idx = cpkl_shgetfreeidx(sh);

	/* init number of free blocks */
	shs->n_fblk = sh->bps;

	/* set the new slab's range */
	shs->rgl = (void *)shs;
	shs->rgr = CPKL_V2P(CPKL_P2V(shs) + sh->s_slb);

	/* init the free list */
	fp = (cpkl_nfblock_t *)(shs + 1);
	shs->freepos = NULL;
	/* the first free block's next is NULL */
	while ((CPKL_P2V(fp) + (int)(sh->s_blk)) <= CPKL_P2V(shs->rgr))
	{
		fp->next = shs->freepos;
		shs->freepos = fp;
		fp = (cpkl_nfblock_t *)CPKL_V2P(CPKL_P2V(fp) + sh->s_blk);
	}
}

/* slab range compire */
static int cpkl_shslbcmp(cpkl_bstn_t *n1, cpkl_bstn_t *n2)
{
	cpkl_shs_t *shs1 = CPKL_GETCONTAINER(n1, cpkl_shs_t, spbst);
	cpkl_shs_t *shs2 = CPKL_GETCONTAINER(n2, cpkl_shs_t, spbst);

	if (CPKL_P2V(shs1->rgr) <= CPKL_P2V(shs2->rgl))
		return CPKL_BSTCMP_1LT2;
	else if (CPKL_P2V(shs2->rgr) <= CPKL_P2V(shs1->rgl))
		return CPKL_BSTCMP_1BT2;
	else if ((CPKL_P2V(shs1->rgl) == CPKL_P2V(shs2->rgl)) &&
			 (CPKL_P2V(shs1->rgr) == CPKL_P2V(shs2->rgr)))
		return CPKL_BSTCMP_1EQ2;
	else if ((CPKL_P2V(shs1->rgl) >= CPKL_P2V(shs2->rgl)) &&
			 (CPKL_P2V(shs1->rgr) <= CPKL_P2V(shs2->rgr)))
		return CPKL_BSTCMP_1IN2;
	else if ((CPKL_P2V(shs1->rgl) <= CPKL_P2V(shs2->rgl)) &&
			 (CPKL_P2V(shs1->rgr) >= CPKL_P2V(shs2->rgr)))
		return CPKL_BSTCMP_2IN1;
	else
		return CPKL_BSTCMP_OVLP;
}

CPKL_FCTNEW_DEFINE(cpkl_sh_t)
{
	if (param == NULL)
		return NULL;

	cpkl_shfcp_t *fcp = (cpkl_shfcp_t *)param;

	/* size of each block should bigger than hsp_nfblock_t */
	if (fcp->s_blk < sizeof(cpkl_nfblock_t))
	{
		return NULL;
	}
	/* size of the slab should include the mngr struct */
	if (fcp->s_slb < (sizeof(cpkl_shs_t) + fcp->s_blk))
	{
		return NULL;
	}

	cpkl_sh_t *newsh = (cpkl_sh_t *)cpkl_malloc(sizeof(cpkl_sh_t));
	if (newsh == NULL)
		return NULL;


	/* object functions */

	/* input parameters */
	newsh->s_blk 	= (fcp->s_blk + sizeof(cpkl_nfblock_t) - 1) & (~(sizeof(cpkl_nfblock_t) - 1));
	newsh->s_slb 	= fcp->s_slb;
	newsh->needlock	= fcp->needlock;

	/* default parameters */
	cpkl_initlisthead(&(newsh->afh));
	cpkl_initlisthead(&(newsh->hfh));
	cpkl_initlisthead(&(newsh->nfh));

	/* align the block to  */
	newsh->bps = (newsh->s_slb - sizeof(cpkl_shs_t)) / newsh->s_blk;

	newsh->slbtroot = NULL;

	if (newsh->needlock)
	{
		if (cpkl_mtxcreate(&(newsh->mtx)) != 0)
		{
			cpkl_free(newsh);
			return NULL;
		}
	}

	newsh->n_afs = newsh->n_hfs = newsh->n_nfs = 0;
	newsh->n_cb = 0;

#ifdef CPKL_CONFIG_DEBUG
	newsh->n_ea = 0;
	newsh->n_ef = 0;
	newsh->n_sd = 0;
#endif

	return newsh;
}

CPKL_FCTDEL_DEFINE(cpkl_sh_t)
{
	if (obj == NULL)
		return;

	cpkl_sh_t *sh = (cpkl_sh_t *)obj;
	cpkl_shs_t *p, *n;
	/* free all slabs */
	CPKL_LISTENTRYWALK_SAVE(p, n, cpkl_shs_t, &(sh->afh), curslb)		/* all free list */
	{
		cpkl_free(p);
	}
	CPKL_LISTENTRYWALK_SAVE(p, n, cpkl_shs_t, &(sh->hfh), curslb)		/* half free list */
	{
		cpkl_free(p);
	}
	CPKL_LISTENTRYWALK_SAVE(p, n, cpkl_shs_t, &(sh->nfh), curslb)		/* no free list */
	{
		cpkl_free(p);
	}

	/*
	 * AVL's nodes are all embedded in the slab struct
	 * no need to free
	 */

	/* destroy the sig if needed */
	if (sh->needlock)
		cpkl_mtxdsty(&(sh->mtx));

	cpkl_free(sh);
}

void *cpkl_shalloc(cpkl_sh_t *sh)
{
	cpkl_shs_t *shs;
	cpkl_nfblock_t *fp = NULL;

	if (sh->needlock)
	{
		cpkl_mtxlock(&(sh->mtx));
	}

	if (!(CPKL_LISTISEMPLY(&(sh->hfh))))		/* likely */
	{
		/* get one slab from half free list */
		shs = CPKL_GETCONTAINER(sh->hfh.next, cpkl_shs_t, curslb);
	}
	else
	{
		/*  */
		if (!(CPKL_LISTISEMPLY(&(sh->afh))))
		{
			/* get one slab from all free list */
			shs = CPKL_GETCONTAINER(sh->afh.next, cpkl_shs_t, curslb);
		}
		else
		{
			shs = (cpkl_shs_t *)cpkl_malloc(sh->s_slb);
			if (shs == NULL)
			{
#ifdef CPKL_CONFIG_DEBUG
				(sh->n_ea)++;
#endif
				goto cpkl_shalloc_ret;
			}
			cpkl_shinitslab(sh, shs);

			/* let's insert this new slab into all free list */
			cpkl_listadd(&(shs->curslb), &(sh->afh));
			(sh->n_afs)++;
		}
	}

	/* remove the first block from the freelist */
	fp = shs->freepos;
	shs->freepos = fp->next;

	cpkl_pdf_memset(fp, 0, sh->s_blk);

	/*
	 * if current number of free blocks eq to number of blocks per slab
	 * remove the slab from all free list
	 * and insert it into half free list
	 */
	if (shs->n_fblk == sh->bps)
	{
		cpkl_listdel(&(shs->curslb));
		cpkl_listadd(&(shs->curslb), &(sh->hfh));
		(sh->n_afs)--;
		(sh->n_hfs)++;
		/* now the  */
		CPKL_ASSERT(0 == cpkl_bst_insert(&(sh->slbtroot), &(shs->spbst), cpkl_shslbcmp));
	}

	(shs->n_fblk)--;

	/*
	 * if current number of free blocks eq to 0
	 * remove the slab from half free list
	 * and insert it into no free list
	 */
	if (shs->n_fblk == 0)
	{
		cpkl_listdel(&(shs->curslb));
		cpkl_listadd(&(shs->curslb), &(sh->nfh));
		(sh->n_hfs)--;
		(sh->n_nfs)++;
	}

	(sh->n_cb)++;

cpkl_shalloc_ret:
	if (sh->needlock)
	{
		cpkl_mtxunlock(&(sh->mtx));
	}

	return (void *)fp;
}

void cpkl_shfree(cpkl_sh_t *sh, void *blk)
{
	/* this is the fake shs, just used to AVL lookup */
	cpkl_nfblock_t *fp = (cpkl_nfblock_t *)blk;
	cpkl_shs_t lkupshs, *dstshs;
	lkupshs.rgl = blk;
	lkupshs.rgr = CPKL_V2P(CPKL_P2V(blk) + sh->s_blk);
	/* find the blk's corrodinate slab by AVL, just save the result in dstshs */
	dstshs = (cpkl_shs_t *)cpkl_bst_lkup(sh->slbtroot, &(lkupshs.spbst), cpkl_shslbcmp);
	/* we can't find in AVL, the block is not alloced from this sh */
	if (NULL == dstshs)
	{
#ifdef CPKL_CONFIG_DEBUG
		(sh->n_ef)++;
#endif
		return;
	}
	dstshs = CPKL_GETCONTAINER(dstshs, cpkl_shs_t, spbst);

	if (sh->needlock)
	{
		cpkl_mtxlock(&(sh->mtx));
	}

	/* insert this block into slab's freelist */
	fp->next = dstshs->freepos;
	dstshs->freepos = fp;

	/*
	 * if current number of free blocks eq to 0
	 * remove the slab from no free list
	 * and insert it into half free list
	 */
	if (dstshs->n_fblk == 0)
	{
		cpkl_listdel(&(dstshs->curslb));
		cpkl_listadd(&(dstshs->curslb), &(sh->hfh));
		(sh->n_nfs)--;
		(sh->n_hfs)++;
	}

	(dstshs->n_fblk)++;

	/*
	 * if current number of free blocks eq to number of blocks per slab
	 * remove the slab from half free list
	 * and insert it into all free list
	 */
	if (dstshs->n_fblk == sh->bps)
	{
		cpkl_listdel(&(dstshs->curslb));
		cpkl_listadd(&(dstshs->curslb), &(sh->afh));
		(sh->n_hfs)--;
		(sh->n_afs)++;
		/*
		 * the valid block range CAN'T include the blocks which are in the all free list
		 * so we need to remove the all free slabs from valid range AVL
		 */
		cpkl_bst_remove(&(sh->slbtroot), &(dstshs->spbst));
	}

	(sh->n_cb)--;

	if (sh->needlock)
	{
		cpkl_mtxunlock(&(sh->mtx));
	}
}

void cpkl_shreset(cpkl_sh_t *sh)
{
	cpkl_listhead_t *p, *n;
	cpkl_shs_t *shs;

	CPKL_LISTWALK_SAVE(p, n, &(sh->hfh))		/* half free */
	{
		shs = CPKL_GETCONTAINER(p, cpkl_shs_t, curslb);

		cpkl_listdel(p);

		/* reset this slab */
		cpkl_shinitslab(sh, shs);

		/* insert this slab into all free list */
		cpkl_listadd(p, &(sh->afh));

		/*
		 * the valid block range CAN'T include the blocks which are in the all free list
		 * so we need to remove the all free slabs from valid range AVL
		 */
		cpkl_bst_remove(&(sh->slbtroot), &(shs->spbst));
	}
	CPKL_ASSERT(CPKL_LISTISEMPLY(&(sh->hfh)));

	CPKL_LISTWALK_SAVE(p, n, &(sh->nfh))		/* no free */
	{
		shs = CPKL_GETCONTAINER(p, cpkl_shs_t, curslb);

		cpkl_listdel(p);

		/* reset this slab */
		cpkl_shinitslab(sh, shs);

		/* insert this slab into all free list */
		cpkl_listadd(p, &(sh->afh));

		/*
		 * the valid block range CAN'T include the blocks which are in the all free list
		 * so we need to remove the all free slabs from valid range AVL
		 */
		cpkl_bst_remove(&(sh->slbtroot), &(shs->spbst));
	}
	CPKL_ASSERT(CPKL_LISTISEMPLY(&(sh->nfh)));

	sh->n_afs += sh->n_hfs + sh->n_nfs;
	sh->n_hfs = 0;
	sh->n_nfs = 0;

	sh->n_cb = 0;
}

void cpkl_shdrainslb(cpkl_sh_t *sh)
{
	cpkl_shs_t *p, *n;

	if (CPKL_LISTISEMPLY(&(sh->afh)))
		return;

	if (sh->needlock)
	{
		cpkl_mtxlock(&(sh->mtx));
	}

	CPKL_LISTENTRYWALK_SAVE(p, n, cpkl_shs_t, &(sh->afh), curslb)		/* all free list */
	{
		cpkl_free(p);
	}
	cpkl_initlisthead(&(sh->afh));

	sh->n_afs = 0;

#ifdef CPKL_CONFIG_DEBUG
	(sh->n_sd)++;
#endif

	if (sh->needlock)
	{
		cpkl_mtxunlock(&(sh->mtx));
	}
}

u32 cpkl_shgetblkidx(cpkl_sh_t *sh, void *blk)
{
	u32 retidx = CPKL_INVALID_IDX;
	/* this is the fake shs, just used to AVL lookup */
	cpkl_shs_t lkupshs, *dstshs;
	lkupshs.rgl = blk;
	lkupshs.rgr = CPKL_V2P(CPKL_P2V(blk) + sh->s_blk);
	/* locate the shs, find the blk's corrodinate slab by AVL, just save the result in dstshs */
	dstshs = (cpkl_shs_t *)cpkl_bst_lkup(sh->slbtroot, &(lkupshs.spbst), cpkl_shslbcmp);
	/* we can't find in AVL, the block is not alloced from this sh */
	if (NULL == dstshs)
	{
		return retidx;
	}
	dstshs = CPKL_GETCONTAINER(dstshs, cpkl_shs_t, spbst);
	retidx = dstshs->shs_idx * sh->bps;

	retidx += (u32)((CPKL_P2V(blk) - CPKL_P2V(dstshs->rgl) - sizeof(cpkl_shs_t)) / sh->s_blk);

	return retidx;
}

void *cpkl_shgetblkbyidx(cpkl_sh_t *sh, u32 idx)
{
	cpkl_shs_t *p;
	u32 shs_idx = idx / sh->bps;
	cpkl_nfblock_t *fp;
	void *ret;

	CPKL_LISTENTRYWALK(p, cpkl_shs_t, &(sh->afh), curslb)
	{
		if (shs_idx == p->shs_idx)
			goto cpkl_shgetblkbyidx_getblk;
	}

	CPKL_LISTENTRYWALK(p, cpkl_shs_t, &(sh->hfh), curslb)
	{
		if (shs_idx == p->shs_idx)
			goto cpkl_shgetblkbyidx_getblk;
	}

	CPKL_LISTENTRYWALK(p, cpkl_shs_t, &(sh->nfh), curslb)
	{
		if (shs_idx == p->shs_idx)
			goto cpkl_shgetblkbyidx_getblk;
	}

	/* invalid idx */
	return NULL;

cpkl_shgetblkbyidx_getblk:

	/* now locate this block */
	ret = CPKL_V2P(CPKL_P2V(p->rgl) + sizeof(cpkl_shs_t) + (idx % sh->bps) * sh->s_blk);
#if 1
	/* this block pointer should NOT in free list */
	fp = p->freepos;
	while (fp)
	{
		if (ret == fp)
			return NULL;

		fp = fp->next;
	}
#endif
	return ret;
}

u32 cpkl_shgetnumidx(cpkl_sh_t *sh)
{
	return (sh->n_hfs + sh->n_nfs) * sh->bps;
}

#ifdef CPKL_CONFIG_DEBUG

#define CPKL_SHTEST_1M				(0x400 * 0x400)
#define CPKL_SHTEST_NBLK			(CPKL_SHTEST_1M * 2)
#define CPKL_SHTEST_SBLK			(32)
#define CPKL_SHTEST_NCYL			(CPKL_SHTEST_1M * 16)
void *shtestbuf[CPKL_SHTEST_NBLK];

#if 0
void cpkl_shtest(void)
{
	cpkl_tmsreset(0, "shtest alloc");
	cpkl_tmsreset(1, "shtest free");

	u32 i;
	cpkl_sh_t testsh;
	cpkl_shinit(&testsh, CPKL_SHTEST_SBLK, CPKL_SHTEST_1M, 0);

	cpkl_ri_seed();

	cpkl_tms(0, CPKL_TMS_ON);
	for (i = 0; i < CPKL_SHTEST_NBLK; i++)
	{
		shtestbuf[i] = cpkl_shalloc(&testsh);
	}
	cpkl_tms(0, CPKL_TMS_OFF);

	cpkl_tms(1, CPKL_TMS_ON);
	for (i = 0; i < CPKL_SHTEST_NBLK; i++)
	{
		cpkl_shfree(&testsh, shtestbuf[i]);
	}
	cpkl_tms(1, CPKL_TMS_OFF);

	CPKL_ASSERT(testsh.n_ea == 0);
	CPKL_ASSERT(testsh.n_ef == 0);

	cpkl_shdsty(&testsh);

	cpkl_tmreport(CPKL_TMSREPORTALL);
	cpkl_mmcheck();
}
#else
void cpkl_shtest(void)
{
	u32 i = 0, j = 0, idx;
	cpkl_sh_t *testsh;

	cpkl_ri_seed();

	cpkl_shfcp_t fcp = {
		CPKL_SHTEST_SBLK,
		CPKL_SHTEST_1M,
		0
	};
	testsh = CPKL_FCTNEW(cpkl_sh_t, &fcp);

	cpkl_pdf_memset(shtestbuf, 0, CPKL_SHTEST_NBLK * sizeof(shtestbuf[0]));

    int sign = -1;
    u32 sw = 0, thr = 2;
	while (1)
	{
		sw = cpkl_ri_rand(0, 8);
		if (sw >= thr)
		{
			sw = 1;
		}
		else
			sw = 0;

		idx = cpkl_ri_rand(0, CPKL_SHTEST_NBLK);

		if (shtestbuf[idx])
		{
			if (sw == 0)
			{
				cpkl_shfree(testsh, shtestbuf[idx]);
				shtestbuf[idx] = NULL;
			}
		}
		else
		{
			if (sw == 1)
				shtestbuf[idx] = cpkl_shalloc(testsh);
		}

		i++;

		if ((i % 0x800000) == 0)
		{
			cpkl_shdrainslb(testsh);


			cpkl_mmstat_t mmstat;
			cpkl_mmgetstat(&mmstat);

			cpkl_printf("\nthr:0x%4x, totalmem: %lldM\t%lldK   ""cur hf:%3d nf:%3d",
						thr, (mmstat.real) >> 20, ((mmstat.real) >> 10) & ((0x1 << 10) - 1),
						testsh->n_hfs, testsh->n_nfs);

			j++;
            if ((j % 10) == 0)
            {
                thr += sign;

                if (thr == 8)
                    sign = -sign;
                if (thr == 0)
                    sign = -sign;
            }
		}
	}
}

#endif

#endif

CODE_SECTION("====================")
CODE_SECTION("Slab Stack")
CODE_SECTION("====================")

/* slab stack new slab */
static cpkl_sss_t *hsp_ssns(cpkl_ss_t *ss)
{
	cpkl_sss_t *sss;

    sss = (cpkl_sss_t *)cpkl_malloc(ss->s_slb);
	if (sss == 0)
	{
		return 0;
	}
	/* the space to store blocks just follow the 'hsp_sss_t' */
	sss->freepos = sss + 1;
	/* newslab is empty slab */
    sss->n_blk = 0;

	(ss->n_slb)++;
	/* insert this slab into slabstack */
	cpkl_listadd_tail(&(sss->curslb), &(ss->list));
	/* ok, the sh's curslab is this new slab */
	ss->freeslb = &(sss->curslb);

	return sss;
}

CPKL_FCTNEW_DEFINE(cpkl_ss_t)
{
	if (param == NULL)
		return NULL;

	cpkl_ssfcp_t *fcp = (cpkl_ssfcp_t *)param;
	if ((fcp->s_blk == 0) || (fcp->s_slb == 0))
	{
		return NULL;
	}

	cpkl_ss_t *newss = (cpkl_ss_t *)cpkl_malloc(sizeof(cpkl_ss_t));
	if (newss == NULL)
		return NULL;

	/* object functions */

	/* input parameters */
	newss->s_blk	= fcp->s_blk;
	newss->s_slb	= fcp->s_slb;
	newss->needsig	= fcp->needsig;

	/* default parameters */
	cpkl_initlisthead(&(newss->list));

	newss->bps		= (newss->s_slb - sizeof(cpkl_sss_t)) / newss->s_blk;
	newss->n_slb	= 0;
	newss->n_blk	= 0;
#ifdef CPKL_CONFIG_DEBUG
	newss->maxslb	= 0;
	newss->maxblk	= 0;
#endif

	if (newss->needsig)
	{
		if (cpkl_sigcreate(&(newss->sig), 1, 1) != 0)
		{
			cpkl_free(newss);

			return NULL;
		}
	}

	/* judge the hsp_ssns() */
	if (hsp_ssns(newss) == NULL)
	{
		if (newss->needsig)
			cpkl_sigdsty(&(newss->sig));

		cpkl_free(newss);

		return NULL;
	}

	return newss;
}

CPKL_FCTDEL_DEFINE(cpkl_ss_t)
{
	if (obj == NULL)
		return;

	cpkl_ss_t *ss = (cpkl_ss_t *)obj;

	cpkl_sss_t *p, *n;
	u32 slabsum = 0;
	CPKL_LISTENTRYWALK_SAVE(p, n, cpkl_sss_t, &(ss->list), curslb)
	{
		cpkl_free(p);
		slabsum++;
	}
	CPKL_ASSERT(slabsum == ss->n_slb);

	if (ss->needsig)
		cpkl_sigdsty(&(ss->sig));

	cpkl_free(ss);
}

void *cpkl_ssalloc(cpkl_ss_t *ss)
{
	cpkl_sss_t *vs;
	void *ret;

	CPKL_ASSERT(ss->freeslb != 0);

	if (ss->needsig)
	{
		cpkl_sigwait(&(ss->sig));
	}

	vs			= CPKL_GETCONTAINER(ss->freeslb, cpkl_sss_t, curslb);
	ret			= vs->freepos;
	vs->freepos	= CPKL_V2P(CPKL_P2V(vs->freepos) + ss->s_blk);
    (vs->n_blk)++;
	(ss->n_blk)++;

	cpkl_pdf_memset(ret, 0, ss->s_blk);

	/* there is no valid block in current slab, shift to next slab */
	if (vs->n_blk == ss->bps)
	{
		ss->freeslb = ss->freeslb->next;

		/* there is no valid slab, we need to alloc one. */
		if (ss->freeslb == &(ss->list))
		{
			vs = hsp_ssns(ss);
			if (vs == 0)
			{
				/* roll back this alloc if hsp_ssns() faild */
				ss->freeslb	= ss->freeslb->prev;
				vs			= CPKL_GETCONTAINER(ss->freeslb, cpkl_sss_t, curslb);
                vs->freepos	= CPKL_V2P(CPKL_P2V(vs->freepos) - ss->s_blk);
				(vs->n_blk)--;
                (ss->n_blk)--;

				ret = NULL;
				goto cpkl_shalloc_ret;
			}
		}
	}

#ifdef CPKL_CONFIG_DEBUG
    if ((ss->n_blk) > (ss->maxblk))
	{
		ss->maxblk = ss->n_blk;
	}
	if ((ss->n_slb) > (ss->maxslb))
	{
		ss->maxslb = ss->n_slb;
	}
#endif

cpkl_shalloc_ret:
	if (ss->needsig)
	{
		cpkl_sigsend(&(ss->sig));
	}

	return ret;
}

void cpkl_ssfree(cpkl_ss_t *ss, u32 n_blk)
{
	cpkl_sss_t *vs;
	u32 curfreecount;

	CPKL_ASSERT(ss->freeslb != 0);

	if (ss->needsig)
	{
		cpkl_sigwait(&(ss->sig));
	}

	while (n_blk)
	{
		vs = CPKL_GETCONTAINER(ss->freeslb, cpkl_sss_t, curslb);

		if (0 == vs->n_blk)
		{
			CPKL_ASSERT(ss->freeslb->prev != &(ss->list));
			ss->freeslb	= ss->freeslb->prev;
			continue;
		}

		if (vs->n_blk < n_blk)
            curfreecount = vs->n_blk;
		else
			curfreecount = n_blk;

		vs->freepos = CPKL_V2P(CPKL_P2V(vs->freepos) - curfreecount * ss->s_blk);
		vs->n_blk -= curfreecount;
        ss->n_blk -= curfreecount;

		n_blk -= curfreecount;
	}

	if (ss->needsig)
	{
		cpkl_sigsend(&(ss->sig));
	}
}

void cpkl_ssreset(cpkl_ss_t *ss)
{
    if (ss->n_blk)
        cpkl_ssfree(ss, ss->n_blk);
}

CODE_SECTION("====================")
CODE_SECTION("Hash List")
CODE_SECTION("====================")

CPKL_FCTNEW_DEFINE(cpkl_hl_t)
{
	if (param == NULL)
		return NULL;

	cpkl_hlfcp_t *fcp	= (cpkl_hlfcp_t *)param;

	cpkl_hl_t *newhl	= (cpkl_hl_t *)cpkl_malloc(sizeof(cpkl_hl_t) +
												   (fcp->n_bkt - 1) * (sizeof(cpkl_hlbkt_t)));
	if (newhl == NULL)
		return NULL;

	/* object functions */

	/* input parameters */
	newhl->keylen	= fcp->keylen;
	newhl->rstlen	= fcp->rstlen;
	newhl->n_bkt	= fcp->n_bkt;

	/* default parameters */
	cpkl_initlisthead(&(newhl->glh));
	cpkl_mtxcreate(&(newhl->lock));

#ifdef CPKL_CONFIG_HL_USESH
{
	cpkl_shfcp_t shfcp	= {sizeof(cpkl_hlnd_t) - 1 + fcp->keylen + fcp->rstlen,
						   256 * 1024, 0};
	newhl->hlndsh		= CPKL_FCTNEW(cpkl_sh_t, &shfcp);

	if (newhl->hlndsh == NULL)
	{
		cpkl_free(newhl);
		return NULL;
	}
}
#endif

	newhl->n_total= 0;

	u32 i;
	for (i = 0; i < fcp->n_bkt; i++)
	{
		cpkl_initlisthead(&(newhl->bktlist[i].hlhead));
		newhl->bktlist[i].n_entry = 0;
	}

	return newhl;
}

CPKL_FCTDEL_DEFINE(cpkl_hl_t)
{
	if (obj == NULL)
		return;

	cpkl_hl_t *hl = (cpkl_hl_t *)obj;

	cpkl_hlreset(hl);

#ifdef CPKL_CONFIG_HL_USESH
	CPKL_FCTDEL(cpkl_sh_t, hl->hlndsh);
#endif

	cpkl_mtxdsty(&(hl->lock));

    cpkl_free(hl);
}

void cpkl_hlreset(cpkl_hl_t *hl)
{
	u32 i;

	if (hl == NULL)
		return;

	cpkl_mtxlock(&(hl->lock));

#ifdef CPKL_CONFIG_HL_USESH
	cpkl_shreset(hl->hlndsh);

	for (i = 0; i < hl->n_bkt; i++)
	{
		cpkl_initlisthead(&(hl->bktlist[i].hlhead));
		hl->bktlist[i].n_entry = 0;
	}
#else
	cpkl_hlnd_t *curnode, *t;

	for (i = 0; i < hl->n_bkt; i++)
	{
		/*  */
		CPKL_LISTENTRYWALK_SAVE(curnode, t, cpkl_hlnd_t, &(hl->bktlist[i].hlhead), listnode)
		{
			cpkl_listdel(&(curnode->listnode));
			cpkl_free(curnode);
		}

		CPKL_ASSERT(CPKL_LISTISEMPLY(&(hl->bktlist[i].hlhead)));
		hl->bktlist[i].n_entry = 0;
	}
#endif

	cpkl_initlisthead(&(hl->glh));
	hl->n_total= 0;

	cpkl_mtxunlock(&(hl->lock));
}

/* note */
static cpkl_hlnd_t* cpkl_hllkup_ex(cpkl_hl_t *hl, const void *key, u32 *bktidx, u32 lock)
{
	u32 keyhash = cpkl_alg_crc32(key, hl->keylen);
	u32 idx = keyhash % (hl->n_bkt);
	cpkl_listhead_t *dstlisthead;
	cpkl_hlnd_t *curnode;

	if (lock)
		cpkl_mtxlock(&(hl->lock));

	dstlisthead = &(hl->bktlist[idx].hlhead);

	CPKL_LISTENTRYWALK(curnode, cpkl_hlnd_t, dstlisthead, listnode)
	{
		if (cpkl_pdf_memcmp(curnode->keyrst, key, hl->keylen) == 0)
		{
			if (bktidx)
				*bktidx = idx;

			goto _ret;
		}
	}
	curnode = NULL;

_ret:
	if (lock)
		cpkl_mtxunlock(&(hl->lock));

	return curnode;
}

cpkl_hlnd_t* cpkl_hllkup(cpkl_hl_t *hl, const void *key, u32 *bktidx)
{
	return cpkl_hllkup_ex(hl, key, bktidx, 1);
}

void cpkl_hlwalk(cpkl_hl_t *hl, cpkl_hlwalk_func walk, void *param)
{
	cpkl_hlnd_t *p, *n;
	u32 n_node = 0;

	cpkl_mtxlock(&(hl->lock));

	CPKL_LISTENTRYWALK_SAVE(p, n, cpkl_hlnd_t, &(hl->glh), gl)
	{
		if (walk(p, param) != 0)
			goto _ret;
		n_node++;
	}

	CPKL_ASSERT(hl->n_total == n_node);

_ret:
	cpkl_mtxunlock(&(hl->lock));
}

int cpkl_hlinsert(cpkl_hl_t *hl, const void *key, const void *rst)
{
	u32 bktidx;
	cpkl_listhead_t *dstlisthead;
	cpkl_hlnd_t *curnode;
	int ret;

	cpkl_mtxlock(&(hl->lock));

	curnode = cpkl_hllkup_ex(hl, key, NULL, 0);

	if (curnode == NULL)
	{
#ifdef CPKL_CONFIG_HL_USESH
		curnode = (cpkl_hlnd_t *)cpkl_shalloc(hl->hlndsh);
#else
		curnode = (cpkl_hlnd_t *)cpkl_malloc(sizeof(cpkl_hlnd_t) - 1 + (hl->keylen) + (hl->rstlen));
#endif
		if (curnode == NULL)
		{
			cpkl_mtxunlock(&(hl->lock));
			return -1;
		}
		cpkl_pdf_memcpy(&(curnode->keyrst[0]), key, hl->keylen);

		bktidx = cpkl_alg_crc32(key, hl->keylen) % (hl->n_bkt);
		dstlisthead = &(hl->bktlist[bktidx].hlhead);
		cpkl_listadd(&(curnode->listnode), dstlisthead);

		/* link it into global list */
		cpkl_listadd(&(curnode->gl), &(hl->glh));

		(hl->bktlist[bktidx].n_entry)++;
		(hl->n_total)++;

		ret = 0;
	}
	else
	{
		/* there is a entry with same key */
		ret = 1;
	}

	/* just save the result */
	cpkl_pdf_memcpy(&(curnode->keyrst[hl->keylen]), rst, hl->rstlen);

	cpkl_mtxunlock(&(hl->lock));

	return ret;
}

void cpkl_hlremove(cpkl_hl_t *hl, const void *key)
{
	u32 bktidx = 0;
	cpkl_hlnd_t *curnode;

	cpkl_mtxlock(&(hl->lock));

	curnode = cpkl_hllkup_ex(hl, key, &bktidx, 0);

	if (curnode)
	{
		/* remove from global list */
		cpkl_listdel(&(curnode->gl));

		cpkl_listdel(&(curnode->listnode));
#ifdef CPKL_CONFIG_HL_USESH
		cpkl_shfree(hl->hlndsh, curnode);
#else
		cpkl_free(curnode);
#endif

		(hl->bktlist[bktidx].n_entry)--;
		(hl->n_total)--;
	}
#ifdef CPKL_CONFIG_DEBUG
	else
	{
		cpkl_printf("\nhashlist remove, entry NOT exist.");
	}
#endif

	cpkl_mtxunlock(&(hl->lock));
}

/* add some test code for the BST */
#ifdef CPKL_CONFIG_DEBUG

#define CPKL_HSTEST_KEYLEN			(32)
#define CPKL_HSTEST_RSTLEN			(8)
#define CPKL_HSTEST_TESTTIMES		(2048)

typedef struct _cpkl_hsteststr {
	u8	key[CPKL_HSTEST_KEYLEN];
	u8	rst[CPKL_HSTEST_RSTLEN];
} cpkl_hsteststr_t;

void cpkl_hltest(void)
{
	u32 i, j, disbuf[CPKL_HSTEST_KEYLEN], base, *randdis;
	cpkl_hl_t *tmp = NULL;
	cpkl_hlfcp_t fcp = {CPKL_HSTEST_KEYLEN, CPKL_HSTEST_RSTLEN, 512};
	cpkl_hsteststr_t *cmpbuff;

	cpkl_tmsreset(0, "hashlist test insert");
	cpkl_tmsreset(1, "hashlist test lookup");
	cpkl_tmsreset(2, "hashlist test remove");

	cpkl_ri_seed();

	cmpbuff = (cpkl_hsteststr_t *)cpkl_malloc(sizeof(cpkl_hsteststr_t) * CPKL_HSTEST_TESTTIMES);

	tmp = CPKL_FCTNEW(cpkl_hl_t, &fcp);

	for (i = 0; i < CPKL_HSTEST_KEYLEN; i++)
	{
		disbuf[i] = i;
	}

	for (i = 0; i < CPKL_HSTEST_TESTTIMES; i++)
	{
		base = cpkl_ri_rand(0, 0x1000);
		randdis = cpkl_ri_rdgen(disbuf, CPKL_HSTEST_KEYLEN);
		for (j = 0; j < CPKL_HSTEST_KEYLEN; j++)
		{
			cmpbuff[i].key[j] = (u8)(base + (randdis[j] << 2));
		}
		cpkl_pdf_memcpy(cmpbuff[i].rst, randdis, CPKL_HSTEST_RSTLEN);

		cpkl_free(randdis);
		cpkl_tms(0, CPKL_TMS_ON);
		cpkl_hlinsert(tmp, cmpbuff[i].key, cmpbuff[i].rst);
		cpkl_tms(0, CPKL_TMS_OFF);
	}

	for (i = 0; i < CPKL_HSTEST_TESTTIMES; i++)
	{
		cpkl_tms(1, CPKL_TMS_ON);
		cpkl_hlnd_t *lkup = cpkl_hllkup(tmp, cmpbuff[i].key, NULL);
		cpkl_tms(1, CPKL_TMS_OFF);
		CPKL_ASSERT(cpkl_pdf_memcmp(lkup->keyrst, cmpbuff[i].key, CPKL_HSTEST_KEYLEN) == 0);
		CPKL_ASSERT(cpkl_pdf_memcmp(CPKL_V2P(CPKL_P2V(lkup->keyrst) + CPKL_HSTEST_KEYLEN), cmpbuff[i].rst, CPKL_HSTEST_RSTLEN) == 0);
	}

	for (i = 0; i < CPKL_HSTEST_TESTTIMES; i++)
	{
		cpkl_tms(2, CPKL_TMS_ON);
		cpkl_hlremove(tmp, cmpbuff[i].key);
		cpkl_tms(2, CPKL_TMS_OFF);
	}

	CPKL_FCTDEL(cpkl_hl_t, tmp);

	cpkl_free(cmpbuff);

	cpkl_tmreport(CPKL_TMSREPORTALL);

	cpkl_mmcheck();
}

#endif

CODE_SECTION("====================")
CODE_SECTION("RangeResouce Mngr")
CODE_SECTION("====================")

static int cpkl_rrndcmp(cpkl_bstn_t *n1, cpkl_bstn_t *n2)
{
	cpkl_rrnd_t	*p1 = CPKL_GETCONTAINER(n1, cpkl_rrnd_t, bstn);
	cpkl_rrnd_t	*p2 = CPKL_GETCONTAINER(n2, cpkl_rrnd_t, bstn);
	u64 begin1 = p1->begin, begin2 = p2->begin;
	u64 end1 = begin1 + p1->sz, end2 = begin2 + p2->sz;

	if (begin2 >= end1)
		return CPKL_BSTCMP_1LT2;
	if (begin1 >= end2)
		return CPKL_BSTCMP_1BT2;
	if ((begin1 == begin2) && (end1 == end2))
		return CPKL_BSTCMP_1EQ2;
	if ((begin1 >= begin2) && (end1 <= end2))
		return CPKL_BSTCMP_1IN2;
	if ((begin2 >= begin1) && (end2 <= end1))
		return CPKL_BSTCMP_2IN1;

	return CPKL_BSTCMP_OVLP;
}

CPKL_FCTNEW_DEFINE(cpkl_rrmngr_t)
{
	if (param == NULL)
		return NULL;

	cpkl_rrmngrfcp_t *fcp = (cpkl_rrmngrfcp_t *)param;

	cpkl_rrmngr_t *newrrmngr = (cpkl_rrmngr_t *)cpkl_malloc(sizeof(cpkl_rrmngr_t));
	if (newrrmngr == NULL)
		return NULL;

	/* object functions */

	/* input parameters */
	newrrmngr->begin = fcp->begin;
	newrrmngr->total = fcp->total;

	/* default parameters */
	cpkl_initlisthead(&(newrrmngr->lh));
	newrrmngr->root = NULL;
	if (fcp->ndsh)
	{
		newrrmngr->ndsh = fcp->ndsh;
	}
	else
	{
		cpkl_shfcp_t fcp = {sizeof(cpkl_rrnd_t), 1 * 1024, 0};
		newrrmngr->ndsh = CPKL_FCTNEW(cpkl_sh_t, &fcp);
		if (newrrmngr->ndsh == NULL)
		{
			goto _err;
		}
		newrrmngr->shnf = 1;
	}

	/* insert the root cpkl_rrnd_t which cover the whole range */
	cpkl_rrnd_t *whole = (cpkl_rrnd_t *)cpkl_shalloc(newrrmngr->ndsh);
	if (whole == NULL)
	{
		goto _err;
	}
	whole->begin = newrrmngr->begin;
	whole->sz = newrrmngr->total;
	whole->type = fcp->inittype;
	cpkl_listadd(&(whole->ln), &(newrrmngr->lh));
	cpkl_bst_insert(&(newrrmngr->root), &(whole->bstn), cpkl_rrndcmp);

	return newrrmngr;

_err:
	CPKL_FCTDEL(cpkl_rrmngr_t, newrrmngr);
	return NULL;
}

CPKL_FCTDEL_DEFINE(cpkl_rrmngr_t)
{
	if (obj == NULL)
		return;

	cpkl_rrmngr_t *rrmngr = (cpkl_rrmngr_t *)obj;

	if (rrmngr->shnf)
		CPKL_FCTDEL(cpkl_sh_t, rrmngr->ndsh);

	cpkl_free(rrmngr);
}

int cpkl_rrlookup(cpkl_rrmngr_t *rrmngr, u64 begin, u64 size, u32 *type)
{
	cpkl_rrnd_t *p, lkupnd;

	lkupnd.begin = begin;
	lkupnd.sz = size;
	/* lookup this range in the BST */
	cpkl_bstn_t *res = cpkl_bst_lkup(rrmngr->root, &(lkupnd.bstn), cpkl_rrndcmp);
	if (res == NULL)
		return -1;

	p = CPKL_GETCONTAINER(res, cpkl_rrnd_t, bstn);
	if (type)
		*type = p->type;

	return 0;
}

/*
 * set range's type
 * rrmgnr:
 * begin : range begin need to occupy
 * size  : range size
 * type  :
 */
int cpkl_rrset(cpkl_rrmngr_t *rrmngr, u64 begin, u64 size, u32 type)
{
	u64 oldbegin, oldsz;
	u32	oldtype;
	cpkl_listhead_t *lnd;
	cpkl_rrnd_t *newrrnd, *p, *prev, *next, lkupnd;
	lkupnd.begin = begin;
	lkupnd.sz = size;

	if (size == 0)
		return 0;

	/* lookup this range in the BST */
	cpkl_bstn_t *res = cpkl_bst_lkup(rrmngr->root, &(lkupnd.bstn), cpkl_rrndcmp);
	if (res == NULL)
		return -1;

	p = CPKL_GETCONTAINER(res, cpkl_rrnd_t, bstn);
	if (p->type == type)
		return -2;

	oldbegin = p->begin;
	oldsz = p->sz;
	oldtype = p->type;

	/* modify this rrnd */
	p->begin = begin;
	p->sz = size;
	p->type = type;

	if (oldbegin != begin)
	{
		/*  */
		newrrnd = (cpkl_rrnd_t *)cpkl_shalloc(rrmngr->ndsh);
		if (newrrnd == NULL)
			CPKL_ASSERT(0);

		newrrnd->begin = oldbegin;
		newrrnd->sz = begin - oldbegin;
		newrrnd->type = oldtype;
		cpkl_listadd_(&(newrrnd->ln), p->ln.prev, &(p->ln));
		prev = newrrnd;

		cpkl_bst_insert(&(rrmngr->root), &(newrrnd->bstn), cpkl_rrndcmp);
	}
	else
	{
		lnd = cpkl_listprev(&(p->ln), &(rrmngr->lh));
		if (lnd)
			prev = CPKL_GETCONTAINER(lnd, cpkl_rrnd_t, ln);
		else
			prev = NULL;
	}

	if ((oldbegin + oldsz) != (begin + size))
	{
		/*  */
		newrrnd = (cpkl_rrnd_t *)cpkl_shalloc(rrmngr->ndsh);
		if (newrrnd == NULL)
			CPKL_ASSERT(0);

		newrrnd->begin = begin + size;
		newrrnd->sz = (oldbegin + oldsz) - (begin + size);
		newrrnd->type = oldtype;
		cpkl_listadd_(&(newrrnd->ln), &(p->ln), p->ln.next);
		next = newrrnd;

		cpkl_bst_insert(&(rrmngr->root), &(newrrnd->bstn), cpkl_rrndcmp);
	}
	else
	{
		lnd = cpkl_listnext(&(p->ln), &(rrmngr->lh));
		if (lnd)
			next = CPKL_GETCONTAINER(lnd, cpkl_rrnd_t, ln);
		else
			next = NULL;
	}

	/* combine if needed */
	if (prev && (prev->type == p->type))
	{
		cpkl_listdel(&(prev->ln));
		cpkl_bst_remove(&(rrmngr->root), &(prev->bstn));

		p->begin = prev->begin;
		p->sz += prev->sz;

		cpkl_shfree(rrmngr->ndsh, prev);
	}

	if (next && (next->type == p->type))
	{
		cpkl_listdel(&(next->ln));
		cpkl_bst_remove(&(rrmngr->root), &(next->bstn));

		p->sz += next->sz;

		cpkl_shfree(rrmngr->ndsh, next);
	}

	return 0;
}

int cpkl_rrwalk(cpkl_rrmngr_t *rrmngr, cpkl_rrwalk_func walk, void *param)
{
	cpkl_rrnd_t *p;
	int ret;
	u32 n = 0;

	CPKL_LISTENTRYWALK(p, cpkl_rrnd_t, &(rrmngr->lh), ln)
	{
		ret = walk(p, param);
		if (ret)
			return ret;

		n++;
	}
	CPKL_ASSERT(n == rrmngr->ndsh->n_cb);

	return 0;
}

#ifdef CPKL_CONFIG_DEBUG

static int cpkl_rrmngrtest_walk(cpkl_rrnd_t *rrnd, void *param)
{
	cpkl_printf("[%03lld ~ %03lld), type(%d)\n",
		rrnd->begin,
		rrnd->begin + rrnd->sz,
		rrnd->type);

	return 0;
}

void cpkl_rrmngrtest(void)
{
	cpkl_rrmngrfcp_t fcp = {0, 16, 0};
	cpkl_rrmngr_t *mngr = CPKL_FCTNEW(cpkl_rrmngr_t, &fcp);

	CPKL_ASSERT(cpkl_rrset(mngr, 0, 17, 1));
	CPKL_ASSERT(cpkl_rrset(mngr, 15, 2, 1));
	CPKL_ASSERT(cpkl_rrset(mngr, 17, 2, 1));

	cpkl_rrset(mngr, 0, 1, 1);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);
	cpkl_rrset(mngr, 0, 1, 0);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);

	cpkl_rrset(mngr, 15, 1, 2);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);
	CPKL_ASSERT(cpkl_rrset(mngr, 15, 1, 2));
	cpkl_rrset(mngr, 15, 1, 0);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);

	cpkl_rrset(mngr, 8, 2, 2);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);
	cpkl_rrset(mngr, 8, 2, 0);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);


	cpkl_rrset(mngr, 8, 2, 3);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);
	CPKL_ASSERT(cpkl_rrset(mngr, 9, 2, 3));
	cpkl_rrset(mngr, 10, 1, 3);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);
	cpkl_rrset(mngr, 12, 2, 4);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);
	cpkl_rrset(mngr, 11, 1, 3);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);
	cpkl_rrset(mngr, 12, 2, 3);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);
	cpkl_rrset(mngr, 10, 3, 0);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);
	cpkl_rrset(mngr, 13, 1, 0);
	cpkl_rrwalk(mngr, cpkl_rrmngrtest_walk, NULL);

	CPKL_FCTDEL(cpkl_rrmngr_t, mngr);
}

#endif


CODE_SECTION("====================")
CODE_SECTION("Custom File Operation")
CODE_SECTION("====================")

#ifdef HSP_CONFIG_CUSTOM_FILEOP

extern unsigned long DRV_FILE_Read(const char *szFileName, unsigned short bNeedUnArj, unsigned char **ppucDataBuf, unsigned *puiFileLen);


CPKL_FILE *cpkl_fopen(const char *name, const char *mode)
{
	CPKL_FILE *newdesc = (CPKL_FILE *)cpkl_malloc(sizeof(CPKL_FILE));
	if (newdesc)
	{
		newdesc->curpos = 0;
		if (ERROR_SUCCESS != DRV_FILE_Read(name, 0, &(newdesc->databuff), &(newdesc->totallen)))
		{
			cpkl_free(newdesc);
			newdesc = 0;
		}
	}

	return newdesc;
}

void cpkl_fclose(CPKL_FILE *fdesc)
{
	MEM_Free(fdesc->databuff);
	cpkl_free(fdesc);
}

int cpkl_feof(CPKL_FILE *fdesc)
{
	return (fdesc->curpos == fdesc->totallen);
}

char *cpkl_fgets(char *s, int size, CPKL_FILE *fdesc)
{
	u8 *src = fdesc->databuff;
	char *dst = s;

	if ((size == 0) || cpkl_feof(fdesc))
	{
		return 0;
	}
	else
	{
		size--;
	}

	do
	{
		if (cpkl_feof(fdesc) || (size == 0))
		{
			break;
		}
		*dst++ = src[fdesc->curpos];
		size--;
	} while (src[fdesc->curpos++] != 0x0a);

	*dst++ = 0;

	return s;
}

#endif

CODE_SECTION("====================")
CODE_SECTION("ConfigFile parser")
CODE_SECTION("====================")

static cpkl_cpctx_t* cpkl_cpstop_idle(cpkl_cpctx_t *ctx, u8 c, u32 *op)
{
	CPKL_ASSERT(0);

	return NULL;
}

static cpkl_cpctx_t* cpkl_cpstop_tag(cpkl_cpctx_t *ctx, u8 c, u32 *op)
{
	cpkl_cpctx_t *ret = ctx;
	u32 retop = 0;

	switch (c)
	{
	case ']':
	{
		/* now we need to lookup the tag which store in current ctx, match the correspond 'cpent' */
		cpkl_cpent_t *p;
		CPKL_LISTENTRYWALK(p, cpkl_cpent_t, &(ctx->curcpent->subent), listent)
		{
			/*  */
			CPKL_ASSERT(p->parent == ctx->curcpent);

			if (cpkl_pdf_strlen(p->tag) != (ctx->n_char - 1))		/* skip the first '[' */
				continue;

			if (cpkl_pdf_memcmp(&(ctx->dstbuf[1]),					/* skip the first '[' */
								p->tag, cpkl_pdf_strlen(p->tag)))
				continue;

			/* find one matched tag, change the tagcpent */
			ctx->parent->tagcpent = p;

			/*  */
			retop |= CPKP_CPCTXOP_CTXFREE | CPKP_CPCTXOP_SHIFTB;

			if (cpkl_pdf_strlen(p->tag) == 0)
				retop |= CPKP_CPCTXOP_CTXCAT;

			goto cpkl_cpstop_tag_ret;
		}
		/* find one matched tag, change the tagcpent */
		ctx->parent->tagcpent = CPKL_GETCONTAINER(ctx->curcpent->subent.next, cpkl_cpent_t, listent);

		/* we don't find matched tag, just process the 'tag' as normal string */
		retop |= CPKP_CPCTXOP_CTXFREE | CPKP_CPCTXOP_SHIFTB | CPKP_CPCTXOP_CTXCAT;

		break;
	}
	default:
	{
		break;
	}
	}

cpkl_cpstop_tag_ret:
	/* we need to insert this charactor into current ctx, even thouth the ctx has just changed */
	CPKL_ASSERT(ret->n_char < ret->bufsize);
	ret->dstbuf[ret->n_char] = c;
	(ret->n_char)++;

	*op = retop;

	return ret;
}

static cpkl_cpctx_t* cpkl_cpstop_body(cpkl_cpctx_t *ctx, u8 c, u32 *op)
{
	cpkl_cpctx_t *ret = ctx;
	u32 retop = 0;

	switch (c)
	{
	case '/':
	{
		/* may be one new annotation, need to contruct new ctx */
		cpkl_cpctxfcp_t ctxfcp = {
			ctx->bufsize - ctx->n_char, ctx,
			ctx->cp, ctx->up,
			cpkl_cps_pa,			/* change state, prepare annotate */
			ctx->curcpent
		};
		ret = CPKL_FCTNEW(cpkl_cpctx_t, &ctxfcp);
		if (ret == NULL)
		{

		}

		retop |= CPKP_CPCTXOP_SHIFTF;

		break;
	}
	case '[':
	{
		/* may be one new tag, need to contruct new ctx */
		cpkl_cpctxfcp_t ctxfcp = {
			ctx->bufsize - ctx->n_char, ctx,
			ctx->cp, ctx->up,
			cpkl_cps_tag,			/* change state, prepare tag */
			ctx->curcpent
		};
		ret = CPKL_FCTNEW(cpkl_cpctx_t, &ctxfcp);
		if (ret == NULL)
		{

		}

		retop |= CPKP_CPCTXOP_SHIFTF;

		break;
	}
	case '{':
	{
		/* find new body, need to contruct new ctx */
		cpkl_cpctxfcp_t ctxfcp = {
			ctx->bufsize - ctx->n_char, ctx,
			ctx->cp, ctx->up,
			cpkl_cps_body,
			ctx->tagcpent
		};
		ret = CPKL_FCTNEW(cpkl_cpctx_t, &ctxfcp);
		if (ret == NULL)
		{

		}

		/* body start, call the registed start function */
		if (ret->curcpent->start)
			ret->curcpent->start(ret->up);

		retop |= CPKP_CPCTXOP_SHIFTF;

		break;
	}
	case '}':
	{
		/* body end, call the registed parse function */
		if (ret->curcpent->parse)
			ctx->curcpent->parse(&(ctx->dstbuf[1]),		/* skip the first char '{' */
								 ctx->n_char - 1,
								 ctx->up);

		/*  */
		retop |= CPKP_CPCTXOP_CTXFREE | CPKP_CPCTXOP_SHIFTB;

		if (cpkl_pdf_strlen(ctx->curcpent->tag) == 0)
			retop |= CPKP_CPCTXOP_CTXCAT;

		break;
	}
	default:
	{
		break;
	}
	}

	/* we need to insert this charactor into current ctx, even thouth the ctx has just changed */
	CPKL_ASSERT(ret->n_char < ret->bufsize);
	ret->dstbuf[ret->n_char] = c;
	(ret->n_char)++;

	*op = retop;

	return ret;
}

/* prepare to annotate */
static cpkl_cpctx_t* cpkl_cpstop_pa(cpkl_cpctx_t *ctx, u8 c, u32 *op)
{
	cpkl_cpctx_t *ret = ctx;
	u32 retop = 0;

	switch (c)
	{
	case '/':
	{
		/* change state, line annotate */
		ctx->state = cpkl_cps_la;

		break;
	}
	case '*':
	{
		/* change state, block annotate */
		ctx->state = cpkl_cps_ba;

		break;
	}
	default:
	{
		CPKL_ASSERT(ctx->parent != NULL);

		/* current context contant some charactor which is NOT annotation
		 * we need to copy them back to the parent context
		 */
		retop |= CPKP_CPCTXOP_SHIFTB | CPKP_CPCTXOP_CTXCAT | CPKP_CPCTXOP_CTXFREE;

		break;
	}
	}

	/* we need to insert this charactor into current ctx, even thouth the ctx has just changed */
	CPKL_ASSERT(ret->n_char < ret->bufsize);
	ret->dstbuf[ret->n_char] = c;
	(ret->n_char)++;

	*op = retop;

	return ret;
}

static cpkl_cpctx_t* cpkl_cpstop_la(cpkl_cpctx_t *ctx, u8 c, u32 *op)
{
	cpkl_cpctx_t *ret = ctx;
	u32 retop = 0;

	switch (c)
	{
	case 0x0d:
	case 0x0a:
	{
		retop |= CPKP_CPCTXOP_SHIFTB | CPKP_CPCTXOP_CTXFREE;

		break;
	}
	default:
	{
		break;
	}
	}

	/* we need to insert this charactor into current ctx, even thouth the ctx has just changed */
	CPKL_ASSERT(ret->n_char < ret->bufsize);
	ret->dstbuf[ret->n_char] = c;
	(ret->n_char)++;

	*op = retop;

	return ret;
}

static cpkl_cpctx_t* cpkl_cpstop_ba(cpkl_cpctx_t *ctx, u8 c, u32 *op)
{
	cpkl_cpctx_t *ret = ctx;
	u32 retop = 0;

	switch (c)
	{
	case '*':
	{
		/* move to the 'cpkl_cps_baq' state */
		ctx->state = cpkl_cps_baq;

		break;
	}
	default:
	{
		break;
	}
	}

	/* we need to insert this charactor into current ctx, even thouth the ctx has just changed */
	CPKL_ASSERT(ret->n_char < ret->bufsize);
	ret->dstbuf[ret->n_char] = c;
	(ret->n_char)++;

	*op = retop;

	return ret;
}

static cpkl_cpctx_t* cpkl_cpstop_baq(cpkl_cpctx_t *ctx, u8 c, u32 *op)
{
	cpkl_cpctx_t *ret = ctx;
	u32 retop = 0;

	switch (c)
	{
	case '/':
	{
		/* block annotate stop, we need to restore the parse process */
		retop |= CPKP_CPCTXOP_SHIFTB | CPKP_CPCTXOP_CTXFREE;

		break;
	}
	case '*':
	{
		/* just stay in this state */
		break;
	}
	default:
	{
		/* move back to the 'cpkl_cps_ba' state */
		ctx->state = cpkl_cps_ba;

		break;
	}
	}

	/* we need to insert this charactor into current ctx, even thouth the ctx has just changed */
	CPKL_ASSERT(ret->n_char < ret->bufsize);
	ret->dstbuf[ret->n_char] = c;
	(ret->n_char)++;

	*op = retop;

	return ret;
}

/* this sequence should be as same as 'cpkl_cpstate_e' */
static cpkl_cpstop g_cpstop[] = {
	cpkl_cpstop_idle,

	cpkl_cpstop_tag,
	cpkl_cpstop_body,

	cpkl_cpstop_pa,
	cpkl_cpstop_la,
	cpkl_cpstop_ba,
	cpkl_cpstop_baq
};

CPKL_FCTNEW_DEFINE(cpkl_cpctx_t)
{
	if (param == NULL)
		return NULL;

	cpkl_cpctxfcp_t *fcp = (cpkl_cpctxfcp_t *)param;

	cpkl_cpctx_t *newctx = (cpkl_cpctx_t *)cpkl_malloc(sizeof(cpkl_cpctx_t) + fcp->bufsize);
	if (newctx == NULL)
		return NULL;

	/* object functions */

	/* input parameters */
	newctx->bufsize		= fcp->bufsize;
	newctx->parent		= fcp->parent;
	newctx->cp			= fcp->cp;
	newctx->up			= fcp->up;
	newctx->state		= fcp->state;
	newctx->curcpent	= fcp->curcpent;

	/* default parameters */
	newctx->tagcpent	= newctx->curcpent;
	newctx->n_char		= 0;

	return newctx;
}

CPKL_FCTDEL_DEFINE(cpkl_cpctx_t)
{
	if (obj == NULL)
		return;

	cpkl_cpctx_t *ctx = (cpkl_cpctx_t *)obj;

	cpkl_free(ctx);
}

static cpkl_cpent_t* cpkl_cp_addcpent_ex
(
	cpkl_cp_t *cp,
	cpkl_cpent_t *parent,
	u8 *tag,
	cpkl_cpstart start,
	cpkl_cpparse parse
)
{
	/* todo: need to check the 'parent' is one of the cpent of the 'cp'
	 * it can be checked by sh's AVL.
	 */

	cpkl_cpent_t *newcpent = (cpkl_cpent_t *)cpkl_shalloc(cp->cpent_sh);

	cpkl_pdf_memset(newcpent->tag, 0, CPKL_CP_TAGLEN_MAX);
	cpkl_pdf_memcpy(newcpent->tag, tag, cpkl_pdf_strlen(tag));
	newcpent->start = start;
	newcpent->parse = parse;
	newcpent->parent = parent;
	newcpent->n_subent = 0;
	cpkl_initlisthead(&(newcpent->subent));
	cpkl_initlisthead(&(newcpent->listent));

	/**/
	cpkl_listadd_tail(&(newcpent->listent), &(parent->subent));
	(parent->n_subent)++;

	return newcpent;
}

static cpkl_cpent_t* cpkl_cp_addcpent
(
	cpkl_cp_t *cp,
	cpkl_cpent_t *parent,
	u8 *tag,
	cpkl_cpstart start,
	cpkl_cpparse parse
)
{
	cpkl_cpent_t *newcpent;

	/*  */
	newcpent = cpkl_cp_addcpent_ex(cp, parent, tag, start, parse);

	/* each 'cpent' has the 'empty tag' subcpent */
	cpkl_cp_addcpent_ex(cp, newcpent, (u8 *)"", NULL, NULL);

	return newcpent;
}


static int cpkl_cp_parse(cpkl_cp_t *cp, u8 *src, u32 len, void *p)
{
	int ret = 0;
	u32 op;
	cpkl_cpctx_t *ctx, *newctx;

	cpkl_cpctxfcp_t ctxfcp = {len, NULL, cp, p, cpkl_cps_body, cp->root};
	/* this is the root ctx */
	ctx = CPKL_FCTNEW(cpkl_cpctx_t, &ctxfcp);
	if (ctx == NULL)
	{
		ret = -1;
		goto cpkl_cp_parse_ret;
	}

	/* first, we call the global start func */
	if (ctx->curcpent->start)
		ctx->curcpent->start(p);

	while (len && (*src))
	{
		/* charactor parse
		 * the state may changed, the ctx may changed either
		 */
		newctx = g_cpstop[ctx->state](ctx, *src, &op);

		if (op & CPKP_CPCTXOP_SHIFTF)
		{
			ctx = newctx;
		}
		else if (op & CPKP_CPCTXOP_SHIFTB)
		{
			ctx = ctx->parent;
		}

		if (op & CPKP_CPCTXOP_CTXCAT)
		{
			CPKL_ASSERT(newctx != ctx);

			cp->bufcat(cp, newctx, ctx);
		}

		if (op & CPKP_CPCTXOP_CTXFREE)
		{
			CPKL_FCTDEL(cpkl_cpctx_t, newctx);
		}

		if (op & CPKP_CPCTXOP_ERR)
		{
			ret = -1;
		}

		src++;
		len--;
	}


	/* at last, we call the global parse func */
	if (ctx->curcpent->parse)
		ret = ctx->curcpent->parse(ctx->dstbuf, ctx->n_char, p);

	CPKL_FCTDEL(cpkl_cpctx_t, ctx);

cpkl_cp_parse_ret:

	return ret;
}

/*
 * return: number of copy bytes
 */
static int cpkl_cp_bufcat(cpkl_cp_t *cp, cpkl_cpctx_t *ctxfrom, cpkl_cpctx_t *ctxto)
{
	int ret = 0;

	if ((ctxto->n_char + ctxfrom->n_char) <= ctxto->bufsize)
	{
		cpkl_pdf_memcpy(&(ctxto->dstbuf[ctxto->n_char]),
						ctxfrom->dstbuf, ctxfrom->n_char);

		ctxto->n_char += ctxfrom->n_char;
		ret = ctxfrom->n_char;
	}

	return ret;
}

CPKL_FCTNEW_DEFINE(cpkl_cp_t)
{
	if (param == NULL)
		return NULL;

	cpkl_cpfcp_t *fcp	= (cpkl_cpfcp_t *)param;

	cpkl_cp_t *newcp	= (cpkl_cp_t *)cpkl_malloc(sizeof(cpkl_cp_t));
	if (newcp == NULL)
		return NULL;


	/* object functions */
	newcp->addcpent		= cpkl_cp_addcpent;
	newcp->parse		= cpkl_cp_parse;
	newcp->bufcat		= cpkl_cp_bufcat;

	/* input parameters */


	/* default parameters */
{
	cpkl_shfcp_t shfcp	= {sizeof(cpkl_cpent_t), 256, 0};
	newcp->cpent_sh		= CPKL_FCTNEW(cpkl_sh_t, &shfcp);
	if (newcp->cpent_sh == NULL)
	{
		CPKL_FCTDEL(cpkl_cp_t, newcp);
		return NULL;
	}
	newcp->root	= (cpkl_cpent_t *)cpkl_shalloc(newcp->cpent_sh);
	/* we have just 'new' cpent_sh succeed, so the cpkl_shalloc will NOT failed. */
	CPKL_ASSERT(newcp->root);

	/* the 'newcp->root->tag' should be NULL */
	cpkl_pdf_memset(newcp->root->tag, 0, CPKL_CP_TAGLEN_MAX);

	newcp->root->start	= fcp->g_start;
	newcp->root->parse	= fcp->g_parse;
	newcp->root->parent = NULL;
	newcp->root->n_subent = 0;
	cpkl_initlisthead(&(newcp->root->subent));
	cpkl_initlisthead(&(newcp->root->listent));

	/* root 'cpent' has the 'empty tag' subcpent */
	cpkl_cp_addcpent_ex(newcp, newcp->root, (u8 *)"", NULL, NULL);
}

	return newcp;
}

CPKL_FCTDEL_DEFINE(cpkl_cp_t)
{
	if (obj == NULL)
		return;

	cpkl_cp_t *cp = (cpkl_cp_t *)obj;

	if (cp->cpent_sh)
		CPKL_FCTDEL(cpkl_sh_t, cp->cpent_sh);

	cpkl_free(cp);
}

CODE_SECTION("====================")
CODE_SECTION("Thread Pool")
CODE_SECTION("====================")

#ifdef CPKL_CONFIG_THREADPOLL

/* just only one instance */
cpkl_threadpool_t unique_tp = {{{{0}}}, 0};

/* the hsp_tppubent() is the entry function of all the thread in the thread pool. */
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
static DWORD WINAPI cpkl_tppubent(LPVOID param)
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
static void * cpkl_tppubent(void *param)
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
static int cpkl_tppubent(void *param)
#endif
{
	u32 tidx = (u32)CPKL_P2V(param);
	CPKL_ASSERT(tidx < unique_tp.n_tpslot);
	cpkl_tpslot_t *tpslot = &(unique_tp.tpslotlist[tidx]);
#ifdef CPKL_CONFIG_DEBUG
	CPKL_ASSERT(tidx == tpslot->slotidx);
#endif
	cpkl_tpblktsk_t *blktsk;
	while (!(tpslot->tmflag))
	{
		tpslot->state = CPKL_TPSTATE_IDLE;
		/* wait for the signal */
		cpkl_sigwait(&(tpslot->tskblksig));
		tpslot->state = CPKL_TPSTATE_RUNNING;

		while (tpslot->n_blktsk)
		{
			/* lock the list */
			cpkl_mtxlock(&(tpslot->listlock));

			/* remove the tail entry of blocked list */
			blktsk = CPKL_GETCONTAINER(tpslot->blktsk.prev, cpkl_tpblktsk_t, node);
			cpkl_listdel(tpslot->blktsk.prev);
			(tpslot->n_blktsk)--;

			/* unlock the list */
			cpkl_mtxunlock(&(tpslot->listlock));

			/* run the task function */
			blktsk->entry(blktsk->param);

			//
			(tpslot->n_cum)++;

			/* send the terminate signal, notify the parent thread */
			if (blktsk->ternsig)
			{
				cpkl_sigsend(blktsk->ternsig);
			}

			cpkl_free(blktsk);
		}
	}

	cpkl_sigsend(&(tpslot->teminalsig));

	return 0;
}

/* this is the global unique thread pool init */
int cpkl_tpstart(u32 n_thread)
{
	u32 i;
	unique_tp.n_tpslot = n_thread;
	for (i = 0; i < n_thread; i++)
	{
		/*  */
		cpkl_sigcreate(&(unique_tp.tpslotlist[i].tskblksig), 0, 1);

		/* init the list lock, make sure it's signaled after init */
		cpkl_mtxcreate(&(unique_tp.tpslotlist[i].listlock));
		/* after init, without ant block task at all */
		cpkl_initlisthead(&(unique_tp.tpslotlist[i].blktsk));

		unique_tp.tpslotlist[i].tmflag = 0;
		unique_tp.tpslotlist[i].n_blktsk = 0;
		unique_tp.tpslotlist[i].n_cum = 0;
		unique_tp.tpslotlist[i].state = CPKL_TPSTATE_IDLE;

#ifdef CPKL_CONFIG_DEBUG
		unique_tp.tpslotlist[i].slotidx = i;
#endif

		/* create all work thread in the thread pool
		 * the thread create functions is platform dependent
		 */
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
		HANDLE htmp = CreateThread(NULL, 0, cpkl_tppubent, (LPVOID)i, 0, 0);
		/* no need to maintain the handle, just close it */
		CloseHandle(htmp);
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
		pthread_create(&(unique_tp.thdesc[i]), NULL, cpkl_tppubent, CPKL_V2P(i));
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
		unique_tp.thdesc[i] = kthread_create(cpkl_tppubent, (void *)(unsigned long)i, "cpkl_threadpool-%d", i);
		cgroup_kernel_attach_path("./", unique_tp.thdesc[i]);
		wake_up_process(unique_tp.thdesc[i]);
#endif
	}

	unique_tp.init_flag = 1;

	return 0;
}

/* wait for all thread terminated */
void cpkl_tpstop(void)
{
	u32 i;

	if (unique_tp.init_flag == 0)
	{
		cpkl_printf("\nthread pool NOT init.");
		return;
	}

	for (i = 0; i < unique_tp.n_tpslot; i++)
	{
		cpkl_sigcreate(&(unique_tp.tpslotlist[i].teminalsig), 0, 1);
		unique_tp.tpslotlist[i].tmflag = 1;
		cpkl_sigsend(&(unique_tp.tpslotlist[i].tskblksig));
	}

	for (i = 0; i < unique_tp.n_tpslot; i++)
		cpkl_sigwait(&(unique_tp.tpslotlist[i].teminalsig));

	for (i = 0; i < unique_tp.n_tpslot; i++)
	{
		cpkl_sigdsty(&(unique_tp.tpslotlist[i].tskblksig));
		cpkl_sigdsty(&(unique_tp.tpslotlist[i].teminalsig));
		cpkl_mtxdsty(&((unique_tp.tpslotlist[i].listlock)));
	}
}

int cpkl_tpinsert(cpkl_tpentry entry, void *param, cpkl_custsig_t *tersig)
{
	if (unique_tp.init_flag == 0)
	{
		cpkl_printf("\nthread pool NOT init.");
		return -1;
	}

	static unsigned i = 0;
	cpkl_tpslot_t *dstslot = &(unique_tp.tpslotlist[0]);
	cpkl_tpblktsk_t *newtsk = (cpkl_tpblktsk_t *)cpkl_malloc(sizeof(cpkl_tpblktsk_t));

	newtsk->entry = entry;
	newtsk->param = param;
	newtsk->ternsig = tersig;

	/* find ths slot with the most less blocked tasks */
	dstslot = &(unique_tp.tpslotlist[i]);
	i = (i + 1) % unique_tp.n_tpslot;

	/* let's insert this new block task */
	/* lock the list */
	cpkl_mtxlock(&(dstslot->listlock));

	cpkl_listadd(&(newtsk->node), &(dstslot->blktsk));
	(dstslot->n_blktsk)++;

	/* unlock the list */
	cpkl_mtxunlock(&(dstslot->listlock));

	/* wake up the work thread */
	cpkl_sigsend(&(dstslot->tskblksig));

    return 0;
}

/* dump the thread pool statistic info */
void cpkl_tpstat(void)
{
	u32 i;
	if (unique_tp.init_flag == 0)
	{
		cpkl_printf("\nthread pool NOT init.");
		return;
	}

	cpkl_printf("\nthread pool stat: blk done");
	for (i = 0; i < unique_tp.n_tpslot; i++)
	{
		cpkl_printf("\nslot%2d : %s  %d   %d",
					i,
					unique_tp.tpslotlist[i].state == CPKL_TPSTATE_IDLE    ? "IDLE    " :
					unique_tp.tpslotlist[i].state == CPKL_TPSTATE_RUNNING ? "RUNNING " :
						"ERRSTATE",
					unique_tp.tpslotlist[i].n_blktsk, unique_tp.tpslotlist[i].n_cum);
	}
}

#else
/* even thouth we don't impl the threadpool facility,
 * we need to run the task function when call this hsp_tpinsert()
 */
int cpkl_tpinsert(cpkl_tpentry entry, void *param, cpkl_custsig_t *tersig)
{
	/* run the task function */
	entry(param);
	/* send the terminate signal, notify the parent thread */
	if (tersig)
	{
		cpkl_sigsend(tersig);
	}

    return 0;
}

#endif

CODE_SECTION("====================")
CODE_SECTION("Custom qsort")
CODE_SECTION("====================")

#ifdef CPKL_CONFIG_CUSTOM_QSORT

#define CUTOFF 8            /* testing shows that this is good value */

/* Note: the theoretical number of stack entries required is
   no more than 1 + log2(num).  But we switch to insertion
   sort for CUTOFF elements or less, so we really only need
   1 + log2(num) - log2(CUTOFF) stack entries.  For a CUTOFF
   of 8, that means we need no more than 30 stack entries for
   32 bit platforms, and 62 for 64-bit platforms. */
#define STKSIZ (8*sizeof(void*) - 2)

#define __COMPARE(context, p1, p2) comp(p1, p2)
#define __SHORTSORT(lo, hi, width, comp, context) shortsort(lo, hi, width, comp);

static inline void buff_swap (char *a, char *b, unsigned width)
{
    char tmp;

    if ( a != b )
        /* Do the swap one character at a time to avoid potential alignment
           problems. */
        while ( width-- ) {
            tmp = *a;
            *a++ = *b;
            *b++ = tmp;
        }
}

static void shortsort (char *lo, char *hi, unsigned width, int (*comp)(const void *, const void *))
{
    char *p, *max;

    /* Note: in assertions below, i and j are alway inside original bound of
       array to sort. */

    while (hi > lo) {
        /* A[i] <= A[j] for i <= j, j > hi */
        max = lo;
        for (p = lo+width; p <= hi; p += width) {
            /* A[i] <= A[max] for lo <= i < p */
            if (__COMPARE(context, p, max) > 0) {
                max = p;
            }
            /* A[i] <= A[max] for lo <= i <= p */
        }

        /* A[i] <= A[max] for lo <= i <= hi */

        buff_swap(max, hi, width);

        /* A[i] <= A[hi] for i <= hi, so A[i] <= A[j] for i <= j, j >= hi */

        hi -= width;

        /* A[i] <= A[j] for i <= j, j > hi, loop top condition established */
    }
    /* A[i] <= A[j] for i <= j, j > lo, which implies A[i] <= A[j] for i < j,
       so array is sorted */
}

void cpkl_qsort(void *base, unsigned num, unsigned width, int (*comp)(const void *, const void *))
{
    char *lo, *hi;              /* ends of sub-array currently sorting */
    char *mid;                  /* points to middle of subarray */
    char *loguy, *higuy;        /* traveling pointers for partition step */
    unsigned size;                /* size of the sub-array */
    char *lostk[STKSIZ], *histk[STKSIZ];
    int stkptr;                 /* stack for saving sub-array to be processed */

    if (num < 2)
        return;                 /* nothing to do */

    stkptr = 0;                 /* initialize stack */

    lo = (char *)base;
    hi = (char *)base + width * (num-1);        /* initialize limits */

    /* this entry point is for pseudo-recursion calling: setting
       lo and hi and jumping to here is like recursion, but stkptr is
       preserved, locals aren't, so we preserve stuff on the stack */
recurse:

    size = (hi - lo) / width + 1;        /* number of el's to sort */

    /* below a certain size, it is faster to use a O(n^2) sorting method */
    if (size <= CUTOFF) {
        __SHORTSORT(lo, hi, width, comp, context);
    }
    else {
        /* First we pick a partitioning element.  The efficiency of the
           algorithm demands that we find one that is approximately the median
           of the values, but also that we select one fast.  We choose the
           median of the first, middle, and last elements, to avoid bad
           performance in the face of already sorted data, or data that is made
           up of multiple sorted runs appended together.  Testing shows that a
           median-of-three algorithm provides better performance than simply
           picking the middle element for the latter case. */

        mid = lo + (size / 2) * width;      /* find middle element */

        /* Sort the first, middle, last elements into order */
        if (__COMPARE(context, lo, mid) > 0) {
            buff_swap(lo, mid, width);
        }
        if (__COMPARE(context, lo, hi) > 0) {
            buff_swap(lo, hi, width);
        }
        if (__COMPARE(context, mid, hi) > 0) {
            buff_swap(mid, hi, width);
        }

        /* We now wish to partition the array into three pieces, one consisting
           of elements <= partition element, one of elements equal to the
           partition element, and one of elements > than it.  This is done
           below; comments indicate conditions established at every step. */

        loguy = lo;
        higuy = hi;

        /* Note that higuy decreases and loguy increases on every iteration,
           so loop must terminate. */
        for (;;) {
            /* lo <= loguy < hi, lo < higuy <= hi,
               A[i] <= A[mid] for lo <= i <= loguy,
               A[i] > A[mid] for higuy <= i < hi,
               A[hi] >= A[mid] */

            /* The doubled loop is to avoid calling comp(mid,mid), since some
               existing comparison funcs don't work when passed the same
               value for both pointers. */

            if (mid > loguy) {
                do  {
                    loguy += width;
                } while (loguy < mid && __COMPARE(context, loguy, mid) <= 0);
            }
            if (mid <= loguy) {
                do  {
                    loguy += width;
                } while (loguy <= hi && __COMPARE(context, loguy, mid) <= 0);
            }

            /* lo < loguy <= hi+1, A[i] <= A[mid] for lo <= i < loguy,
               either loguy > hi or A[loguy] > A[mid] */

            do  {
                higuy -= width;
            } while (higuy > mid && __COMPARE(context, higuy, mid) > 0);

            /* lo <= higuy < hi, A[i] > A[mid] for higuy < i < hi,
               either higuy == lo or A[higuy] <= A[mid] */

            if (higuy < loguy)
                break;

            /* if loguy > hi or higuy == lo, then we would have exited, so
               A[loguy] > A[mid], A[higuy] <= A[mid],
               loguy <= hi, higuy > lo */

            buff_swap(loguy, higuy, width);

            /* If the partition element was moved, follow it.  Only need
               to check for mid == higuy, since before the swap,
               A[loguy] > A[mid] implies loguy != mid. */

            if (mid == higuy)
                mid = loguy;

            /* A[loguy] <= A[mid], A[higuy] > A[mid]; so condition at top
               of loop is re-established */
        }

        /*     A[i] <= A[mid] for lo <= i < loguy,
               A[i] > A[mid] for higuy < i < hi,
               A[hi] >= A[mid]
               higuy < loguy
           implying:
               higuy == loguy-1
               or higuy == hi - 1, loguy == hi + 1, A[hi] == A[mid] */

        /* Find adjacent elements equal to the partition element.  The
           doubled loop is to avoid calling comp(mid,mid), since some
           existing comparison funcs don't work when passed the same value
           for both pointers. */

        higuy += width;
        if (mid < higuy) {
            do  {
                higuy -= width;
            } while (higuy > mid && __COMPARE(context, higuy, mid) == 0);
        }
        if (mid >= higuy) {
            do  {
                higuy -= width;
            } while (higuy > lo && __COMPARE(context, higuy, mid) == 0);
        }

        /* OK, now we have the following:
              higuy < loguy
              lo <= higuy <= hi
              A[i]  <= A[mid] for lo <= i <= higuy
              A[i]  == A[mid] for higuy < i < loguy
              A[i]  >  A[mid] for loguy <= i < hi
              A[hi] >= A[mid] */

        /* We've finished the partition, now we want to sort the subarrays
           [lo, higuy] and [loguy, hi].
           We do the smaller one first to minimize stack usage.
           We only sort arrays of length 2 or more.*/

        if ( higuy - lo >= hi - loguy ) {
            if (lo < higuy) {
                lostk[stkptr] = lo;
                histk[stkptr] = higuy;
                ++stkptr;
            }                           /* save big recursion for later */

            if (loguy < hi) {
                lo = loguy;
                goto recurse;           /* do small recursion */
            }
        }
        else {
            if (loguy < hi) {
                lostk[stkptr] = loguy;
                histk[stkptr] = hi;
                ++stkptr;               /* save big recursion for later */
            }

            if (lo < higuy) {
                hi = higuy;
                goto recurse;           /* do small recursion */
            }
        }
    }

    /* We have sorted the array, except for any pending sorts on the stack.
       Check if there are any, and do them. */

    --stkptr;
    if (stkptr >= 0) {
        lo = lostk[stkptr];
        hi = histk[stkptr];
        goto recurse;           /* pop subarray from stack */
    }
    else
        return;                 /* all subarrays done */
}
#endif

CODE_SECTION("====================")
CODE_SECTION("Timer Linker")
CODE_SECTION("====================")

#ifdef CPKL_CONFIG_TIMERLINK

/* we need to start the only real timer */
static cpkl_tmlk_t	tmlk;

static void cpkl_tmpub(void)
{
	cpkl_tmentry_t *p, *n;

	cpkl_mtxlock(&(tmlk.tml_lock));

	CPKL_LISTENTRYWALK_SAVE(p, n, cpkl_tmentry_t, &(tmlk.tml), ln)
	{
		(p->n_count)--;

		if (p->n_count == 0)
		{
			p->n_count = p->n_tm;

			/* call handle with register param */
			p->handle(p->param, p->state);

			if (p->state == cpkl_tmestat_init)
				p->state = cpkl_tmestat_normal;
		}
	}

	cpkl_mtxunlock(&(tmlk.tml_lock));
}

#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
static void CALLBACK cpkl_tmpubentry(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	cpkl_tmpub();
}
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
#ifdef CPKL_CONFIG_TMBYSELECT
static void * cpkl_tmselect(void *param)
{
	struct timeval temp;

	while (1)
	{
		temp.tv_sec = tmlk.pubintv / 1000;
		temp.tv_usec = (tmlk.pubintv * 1000) % 1000000;

		select(0, NULL, NULL, NULL, &temp);
		cpkl_tmpub();
	}

	return NULL;
}

#else
static void cpkl_tmpubentry(int sig)
{
	cpkl_tmpub();
}
#endif
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD

#endif

static void cpkl_tmlkstart()
{
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
	timeSetEvent(tmlk.pubintv, 1, cpkl_tmpubentry, NULL, TIME_PERIODIC);
	// timeKillEvent();
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
#ifdef CPKL_CONFIG_TMBYSELECT
	pthread_t tmthread;
	pthread_create(&tmthread, NULL, cpkl_tmselect, NULL);
#else
	signal(SIGALRM, cpkl_tmpubentry);
	struct itimerval timer;
	timer.it_value.tv_sec	= timer.it_interval.tv_sec	= tmlk.pubintv / 1000;
	timer.it_value.tv_usec	= timer.it_interval.tv_usec	= (tmlk.pubintv * 1000) % 1000000;
	setitimer(ITIMER_REAL, &timer, NULL);
#endif
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
#endif

	tmlk.tmst = CPKL_TMLKSTATE_PUBTMSTART;
}

int cpkl_tmlkinit(u32 pubintv)
{
	if (tmlk.tmst != CPKL_TMLKSTATE_UNINIT)
	{
		return -1;
	}

	cpkl_initlisthead(&(tmlk.tml));
	cpkl_mtxcreate(&(tmlk.tml_lock));
	tmlk.tmst = CPKL_TMLKSTATE_PUBTMSTOP;
	tmlk.pubintv = pubintv;

	return 0;
}

/* register timer */
int cpkl_tmreg(u32 n_pubintv, cpkl_tmhandle handle, void *param)
{
	static u32 _id = 0;

	if ((n_pubintv == 0) || (tmlk.tmst == CPKL_TMLKSTATE_UNINIT))
		return -1;

	if (tmlk.tmst == CPKL_TMLKSTATE_PUBTMSTOP)
		cpkl_tmlkstart();

	cpkl_tmentry_t *newtm = (cpkl_tmentry_t *)cpkl_malloc(sizeof(cpkl_tmentry_t));
	newtm->handle = handle;
	newtm->param = param;
	newtm->n_tm = newtm->n_count = n_pubintv;
	newtm->id = ++_id;
	newtm->state = cpkl_tmestat_init;

	cpkl_mtxlock(&(tmlk.tml_lock));
	cpkl_listadd(&(newtm->ln), &(tmlk.tml));
	cpkl_mtxunlock(&(tmlk.tml_lock));

	return newtm->id;
}

/* unregister timer */
void cpkl_tmunreg(u32 id)
{
	cpkl_tmentry_t *p, *n;

	cpkl_mtxlock(&(tmlk.tml_lock));

	CPKL_LISTENTRYWALK_SAVE(p, n, cpkl_tmentry_t, &(tmlk.tml), ln)
	{
		if (p->id == id)
		{
			if (p->state == cpkl_tmestat_normal)
				p->handle(p->param, cpkl_tmestat_closing);

			/* remove from link list */
			cpkl_listdel(&(p->ln));
			/* remove this tm entry */
			cpkl_free(p);

			break;
		}
	}

	cpkl_mtxunlock(&(tmlk.tml_lock));
}

#endif
