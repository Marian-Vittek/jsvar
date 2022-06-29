//////////////////////////////////////////////////////////////////////
//
// This  is  a more  complex  example  demonstrating  one way  how  to
// implement web sites with login and personalized pages. This program
// provides a simple "Kitchen Timer"  as one of previous examples with
// the  difference that  this one  can be  used by  multiple connected
// users having each their own timer.
//
// This  example uses  function  "jsVarEval" which  allows  to eval  a
// string on a specific client  (compared to jsVarEvalAll sending eval
// request to all connected clients).
//
// When creating web server we have also required that each connection
// structure which is  used internally by jsvar and  sent to callbacks
// has  an additional  space allocated  to be  able to  store our  own
// connection   specific  variables   as   defined   in  our   "struct
// kitchenTimeraio".
//
// URL: https://localhost:4321
//

#include <time.h>
#include <string.h>

#include "jsvar.h"

#define TIMER_MIN -9999

// All  user specific  variables have  been put  into this  connection
// specific structure which is "derived" from struct jsVaraio.
struct kitchenTimeraio {
    // Must start with "struct jsVaraio" to allow free casting between
    // struct baio, wsaio, jsVaraio and this kitchenTimeraio
    struct jsVaraio jj;

    char            *login;
    int             timer;
    int             timerInitValue;
};

int kitchenTimerWebsocketCallback(struct jsVaraio *js, int i, int msglen) {
    struct kitchenTimeraio *pp;
    char *msg;

    // we can safely convert to our structure because we have reserved
    // space for it when creating server
    pp = (struct kitchenTimeraio *) js;
    
    // this is the place where the incoming message starts
    msg = js->w.b.readBuffer.b+i;

    // process the incoming message
    // printf("Got message: %s\n", msg);
    if (strncmp(msg, "login=", 6) == 0) {
        // New user logged in. Init it
        pp->login = strdup(msg+6);
        pp->timer = TIMER_MIN;
        pp->timerInitValue = 10;
        jsVarEval(js, "document.getElementById('startingval').value = '%d';", pp->timerInitValue);
        // put actual login name into all spans of class loginname
        jsVarEval(js, "document.querySelectorAll('.loginname').forEach(function(e) {e.innerHTML = '%s\\'s ';});", pp->login);
        // show timer "page"
        jsVarEval(js, "document.getElementById('logindiv').style.display='none';");
        jsVarEval(js, "document.getElementById('timerdiv').style.display='block';");
    } else if (strcmp(msg, "logout") == 0) {
        // Logout pressed, clean everything
        free(pp->login);
        pp->login = NULL;
        jsVarEval(js, "document.getElementById('timer').innerHTML = '&nbsp;';");
        jsVarEval(js, "document.getElementById('ring').innerHTML = '&nbsp;';");
        jsVarEval(js, "document.getElementById('loginin').value = '';");
        jsVarEval(js, "document.getElementById('stopbutton').style.display = 'none';");
        // sjow login "page"
        jsVarEval(js, "document.getElementById('timerdiv').style.display='none';");     
        jsVarEval(js, "document.getElementById('logindiv').style.display='block';");
    } else if (strcmp(msg, "start") == 0) {
        // Start timer
        pp->timer = pp->timerInitValue;
    } else if (strncmp(msg, "stop", 5) == 0) {
        // Stop timer
        pp->timer = TIMER_MIN;
        // clear "RING" message
        jsVarEval(js, "document.getElementById('ring').innerHTML = '&nbsp;';");
        // make the "stop ring" button invisible
        jsVarEval(js, "document.getElementById('stopbutton').style.display = 'none';");
    } else if (strncmp(msg, "sval=", 5) == 0) {
        // New timer value
        pp->timerInitValue = atoi(msg+5);
    }

    // if processed, you have to "consume" the message from the input buffer
    js->w.b.readBuffer.i += msglen;
    return(1);
}

void kitchenTimerServeTimers() {
    int                         i;
    struct jsVaraio             *js;
    struct kitchenTimeraio  *pp;

    // Loop through all connected clients
    for(i=0; i<baioTabMax; i++) {
        if (jsVarIsActiveConnection(baioTab[i], JSVAR_CON_WEBSOCKET_SERVER_CLIENT)) {
            js = (struct jsVaraio *)baioTab[i];
            pp = (struct kitchenTimeraio *)js;

            // If logged in update his timers
            if (pp->login != NULL) {
                if (pp->timer > TIMER_MIN) pp->timer --;
                if (pp->timer >= 0) {
                    // update displayed timer value
                    jsVarEval(js, "document.getElementById('timer').innerHTML = '%d';", pp->timer);
                } else if (pp->timer > TIMER_MIN) {
                    // ringing, show "RING" message in ring span
                    jsVarEval(js, "document.getElementById('ring').innerHTML = '%*s RING!';", -pp->timer%2, "");
                    // make 'stop ring' button visible
                    jsVarEval(js, "document.getElementById('stopbutton').style.display = 'block';");
                }
            }
        }
    }
}

// This  is  the  HTML  content  of   our  web  page.   We  use  macro
// JSVAR_STRINGIFY converting  its argument to  a C string.  It avoids
// annoying quotes when typing page content.
//
// The web page has two "divs"  one for login, another for the kitchen
// timer. At the  first time the login div is  displayed.  Once logged
// in, the second  one is displayed.

char *myServerWebPage = JSVAR_STRINGIFY(
    <html>
    <body>
    <script src='/jsvarmainjavascript.js'>
    </script>

    // Login div
    <div id=logindiv>
      Login: <input type=text id=loginin placeholder='Enter any name' onkeydown='if(event.key === "Enter") jsvar.callback("login="+this.value);' />
    </div>

    // Logged in div
    <div id=timerdiv style='display:none'>
    
      <h2><span class='loginname'></span> Kitchen Timer</h2>
      Welcome <span class='loginname'></span>!<br><br>
      Current Data/Time: <span id=mainSpan></span>
      <h3>Timer: <span id=timer></span></h3>
      <pre id=ring>&nbsp;</pre><br>
      Starting value: <input type=number id=startingval onchange='jsvar.callback("sval="+this.value);' /> &emsp; 
      <input type=button value='Click to Start' onclick='jsvar.callback("start");' />
      <br><input type=button value='Logout' onclick='jsvar.callback("logout");'/>
      <br><br><input type=button value='Stop Ring' id=stopbutton onclick='jsvar.callback("stop");' style='display:none' />
     </div
    
    </body>
    </html>
    );



/////////////////////////////////////////////////////////////////////////////
// Finaly, the main program

int main() {
    struct jsVaraio *js;
    time_t          currentTime;
    time_t          lastRefreshTime;



    // Create  new ssl  web/websocket  server listening  on port  4321
    // showing my page.  Allocate additional  space to hold my "struct
    // kitchenTimeraio"
    js = jsVarNewSinglePageServer(4321, BAIO_SSL_YES, sizeof(struct kitchenTimeraio)-sizeof(struct jsVaraio), myServerWebPage);

    // setup callback for incoming websocket messages
    jsVarCallBackAddToHook(&js->w.callBackOnWebsocketGetMessage, kitchenTimerWebsocketCallback);
    
    lastRefreshTime = currentTime = time(NULL);

    for(;;) {
        currentTime = time(NULL);
        if (lastRefreshTime != currentTime) {
            lastRefreshTime = currentTime;
            // update currently displayed date and time for all users
            jsVarEvalAll("document.getElementById('mainSpan').innerHTML = '%.20s';", ctime(&lastRefreshTime));
            // update timers for all logged in users
            kitchenTimerServeTimers();
        }
        // do all pending i/o requests, wait 100ms if there is no I/O.
        // baioPoll also calls jsVar callbacks,
        baioPoll(100000);
    }
}

