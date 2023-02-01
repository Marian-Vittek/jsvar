//////////////////////////////////////////////////////////////////////
//
// This program demonstrates howto  install your SSL certificates. The
// program itself is the initial  example showing server time.  An SSL
// server requires two  things: a private key and  a certificate.  You
// can either specify them directly  (as string) or specify files from
// where they  will be read.   This program demonstrates both  way, if
// the  macro  READ_CERTIFICATE_FROM_FILE  is  defined,  this  program
// specifies files to  be read, if not, it  uses certificates assigned
// to string variables.

//
// URL: https://localhost:4321
//

#include <time.h>

#include "jsvar.h"



int main() {
    struct jsVaraio *js;
    time_t          lastSentTime;

    // Custom  SSL private  key and  certificate has  to be  specified
    // BEFORE the first invocation of jsvar!
#if READ_CERTIFICATE_FROM_FILE

    // If   you   set    up   variables   "baioSslServerKeyFile"   and
    // "baioSslServerCertFile"  to  file   names  containing  key  and
    // certificate,  then they  will  be read  from  those files.  For
    // example:
    //
    baioSslServerKeyFile = "server.key";
    baioSslServerCertFile = "server.crt";
    
    // Note that  self signed version  of such files can  be generated
    // by the command:  
    // openssl req -x509 -nodes -days 99 -newkey rsa:2048 -keyout server.key -out server.crt
    
#else

    // Alternativaly, you can include  private key and the certificate
    // directly into source code by assigning to "baioSslServerKeyPem"
    // and "baioSslServerCertPem".
    baioSslServerCertPem = 
	"-----BEGIN CERTIFICATE-----\n"
	"MIIDazCCAlOgAwIBAgIUIdM7LCWeVWHzn4U/00LJihLpEI0wDQYJKoZIhvcNAQEL\n"
	"BQAwRTELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM\n"
	"GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0yMzAyMDExMTQzNTZaFw0yMzA1\n"
	"MTExMTQzNTZaMEUxCzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEw\n"
	"HwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQwggEiMA0GCSqGSIb3DQEB\n"
	"AQUAA4IBDwAwggEKAoIBAQCuQpVHQ47cPH53+eIM17XYuYpx9uj5kzsLCiEaGFfy\n"
	"TA6G3NzEPtkAahP7IDNa+YCkQmccHbGU9L6RSBU9LSSzAA7JTedYKcV/K5jQooEU\n"
	"azjQvPS9Tu9/9o5IFspVnIKlzn2I/ExihrNClGjD2i8rDeZxcI/xm2dGJUngm+rV\n"
	"t1HHbAm+BAInTpez5mRCIWtLLV0HAgOxoU+POECk2CospXLt58jC+Ld0y3iDcLuR\n"
	"E0VHv3Nbh7C8CtzYFdgP3/pWOGL32MijlvYxhTq31ByZNTG5DdD60ixYvU8z/nrU\n"
	"L/fVFgcT1ju1MIZEH1Vnp+363C4Pw2LcW1HzIKwhUV9VAgMBAAGjUzBRMB0GA1Ud\n"
	"DgQWBBS0BCAD800GdENGmJprTpp+tHpZHDAfBgNVHSMEGDAWgBS0BCAD800GdENG\n"
	"mJprTpp+tHpZHDAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQAY\n"
	"XnUu6ZSU++Y7HPZ5AU9KiUKCQwFNIgXORZFTLcr3mnjtBwYVL8r0SlNT8LDdD6Ys\n"
	"qO1obgzv+XHuWg5Sr7+GTOY4J8uAP7UsoM0i3uVv3FtvRz3HT/w90GLnU+F7OBfa\n"
	"qFnpYLhAPk+I3iuUQAI1nRvnWKSVys1o25a93avElq5DR07misZoVAam/2U+zGKO\n"
	"5UNE6ohRy6+6ohCniqGE4am6EPLbHPTkGzozb3QNlDDGJOmfLRZldvIIKEGSZR1r\n"
	"3Mi5OrFY4Ey2VH40R10bi5QMR6H8PeprvEjtaSfyHkTSjQRT+hE6edW0tu5i6r6Q\n"
	"G0s4QLNwajxqecWQ1hij\n"
	"-----END CERTIFICATE-----\n"
        ;
    baioSslServerKeyPem = 
	"-----BEGIN PRIVATE KEY-----\n"
	"MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCuQpVHQ47cPH53\n"
	"+eIM17XYuYpx9uj5kzsLCiEaGFfyTA6G3NzEPtkAahP7IDNa+YCkQmccHbGU9L6R\n"
	"SBU9LSSzAA7JTedYKcV/K5jQooEUazjQvPS9Tu9/9o5IFspVnIKlzn2I/ExihrNC\n"
	"lGjD2i8rDeZxcI/xm2dGJUngm+rVt1HHbAm+BAInTpez5mRCIWtLLV0HAgOxoU+P\n"
	"OECk2CospXLt58jC+Ld0y3iDcLuRE0VHv3Nbh7C8CtzYFdgP3/pWOGL32MijlvYx\n"
	"hTq31ByZNTG5DdD60ixYvU8z/nrUL/fVFgcT1ju1MIZEH1Vnp+363C4Pw2LcW1Hz\n"
	"IKwhUV9VAgMBAAECggEADMW1kk0Z1fubG2kVz/IpknJ6+sJprg1ECRmbdoGtRIWd\n"
	"lPXwOnQOzLB5uXDRQsxByQhS7WUhxHSx2Q0q/VpnM7V/3/JVUAlzv8/uufAHPPtk\n"
	"5SvMVmnINR1ZrV/6QY8gpk19twIjCR+tWOZuzVgbF/FDYDIrYr6Msb/+67cR3csc\n"
	"Uu63P8Fc0tshs+TpLMa89hFzZ+JXzGjkHmpl16F5NglnJ4IYXwoKJ5fx7f6O/Cvh\n"
	"QfsR4imRLHvvyE8dxAReMHSBqZOC+lTKC2Jnyupjc/z1jbNCX1qs1xCmRhWOWM8Z\n"
	"jTdFGZlBC4T65JFgyhKJg/gu9Ttvr+9sbYGgSBDJsQKBgQDqTZHSZhAYI3zle5Ir\n"
	"sui8bn8X3naCjGfpxDP6UCf2Dgmx6/D12PUYO9z7Sl34U4U5ma9HmPwJw6Uy2I/J\n"
	"FQEn0n1bv1qmEMAJvb1u1E8obv8rlEOuf0r+Ojpbe/3P1sEwHe2T7dmgAHnblvmK\n"
	"LfjHwFT3PQgRWApFI0pOERCE2QKBgQC+ZaACSltT7RCUnRN9ucgcys4db6c1NsZO\n"
	"g36gBh2Lroz7Lu5vudc3ozIJ5Z1h9z1xBtxnEHb0wDgHn5lY3ZrH6xyGMLql5kjS\n"
	"D++ZFP5dloKUTFgUpYWEhfu51BkNx0nT7gHtfiuUY9Z8+Up+pNC4XJlx2e6eX9Sk\n"
	"MtQ4ZpYw3QKBgQCc+gNstzypL32KMlQoOuF6/Xzg/QbhSDiGghFg7zsWuyj7r1vt\n"
	"GsJ6zgCry5NRwINNqA2rJnAWCqIvgrAyxIPVrkyWn4mYITjITfsQueWe7V7AT8FY\n"
	"s6gG0/QtPtE54mPkXOjDZ2OaszbxTCE71rkK+2zxiE5TcGzRDWfHDd/HYQKBgE3/\n"
	"k+1cSM248HHxZ2q6ESC6dHXap8VFCzhe5iDoYHI4r8i3ETb0Cxbf5D/psO/ROXp0\n"
	"NRaDyDe8BzgYSdn97sq3ppfSnqQYEvz7SkyMLShp4FSgcfUDWg9QVC9slFbwrW4E\n"
	"swV4CqJfxB6ugbYgDuF4DeR2QyreV15s9EOmwyCBAoGAKtuqPa6z346ClPuCYt/1\n"
	"3uFYE9YWCAkb+gd/paMm+35ps8ceDeEGiaWWkhAnmzQADkaeWiZ0/IbZwcRmv4fz\n"
	"5dXXwND/oqaJ4e4fGLxIA2v2Q1MMO98ikyvzABn2XTdeCjUTMSwANZa608VXNGFh\n"
	"hqpwUPywBLahR8uvpLYrd7c=\n"
	"-----END PRIVATE KEY-----\n"
        ;
#endif
    
    // Create new web/websocket server showing the following html page on each request
    // An "active" pages must include the script "/jsvarmainjavascript.js"!
    js = jsVarNewSinglePageServer(
        4321, BAIO_SSL_YES, 0,
        "<html><body><script src='/jsvarmainjavascript.js'></script>"
        "Server Time: <span id=mainSpan>nothing here for now</span>"
        "</body></html>"
        );

    lastSentTime = 0;
    for(;;) {
        if (lastSentTime != time(NULL)) {
            lastSentTime = time(NULL);
            // send eval request to all connected clients
            jsVarEvalAll("document.getElementById('mainSpan').innerHTML = '%.20s';", ctime(&lastSentTime));
        }
        // do all pending i/o requests, wait 100ms if there is no I/O.
        baioPoll(100000);
    }
}
