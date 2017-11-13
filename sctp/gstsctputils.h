#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SCTP_LTRACE_CHUNK_ENABLE			0x00400000
#define SCTP_LTRACE_ERROR_ENABLE			0x00800000

GST_EXPORT void hexDump (char *desc, void *addr, int len);
GST_EXPORT void gst_usrsctp_debug_init(void);
void print_rtp_header (GstElement *obj, unsigned char *buffer);
int usrsctp_addrs_to_string(struct sockaddr *addrs, int n, GString *str);
void usrsctp_debug_printf_sender(const char *format, ...);
void usrsctp_debug_printf_receiver(const char *format, ...);

#ifdef __WIN32__
#define PACKED
#pragma pack(push,1)
#else
#define PACKED __attribute__ ((__packed__))
#endif

//---------------------- STATIC ASSERT ----------------------------------
//Source: http://www.pixelbeat.org/programming/gcc/static_assert.html
#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
/* These can't be used after statements in c89. */
#ifdef __COUNTER__
#define STATIC_ASSERT(e,m) \
    ;enum { ASSERT_CONCAT(static_assert_, __COUNTER__) = 1/(!!(e)) }
#else
  /* This can't be used twice on the same line so ensure if using in headers
   * that the headers are not included twice (by wrapping in #ifndef...#endif)
   * Note it doesn't cause an issue when used on same line of separate modules
   * compiled with gcc -combine -fwhole-program.  */
#define STATIC_ASSERT(e,m) \
    ;enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(!!(e)) }
#endif

typedef struct _RTPHeader
    {
      //first byte
    #if G_BYTE_ORDER == G_LITTLE_ENDIAN
      unsigned int         CC:4;        /* CC field */
      unsigned int         X:1;         /* X field */
      unsigned int         P:1;         /* padding flag */
      unsigned int         version:2;
    #elif G_BYTE_ORDER == G_BIG_ENDIAN
      unsigned int         version:2;
      unsigned int         P:1;         /* padding flag */
      unsigned int         X:1;         /* X field */
      unsigned int         CC:4;        /* CC field*/
    #else
    #error "G_BYTE_ORDER should be big or little endian."
    #endif
      //second byte
    #if G_BYTE_ORDER == G_LITTLE_ENDIAN
      unsigned int         PT:7;     /* PT field */
      unsigned int         M:1;       /* M field */
    #elif G_BYTE_ORDER == G_BIG_ENDIAN
      unsigned int         M:1;         /* M field */
      unsigned int         PT:7;       /* PT field */
    #else
    #error "G_BYTE_ORDER should be big or little endian."
    #endif
      guint16              seq_num;      /* length of the recovery */
      guint32              TS;                   /* Timestamp */
      guint32              ssrc;
    } RTPHeader; //12 bytes

STATIC_ASSERT (sizeof (RTPHeader) == 12, "RTPHeader size doesn't seem to be cool.");

#ifdef __WIN32__
#pragma pack(pop)
#undef PACKED

#else
#undef PACKED
#endif


/* Types of logging/KTR tracing  that can be enabled via the
 * sysctl net.inet.sctp.sctp_logging. You must also enable
 * SUBSYS tracing.
 * Note that you must have the SCTP option in the kernel
 * to enable these as well.
 * from usrsctplib/netinet/sctp.h
 */
#define SCTP_BLK_LOGGING_ENABLE				0x00000001
#define SCTP_CWND_MONITOR_ENABLE			0x00000002
#define SCTP_CWND_LOGGING_ENABLE			0x00000004
#define SCTP_FLIGHT_LOGGING_ENABLE			0x00000020
#define SCTP_FR_LOGGING_ENABLE				0x00000040
#define SCTP_LOCK_LOGGING_ENABLE			0x00000080
#define SCTP_MAP_LOGGING_ENABLE				0x00000100
#define SCTP_MBCNT_LOGGING_ENABLE			0x00000200
#define SCTP_MBUF_LOGGING_ENABLE			0x00000400
#define SCTP_NAGLE_LOGGING_ENABLE			0x00000800
#define SCTP_RECV_RWND_LOGGING_ENABLE			0x00001000
#define SCTP_RTTVAR_LOGGING_ENABLE			0x00002000
#define SCTP_SACK_LOGGING_ENABLE			0x00004000
#define SCTP_SACK_RWND_LOGGING_ENABLE			0x00008000
#define SCTP_SB_LOGGING_ENABLE				0x00010000
#define SCTP_STR_LOGGING_ENABLE				0x00020000
#define SCTP_WAKE_LOGGING_ENABLE			0x00040000
#define SCTP_LOG_MAXBURST_ENABLE			0x00080000
#define SCTP_LOG_RWND_ENABLE    			0x00100000
#define SCTP_LOG_SACK_ARRIVALS_ENABLE			0x00200000
#define SCTP_LTRACE_CHUNK_ENABLE			0x00400000
#define SCTP_LTRACE_ERROR_ENABLE			0x00800000
#define SCTP_LAST_PACKET_TRACING			0x01000000
#define SCTP_THRESHOLD_LOGGING				0x02000000
#define SCTP_LOG_AT_SEND_2_SCTP				0x04000000
#define SCTP_LOG_AT_SEND_2_OUTQ				0x08000000
#define SCTP_LOG_TRY_ADVANCE				0x10000000

