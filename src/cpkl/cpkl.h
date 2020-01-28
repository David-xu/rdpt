#ifndef _CPKL_H_
#define _CPKL_H_

/*****************************************************************************/
/*****************************************************************************/
/*             This is the user config MACROs                                */

#define CPKL_CONFIG_PLATFORM            CPKL_CONFIG_PLATFORM_LINUX_UMOD
#define CPKL_CONFIG_BSTTYPE             CPKL_CONFIG_BSTTYPE_AVL

/* close the debug switch, it will run much more faster */
#define CPKL_CONFIG_DEBUG

/* atomic operation */
#define CPKL_CONFIG_ATOMIC

/* time statistic switch, we use it to statis the procedure time consumption */
#define CPKL_CONFIG_TMS

/* reload the malloc() and free(),
   we use this mechanism to statistic the memory useage. */
#define CPKL_CONFIG_MEMMONITOR

/* random infrastructure switch, kernel mode has not support this */
#define CPKL_CONFIG_RI

/* redirection the printing */
// #define CPKL_CONFIG_COSTUM_RPINTF

/* print with timestamp like [sec.usec]: ... */
// #define CPKL_CONFIG_PRINT_TMSTAMP

/* float number support, some times CPU may NOT support the float point number */
#define CPKL_CONFIG_FNS

/* timer link impl, kernel mode has not support, need to complete */
#define CPKL_CONFIG_TIMERLINK

/* hashlist use the slabheap which store the hash entry, it's more efficent than malloc directly */
#define CPKL_CONFIG_HL_USESH

/* big endian */
// #define CPKL_CONFIG_BE

/* thread pool */
#define CPKL_CONFIG_THREADPOLL

/*             user config MACROs definition end                             */
/*****************************************************************************/
/*****************************************************************************/

#define CODE_SECTION(t)

/*
 * now we just support 3 different types of OS
 * just define the macro which NAMED
 * 'CPKL_CONFIG_PLATFORM'
 * with one of the 3 values below
 */
#define	CPKL_CONFIG_PLATFORM_WINDOWS		1
#define	CPKL_CONFIG_PLATFORM_LINUX_UMOD		2
#define	CPKL_CONFIG_PLATFORM_LINUX_KMOD		3

/*
 * we need to decide the alu operator width
 * just define the macro which NAMED
 * 'CPKL_CONFIG_ALUWIDTH'
 * with on of the 2 values below
 */
#define CPKL_CONFIG_ALUWIDTH_32				1
#define CPKL_CONFIG_ALUWIDTH_64				2

/*
 * some platform dependent headfiles and functions
 */
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment (lib, "Winmm.lib")
#pragma comment (lib, "ws2_32.lib")

#define cpkl_pdf_malloc			malloc
#define cpkl_pdf_free			free
#define cpkl_pdf_printf			printf
#define cpkl_pdf_sprintf		sprintf_s
#define cpkl_pdf_sscanf			sscanf
#define cpkl_pdf_memset			memset
#define cpkl_pdf_memcpy			memcpy
#define cpkl_pdf_memcmp			memcmp
#define cpkl_pdf_strlen(sz)		strlen((const char *)(sz))
#define cpkl_pdf_strcmp			strcmp
#define cpkl_pdf_srand			srand
#define cpkl_pdf_rand			rand
#define	cpkl_pdf_qsort			qsort

#define cpkl_pdf_fopen			fopen
#define cpkl_pdf_fclose			fclose
#define cpkl_pdf_feof			feof
#define cpkl_pdf_fseek			fseek
#define cpkl_pdf_ftell			ftell
#define cpkl_pdf_fgets			fgets
#define cpkl_pdf_fread			fread
#define cpkl_pdf_fwrite			fwrite
#define cpkl_pdf_fprintf		fprintf

#define cpkl_pdf_time			time
#define cpkl_pdf_localtime		localtime
#define cpkl_pdf_usleep(n)		Sleep((n) / 1000)

#define CPKL_PATHDASH			"\\"

// #define CPKL_MUTEX_CS

#define __attribute__(attib)

#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD

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
#include <sys/types.h>

#define cpkl_pdf_malloc			malloc
#define cpkl_pdf_free			free
#define cpkl_pdf_printf			printf
#define cpkl_pdf_sprintf		snprintf
#define cpkl_pdf_sscanf			sscanf
#define cpkl_pdf_memset			memset
#define cpkl_pdf_memcpy			memcpy
#define cpkl_pdf_memcmp			memcmp
#define cpkl_pdf_strlen(sz)		strlen((const char *)(sz))
#define cpkl_pdf_strcmp			strcmp
#define cpkl_pdf_srand			srand
#define cpkl_pdf_rand			rand
#define	cpkl_pdf_qsort			qsort

#define cpkl_pdf_fopen			fopen
#define cpkl_pdf_fclose			fclose
#define cpkl_pdf_feof			feof
#define cpkl_pdf_fseek			fseek
#define cpkl_pdf_ftell			ftell
#define cpkl_pdf_fgets			fgets
#define cpkl_pdf_fread			fread
#define cpkl_pdf_fwrite			fwrite
#define cpkl_pdf_fprintf		fprintf

#define cpkl_pdf_time			time
#define cpkl_pdf_localtime		localtime
#define cpkl_pdf_usleep			usleep

#define CPKL_PATHDASH			"/"

#define CPKL_CONFIG_TMBYSELECT

#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD

#ifdef CPKL_CONFIG_PRINT_TMSTAMP
#undef CPKL_CONFIG_PRINT_TMSTAMP
#endif

#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#define cpkl_pdf_malloc(sz)		kmalloc(sz, GFP_KERNEL)
#define cpkl_pdf_free			kfree
#define cpkl_pdf_printf			printk
#define cpkl_pdf_sprintf		sprintf
#define cpkl_pdf_sscanf			sscanf
#define cpkl_pdf_memset			memset
#define cpkl_pdf_memcpy			memcpy
#define cpkl_pdf_memcmp			memcmp
#define cpkl_pdf_strlen			strlen
#define cpkl_pdf_strcmp			strcmp
#define cpkl_pdf_srand			srand
#define cpkl_pdf_rand			rand

#define CPKL_PATHDASH			"/"

/* there is no fileop funtions, we need to impl */
#define CPKL_CONFIG_CUSTOM_FILEOP

/* there is no qsort funtion, we need to impl */
#define CPKL_CONFIG_CUSTOM_QSORT

#else
#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif

typedef	char					s8;
typedef	short					s16;
typedef	int						s32;
typedef	long long				s64;


typedef	unsigned char			u8;
typedef	unsigned short			u16;
typedef	unsigned int			u32;
typedef	unsigned long long		u64;

#ifdef CPKL_CONFIG_FNS
typedef float					f32;
typedef double					f64;
#endif

#define CPKL_P2V(p)				((char *)(p) - (char *)0)
#define CPKL_V2P(v)				((void *)((char *)0 + (v)))

#define CPKL_ARRAY_SIZE(ar)		(sizeof(ar) / sizeof((ar)[0]))
#define CPKL_FIELD_SIZE(type, field)	\
	(sizeof(((type *)(0))->field))

#define CPKL_ALIGN(l, align)	((((l) + (align) - 1) / (align)) * (align))
#define CPKL_FIELDLEN(s, f)		(sizeof(((s *)(0))->f))
/* some special value which CPKL used */
#define	CPKL_INVALID_IDX		((u32)(-1))

static inline char cpkl_hex2num(char c)
{
	return ((c >= '0') && (c <= '9')) ? (c - '0') :
			(((c >= 'A') && (c <= 'F')) ? (c - 'A' + 10) : (c - 'a' + 10));
}

#ifdef CPKL_CONFIG_DEBUG
#define CPKL_ASSERT(cond)												\
	do {																\
		if (!(cond))													\
		{																\
			cpkl_printf("\n%s:%d, %s", __FILE__, __LINE__, #cond);		\
			while (1);													\
		}																\
	} while (0)
#else
#define CPKL_ASSERT(cond)		do {(void)(cond);} while(0);
#endif

#ifdef CPKL_CONFIG_PRINT_TMSTAMP
int cpkl_printf(const char *fmt, ...);
#else
/* no need time stamp, just call platform printf */
#define cpkl_printf				cpkl_pdf_printf
#endif

CODE_SECTION("====================")
CODE_SECTION("Factory definition")
CODE_SECTION("====================")

#define CPKL_FCTNEW_DEFINE(type)				void *cpkl_new_##type(void *param)
#define CPKL_FCTNEW(type, param)				(type *)cpkl_new_##type(param)
#define CPKL_FCTDEL_DEFINE(type)				void cpkl_delete_##type(void *obj)
#define CPKL_FCTDEL(type, obj)					cpkl_delete_##type(obj)

CODE_SECTION("====================")
CODE_SECTION("Atomic operation")
CODE_SECTION("====================")
typedef struct _cpkl_atomic {
	volatile long long		__v;
} cpkl_atomic_t;

#ifdef CPKL_CONFIG_ATOMIC
/*
 * we use some gcc builtin functions, so gcc is prerequisite under linux platform.
 */
static inline long long cpkl_atomic_add(cpkl_atomic_t *atom, long long val)
{
	long long __ret;
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
	__ret = InterlockedExchangeAdd64(&(atom->__v), val);
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	__ret = __sync_fetch_and_add(&(atom->__v), val);
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD

#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif

	return __ret;
}

static inline long long cpkl_atomic_sub(cpkl_atomic_t *atom, long long val)
{
	long long __ret;
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
	__ret = InterlockedExchangeSubtract((u64 *)&(atom->__v), (u64)val);
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	__ret = __sync_fetch_and_sub(&(atom->__v), val);
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD

#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif

	return __ret;
}


#else

static inline long long cpkl_atomic_add(cpkl_atomic_t *atom, long long val)
{
	long long _tmp = atom->__v;
	atom->__v += val;
	return _tmp;
}

static inline long long cpkl_atomic_sub(cpkl_atomic_t *atom, long long val)
{
	long long _tmp = atom->__v;
	atom->__v -= val;
	return _tmp;
}

#endif

CODE_SECTION("====================")
CODE_SECTION("Double LinkList")
CODE_SECTION("====================")

#define	CPKL_GETCONTAINER(p, type, field)				\
	((type *)CPKL_V2P(CPKL_P2V(p) - (CPKL_P2V(&(((type *)0)->field)))))

typedef struct _cpkl_listhead {
	struct _cpkl_listhead	*prev;
	struct _cpkl_listhead	*next;
} cpkl_listhead_t;

#define	CPKL_LISTHEAD_INIT(head)		{&head, &head};

#define CPKL_LISTHEAD(head)									\
	cpkl_listhead_t head = CPKL_LISTHEAD_INIT(head);

#define	CPKL_LISTISEMPLY(head)			((head) == ((head)->next))

#define CPKL_LISTWALK(p, head)								\
		for (p = (head)->next;								\
			 p != (head);									\
			 p = p->next)

#define CPKL_LISTWALK_SAVE(p, n, head)						\
		for (p = (head)->next, n = p->next;					\
			 p != (head);									\
			 p = n, n = p->next)

#define CPKL_LISTENTRYWALK(p, type, head, field)					\
		for (p = CPKL_GETCONTAINER((head)->next, type, field);		\
			 &(p->field) != (head);									\
			 p = CPKL_GETCONTAINER((p)->field.next, type, field))

#define CPKL_LISTENTRYWALK_SAVE(p, n, type, head, field)			\
		for (p = CPKL_GETCONTAINER((head)->next, type, field),		\
			 n = CPKL_GETCONTAINER((head)->next->next, type, field);\
			 &(p->field) != (head);									\
			 p = n, n = CPKL_GETCONTAINER(p->field.next, type, field))

static inline void cpkl_initlisthead(cpkl_listhead_t *head)
{
	head->next = head->prev = head;
}

static inline cpkl_listhead_t *cpkl_listprev(cpkl_listhead_t *p, cpkl_listhead_t *head)
{
	p = p->prev;

	return p != head ? p : NULL;
}

static inline cpkl_listhead_t *cpkl_listnext(cpkl_listhead_t *p, cpkl_listhead_t *head)
{
	p = p->next;

	return p != head ? p : NULL;
}
static inline void cpkl_listadd_(cpkl_listhead_t *p, cpkl_listhead_t *prev, cpkl_listhead_t *next)
{
	prev->next = p;
	p->prev = prev;
	p->next = next;
	next->prev = p;
}

static inline void cpkl_listdel_(cpkl_listhead_t *prev, cpkl_listhead_t *next)
{
	prev->next = next;
	next->prev = prev;
}

/* add the element @p to head */
static inline void cpkl_listadd(cpkl_listhead_t *p, cpkl_listhead_t *head)
{
	cpkl_listadd_(p, head, head->next);
}
static inline void cpkl_listadd_tail(cpkl_listhead_t *p, cpkl_listhead_t *head)
{
	cpkl_listadd_(p, head->prev, head);
}

static inline void cpkl_listdel(cpkl_listhead_t *p)
{
	if (!CPKL_LISTISEMPLY(p))
		cpkl_listdel_(p->prev, p->next);
}

static inline void cpkl_listndmv(cpkl_listhead_t *from, cpkl_listhead_t *to)
{
	from->prev->next = to;
	from->next->prev = to;

	to->prev = from->prev;
	to->next = from->next;
}

static inline void cpkl_listmove2tail(cpkl_listhead_t *p, cpkl_listhead_t *head)
{
	cpkl_listdel(p);
	cpkl_listadd_tail(p, head);
}

/*
 *     list1             list2
 *   /       \         /       \
 * node1 - node2     node3 - node4
 *                ||
 *                ||
 *                \/
 *              list1                     list2
 *    /                      \
 *  node1 _ node2 - node3 - node4
 */
static inline void cpkl_listmerge(cpkl_listhead_t *list1, cpkl_listhead_t *list2)
{
	if (!CPKL_LISTISEMPLY(list2))
	{
		list1->prev->next = list2->next;
		list2->next->prev = list1->prev;

		list1->prev = list2->prev;
		list2->prev->next = list1;

		cpkl_initlisthead(list2);
	}
}

CODE_SECTION("====================")
CODE_SECTION("Singal LinkList")
CODE_SECTION("====================")

typedef struct _cpkl_slisthead {
	struct _cpkl_slisthead	*next;
} cpkl_slisthead_t;

#define CPKL_SLISTHEAD(head)								\
	cpkl_slisthead_t head = {NULL};

#define	CPKL_SLISTISEMPLY(head)			((head)->next == NULL)

#define CPKL_SLISTWALK(p, head)								\
		for (p = (head)->next;								\
			 p != NULL;										\
			 p = p->next)

#define CPKL_SLISTWALK_SAVE(p, n, head)						\
		for (p = (head)->next, n = p->next;					\
			 p != NULL;										\
			 p = n, n = ((p == NULL) ? NULL : p->next))

static inline void cpkl_initslisthead(cpkl_slisthead_t *head)
{
	head->next = NULL;
}

/* add the element @p between pos and pos->next */
static inline void cpkl_slistadd(cpkl_slisthead_t *p, cpkl_slisthead_t *pos)
{
	p->next = pos->next;
	pos->next = p;
}

static inline void cpkl_slistreverse(cpkl_slisthead_t **head)
{
	cpkl_slisthead_t *p, *prev, *next;

	p = *head;
	prev = NULL;
	while (p)
	{
		next = p->next;
		p->next = prev;
		prev = p;
		p = next;
	}
	*head = prev;
}

CODE_SECTION("====================")
CODE_SECTION("Some Public infrastruct")
CODE_SECTION("====================")

#ifndef CPKL_CONFIG_BE
#define CPKL_HTONS(w)			((((w) & 0x00FF) << 8) | (((w) & 0xFF00) >> 8))
#define CPKL_HTONL(w)			((((w) & 0xFF) << 24) | (((w) & 0xFF00) << 8) | (((w) & 0xFF0000) >> 8) | (((w) & 0xFF000000) >> 24))
#else
#define CPKL_HTONS(w)			(w)
#define CPKL_HTONL(w)			(w)
#endif
static inline u32 cpkl_alg_getbw32(u32 src)
{
	u32 len = 0;

	if (src & 0xFFFF0000)
	{
		src >>= 16;
		len += 16;
	}
	if (src & 0xFF00)
	{
		src >>= 8;
		len += 8;
	}
	if (src & 0xF0)
	{
		src >>= 4;
		len += 4;
	}
	if (src & 0xC)
	{
		src >>= 2;
		len += 2;
	}
	if (src & 0x2)
	{
		src >>= 1;
		len += 1;
	}
	if (src & 0x1)
		len += 1;

	return len;
}

u32 cpkl_alg_crc32(const void *pv, u32 size);
u32 cpkl_alg_crc32c(const void* pv, u32 size);
u64 cpkl_alg_crc64(const void *pv, u32 size);
u64 cpkl_alg_crc64ck(const void *pv, u32 size);
u16 cpkl_alg_foldxor(const void *key, u32 size);
void *cpkl_alg_bsch(void *base, unsigned num, unsigned width, void *dst, int (*comp)(const void *, const void *));
#define CPKL_STDIVCTRL_EMPTYSUBSTR			0x1
int cpkl_stdiv(char *buf, int buflen, int n_argv, char *argv[], u32 len[], int n_divflag, char *divflag, u32 ctrl);
void cpkl_bswap(void *p, u32 size);
void cpkl_hexdump(void *buf, u32 len, char *prefix);

CODE_SECTION("====================")
CODE_SECTION("Time statistic")
CODE_SECTION("====================")

#define CPKL_TMSREPORTALL				((int)-1)

enum {
	CPKL_TMS_OFF = 0,
	CPKL_TMS_ON,				/* time statistic switch on */
};

/*
 * return: the us of current time
 */
u64 cpkl_tmsstamp(void);

#ifdef CPKL_CONFIG_TMS

void cpkl_tms(int tmsidx, int swch);
/* we can add some commment with the time statistic */
void cpkl_tmsreset(int tmsidx, char *comm);
/*  */
void cpkl_tmreport(int tmsidx);
#else
/*
 * set the @swch with CPKL_TMS_ON to turn on the tms or
 * set it with CPKL_TMS_OFF to turn off
 */
static inline void cpkl_tms(int tmsidx, int swch) {}
static inline void cpkl_tmsreset(int tmsidx, char *comm) {}
static inline void cpkl_tmreport(int tmsidx) {}
#endif

CODE_SECTION("====================")
CODE_SECTION("custom print")
CODE_SECTION("====================")
#ifdef CPKL_CONFIG_COSTUM_RPINTF
/* import this function, just redirect the printf into some special device */
extern int cpkl_import_custprintf(const char *fmt, ...);
#define cpkl_pdf_printf				cpkl_import_custprintf
#endif

CODE_SECTION("====================")
CODE_SECTION("Random Infrastructure")
CODE_SECTION("====================")
#ifdef CPKL_CONFIG_RI
void cpkl_ri_seed(void);
/* this function reture a random num which in range [begin, begin + count - 1] */
u32 cpkl_ri_rand(u32 begin, u32 count);
/* random distribute generator */
u32 *cpkl_ri_rdgen(u32 *distri, u32 n_distri);

#ifdef CPKL_CONFIG_DEBUG
void cpkl_ri_test(void);
#else
static inline void cpkl_ri_test(void){}
#endif
#else
static inline void cpkl_ri_seed(void){}
static inline u32 cpkl_ri_rand(u32 begin, u32 count){return 0;}
static inline u32 *cpkl_ri_rdgen(u32 *distri, u32 n_distri){return NULL;}
static inline void cpkl_ri_test(void){}
#endif

CODE_SECTION("====================")
CODE_SECTION("Custom Signal")
CODE_SECTION("====================")

typedef struct _cpkl_custsig {
	u32			maxsig;
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
	HANDLE		sig;
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	sem_t		u_sem;
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
	struct semaphore k_sem;
#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif

#ifdef CPKL_CONFIG_DEBUG
	u64 tmsum;
	u64 times;
#endif

} cpkl_custsig_t;

int _cpkl_sigcreate(cpkl_custsig_t *sig, u32 initsig, u32 maxsig, char *filename, const char *funcname, u32 line);
#define cpkl_sigcreate(sig, initsig, maxsig)	_cpkl_sigcreate(sig, initsig, maxsig, __FILE__, __FUNCTION__, __LINE__)

void _cpkl_sigdsty(cpkl_custsig_t *sig, char *filename, const char *funcname, u32 line);
#define cpkl_sigdsty(sig)	_cpkl_sigdsty(sig, __FILE__, __FUNCTION__, __LINE__)

void _cpkl_sigsend(cpkl_custsig_t *sig, char *filename, const char *funcname, u32 line);
#define cpkl_sigsend(sig)	_cpkl_sigsend(sig, __FILE__, __FUNCTION__, __LINE__)

/* wait for signal */
int _cpkl_sigwait(cpkl_custsig_t *sig, const char *filename, const char *funcname, u32 line);
#define cpkl_sigwait(sig)	_cpkl_sigwait(sig, __FILE__, __FUNCTION__, __LINE__)

CODE_SECTION("====================")
CODE_SECTION("Custom Mutex")
CODE_SECTION("====================")

typedef struct _cpkl_custmtx {
#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS
#ifdef CPKL_MUTEX_CS
	CRITICAL_SECTION	cs;
#else
	HANDLE			mtx;
#endif
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	pthread_mutex_t	mtx;
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
#else
	#error "Platform not support, check the MACRO 'CPKL_CONFIG_PLATFORM' definition."
#endif

#ifdef CPKL_CONFIG_DEBUG
	u64 tmsum;
	u64 times;
#endif

} cpkl_custmtx_t;

int _cpkl_mtxcreate(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line);
#define cpkl_mtxcreate(mtx)		_cpkl_mtxcreate(mtx, __FILE__, __FUNCTION__, __LINE__)

void _cpkl_mtxdsty(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line);
#define cpkl_mtxdsty(mtx)		_cpkl_mtxdsty(mtx, __FILE__, __FUNCTION__, __LINE__)

int _cpkl_mtxlock(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line);
#define cpkl_mtxlock(mtx)		_cpkl_mtxlock(mtx, __FILE__, __FUNCTION__, __LINE__)

int _cpkl_mtxtrylock(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line);
#define cpkl_mtxtrylock(mtx)	_cpkl_mtxtrylock(mtx, __FILE__, __FUNCTION__, __LINE__)

void _cpkl_mtxunlock(cpkl_custmtx_t *mtx, char *filename, const char *funcname, u32 line);
#define cpkl_mtxunlock(mtx)		_cpkl_mtxunlock(mtx, __FILE__, __FUNCTION__, __LINE__)

CODE_SECTION("====================")
CODE_SECTION("error log system")
CODE_SECTION("====================")

typedef struct _cpkl_errlog {
	cpkl_listhead_t	ln;
	time_t			stamp;
	u8				buf[1];
} cpkl_errlog_t;

struct _cpkl_els;
typedef void (*cpkl_el_fmt)(struct _cpkl_els *els, cpkl_errlog_t *el);

/* error log system */
typedef struct _cpkl_els {
	cpkl_listhead_t	head;
	cpkl_custmtx_t	lock;
	u32				n_el;
} cpkl_els_t;

typedef struct _cpkl_elsfcp {
} cpkl_elsfcp_t;

cpkl_errlog_t* cpkl_elnew(cpkl_els_t *els, void *buf, u32 len);
cpkl_errlog_t* cpkl_eldel(cpkl_els_t *els);

CPKL_FCTNEW_DEFINE(cpkl_els_t);
CPKL_FCTDEL_DEFINE(cpkl_els_t);

CODE_SECTION("====================")
CODE_SECTION("Binary Search Tree")
CODE_SECTION("====================")

/*
 * Binary Search Tree interface
 * We incapsulate different type of BST implimentation (like AVL, RBTree etc.)
 * into unify interface(controled by CONFIG_ macro), easy to use
 */
#define	CPKL_CONFIG_BSTTYPE_AVL					1
#define	CPKL_CONFIG_BSTTYPE_RBTREE				2

enum {
	CPKL_BSTCMP_1LT2 = 1,	/* v1 < v2 */
	CPKL_BSTCMP_1EQ2,		/* v1 = v2 */
	CPKL_BSTCMP_1BT2,		/* v1 > v2 */
	CPKL_BSTCMP_1IN2,		/* v1 ( v2 */
	CPKL_BSTCMP_2IN1,		/* v2 ( v1 */
	CPKL_BSTCMP_OVLP,		/* overlap */
};

/*
 * some walkthrough type: L(left) M(middle) R(right)
 */
enum {
	CPKL_BSTWALKTYPE_LMR = 0,
	CPKL_BSTWALKTYPE_LRM,
	CPKL_BSTWALKTYPE_MLR,
	CPKL_BSTWALKTYPE_MRL,
	CPKL_BSTWALKTYPE_RLM,
	CPKL_BSTWALKTYPE_RML,
};

#if CPKL_CONFIG_BSTTYPE == CPKL_CONFIG_BSTTYPE_AVL

/* AVL tree node */
typedef struct _cpkl_avln {
	struct _cpkl_avln *f;				/* father node */
	struct _cpkl_avln *lc, *rc;			/* left and right child node */
	u32			subth;					/* subtree high, >= 1 */
} cpkl_avln_t;

/*
 * This is the AVL operator struct, please embedded this struct
 * into the owner struct which wan't to use the BST alg.
 */
typedef cpkl_avln_t			cpkl_bstn_t;

#elif CPKL_CONFIG_BSTTYPE == CPKL_CONFIG_BSTTYPE_RBTREE
#error "Now we don't support the RBTree as the BST, try to use the AVL"
#else
#error "Binary Search Tree algtype not support, check the MARCO 'CPKL_CONFIG_BSTTYPE' definition."
#endif

/*
 * Binary Search Tree node compire function
 * return: CPKL_BSTCMP_1LT2
 *         CPKL_BSTCMP_1EQ2
 *         CPKL_BSTCMP_1BT2
 *         CPKL_BSTCMP_1IN2
 *         CPKL_BSTCMP_2IN1
 *         CPKL_BSTCMP_OVLP
 */
typedef int (*cpkl_bstncmp)(cpkl_bstn_t *n1, cpkl_bstn_t *n2);

/*
 * return:  0 ---> walk success
 * return: -1 ---> terminate with some reason
 */
typedef int (*cpkl_bstwkop)(cpkl_bstn_t *n1, void *param);

/*
 * BST interface definition
 */
int cpkl_bst_insert(cpkl_bstn_t **root, cpkl_bstn_t *newnode, cpkl_bstncmp cmpf);
void cpkl_bst_remove(cpkl_bstn_t **root, cpkl_bstn_t *rmnode);
cpkl_bstn_t* cpkl_bst_lkup(cpkl_bstn_t *root, cpkl_bstn_t* dest, cpkl_bstncmp cmpf);
int cpkl_bst_walk(cpkl_bstn_t *root, u32 walktype, cpkl_bstwkop op, void *param);
void cpkl_bst_ndmv(cpkl_bstn_t **root, cpkl_bstn_t *from, cpkl_bstn_t *to);
#ifdef CPKL_CONFIG_DEBUG
int cpkl_avlvldck(cpkl_bstn_t *root, cpkl_bstncmp cmpf);
void cpkl_bsttest(void);
#else
static inline int cpkl_avlvldck(cpkl_bstn_t *root, cpkl_bstncmp cmpf){return 0;}
static inline void cpkl_bsttest(void){}
#endif


CODE_SECTION("====================")
CODE_SECTION("Memory monitor")
CODE_SECTION("====================")

#ifdef CPKL_CONFIG_MEMMONITOR

typedef struct _cpkl_mmstat {
	u64		real, occupy;
	u64		real_max, occupy_max;
	const char	*max_filename, *max_funcname;
	u32		max_line;
} cpkl_mmstat_t;

void *_cpkl_malloc(u32 size, const char *filename, const char *funcname, u32 line);
#define cpkl_malloc(size)		_cpkl_malloc((size), __FILE__, __FUNCTION__, __LINE__)

void _cpkl_free(void *p, const char *filename, const char *funcname, u32 line);
#define cpkl_free(p)		_cpkl_free((p), __FILE__, __FUNCTION__, __LINE__)

/* memory monitor init */
void cpkl_mmcheck(void);
void cpkl_mmgetstat(cpkl_mmstat_t *stat);

#else
#define cpkl_malloc				cpkl_pdf_malloc
#define cpkl_free				cpkl_pdf_free

static inline void cpkl_mmcheck(void){}
static inline void cpkl_mmgetstat(cpkl_mmstat_t *stat){}
#endif

CODE_SECTION("====================")
CODE_SECTION("Slab Heap")
CODE_SECTION("====================")

/*
 * This is the slabheap infrastruct definision
 * We manage all the halffree and nofree slabs
 */

#define CPKL_SLABHEAP_NOMORE				NULL

/* next free block */
typedef struct _cpkl_nfblock {
	struct _cpkl_nfblock	*next;
} cpkl_nfblock_t;

/* slab heap's slab */
typedef struct _cpkl_shs {
	cpkl_listhead_t		curslb;					/*  */
	cpkl_bstn_t			spbst;					/* this is the space range BST node */
	void				*rgl, *rgr;				/* the slab memspace range, [rgl, rgr) */
	cpkl_nfblock_t		*freepos;				/* next free block possition in this slab */
	u32					n_fblk;					/* number of current free block in this slab */
	u32					shs_idx;				/*  */
	/* the block space */
} cpkl_shs_t;

/* slabheap */
typedef struct _cpkl_sh {
	/*
	 * af: all free
	 * hf: half free
	 * nf: no free
	 */
	cpkl_listhead_t	afh, hfh, nfh;				/* 3 kinds of slab listhead */
	/*
	 * 'no free' and 'half free' slabs are organized by BST
	 * when free blocks, we find the corresponding slab struct by this BST
	 */
	cpkl_bstn_t			*slbtroot;

	u32					s_slb;					/* size of slab, include the mngt's size */
	u32					s_blk;					/* size of block */
	u32					bps;					/* number of blocks per slab */
	cpkl_custmtx_t		mtx;					/* when used in multi thread env, we need it */
	u32					needlock;

	/* number of 'all free' 'half free' and 'no free' slabs */
	u32					n_afs, n_hfs, n_nfs;
	u32					n_cb;					/* number of current blocks */

#ifdef CPKL_CONFIG_DEBUG
	u32					n_ea;					/* times of error alloc */
	u32					n_ef;					/* times of error free */
	u32					n_sd;					/* times of success drain */
#endif
} cpkl_sh_t;

typedef struct _cpkl_shfcp {
	u32 s_blk, s_slb;
	u32 needlock;
} cpkl_shfcp_t;

CPKL_FCTNEW_DEFINE(cpkl_sh_t);
CPKL_FCTDEL_DEFINE(cpkl_sh_t);

void *cpkl_shalloc(cpkl_sh_t *sh);
void cpkl_shfree(cpkl_sh_t *sh, void *blk);
void cpkl_shreset(cpkl_sh_t *sh);
/* this is used to drain such all free slab's */
void cpkl_shdrainslb(cpkl_sh_t *sh);
u32 cpkl_shgetblkidx(cpkl_sh_t *sh, void *blk);
void *cpkl_shgetblkbyidx(cpkl_sh_t *sh, u32 idx);
u32 cpkl_shgetnumidx(cpkl_sh_t *sh);
#ifdef CPKL_CONFIG_DEBUG
void cpkl_shtest(void);
#else
static inline void cpkl_shtest(void){}
#endif

CODE_SECTION("====================")
CODE_SECTION("Slab Stack")
CODE_SECTION("====================")

/*
 * This is the 'slabstack' infrastruct definision
 * we manage the memory by list of slabs, each slab consists by blocks
 * The whole blocks in the slablist is opertated like stack
 * The blocks alloced last, they will be free first, that's why it called as 'slabstack'.
 */

/* slab stack's slab */
typedef struct _cpkl_sss {
	cpkl_listhead_t		curslb;
	void				*freepos;				/* next free block possition in this slab */
	u32					n_blk;					/* number of blocks in this slab */
	/* the block space */
} cpkl_sss_t;

/* slabstack */
typedef struct _cpkl_ss {
	cpkl_listhead_t		list;					/* link all the hsp_sss_t */
	cpkl_listhead_t		*freeslb;				/* point to the free hsp_sss_t */
	u32					n_slb;					/* total number of slabs, NOT the number of list's element */
	u32					n_blk;					/* total number of blocks */
	u32					s_slb;					/* size of slab, include the mngt's size */
	u32					s_blk;					/* size of block */
	u32					bps;					/* number of blocks per slab */
	cpkl_custsig_t		sig;					/* when used in multi thread env, we need it */
	u32					needsig;
#ifdef CPKL_CONFIG_DEBUG
	u32					maxslb;
	u32					maxblk;
#endif
} cpkl_ss_t;

typedef struct _cpkl_ssfcp {
	u32 s_blk, s_slb;
	u8 needsig;
} cpkl_ssfcp_t;

CPKL_FCTNEW_DEFINE(cpkl_ss_t);
CPKL_FCTDEL_DEFINE(cpkl_ss_t);

void *cpkl_ssalloc(cpkl_ss_t *ss);
void cpkl_ssfree(cpkl_ss_t *ss, u32 n_blk);
void cpkl_ssreset(cpkl_ss_t *ss);

CODE_SECTION("====================")
CODE_SECTION("Hash List")
CODE_SECTION("====================")

/* hashlist node */
typedef struct _cpkl_hlnd {
	cpkl_listhead_t	listnode;
	cpkl_listhead_t	gl;				/* hl global list */
	u8				keyrst[1];
} cpkl_hlnd_t;

typedef struct _cpkl_hlbkt {
	cpkl_listhead_t	hlhead;
	u32				n_entry;
} cpkl_hlbkt_t;

typedef int (*cpkl_hlwalk_func)(cpkl_hlnd_t *hlnd, void *param);

/* hashlist */
typedef struct _cpkl_hl {
	cpkl_listhead_t	glh;			/* hl global list head */
	u32				keylen, rstlen;
	u32				n_bkt;
	u32				n_total;
#ifdef CPKL_CONFIG_HL_USESH
	cpkl_sh_t		*hlndsh;
#endif
	cpkl_custmtx_t	lock;
	cpkl_hlbkt_t	bktlist[1];
} cpkl_hl_t;

typedef struct _cpkl_hlfcp {
	u32				keylen, rstlen;
	u32				n_bkt;
} cpkl_hlfcp_t;

CPKL_FCTNEW_DEFINE(cpkl_hl_t);
CPKL_FCTDEL_DEFINE(cpkl_hl_t);

/* hashlist reset */
void cpkl_hlreset(cpkl_hl_t *hl);
/* hashlist lookup */
cpkl_hlnd_t* cpkl_hllkup(cpkl_hl_t *hl, const void *key, u32 *bktidx);
/* hashlist walk */
void cpkl_hlwalk(cpkl_hl_t *hl, cpkl_hlwalk_func walk, void *param);
/* hashlist insert */
int cpkl_hlinsert(cpkl_hl_t *hl, const void *key, const void *rst);
/* hashlist remove */
void cpkl_hlremove(cpkl_hl_t *hl, const void *key);

#ifdef CPKL_CONFIG_DEBUG
void cpkl_hltest(void);
#else
static inline void cpkl_hltest(void){}
#endif

CODE_SECTION("====================")
CODE_SECTION("RangeResouce Mngr")
CODE_SECTION("====================")

typedef struct _cpkl_rrnd {
	cpkl_listhead_t	ln;
	cpkl_bstn_t		bstn;
	u64				begin, sz;
	u32				type;
} cpkl_rrnd_t;

typedef int (*cpkl_rrwalk_func)(cpkl_rrnd_t *rrnd, void *param);

typedef struct _cpkl_rrmngr {
	cpkl_listhead_t	lh;
	cpkl_bstn_t		*root;			/* bst root */
	cpkl_sh_t		*ndsh;			/* mngr all the cpkl_rrnd_t */
	u64				begin, total;
	u32				shnf;			/* ndsh need free */
} cpkl_rrmngr_t;

typedef struct _cpkl_rrmngrfcp {
	u64				begin, total;
	u32				inittype;
	cpkl_sh_t		*ndsh;
} cpkl_rrmngrfcp_t;

CPKL_FCTNEW_DEFINE(cpkl_rrmngr_t);
CPKL_FCTDEL_DEFINE(cpkl_rrmngr_t);

int cpkl_rrlookup(cpkl_rrmngr_t *rrmngr, u64 begin, u64 size, u32 *type);
int cpkl_rrset(cpkl_rrmngr_t *rrmngr, u64 begin, u64 size, u32 type);
int cpkl_rrwalk(cpkl_rrmngr_t *rrmngr, cpkl_rrwalk_func walk, void *param);

#ifdef CPKL_CONFIG_DEBUG
void cpkl_rrmngrtest(void);
#else
static inline void cpkl_rrmngrtest(void) {}
#endif

CODE_SECTION("====================")
CODE_SECTION("Custom File Opeartion")
CODE_SECTION("====================")

#ifdef CPKL_CONFIG_CUSTOM_FILEOP
typedef struct _cpkl_file {
	u8				*databuff;
	u32				curpos, totallen;
} CPKL_FILE;

#define CPKL_SEEK_CUR    		(1)
#define CPKL_SEEK_END    		(2)
#define CPKL_SEEK_SET    		(0)

CPKL_FILE *cpkl_fopen(const char *name, const char *mode);
void cpkl_fclose(CPKL_FILE *fdesc);
int cpkl_feof(CPKL_FILE *fdesc);
char *cpkl_fgets(char *s, int size, CPKL_FILE *stream);

/* todo: fseek ftell */

#else
#define	CPKL_FILE		FILE

#define CPKL_SEEK_CUR	(SEEK_CUR)
#define CPKL_SEEK_END	(SEEK_END)
#define CPKL_SEEK_SET	(SEEK_SET)

#define cpkl_fopen		cpkl_pdf_fopen
#define cpkl_fclose		cpkl_pdf_fclose
#define cpkl_feof		cpkl_pdf_feof
#define cpkl_fseek		cpkl_pdf_fseek
#define cpkl_ftell		cpkl_pdf_ftell
#define cpkl_fgets		cpkl_pdf_fgets
#define cpkl_fread		cpkl_pdf_fread
#define cpkl_fwrite		cpkl_pdf_fwrite

#endif

CODE_SECTION("====================")
CODE_SECTION("ConfigFile parser")
CODE_SECTION("====================")

#define CPKL_CP_TAGLEN_MAX				(32)

struct _cpkl_cp;
struct _cpkl_cpctx;

typedef enum _cpkl_cpstate {
	cpkl_cps_idle = 0,

	cpkl_cps_tag,				//
	cpkl_cps_body,

	cpkl_cps_pa,				/* prepare to annotate */
	cpkl_cps_la,				/* line annotation */
	cpkl_cps_ba,				/* block annotation */
	cpkl_cps_baq,				/* ba quit */
} cpkl_cpstate_e;

#define CPKP_CPCTXOP_CTXFREE		(0x1)
#define CPKP_CPCTXOP_CTXCAT			(0x2)
#define CPKP_CPCTXOP_SHIFTF			(0x4)			/* current context shift forward */
#define CPKP_CPCTXOP_SHIFTB			(0x8)			/* current context shift back */
#define CPKP_CPCTXOP_ERR			(0x80000000)


typedef struct _cpkl_cpctx* (*cpkl_cpstop)(struct _cpkl_cpctx *ctx, u8 c, u32 *op);

typedef int (*cpkl_cpstart)(void *up);
typedef int (*cpkl_cpparse)(u8 *src, u32 len, void *up);

/* configfile parser entry */
typedef struct _cpkl_cpent {
	u8					tag[CPKL_CP_TAGLEN_MAX];		/* charactor ' ' in '[]', just discard it */
	cpkl_cpstart 		start;							/* find this tag entry, when met the '{' */
	cpkl_cpparse 		parse;							/* default parse func, when met the '}' */
	struct _cpkl_cpent	*parent;
	u32					n_subent;
	cpkl_listhead_t		subent;							/* listhead */
	cpkl_listhead_t		listent;						/* listed anchol */
} cpkl_cpent_t;

typedef struct _cpkl_cpctx {
	struct _cpkl_cpctx	*parent;

	struct _cpkl_cp	*cp;
	void			*up;		/* this is the user parameter */

	cpkl_cpstate_e	state;
	cpkl_cpent_t	*curcpent;	/**/
	cpkl_cpent_t	*tagcpent;	/* 'cpent' appointed by last tag we have parsed */

	u32				bufsize, n_char;

	u8				dstbuf[1];	/* more than one byte */
} cpkl_cpctx_t;

typedef struct _cpkl_cpctxfcp {
	u32				bufsize;
	cpkl_cpctx_t	*parent;
	struct _cpkl_cp	*cp;
	void			*up;		/* this is the user parameter */
	cpkl_cpstate_e	state;
	cpkl_cpent_t	*curcpent;	/**/
} cpkl_cpctxfcp_t;

CPKL_FCTNEW_DEFINE(cpkl_cpctx_t);
CPKL_FCTDEL_DEFINE(cpkl_cpctx_t);

/* configfile parser object */
typedef struct _cpkl_cp {
	cpkl_cpent_t* (*addcpent)(struct _cpkl_cp *cp, cpkl_cpent_t *parent, u8 *tag, cpkl_cpstart start, cpkl_cpparse parse);

	int (*parse)(struct _cpkl_cp *cp, u8 *src, u32 len, void *p);

	/* copy the date in the 'ctxfrom' to the 'ctxto' */
	int (*bufcat)(struct _cpkl_cp *cp, struct _cpkl_cpctx *ctxfrom, struct _cpkl_cpctx *ctxto);

	cpkl_sh_t		*cpent_sh;
	cpkl_cpent_t	*root;		/* root of cpentry tree */
} cpkl_cp_t;

typedef struct _cpkl_cpfcp {
	cpkl_cpstart		g_start;
	cpkl_cpparse		g_parse;
} cpkl_cpfcp_t;

CPKL_FCTNEW_DEFINE(cpkl_cp_t);
CPKL_FCTDEL_DEFINE(cpkl_cp_t);

CODE_SECTION("====================")
CODE_SECTION("Thread Pool")
CODE_SECTION("====================")

typedef int (*cpkl_tpentry)(void *param);
#ifdef CPKL_CONFIG_THREADPOLL

#ifndef CPKL_CONFIG_TPMAXTHREAD
#define	CPKL_CONFIG_TPMAXTHREAD				(32)
#endif

enum {
	CPKL_TPSTATE_IDLE = 0,
	CPKL_TPSTATE_RUNNING,
};

typedef struct _cpkl_tpblktsk {
	cpkl_listhead_t node;
	cpkl_tpentry entry;			/* task function entry */
	void *param;				/* task function param */
	cpkl_custsig_t *ternsig;	/*  */
} cpkl_tpblktsk_t;

typedef struct _cpkl_tpslot {
	/* send sig to this slot when there are block tasks exist. */
	cpkl_custsig_t tskblksig;

	/*  */
	cpkl_custsig_t teminalsig;
	u32			tmflag;

	/* lock this slot during the task insert and remove operation */
	cpkl_custmtx_t listlock;
	cpkl_listhead_t blktsk;			/* hsp_tpblktask_t entry list */
	u32			n_blktsk;			/* now there is number of n_blktsk tasks blocked */
	u32			n_cum;				/* cumulate tasks in this work thread */
	u32			state;

#ifdef CPKL_CONFIG_DEBUG
	u32			slotidx;
#endif
} cpkl_tpslot_t;

typedef struct _cpkl_threadpool {
	/*  */
	cpkl_tpslot_t	tpslotlist[CPKL_CONFIG_TPMAXTHREAD];
	u32				n_tpslot;		/* this the number of thread in the pool */
	u32				init_flag;

#if CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_WINDOWS

#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_UMOD
	pthread_t thdesc[CPKL_CONFIG_TPMAXTHREAD];
#elif CPKL_CONFIG_PLATFORM == CPKL_CONFIG_PLATFORM_LINUX_KMOD
	struct task_struct *thdesc[CPKL_CONFIG_TPMAXTHREAD];
#endif
} cpkl_threadpool_t;

int cpkl_tpstart(u32 n_thread);
void cpkl_tpstop(void);
void cpkl_tpstat(void);
#else
static inline int cpkl_tpstart(u32 n_thread) {return 0;}
static inline void cpkl_tpstop(void) {}
static inline void cpkl_tpstat(void) {}
#endif
int cpkl_tpinsert(cpkl_tpentry entry, void *param, cpkl_custsig_t *tersig);



CODE_SECTION("====================")
CODE_SECTION("Custom qsort")
CODE_SECTION("====================")

#ifdef CPKL_CONFIG_CUSTOM_QSORT
void cpkl_qsort(void *base, u32 nmemb, u32 size, int (*compar)(const void *, const void *));
#else
#define cpkl_qsort		cpkl_pdf_qsort
#endif

CODE_SECTION("====================")
CODE_SECTION("Timer Linker")
CODE_SECTION("====================")

typedef enum {
	cpkl_tmestat_init = 0,
	cpkl_tmestat_normal,
	cpkl_tmestat_closing,
	cpkl_tmestat_count
} cpkl_tmestate_e;

typedef void (*cpkl_tmhandle)(void *param, cpkl_tmestate_e state);

typedef struct _cpkl_tmentry {
	cpkl_listhead_t		ln;
	cpkl_tmhandle	handle;
	void	*param;
	u32		n_tm, n_count;					/* number of public inteval */
	u32		id;
	cpkl_tmestate_e	state;
} cpkl_tmentry_t;

enum {
	CPKL_TMLKSTATE_UNINIT = 0,
	CPKL_TMLKSTATE_PUBTMSTOP,
	CPKL_TMLKSTATE_PUBTMSTART,
};

#ifdef CPKL_CONFIG_TIMERLINK

typedef struct _cpkl_tmlk {
	cpkl_listhead_t		tml;
	cpkl_custmtx_t		tml_lock;
	u32					tmst;
	u32					pubintv;			/* ms */
} cpkl_tmlk_t;

/* pubintv: ms */
int cpkl_tmlkinit(u32 pubintv);

/* register timer */
int cpkl_tmreg(u32 n_pubintv, cpkl_tmhandle handle, void *param);
/* unregister timer */
void cpkl_tmunreg(u32 id);
#else

/* pubintv: ms */
static inline int cpkl_tmlkinit(u32 pubintv) {return -1;}

static inline int cpkl_tmreg(u32 n_pubintv, cpkl_tmhandle handle, void *param) {return -1;}
static inline void cpkl_tmunreg(u32 id) {}

#endif

#endif

