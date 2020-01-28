void rdpt_hexdump(void *buf, unsigned len, const char *prefix)
{
#define HEXDUMP_LINEWIDTH			(16)
	unsigned char *p = (unsigned char *)buf;
	unsigned i;

	printf("\n%s"
				"       ",
				prefix);
	for (i = 0; i < HEXDUMP_LINEWIDTH; i++)
	{
		if ((i % 4) == 0)
			printf("%02x ", i);
		else
			printf("   ");
	}

	for (i = 0; i < len; i++)
	{
		if ((i % HEXDUMP_LINEWIDTH) == 0)
			printf("\n%s"
						"0x%03X: ",
						prefix,
						i);
		printf("%02X ", *p++);
	}
}


#define STDIVCTRL_EMPTYSUBSTR			0x1

/*
 * search and compare with the divflag array, cut the string into several sub string
 * the substring just without the divflag
 */
int stdiv
(
	char *buf,			/* input */
	int buflen,			/* input */
	int n_argv,			/* input: sizeof argv */
	char *argv[],		/* output */
	unsigned len[],		/* output */
	int n_divflag,		/* input */
	const char *divflag,/* input */
	unsigned ctrl		/* input */
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
					if (ctrl & STDIVCTRL_EMPTYSUBSTR)
					{
						/* need to save this empty flag */

						if (ret == n_argv)
							return ret;

						ret++;
					}
					break;
				default:
					break;
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
			break;
		}

cpkl_stdiv_nextch:

        i++;
	}

    return ret;
}

static char* withparam(char *argv[], int n, const char *param)
{
	int i, len;
	for (i = 0; i < n; i++)
	{
		len = strlen(param);
		if (memcmp(argv[i], param, strlen(param)) == 0)
		{
			return argv[i] + len;
		}
	}

	return NULL;
}

static int isnum(char c)
{
	if ((c >= '0') && (c <= '9'))
	{
		return 1;
	}

	return 0;
}

static int isipaddr(const char *str, unsigned len)
{
	unsigned i, n_dig = 0, n_num = 0;

	for (i = 0; i < len; i++)
	{
		if (isnum(str[i]))
		{
			n_dig++;
		}
		else if (str[i] == '.')
		{
			if (n_dig > 3)
				return 0;

			n_dig = 0;
			n_num++;
		}
		else
			return 0;
	}

	if (n_num != 4)
		return 0;

	return 1;
}

/* www.xxx.com:1234 or 1.2.3.4:1234 */
static int url2addr(char *url, struct sockaddr_in *addr)
{
	char *argv[2];
	unsigned len[2], port;
	char tmpstr[32];
	int n_arg = stdiv(url, strlen(url), 2, argv, len, 1, ":", 0);
	if (n_arg != 2)
		return RDPT_SOCKSTATE_SOCKOPEN_INVALID_URL;

	/*  */
	memset(tmpstr, 0, sizeof(tmpstr));
	memcpy(tmpstr, argv[0], len[0]);
	if (!isipaddr(argv[0], len[0]))
	{
		/* get dest ip addr by name */
		struct hostent *h = gethostbyname(tmpstr);
		if (h == NULL)
		{
			return RDPT_SOCKSTATE_SOCKOPEN_GETHOSTBYNAME_FAILD;
		}

		addr->sin_addr.s_addr = *((unsigned *)(h->h_addr));
	}
	else
	{
		addr->sin_addr.s_addr = inet_addr(tmpstr);
	}

	memset(tmpstr, 0, sizeof(tmpstr));
	memcpy(tmpstr, argv[1], len[1]);	/* port */
	sscanf(tmpstr, "%d", &port);
	addr->sin_port = htons((unsigned short)port);

	return RDPT_SOCKSTATE_SOCKOPEN_OK;
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

	return ret;
}

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

/* kbd transform buffer */
static unsigned char g_incmap[] = {
	0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x0a, 0x0b, 0x0c, 0x0d, 0x10, 0x11, 0x12, 0x13,
	0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
	0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
	0x26, 0x27, 0x28, 0x29, 0x2b, 0x2c, 0x2d, 0x2e,
	0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f
};

