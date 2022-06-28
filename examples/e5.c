//////////////////////////////////////////////////////////////////////
//
// This program  demonstrates standard  "multifile" web  server having
// html pages stored in filesystem. It also demostrates usage of third
// party javascript libraries, in this case jquery.
//
// The  program is  very similar  to  the very  first example  showing
// current server time however, html part  of the program is stored in
// the  file  'e5www/index.html'.   To  identify html  element  to  be
// updated, jquery library is used.
//
// URL: https://localhost:4321
//


#include <time.h>

#include "jsvar.h"

int main() {
    struct jsVaraio *js;
    time_t          lastSentTime;
    
    // Create new web/websocket server having "./e5www" as its root directory.
    js = jsVarNewFileServer(4321, BAIO_SSL_YES, 0, "./e5www");

	lastSentTime = 0;
    for(;;) {
        if (lastSentTime != time(NULL)) {
            lastSentTime = time(NULL);
			// send eval request to all connected clients
            jsVarEvalAll("$('#mainSpan').html('%.20s');", ctime(&lastSentTime));
        }
        // do all pending i/o requests, wait 100ms if there is no I/O.
        baioPoll(100000);
    }
}

