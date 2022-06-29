//////////////////////////////////////////////////////////////////////
//
// This program  demonstrates use of  jsVar without openSsl  and howto
// allow debugging messages in browser's javascript console.
//
// The  program launches  a (non-secure)  webserver listening  on port
// 4321  and  dynamically  updating   current  (server)  time  on  all
// connected browsers. This  example does not use OpenSsl  and may not
// work on some paranoid browsers!
//
// URL: http://localhost:4321
//

#include <time.h>

#include "jsvar.h"

int main() {
    struct jsVaraio *js;
    time_t          lastSentTime;
    
    // Create new web/websocket server showing the following html page on each request
    // An "active" pages must include the script "/jsvarmainjavascript.js"!
    js = jsVarNewSinglePageServer(
        4321, BAIO_SSL_NO, 0,
        "<html><body><script src='/jsvarmainjavascript.js'></script>"
        "Server Time: <span id=mainSpan>nothing here for now</span>"
        "</body></html>"
        );

    lastSentTime = 0;
    for(;;) {
        if (lastSentTime != time(NULL)) {
            lastSentTime = time(NULL);
            // send eval request to all connected clients
            // set debug level to all debugging info
            jsVarEvalAll("jsvar.debuglevel = 9999;");
            // update time
            jsVarEvalAll("document.getElementById('mainSpan').innerHTML = '%.20s';", ctime(&lastSentTime));
        }
        // do all pending i/o requests, wait 100ms if there is no I/O.
        baioPoll(100000);
    }
}

