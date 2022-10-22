//////////////////////////////////////////////////////////////////////////
// jsvar v.0.7.x

#ifndef __JSVAR__H
#define __JSVAR__H

// In this part you can tune macros to your installation

// Your memory allocator
//#define JSVAR_ALLOC(p,t)                  {(p) = (t*) malloc(sizeof(t)); if((p)==NULL) {printf("jsVar: Out of memory\n"); exit(1);}}
//#define JSVAR_REALLOC(p,t)                {(p) = (t*) realloc((p), sizeof(t)); if((p)==NULL && (n)!=0) {printf("jsVar: Out of memory\n"); exit(1);}}
//#define JSVAR_ALLOCC(p,n,t)               {(p) = (t*) malloc((n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("jsVar: Out of memory\n"); exit(1);}}
//#define JSVAR_REALLOCC(p,n,t)             {(p) = (t*) realloc((p), (n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("jsVar: Out of memory\n"); exit(1);}}
//#define JSVAR_ALLOC_SIZE(p,t,n)           {(p) = (t*) malloc(n); if((p)==NULL && (n)!=0) {printf("jsVar: Out of memory\n"); exit(1);}}
//#define JSVAR_FREE(p)                     {free(p); }


// Set to string which will prefix all jsvar printed messages (errors, warnings, debug)
//#define JSVAR_PRINT_PREFIX()              "JsVar"

// Uncomment to get access to functions from dynamic strings, baio and wsaio libraries.
//#define JSVAR_ALL_LIBRARIES               1

// Specify whether wchar_t functions are to be used
//#define JSVAR_USE_WCHAR_T             1

// Specify whether the library is linked with openSsl library
//#define JSVAR_USE_OPEN_SSL                1

// Maximum number of simultaneous connections under Windows (default is 64!)
//#define JSVAR_WIN_FD_SETSIZE              1024

// Initial size of strings allocated by jsvar
//#define JSVAR_INITIAL_DYNAMIC_STRING_SIZE (1<<16)

#define JSVAR_INDEX_FILE_NAME               "index.html"

#include <stdint.h>
#include <dirent.h>

#if JSVAR_USE_WCHAR_T
#include <wchar.h>
#endif
// for fd_set definition
#include <sys/types.h>

#if BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#if _WIN32
#if defined(JSVAR_WIN_FD_SETSIZE)
#define FD_SETSIZE                          JSVAR_WIN_FD_SETSIZE
#endif
#include <Winsock2.h>
#define JSVAR_ZERO_SIZED_ARRAY_SIZE         1
#define JSVAR_STR_ERRNO()                   (strerror(WSAGetLastError()))

#else

#define JSVAR_ZERO_SIZED_ARRAY_SIZE         0
#define JSVAR_STR_ERRNO()                   strerror(errno)

#endif


// simplify definition of large strings
#define JSVAR_STRINGIFY(...)                #__VA_ARGS__


enum jsVarDstrAllocationMethods {
    MSAM_NONE,
    MSAM_PURE_MALLOC,
    MSAM_DEFAULT,
    MSAM_MAX,
};

struct jsVarDstr {
    char *s;
    int size;
    int allocatedSize;
    int allocationMethod;
};

#if JSVAR_USE_WCHAR_T
struct jsVarWDstr {
    wchar_t *s;
    int size;
    int allocatedSize;
    int allocationMethod;
};
#endif

#define BAIO_MAX_CONNECTIONS                (1<<10)

enum baioDirection {
    BAIO_IO_DIRECTION_NONE,
    BAIO_IO_DIRECTION_READ,
    BAIO_IO_DIRECTION_WRITE,
    BAIO_IO_DIRECTION_RW,
    BAIO_IO_DIRECTION_MAX,
};

/////////////////////////////////////////////////////////////////////////////////////
// max user defined parameters inside assynchronous I/O structures
#define BAIO_MAX_USER_PARAMS                        4

// baio prefix stands for basic assynchronous input output

#define BAIO_WRITE_BUFFER_DO_FULL_SIZE_RING_CYCLES  0
#define BAIO_EACH_WRITTEN_MSG_HAS_RECORD            0

// Not all combinations of following masks are possible
#define BAIO_STATUS_ZOMBIE                          0x000000
#define BAIO_STATUS_ACTIVE                          0x000001
#define BAIO_STATUS_EOF_READ                        0x000002
#define BAIO_STATUS_PENDING_CLOSE                   0x000004

// event statuses, listen is not clearable status
#define BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN       0x000008
// clearable statuses
#define BAIO_BLOCKED_FOR_WRITE_IN_TCPIP_CONNECT     0x000010
#define BAIO_BLOCKED_FOR_READ_IN_SSL_CONNECT        0x000040
#define BAIO_BLOCKED_FOR_WRITE_IN_SSL_CONNECT       0x000080
#define BAIO_BLOCKED_FOR_READ_IN_SSL_ACCEPT         0x000100
#define BAIO_BLOCKED_FOR_WRITE_IN_SSL_ACCEPT        0x000200
#define BAIO_BLOCKED_FOR_READ_IN_READ               0x001000
#define BAIO_BLOCKED_FOR_WRITE_IN_WRITE             0x002000
#define BAIO_BLOCKED_FOR_READ_IN_SSL_READ           0x004000
#define BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE         0x008000
#define BAIO_BLOCKED_FOR_WRITE_IN_SSL_READ          0x010000
#define BAIO_BLOCKED_FOR_READ_IN_SSL_WRITE          0x020000
#define BAIO_BUFFERED_DATA_AVAILABLE_FOR_SSL_READ   0x040000

enum baioSslFlags {
    BAIO_SSL_NO,
    BAIO_SSL_YES,
};

// user's can have values stored in structure baio. Those params are stored in an array
// of the following UNION type
union baioUserParam {
    void                *p;         // a pointer parameter
    int                 i;          // an integer parameter
    double              d;          // a double parameter
};

// write buffer is composed of "messages". I.e. continuous chunks of bytes
// with maybe non-empty spaces between them. Those spaces are not sent to socket
struct baioMsg {
    int startIndex;
    int endIndex;
};

struct baioMsgBuffer {
    struct baioMsg  *m;
    int             mi,mj;
    int             msize;
};

struct baioReadBuffer {
    // buffered characters
    char    *b;
    // read buffer is simply a linear array. Values between b[i] and b[j]
    // contains unread chars newly readed chars are appended after b[j].
    // Baio can move whole buffer (i.e. move b[i .. j] -> b[0 .. j-i) at any time.
    int     i,j;
    int     size;
};

struct baioWriteBuffer {
    // baio write buffer consists of two ring buffers.
    // The first one is storing bodies of messages (chars to be sent to 
    // socket) and can contain gaps.
    // The second one is storing message boundaries and hence determining 
    // where are the gaps in the first buffer.

    // The first ring buffer: message bodies
    char            *b;
    // We use a ring buffer for write. b[i] is the first unsent character
    // b[j] is the first free place where the next chracter can be stored.
    // b[i] - b[ij] are bytes ready for immediate send out (write). This is basically
    // the part of the first message which was not yet been written. I.e. b[ij] is 
    // the end of the message which is currently being written.
    // If the buffer was wrapped over the end of buffer (I.e. if j < i) 
    // and the currently written message is at the end of the buffer wrap, then b[jj]
    // is the first unused char at the end of the buffer (the b[jj-1] is the 
    // last used char of the buffer)
    // All indexes must be signed integers to avoid arithmetic problems
    int             i, j, ij, jj;
    int             size;

    // The second ring buffer: message boundaries
    struct baioMsg  *m;
    // This buffer is normally active only if
    // there are gaps in the first buffer or during the time when the first buffer wrapps
    // over the end and there are pending unsent chars at the end.
    // This is a ring buffer so valid messages intervals are stored between m[mi] 
    // and m[mj] (indexes modulo msize).
    int             mi,mj;
    int             msize;
};

typedef int (*jsVarCallBackHookFunType)(void *x, ...);

struct jsVarCallBackHook {
    jsVarCallBackHookFunType    *a;
    int                     	i, dim;
};

struct baio {
    int                     baioType;               // type of baio (file, tcpIpClient, tcpIpServer, ...)
    int                     ioDirections;
    int                     fd;
    unsigned                status;
    int                     index;
    int                     baioMagic;
    struct baioReadBuffer   readBuffer;
    struct baioWriteBuffer  writeBuffer;

    // for statistics only
    int                     totalBytesRead;
    int                     totalBytesWritten;

    // when this is client's baio, this keeps client's ip address
    long unsigned           remoteComputerIp;
    // when this is client's baio, this keeps identification of the listening socket (service)
    int                     parentBaioMagic;

    // ssl stuff
    uint8_t                 useSsl;
#if BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL
    SSL                     *sslHandle;
#else
    void                    *sslHandle;
#endif
    char                    *sslSniHostName;
    int                     sslPending;

#if 0
    // Here are specified the actual types of parameters for callbacks
    int                     (*callBackOnRead)(struct baio *, int fromj, int n);
    int                     (*callBackOnWrite)(struct baio *, int fromi, int n);
    int                     (*callBackOnError)(struct baio *);
    int                     (*callBackOnEof)(struct baio *);
    int                     (*callBackOnDelete)(struct baio *);
    int                     (*callBackOnReadBufferShift)(struct baio *, int delta);
    // network client callbacks
    int                     (*callBackOnTcpIpConnect)(struct baio *);
    int                     (*callBackOnConnect)(struct baio *);
    // network server callbacks
    // accept is called as definitive accept (both for ssl and tcpip)
    int                     (*callBackOnAccept)(struct baio *);
    int                     (*callBackAcceptFilter)(struct baio *, unsigned ip, unsigned port);
    int                     (*callBackOnTcpIpAccept)(struct baio *);
#endif

    // If you add a callback hook, do not forget to FREE it in baioFreeZombie and CLONE it in cloneCallBackHooks
    // callbackhooks
    struct jsVarCallBackHook        callBackOnRead;
    struct jsVarCallBackHook        callBackOnWrite;
    struct jsVarCallBackHook        callBackOnError;
    struct jsVarCallBackHook        callBackOnEof;
    struct jsVarCallBackHook        callBackOnDelete;
    struct jsVarCallBackHook        callBackOnReadBufferShift;
    // network client callbacks
    struct jsVarCallBackHook        callBackOnTcpIpConnect;
    struct jsVarCallBackHook        callBackOnConnect;
    // network server callbacks
    // accept is called as definitive accept (both for ssl and tcpip)
    struct jsVarCallBackHook        callBackOnAccept;
    struct jsVarCallBackHook        callBackAcceptFilter;
    struct jsVarCallBackHook        callBackOnTcpIpAccept;

    // config values
    int                     initialReadBufferSize;
    int                     readBufferRecommendedSize;
    int                     initialWriteBufferSize;
    int                     minFreeSizeBeforeRead;
    int                     minFreeSizeAtEndOfReadBuffer;   // if you want to put zero for example
    int                     maxReadBufferSize;
    int                     maxWriteBufferSize;
    uint8_t                 optimizeForSpeed;

    // when we allocate this structure we allocate some additional space
    // for other extensions and we keep the size of additional space
    int                     additionalSpaceAllocated;

    // this is a user defined value, specifying "run time" type of baio pointer
    // it is supposed that struct baio will be included into larger struct, to get
    // the type of that larger type, use that value
    int                     userRuntimeType;

    // user defined values, they must be at the end of the structure!
    union baioUserParam     u[BAIO_MAX_USER_PARAMS];

    // here is the end of the structure
    void                    *userData[JSVAR_ZERO_SIZED_ARRAY_SIZE];
};

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
// wsaio prefix stands for web server assynchronous input output

struct wsaioRequestData {
    char                    *uri;
    char                    *getQuery;
    char                    *postQuery;
};

struct wsaio {
    // underlying I/O, must be the first element of the structure!
    struct baio             b;      

    uint32_t                wsaioSecurityCheckCode;     // magic number 0xcacebaf
    int                     state;

#if 0
    // Callback parameter types
    int                     (*callBackAcceptFilter)(struct wsaio *ww, unsigned ip, unsigned port);
    int                     (*callBackOnAccept)(struct wsaio *ww);
    int                     (*callBackOnWwwGetRequest)(struct wsaio *ww, char *uri);
    int                     (*callBackOnWebsocketAccept)(struct wsaio *ww, char *uri);
    int                     (*callBackOnWebsocketGetMessage)(struct wsaio *ww, char *msg, int n);
    int                     (*callBackOnDelete)(struct wsaio *ww);
#endif

    struct jsVarCallBackHook    callBackAcceptFilter;
    struct jsVarCallBackHook    callBackOnTcpIpAccept;
    struct jsVarCallBackHook    callBackOnAccept;
    struct jsVarCallBackHook    callBackOnWwwGetRequest;
    struct jsVarCallBackHook    callBackOnWwwPostRequest;
    struct jsVarCallBackHook    callBackOnWebsocketAccept;
    struct jsVarCallBackHook    callBackOnWebsocketGetMessage;
    struct jsVarCallBackHook    callBackOnDelete;

    // parsed current request
    struct wsaioRequestData currentRequest;
    // temporary value used for composing http answer
    int                     fixedSizeAnswerHeaderLen;
    int                     requestSize;
    // temporary value for receiving fragmented websocket messages
    int                     previouslySeenWebsocketFragmentsOffset;

    // webserver configuration
    char                    *serverName;
    // there is a space reserved for additional headers added after the message is processed (like "Content-length:xxxx")
    // This is the size of this reserved space
    int                     answerHeaderSpaceReserve;
    // The web request is going to be processed only if there is some minimal space available for writting
    // answer headers, chunk header, etc. (this value shall be larger than answerHeaderSpaceReserve)
    int                     minSpaceInWriteBufferToProcessRequest;  

    // when we allocate this structure we allocate some additional space
    // for other extensions and we keep the size of additional space
    int                     additionalSpaceAllocated;

    // user defined values, they must be at the end of the structure!
    // Hmm. do we need user params in wsaio when we have them in baio?
    // union userParam      u[BAIO_MAX_USER_PARAMS];

    // here is the end of the structure
    void                    *userData[JSVAR_ZERO_SIZED_ARRAY_SIZE];
};

///////////////////////////////////////////////////////////////////////////////////////////
// jsvar server/client

struct jsVaraio {
    struct wsaio                    w;

    char                            *singlePageText;
    char                            *fileServerRootDir;
    uint8_t                         fileServerListDirectories;
    
    // here is the end of the structure
    void                            *userData[JSVAR_ZERO_SIZED_ARRAY_SIZE];
};

// types of possible baio struct records created by jsvar3
enum baioUserRuntimeType {
    JSVAR_CON_TYPE_NONE = 29875,        // a random value
    JSVAR_CON_FILE_WEBSERVER,
    JSVAR_CON_SINGLE_PAGE_WEBSERVER,
    JSVAR_CON_FILE_WEBSERVER_CLIENT,
    JSVAR_CON_SINGLE_PAGE_WEBSERVER_CLIENT,
    JSVAR_CON_WEBSOCKET_SERVER_CLIENT,
    JSVAR_CON_TYPEMAX,
};

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

#if JSVAR_ALL_LIBRARIES

#include <stdarg.h>
// common
void jsVarCallBackClearHook(struct jsVarCallBackHook *h) ;
void jsVarCallBackFreeHook(struct jsVarCallBackHook *h) ;
int jsVarCallBackAddToHook(struct jsVarCallBackHook *h, void *ptr) ;
void jsVarCallBackRemoveFromHook(struct jsVarCallBackHook *h, void *ptr) ;
void jsVarCallBackCloneHook(struct jsVarCallBackHook *dst, struct jsVarCallBackHook *src) ;

struct jsVarDstr *jsVarDstrCreate() ;
struct jsVarDstr *jsVarDstrCreatePureMalloc() ;
void jsVarDstrFree(struct jsVarDstr **p) ;
char *jsVarDstrGetStringAndReinit(struct jsVarDstr *s) ;
void jsVarDstrExpandToSize(struct jsVarDstr *s, int size) ;
struct jsVarDstr *jsVarDstrTruncateToSize(struct jsVarDstr *s, int size) ;
struct jsVarDstr *jsVarDstrTruncate(struct jsVarDstr *s) ;
int jsVarDstrAddCharacter(struct jsVarDstr *s, int c) ;
int jsVarDstrDeleteLastChar(struct jsVarDstr *s) ;
int jsVarDstrAppendData(struct jsVarDstr *s, char *appendix, int len) ;
int jsVarDstrAppend(struct jsVarDstr *s, char *appendix) ;
int jsVarDstrAppendVPrintf(struct jsVarDstr *s, char *fmt, va_list arg_ptr) ;
int jsVarDstrAppendPrintf(struct jsVarDstr *s, char *fmt, ...) ;
int jsVarDstrAppendEscapedStringUsableInJavascriptEval(struct jsVarDstr *ss, char *s, int slen) ;
int jsVarDstrAppendBase64EncodedData(struct jsVarDstr *ss, char *s, int slen) ;
int jsVarDstrAppendBase64DecodedData(struct jsVarDstr *ss, char *s, int slen) ;
void jsVarDstrClear(struct jsVarDstr *s) ;
int jsVarDstrCharCount(struct jsVarDstr *s, int c) ;
int jsVarDstrReplace(struct jsVarDstr *s, char *str, char *byStr, int allOccurencesFlag) ;
int jsVarDstrAppendFile(struct jsVarDstr *res, FILE *ff) ;
int jsVarDstrAppendUtf8EscapedToJsString(struct jsVarDstr *res, char *utf8str) ;
struct jsVarDstr *jsVarDstrCreateByVPrintf(char *fmt, va_list arg_ptr) ;
struct jsVarDstr *jsVarDstrCreateByPrintf(char *fmt, ...) ;
struct jsVarDstr *jsVarDstrCreateFromCharPtr(char *s, int slen) ;
struct jsVarDstr *jsVarDstrCreateCopy(struct jsVarDstr *ss) ;
struct jsVarDstr *jsVarDstrCreateByVFileLoad(int useCppFlag, char *fileNameFmt, va_list arg_ptr) ;
struct jsVarDstr *jsVarDstrCreateByFileLoad(int useCppFlag, char *fileNameFmt, ...) ;
struct jsVarDstr *jsVarDstrCreateByVPipeLoad(char *cmdFmt, va_list arg_ptr) ;
struct jsVarDstr *jsVarDstrCreateByPipeLoad(char *cmdFmt, ...) ;
struct jsVarDstr *jsVarDstrCreateByVPopen2Load(char *sstdin, int sstdinlen, char *cmdFmt, va_list arg_ptr) ;
struct jsVarDstr *jsVarDstrCreateByPopen2Load(char *sstdin, int sstdinlen, char *cmdFmt, ...) ;

int base64_encoded_len(int input_length) ;
int base64_decoded_len(char *d, int input_length) ;
int base64_encode(char *data, int input_length, char *encoded_data, int output_length) ;
int base64_decode(char *data, int input_length, char *decoded_data, int output_length) ;

// baio
extern struct baio *baioTab[BAIO_MAX_CONNECTIONS];
extern int      baioTabMax;
struct baio *baioFromMagic(int baioMagic) ;
struct baio *baioNewBasic(int baioType, int ioDirections, int spaceToAllocate) ;
struct baio *baioNewFile(char *path, int ioDirection, int spaceToAllocate) ;
struct baio *baioNewPseudoFile(char *string, int stringLength, int spaceToAllocate) ;
struct baio *baioNewPipedFile(char *path, int ioDirection, int spaceToAllocate) ;
struct baio *baioNewSocketFile(char *path, int ioDirection, int spaceToAllocate) ;
struct baio *baioNewPipedCommand(char *command, int ioDirection, int spaceToAllocate) ;
struct baio *baioNewTcpipClient(char *hostName, int port, enum baioSslFlags sslFlag, int spaceToAllocate) ;
struct baio *baioNewTcpipServer(int port, enum baioSslFlags sslFlag, int spaceToAllocate) ;
int baioLibraryInit(int deInitializationFlag) ;
int baioSslLibraryInit() ;
int baioReadBufferResize(struct baioReadBuffer *b, int minSize, int maxSize) ;
int baioClose(struct baio *bb) ;
int baioCloseMagic(int baioMagic) ;
int baioCloseMagicOnError(int baioMagic) ;
int baioAttachFd(struct baio *bb, int fd) ;
int baioDettachFd(struct baio *bb, int fd) ;
void baioPurgeInactiveConnections(int maxCheckedItems) ;
int baioPossibleSpaceForWrite(struct baio *b) ;
int baioWriteBufferUsedSpace(struct baio *b) ;
int baioWriteToBuffer(struct baio *bb, char *s, int len) ;
int baioMsgReserveSpace(struct baio *bb, int len) ;
int baioWriteMsg(struct baio *bb, char *s, int len) ;
int baioVprintfToBuffer(struct baio *bb, char *fmt, va_list arg_ptr) ;
int baioPrintfToBuffer(struct baio *bb, char *fmt, ...) ;
int baioVprintfMsg(struct baio *bb, char *fmt, va_list arg_ptr) ;
int baioPrintfMsg(struct baio *bb, char *fmt, ...) ;
void baioLastMsgDump(struct baio *bb) ;
char *baioStaticRingGetTemporaryStringPtr() ;
void baioWriteBufferDump(struct baioWriteBuffer *b) ;

void baioMsgsResizeToHoldSize(struct baio *bb, int newSize) ;
void baioMsgStartNewMessage(struct baio *bb);
struct baioMsg *baioMsgPut(struct baio *bb, int startIndex, int endIndex) ;
int baioMsgLastStartIndex(struct baio *bb) ;
int baioMsgInProgress(struct baio *bb) ;
void baioMsgRemoveLastMsg(struct baio *bb) ;
int baioMsgResetStartIndexForNewMsgSize(struct baio *bb, int newSize) ;
void baioMsgSend(struct baio *bb) ;
void baioMsgLastSetSize(struct baio *bb, int newSize) ;

int baioSetSelectParams(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *t) ;
int baioOnSelectEvent(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds) ;
int baioCloseOnError(struct baio *bb);
int baioSelect(int maxfd, fd_set *r, fd_set *w, fd_set *e, struct timeval *t) ;

// wsaio
struct wsaio *wsaioNewServer(int port, enum baioSslFlags sslFlag, int spaceToAllocate) ;
struct wsaio *wsaioNewWebsocketClient(char *hostName, int port, char *pathAndQuery, char *sheaders, enum baioSslFlags sslFlag, int spaceToAllocate) ;
char *wsaioGetFileMimeType(char *fname) ;
void wsaioHttpStartNewAnswer(struct wsaio *ww) ;
void wsaioHttpFinishAnswer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders) ;
void wsaioHttpFinishAnswerHeaderAndStartChunkedAnswer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders) ;
void wsaioHttpStartChunkInChunkedAnswer(struct wsaio *ww) ;
void wsaioHttpFinalizeAndSendCurrentChunk(struct wsaio *ww, int finalChunkFlag) ;
int wsaioHttpSendFile(struct wsaio *ww, char *fname) ;
int wsaioHttpSendFileAsync(struct wsaio *ww, char *fname, char *additionalHeaders) ;
int wsaioHttpForwardFd(struct wsaio *ww, int fd, char *mimeType, char *additionalHeaders) ;
int wsaioHttpForwardString(struct wsaio *ww, char *str, int strlen, char *mimeType) ;
void wsaioWebsocketStartNewMessage(struct wsaio *ww) ;
void wsaioWebsocketCompleteMessage(struct wsaio *ww) ;
int wsaioWebsocketPrintf(struct wsaio *ww, char *fmt, ...) ;

#endif



extern int jsVarDebugLevel;

#if BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL
extern char     *baioSslServerKeyFile;
extern char     *baioSslServerCertFile;
extern char     *baioSslServerCertPem;
extern char     *baioSslServerKeyPem;
#endif

int baioPoll2(int timeOutUsec, int (*addUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e), void (*processUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e)) ;
int baioPoll(int timeOutUsec) ;

char *jsVarGetEnvPtr(char *env, char *key) ;
char *jsVarGetEnv(char *env, char *key, char *dst, int dstlen) ;
struct jsVarDstr *jsVarGetEnvDstr(char *env, char *key) ;
char *jsVarGetEnv_st(char *env, char *key) ;
int jsVarGetEnvInt(char *env, char *key, int defaultValue) ;
double jsVarGetEnvDouble(char *env, char *key, double defaultValue) ;

void jsVarCallBackClearHook(struct jsVarCallBackHook *h) ;
int jsVarCallBackAddToHook(struct jsVarCallBackHook *h, void *ptr) ;
void jsVarCallBackRemoveFromHook(struct jsVarCallBackHook *h, void *ptr) ;


struct jsVaraio *jsVarNewSinglePageServer(int port, enum baioSslFlags sslFlag, int spaceToAllocate, char *body) ;
struct jsVaraio *jsVarNewFileServer(int port, enum baioSslFlags sslFlag, int spaceToAllocate, char *rootDirectory) ;
int jsVarIsActiveConnection(struct baio *bb, int userRuntimeType);
int jsVarSendData(struct jsVaraio *jj, char *data, int len) ;
int jsVarSendDataAll(char *data, int len) ;
int jsVarEval(struct jsVaraio *jj, char *fmt, ...) ;
void jsVarEvalAll(char *fmt, ...) ;
void jsVarPoll() ;

#if JSVAR_USE_WCHAR_T
int jsVarWEval(struct jsVaraio *jj, wchar_t *fmt, ...) ;
void jsVarWEvalAll(wchar_t *fmt, ...) ;
#endif


#endif
