//////////////////////////////////////////////////////////////////////
//
// This  program demonstrates  the  use of  wchar_t  strings. It  uses
// function  'jsVarWEvalAll' which  is  wide  character equivalent  to
// 'jsVarEvalAll'.    You  have   to   compile   jsvar.c  with   macro
// JSVAR_USE_WCHAR_T defined in order to use wide character functions.
//
// The  program  launches  a  webserver listening  on  port  4321  and
// dynamically updating current (server) time for different timezones.
//
// This sample works on Linux/Unix systems only!
//
// URL: https://localhost:4321
//


#include <time.h>
#include <locale.h>

#include "jsvar.h"

#define STR_MAX			32768

static char *ctimeForTimeZone(char *timeZone) {
	time_t		ts;

	ts = time(NULL);
	setenv("TZ", timeZone, 1); 
	tzset();
	return(ctime(&ts)+11);
}

int main() {
    struct jsVaraio *js;
    time_t          lastSentTime;
	wchar_t			wstr[STR_MAX];
	int				n;

	// Set an UTF-8 locale to allow conversions from wchar_t
	setlocale(LC_ALL, "en_US.UTF-8");

    // Create new web/websocket server
    js = jsVarNewSinglePageServer(
        4321, BAIO_SSL_YES, 0,
        "<html><body><script src='/jsvarmainjavascript.js'></script>"
		"<h1>World Time</h1><span id=mainSpan>nothing here for now</span>"
		"</body></html>"
        );

	lastSentTime = 0;
    for(;;) {
        if (lastSentTime != time(NULL)) {
            lastSentTime = time(NULL);
			n = 0;
			n += swprintf(wstr+n, STR_MAX-n, L"%.8s: New York<br> ", ctimeForTimeZone("America/New_York"));
			n += swprintf(wstr+n, STR_MAX-n, L"%.8s: القاهرة (Cairo)<br>", ctimeForTimeZone("Africa/Cairo"));
			n += swprintf(wstr+n, STR_MAX-n, L"%.8s: Москва  (Moscow)<br>", ctimeForTimeZone("Europe/Moscow"));
			n += swprintf(wstr+n, STR_MAX-n, L"%.8s: 北京市 (Beijing)<br>", ctimeForTimeZone("Asia/Shanghai"));
			n += swprintf(wstr+n, STR_MAX-n, L"%.8s: 東京 (Tokyo)<br>", ctimeForTimeZone("Asia/Tokyo"));
			wstr[STR_MAX-1] = 0;
			// send eval request to all connected clients
            jsVarWEvalAll(L"document.getElementById('mainSpan').innerHTML = '%ls';", wstr);
        }
        // do all pending i/o requests, wait 100ms if there is no I/O.
        baioPoll(100000);
    }
}

