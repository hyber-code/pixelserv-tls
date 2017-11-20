#include "util.h"
#include "logger.h"
#if defined(__GLIBC__) && defined(BACKTRACE)
#include <execinfo.h>
#endif

// stats data
// note that child processes inherit a snapshot copy
// public data (should probably change to a struct)
volatile sig_atomic_t count = 0;
volatile sig_atomic_t avg = 0;
volatile sig_atomic_t rmx = 0;
volatile sig_atomic_t tav = 0;
volatile sig_atomic_t tmx = 0;
volatile sig_atomic_t err = 0;
volatile sig_atomic_t tmo = 0;
volatile sig_atomic_t cls = 0;
volatile sig_atomic_t nou = 0;
volatile sig_atomic_t pth = 0;
volatile sig_atomic_t nfe = 0;
volatile sig_atomic_t ufe = 0;
volatile sig_atomic_t gif = 0;
volatile sig_atomic_t bad = 0;
volatile sig_atomic_t txt = 0;
volatile sig_atomic_t jpg = 0;
volatile sig_atomic_t png = 0;
volatile sig_atomic_t swf = 0;
volatile sig_atomic_t ico = 0;
volatile sig_atomic_t sta = 0;
volatile sig_atomic_t stt = 0;
volatile sig_atomic_t noc = 0;
volatile sig_atomic_t rdr = 0;
volatile sig_atomic_t pst = 0;
volatile sig_atomic_t hed = 0;
volatile sig_atomic_t opt = 0;
volatile sig_atomic_t cly = 0;

volatile sig_atomic_t slh = 0;
volatile sig_atomic_t slm = 0;
volatile sig_atomic_t sle = 0;
volatile sig_atomic_t slc = 0;
volatile sig_atomic_t slu = 0;
volatile sig_atomic_t kcc = 0;
volatile sig_atomic_t kmx = 0;
float kvg = 0.0;
volatile sig_atomic_t krq = 0;
volatile sig_atomic_t clt = 0;

// private data
static struct timespec startup_time = {0, 0};
static clockid_t clock_source = CLOCK_MONOTONIC;

void get_time(struct timespec *time) {
  if (clock_gettime(clock_source, time) < 0) {
    if (errno == EINVAL &&
        clock_source == CLOCK_MONOTONIC) {
      clock_source = CLOCK_REALTIME;
      syslog(LOG_WARNING, "clock_gettime() reports CLOCK_MONOTONIC not supported; switching to less accurate CLOCK_REALTIME");
      get_time(time); // try again with new clock setting
    } else {
      // this should never happen
      syslog(LOG_ERR, "clock_gettime() reported failure getting time: %m");
      time->tv_sec = time->tv_nsec = 0;
    }
  }
}

char* get_version(int argc, char* argv[]) {
  char* retbuf = NULL;
  char* optbuf = NULL;
  unsigned int optlen = 0, i = 1, freeoptbuf = 0;
  unsigned int arglen[argc];

  // capture startup_time if not yet set
  if (!startup_time.tv_sec) {
    get_time(&startup_time);
  }

  // determine total size of all arguments
  for (i = 1; i < argc; ++i) {
    arglen[i] = strlen(argv[i]) + 1; // add 1 for leading space
    optlen += arglen[i];
  }
  if (optlen > 0) {
    // allocate a buffer to hold all arguments
    optbuf = malloc((optlen * sizeof(char)) + 1);
    if (optbuf) {
      freeoptbuf = 1;
      // concatenate arguments into buffer
      for (i = 1, optlen = 0; i < argc; ++i) {
        optbuf[optlen] = ' '; // prepend a space to each argument
        strncpy(optbuf + optlen + 1, argv[i], arglen[i]);
        optlen += arglen[i];
      }
      optbuf[optlen] = '\0';
    } else {
      optbuf = " <malloc error>";
    }
  } else {
    optbuf = " <none>";
  }

  if (asprintf(&retbuf, "%s: %s compiled: %s options:%s", argv[0], VERSION, __DATE__ " " __TIME__, optbuf) < 1) {
    retbuf = " <asprintf error>";
  }

  if (freeoptbuf) {
    free(optbuf);
    freeoptbuf = 0;
  }

  return retbuf;
}

char* get_stats(const int sta_offset, const int stt_offset) {
    char* retbuf = NULL, *uptimeStr = NULL;
    struct timespec current_time;
    long uptime;

	const char* sta_fmt =  "<br><table><tr><td>uts</td><td>%s</td><td>process uptime</td></tr><tr><td>log</td><td>%d</td><td>critical (0) error (1) warning (2) notice (3) info (4) debug (5)</td></tr><tr><td>kcc</td><td>%d</td><td>number of active service threads</td></tr><tr><td>kmx</td><td>%d</td><td>maximum number of service threads</td></tr><tr><td>kvg</td><td>%.2f</td><td>average number of requests per service thread</td></tr><tr><td>krq</td><td>%d</td><td>max number of requests by one service thread</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>req</td><td>%d</td><td>total # of requests (HTTP, HTTPS, success, failure etc)</td></tr><tr><td>avg</td><td>%d bytes</td><td>average size of requests</td></tr><tr><td>rmx</td><td>%d bytes</td><td>largest size of request(s)</td></tr><tr><td>tav</td><td>%d ms</td><td>average processing time (per request)</td></tr><tr><td>tmx</td><td>%d ms</td><td>longest processing time (per request)</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>slh</td><td>%d</td><td># of accepted HTTPS requests</td></tr><tr><td>slm</td><td>%d</td><td># of rejected HTTPS requests (missing certificate)</td></tr><tr><td>sle</td><td>%d</td><td># of rejected HTTPS requests (certificate available but bad)</td></tr><tr><td>slc</td><td>%d</td><td># of dropped HTTPS requests (client disconnect without sending any request)</td></tr><tr><td>slu</td><td>%d</td><td># of dropped HTTPS requests (unknown error)</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>nfe</td><td>%d</td><td># of GET requests for server-side scripting</td></tr><tr><td>gif</td><td>%d</td><td># of GET requests for GIF</td></tr><tr><td>ico</td><td>%d</td><td># of GET requests for ICO</td></tr><tr><td>txt</td><td>%d</td><td># of GET requests for Javascripts</td></tr><tr><td>jpg</td><td>%d</td><td># of GET requests for JPG</td></tr><tr><td>png</td><td>%d</td><td># of GET requests for PNG</td></tr><tr><td>swf</td><td>%d</td><td># of GET requests for SWF</td></tr><tr><td>sta</td><td>%d</td><td># of GET requests for HTML stats</td></tr><tr><td>stt</td><td>%d</td><td># of GET requests for plain text stats</td></tr><tr><td>ufe</td><td>%d</td><td># of GET requests /w unknown file extension</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>opt</td><td>%d</td><td># of OPTIONS requests</td></tr><tr><td>pst</td><td>%d</td><td># of POST requests</td></tr><tr><td>hed</td><td>%d</td><td># of HEAD requests (HTTP 501 response)</td></tr><tr><td>rdr</td><td>%d</td><td># of GET requests resulted in REDIRECT response</td></tr><tr><td>nou</td><td>%d</td><td># of GET requests /w empty URL</td></tr><tr><td>pth</td><td>%d</td><td># of GET requests /w malformed URL</td></tr><tr><td>204</td><td>%d</td><td># of GET requests (HTTP 204 response)</td></tr><tr><td>bad</td><td>%d</td><td># of unknown HTTP requests (HTTP 501 response)</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>tmo</td><td>%d</td><td># of timeout requests (client connect w/o sending a request in 'select_timeout' secs)</td></tr><tr><td>cls</td><td>%d</td><td># of dropped requests (client disconnect without sending any  request)</td></tr><tr><td>cly</td><td>%d</td><td># of dropped requests (client disconnect before response sent)</td></tr><tr><td>clt</td><td>%d</td><td># of dropped requests (reached maximum service threads)</td></tr><tr><td>err</td><td>%d</td><td># of dropped requests (unknown reason)</td></tr></table>";

    const char* stt_fmt = "%d uts, %d log, %d kcc, %d kmx, %.2f kvg, %d krq, %d req, %d avg, %d rmx, %d tav, %d tmx, %d slh, %d slm, %d sle, %d slc, %d slu, %d nfe, %d gif, %d ico, %d txt, %d jpg, %d png, %d swf, %d sta, %d stt, %d ufe, %d opt, %d pst, %d hed, %d rdr, %d nou, %d pth, %d 204, %d bad, %d tmo, %d cls, %d cly, %d clt, %d err";
    get_time(&current_time);
    uptime = difftime(current_time.tv_sec, startup_time.tv_sec);

    asprintf(&uptimeStr, "%dd %02d:%02d", (int)uptime/86400, (int)(uptime%86400)/3600, (int)((uptime%86400)%3600)/60);

    if (asprintf(&retbuf, (sta_offset) ? sta_fmt : stt_fmt,
        (sta_offset) ? (long)uptimeStr : (long)uptime, log_get_verb(), kcc, kmx, kvg, krq, count, avg, rmx, tav, tmx, slh, slm, sle, slc, slu, nfe, gif, ico, txt, jpg, png, swf, sta + sta_offset, stt + stt_offset, ufe, opt, pst, hed, rdr, nou, pth, noc, bad, tmo, cls, cly, clt, err
        ) < 1)
        retbuf = " <asprintf error>";

    free(uptimeStr);
    return retbuf;
}

// Use SMA for the first 500 samples approximated by # of requets. Use EMA afterwards
float ema(float curr, int new, int *cnt) {
    if (count < 500) {
      curr *= *cnt;
      curr = (curr + new) / ++(*cnt);
    } else
      curr += 0.002 * (new - curr);
    return curr;
}

double elapsed_time_msec(const struct timespec start_time) {
  struct timespec current_time = {0, 0};
  struct timespec diff_time = {0, 0};

  if (!start_time.tv_sec &&
      !start_time.tv_nsec) {
    log_msg(LGG_DEBUG, "check_time(): returning because start_time not set");
    return -1.0;
  }

  get_time(&current_time);

  diff_time.tv_sec = difftime(current_time.tv_sec, start_time.tv_sec) + 0.5;
  diff_time.tv_nsec = current_time.tv_nsec - start_time.tv_nsec;
  if (diff_time.tv_nsec < 0) {
    // normalize nanoseconds
    diff_time.tv_sec  -= 1;
    diff_time.tv_nsec += 1000000000;
  }

  return diff_time.tv_sec * 1000 + ((double)diff_time.tv_nsec / 1000000);
}

#if defined(__GLIBC__) && defined(BACKTRACE)
void print_trace() {

  void *buf[32];
  char **strings;
  int size, i;
  size = backtrace(buf, 32);
  strings = backtrace_symbols(buf, size);
  log_msg(LGG_CRIT, "backtrace:");
  for (i = 0; i < size; i++)
    log_msg(LGG_CRIT, "%d %s", buf[i], strings[i]);
  free(strings);
  exit(EXIT_FAILURE);
}
#endif