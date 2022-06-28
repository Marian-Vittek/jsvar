//////////////////////////////////////////////////////////////////////
//
// This "Kitchen  Timer" sample demonstrates the  use of bidirectional
// communication  with javascript.   Javascript  code sends  arbitrary
// strings to C using the function jsvar.callback(string)
//
// The  program launches  a webserver  listening on  port 4321.   Once
// connected, you can set timeout in an input html element, then click
// a button to  start the timer.  Once the timer  decreases to zero, a
// string 'RING'  appears together with  a button 'Stop Ring'  to stop
// "ringing".
//
// URL: https://localhost:4321
//

#include <time.h>
#include <string.h>

#include "jsvar.h"

#define	TIMER_MIN -9999

time_t          currentTime;
time_t          lastRefreshTime;
int          	timer;
int				timerInitValue = 10;

int kitchenTimerWebsocketCallback(struct jsVaraio *js, int i, int msglen) {
	char *msg;

	// this is the place where the incoming message starts
	msg = js->w.b.readBuffer.b+i;

	// process the incoming message
	printf("Got message: %s\n", msg);
	if (strcmp(msg, "start") == 0) {
		// jsvar.callback("start"); was executed
		timer = timerInitValue;
	} else if (strncmp(msg, "stop", 5) == 0) {
		// jsvar.callback("stop"); was executed
		timer = TIMER_MIN;
		// clear "RING" message
		jsVarEvalAll("document.getElementById('ring').innerHTML = '&nbsp;';");
		// make the "stop ring" button invisible
		jsVarEvalAll("document.getElementById('stopbutton').style.display = 'none';");
	} else if (strncmp(msg, "sval=", 5) == 0) {
		timerInitValue = atoi(msg+5);
	}

	// if processed, you have to "consume" the message from the input buffer
	js->w.b.readBuffer.i += msglen;
	return(1);
}

int main() {
    struct jsVaraio *js;

    // Create new ssl web/websocket server listening on port 4321 showing the following html page on each request
    js = jsVarNewSinglePageServer(
        4321, BAIO_SSL_YES, 0,
        "<html><body><script src='/jsvarmainjavascript.js'></script>"
		"<h2>Kitchen Timer</h2>"
		"Current Data/Time: <span id=mainSpan></span>"
		"<h3>Timer: <span id=timer></span></h3>"
		"<pre id=ring>&nbsp;</pre>"
		"<br>Starting value: <input type=number value='10' onchange='jsvar.callback(\"sval=\"+this.value);' /> &emsp; "		
		"<input type=button value='Click to Start' onclick='jsvar.callback(\"start\");' />"
		"<br><br><input type=button value='Stop Ring' id=stopbutton onclick='jsvar.callback(\"stop\");' style='display:none' />"
		"</body></html>"
        );

	// setup callback for incoming websocket messages
	jsVarCallBackAddToHook(&js->w.callBackOnWebsocketGetMessage, kitchenTimerWebsocketCallback);
	
	lastRefreshTime = currentTime = time(NULL);
	timer = TIMER_MIN;

    for(;;) {
		currentTime = time(NULL);
        if (lastRefreshTime != currentTime) {
            lastRefreshTime = currentTime;
			// update currently displayed date and time
            jsVarEvalAll("document.getElementById('mainSpan').innerHTML = '%.20s';", ctime(&lastRefreshTime));
			if (timer > TIMER_MIN) timer --;
			if (timer >= 0) {
				// update displayed timer value
				jsVarEvalAll("document.getElementById('timer').innerHTML = '%d';", timer);
			} else if (timer > TIMER_MIN) {
				// ringing, show "RING" message in ring span
				jsVarEvalAll("document.getElementById('ring').innerHTML = '%*s RING!';", -timer%2, "");
				// make 'stop ring' button visible
				jsVarEvalAll("document.getElementById('stopbutton').style.display = 'block';");
			}
        }
        // do all pending i/o requests, wait 100ms if there is no I/O.
        // baioPoll also calls jsVar callbacks,
        baioPoll(100000);
    }
}

