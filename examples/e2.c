//////////////////////////////////////////////////////////////////////
//
// This program demonstrates how to  dynamically send binary data with
// jsVar.  Binary  websocket messages  are sent  with jsVarSendDataAll
// and data is  retrieved on javascript side in jsvar.data  in form of
// an arraybuffer. User can then convert  it to what he needs (in this
// case to base64).
//
// The program launches  a webserver listening on port  4321 and sends
// pictures  to all  connected  clients.  It will  work  only if  your
// browser supports binary websocket messages!
//
// URL: https://localhost:4321
//

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "jsvar.h"

int main() {
    struct jsVaraio *js;
	char			picname[255];
	char			*p;
	struct stat		st;
	FILE			*ff;
	int				i, len;
	time_t			currentTime, lastSentTime;
    
    // Create new web/websocket server showing the following html page on each request
	// An "active" pages must include the script "/jsvarmainjavascript.js"!
    js = jsVarNewSinglePageServer(
        4321, BAIO_SSL_YES, 0,
        "<html><body><script src='/jsvarmainjavascript.js'></script>"
		"<script>"
		"function arrayBufferToBase64(buffer) {"
		" var bin = '';"
		" var a = new Uint8Array(buffer);"
		" var len = a.byteLength;"
		" for (var i = 0; i < len; i++) {"
        "  bin += String.fromCharCode(a[i]);"
		" }"
		" return window.btoa(bin);"
		"}"
		"</script>"
		"Picture Size <span id=size></span> bytes.<br>"
		"<img id=picdiv style='width:460px;hight:340px;'></img>"
		"</body></html>"
        );

	i = 0;
    for(;;) {
		currentTime = time(NULL);
        if (currentTime != lastSentTime && currentTime % 2 == 0) {
			sprintf(picname, "pic-%02d.jpg", i);
			if (stat(picname, &st) != 0) {
				i = 0;
				continue;
			} else {
				lastSentTime = currentTime;
				ff = fopen(picname, "rb");
				if (ff != NULL) {
					len = st.st_size;
					p = malloc(len);
					if (p != NULL) {
						fread(p, len, 1, ff);
						// jsVarEvalAll("jsvar.debuglevel = 9999;");
						// printf("sending %s: %d bytes\n", picname, len);
						jsVarSendDataAll(p, len);
						jsVarEvalAll("document.getElementById('picdiv').src='data:image/jpg;base64,' + arrayBufferToBase64(jsvar.data);");
						jsVarEvalAll("document.getElementById('size').innerHTML='%d';", len);
						free(p);
					}
					fclose(ff);
					i++;
				}
			}
		}

        // do all pending i/o requests, wait 100ms if there is no I/O.
        baioPoll(100000);
    }
}

