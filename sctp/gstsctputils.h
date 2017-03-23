#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void usrsctp_debug_printf(const char *format, ...);
void hexDump (char *desc, void *addr, int len);
/* void print_rtp_header (GstSctpSrc *obj, unsigned char *buffer); */


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

