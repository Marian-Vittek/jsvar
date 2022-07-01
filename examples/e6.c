//////////////////////////////////////////////////////////////////////
//
// This  program  demonstrates how  to  implement  multiple web  pages
// (images, URLs in  general) with an in memory  server (not necessary
// loading  pages from  files).  Jsvar.c code  must  by compiled  with
// JSVAR_ALL_LIBRARIES macro defined.
//
// URL: https://localhost:4321
//

#include <string.h>

#include "jsvar.h"

int myGetCallback(struct wsaio *ww, char *uri) {

    // This callback is  called before the server answers  a valid GET
    // request. "uri" contains requested URI.
    
    if (strcmp(uri, "/index.html") == 0 || strcmp(uri, "/") == 0) {
        baioPrintfToBuffer(
            &ww->b, 
            //"<html><body><script src='/jsvarmainjavascript.js'></script>"
            "<html><body>"
            "<h3>Index</h3>"
            "<a href='page-one.html'>Page 1</a><br>"
            "<a href='page-two.html'>Page 2</a><br>"
            "</body></html>"
            );
        wsaioHttpFinishAnswer(ww, "200 OK", "text/html", NULL);     
    } else if (strcmp(uri, "/page-one.html") == 0) {
        baioPrintfToBuffer(
            &ww->b, 
            //"<html><body><script src='/jsvarmainjavascript.js'></script>"
            "<html><body>"
            "<h3>This is Page One</h3>"
            "<a href='index.html'>Back to index</a><br>"
            "</body></html>"
            );
        wsaioHttpFinishAnswer(ww, "200 OK", "text/html", NULL);     
    } else if (strcmp(uri, "/page-two.html") == 0) {
        baioPrintfToBuffer(
            &ww->b, 
            //"<html><body><script src='/jsvarmainjavascript.js'></script>"
            "<html><body>"
            "<h3>This is Page Two</h3>"
            "<a href='index.html'>Back to index</a><br>"
            "</body></html>"
            );
        wsaioHttpFinishAnswer(ww, "200 OK", "text/html", NULL);     
    } else {
		// not my page, execute other callbacks on the hook
		return(0);
		// alternatively, you can answer that no such page exists.
        // wsaioHttpFinishAnswer(ww, "404 Not Found", "text/html", NULL);
    }
    
    // returning non-zero value means we took care and answered this URI request
    // returning zero would allow calling of other callbacks on the hook.
    return(1);
}


int main() {
    struct jsVaraio *js;
    
    // Create new  web/websocket server, no  need to specify  the page
    // here. It will captured in URL callback.
    js = jsVarNewSinglePageServer(4321, BAIO_SSL_YES, 0, NULL);

    jsVarCallBackAddToHook(&js->w.callBackOnWwwGetRequest, myGetCallback);
    
    for(;;) {
        // do all pending i/o requests, wait 100ms if there is no I/O.
        baioPoll(100000);
    }
}



