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
    // for example by the command:  
    // openssl req -x509 -nodes -days 99 -newkey rsa:1024 -keyout server.key -out server.crt
    
#else

    // Alternativaly, you can include  private key and the certificate
    // directly into source code by assigning to "baioSslServerKeyPem"
    // and "baioSslServerCertPem".
    baioSslServerCertPem = 
        "-----BEGIN CERTIFICATE-----\n"
        "MIICWzCCAcSgAwIBAgIJALspuLQRgau8MA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV\n"
        "BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n"
        "aWRnaXRzIFB0eSBMdGQwHhcNMjIwNjI3MTMyOTA3WhcNMjIxMDA0MTMyOTA3WjBF\n"
        "MQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50\n"
        "ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKB\n"
        "gQDGFch3zZBJX6iKrht4rXikoTm5vEi6Cs7PLv6Z2EVEZ6vklDrXruY/75Lge/Qr\n"
        "3BOAw0BN20uf8xwFEAUxJBwSjnwj4pzO51KFXtRTei/DjIir78jy/rGNN7pID3Ms\n"
        "WlvTcQUvIagJIgWCNwXMlit7A9EJpzFPN7d1Q4SjtWxOzwIDAQABo1MwUTAdBgNV\n"
        "HQ4EFgQU3NF+2ceI3dui7lQfw/2Eqj+mz4EwHwYDVR0jBBgwFoAU3NF+2ceI3dui\n"
        "7lQfw/2Eqj+mz4EwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOBgQCV\n"
        "afR+43LUTWi4Pax93rDW7meOf6ddZ89TTCtRBDmNfIX6of/nsnQKhjowkbXWhLR6\n"
        "QwfHUGgw1Cyh84aXtWn1GLEs5jnj+ngVPkdRePbJfdEya/Dpsk0YA+Ck2s5mE1hs\n"
        "5/BYg81o1KfGudIzeQ3VyNkHqRI1677VcnlJhYzaDg==\n"
        "-----END CERTIFICATE-----\n"
        ;
    baioSslServerKeyPem = 
        "-----BEGIN PRIVATE KEY-----\n"
        "MIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAMYVyHfNkElfqIqu\n"
        "G3iteKShObm8SLoKzs8u/pnYRURnq+SUOteu5j/vkuB79CvcE4DDQE3bS5/zHAUQ\n"
        "BTEkHBKOfCPinM7nUoVe1FN6L8OMiKvvyPL+sY03ukgPcyxaW9NxBS8hqAkiBYI3\n"
        "BcyWK3sD0QmnMU83t3VDhKO1bE7PAgMBAAECgYAyHzrtjZdP6aOVC78pxwM67QzV\n"
        "QZ5JbQithh+oQAAu8eid2yAUiU37qZxJrzO2kWZh84Xm7XFyVKqnYUlfCNsNKH3K\n"
        "gW6G+MoBi6TRsiGRcN8a/TgsjK5Z2Ttf+5ps6bDg7dogmWTq9ZdmgIAQcYsRs/ze\n"
        "7p6QSffGj4pv8LzpsQJBAPg9AWQP0v35FElAxbwhsxJUzDEyaay1ysVh3ziqIDPR\n"
        "NO6S86Y8uCw8B0xLgIBsC4CVleYOk8Xew1vQbOs6zocCQQDMR1UEkChtqhzotZqC\n"
        "jgRWMac1FW+qVXF3zoiv2H4P4eidGxyRdLpIQ5bcPRWkmTVjuxsmrQXQejGhSyeI\n"
        "7Qd5AkEAoChj/FYFUBzizLxAlze63CnfsCIRcf+8OosBxQJmUmg42W/wSSHFxaxZ\n"
        "HQ1dc/3Bkg1wsARZrQEjU9puW3oOgwJBAKHplBhmzrSFVh6Y+puqRwOunXJ0yCpB\n"
        "SQuF908xkFG0ZHRJ7e3YkGIAuI1eGU56ZRfkUNPp5iblA3ttnytnfDkCQQDIJbAy\n"
        "Ly2O929euQRAnN+BOWfKm8Wx72/Gm32hGOuZd1KBqrkmolny0P2vHhiEE1WzgIV8\n"
        "frXTrRPJTdsEk1jc\n"
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
