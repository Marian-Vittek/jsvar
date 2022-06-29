//////////////////////////////////////////////////////////////////////
//
// This program demonstrates howto dynamically  update web page from C
// using  the  function  'jsVarEvalAll'.   This  function  executes  a
// javascript code  on all connected  clients.  This program  is using
// Open-Ssl  for secure  web  sockets.   It has  to  be compiled  with
// JSVAR_USE_OPEN_SSL defined.  Also  it has to be  linked with libssl
// and libcrypto.
//
// The program launches a secure  webserver listening on port 4321 and
// dynamically  updates   current  (server)  time  on   all  connected
// browsers.
//
// URL: https://localhost:4321
//

#include <time.h>
#include "jsvar.h"

int main() {
    struct jsVaraio *js;
    time_t          lastSentTime;
    
    // Create new web/websocket server showing the following html page
    // on each  request.  An  "active" pages  must include  the script
    // "/jsvarmainjavascript.js"!
    js = jsVarNewSinglePageServer(
        4321, BAIO_SSL_YES, 0,
        "<html><body><script src='/jsvarmainjavascript.js'></script>"
        "Server Time: <span id=mainSpan>nothing here for now</span>"
        "</body></html>"
        );

    lastSentTime = 0;
    
    // The main loop
    for(;;) {
        if (lastSentTime != time(NULL)) {
            // Time changed.
            lastSentTime = time(NULL);
            // Send eval  request to all connected  clients. Note that
            // we  send  eval  requests  only when  there  are  actual
            // changes to be displayed. We do not send it at each pass
            // of the loop, otherwise we risk overflow of buffers.
            jsVarEvalAll("document.getElementById('mainSpan').innerHTML = '%.20s';", ctime(&lastSentTime));
        }
        // Actually,  all previous  jsVar actions  were buffered.   To
        // perform real  I/O operations  one have to  call "baioPoll".
        // It shall be  called as often as possible.  If  there are no
        // I/O  to do,  baioPoll  waits until  timeout,  in this  case
        // 100000us aka 100ms.
        baioPoll(100000);
    }
}

