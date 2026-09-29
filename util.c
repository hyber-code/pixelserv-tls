#include <stdarg.h>
#include <openssl/rand.h>
#include "util.h"
#include "logger.h"
#include "certs.h"
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
volatile sig_atomic_t ers = 0;
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
volatile sig_atomic_t uca = 0;
volatile sig_atomic_t ucb = 0;
volatile sig_atomic_t uce = 0;
volatile sig_atomic_t ush = 0;
volatile sig_atomic_t kcc = 0;
volatile sig_atomic_t kmx = 0;
float kvg = 0.0;
volatile sig_atomic_t krq = 0;
volatile sig_atomic_t clt = 0;
volatile sig_atomic_t v13 = 0;
volatile sig_atomic_t v12 = 0;
volatile sig_atomic_t v10 = 0;
volatile sig_atomic_t zrt = 0;

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

unsigned int process_uptime()
{
    struct timespec now;
    get_time(&now);
    return (unsigned int) difftime(now.tv_sec, startup_time.tv_sec);
}

char* get_version(int argc, char* argv[]) {
  char* retbuf = NULL;
  char* optbuf = NULL;
  unsigned int optlen = 0, freeoptbuf = 0;
  unsigned int arglen[argc];

  // capture startup_time if not yet set
  if (!startup_time.tv_sec) {
    get_time(&startup_time);
  }

  // determine total size of all arguments
  for (int i = 1; i < argc; ++i) {
    arglen[i] = strlen(argv[i]) + 1; // add 1 for leading space
    optlen += arglen[i];
  }
  if (optlen > 0) {
    // allocate a buffer to hold all arguments
    optbuf = malloc((optlen * sizeof(char)) + 1);
    if (optbuf) {
      freeoptbuf = 1;
      // concatenate arguments into buffer
      for (int i = 1, optlen = 0; i < argc; ++i) {
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

  if (asprintf(&retbuf, "pixelserv-tls %s (compiled: %s" FEATURE_FLAGS ") options:%s",
          VERSION, __DATE__ " " __TIME__, optbuf) < 1) {
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
    unsigned int uptime = process_uptime();

	const char* sta_fmt =  "<br><table><tr><td>uts</td><td>%s</td><td>process uptime</td></tr><tr><td>log</td><td>%d</td><td>critical (0) error (1) warning (2) notice (3) info (4) debug (5)</td></tr><tr><td>kcc</td><td>%d</td><td>number of active service threads</td></tr><tr><td>kmx</td><td>%d</td><td>maximum number of service threads</td></tr><tr><td>kvg</td><td>%.2f</td><td>average number of requests per service thread</td></tr><tr><td>krq</td><td>%d</td><td>max number of requests by one service thread</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>req</td><td>%d</td><td>total # of requests (HTTP, HTTPS, success, failure etc)</td></tr><tr><td>avg</td><td>%d bytes</td><td>average size of requests</td></tr><tr><td>rmx</td><td>%d bytes</td><td>largest size of request(s)</td></tr><tr><td>tav</td><td>%d ms</td><td>average processing time (per request)</td></tr><tr><td>tmx</td><td>%d ms</td><td>longest processing time (per request)</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>slh</td><td>%d</td><td># of accepted HTTPS requests</td></tr><tr><td>slm</td><td>%d</td><td># of rejected HTTPS requests (missing certificate)</td></tr><tr><td>sle</td><td>%d</td><td># of rejected HTTPS requests (certificate available but not usable)</td></tr><tr><td>slc</td><td>%d</td><td># of dropped HTTPS requests (client disconnect without sending any request)</td></tr><tr><td>slu</td><td>%d</td><td># of dropped HTTPS requests (other TLS handshake errors)</td></tr><th colspan=\"3\"></th></tr><tr><td>v13</td><td>%d</td><td>slh/slc break-down: TLS 1.3</td></tr><tr><td>v12</td><td>%d</td><td>slh/slc break-down: TLS 1.2</td></tr><tr><td>v10</td><td>%d</td><td>slh/slc break-down: TLS 1.0</td></tr><tr><td>zrt</td><td>%d</td><td>slh break-down: TLS 1.3 Early Data aka 0-RTT</td></tr>    <tr><th colspan=\"3\"></th></tr>    <tr><td>uca</td><td>%d</td><td>slu break-down: # of unknown CA reported by clients</td></tr><tr><td>ucb</td><td>%d</td><td>slu break-down: # of bad certificate reported by clients</td></tr><tr><td>uce</td><td>%d</td><td>slu break-down: # of unknown cert reported by clients</td></tr><tr><td>ush</td><td>%d</td><td>slu break-down: # of shutdown by clients after ServerHello</td></tr><tr><tr><th colspan=\"3\"></th></tr><tr><td>sct</td><td>%d</td><td>cert cache: # of certs in cache</td></tr><tr><td>sch</td><td>%d</td><td>cert cache: # of reuses of cached certs</td></tr><tr><tr><td>scm</td><td>%d</td><td>cert cache: # of misses to find a cert in cache</td></tr><tr><tr><td>scp</td><td>%d</td><td>cert cache: # of purges to give room for a new cert</td></tr><tr><td>ssh</td><td>%d</td><td>sess cache: # of reuses of cached TLS sessions</td></tr><tr><td>ssm</td><td>%d</td><td>sess cache: # of misses to find a TLS session in cache</td></tr><tr><td>ssp</td><td>%d</td><td>sess cache: # of purges to give room for a new TLS session</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>nfe</td><td>%d</td><td># of GET requests for server-side scripting</td></tr><tr><td>gif</td><td>%d</td><td># of GET requests for GIF</td></tr><tr><td>ico</td><td>%d</td><td># of GET requests for ICO</td></tr><tr><td>txt</td><td>%d</td><td># of GET requests for Javascripts</td></tr><tr><td>jpg</td><td>%d</td><td># of GET requests for JPG</td></tr><tr><td>png</td><td>%d</td><td># of GET requests for PNG</td></tr><tr><td>swf</td><td>%d</td><td># of GET requests for SWF</td></tr><tr><td>ufe</td><td>%d</td><td># of GET requests /w unknown file extension</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>opt</td><td>%d</td><td># of OPTIONS requests</td></tr><tr><td>pst</td><td>%d</td><td># of POST requests</td></tr><tr><td>hed</td><td>%d</td><td># of HEAD requests (HTTP 501 response)</td></tr><tr><td>rdr</td><td>%d</td><td># of GET requests resulted in REDIRECT response</td></tr><tr><td>nou</td><td>%d</td><td># of GET requests /w empty URL</td></tr><tr><td>pth</td><td>%d</td><td># of GET requests /w malformed URL</td></tr><tr><td>204</td><td>%d</td><td># of GET requests (HTTP 204 response)</td></tr><tr><td>bad</td><td>%d</td><td># of unknown HTTP requests (HTTP 501 response)</td></tr><tr><th colspan=\"3\"></th></tr><tr><td>cls</td><td>%d</td><td># of dropped requests (client disconnect without sending any  request)</td></tr><tr><td>cly</td><td>%d</td><td># of dropped requests (client disconnect before response sent)</td></tr><tr><td>clt</td><td>%d</td><td># of dropped requests (reached maximum service threads)</td></tr><tr><td>err</td><td>%d</td><td># of dropped requests (unknown reason)</td></tr></table>";

    const char* stt_fmt = "%d uts, %d log, %d kcc, %d kmx, %.2f kvg, %d krq, %d req, %d avg, %d rmx, %d tav, %d tmx, %d slh, %d slm, %d sle, %d slc, %d slu, %d v13, %d v12, %d v10, %d zrt, %d uca, %d ucb, %d uce, %d ush, %d sct, %d sch, %d scm, %d scp, %d ssh, %d ssm, %d ssp, %d nfe, %d gif, %d ico, %d txt, %d jpg, %d png, %d swf, %d ufe, %d opt, %d pst, %d hed, %d rdr, %d nou, %d pth, %d 204, %d bad, %d cls, %d cly, %d clt, %d err";
    int sct = sslctx_tbl_get_cnt_total();
    int sch = sslctx_tbl_get_cnt_hit();
    int scm = sslctx_tbl_get_cnt_miss();
    int scp = sslctx_tbl_get_cnt_purge();
    int sst = sslctx_tbl_get_sess_cnt();
    int ssh = sslctx_tbl_get_sess_hit();
    int ssm = sslctx_tbl_get_sess_miss();
    int ssp = sslctx_tbl_get_sess_purge();

    if (asprintf(&uptimeStr, "%dd %02d:%02d", (int)uptime/86400, (int)(uptime%86400)/3600, (int)((uptime%86400)%3600)/60) < 1
        || asprintf(&retbuf, (sta_offset) ? sta_fmt : stt_fmt,
        (sta_offset) ? (long)uptimeStr : (long)uptime, log_get_verb(), kcc, kmx, kvg, krq, count, avg, rmx, tav, tmx, slh, slm, sle, slc, slu, v13, v12, v10, zrt, uca, ucb, uce, ush, sct, sch, scm, scp, sst + ssh, ssm, ssp, nfe, gif, ico, txt, jpg, png, swf, ufe, opt, pst, hed, rdr, nou, pth, noc, bad, cls, cly, clt, ers
        ) < 1)
        retbuf = " <asprintf error>";

    free(uptimeStr);
    return retbuf;
}

/* ---- persistent statistics --------------------------------------------- */
/* Counters are saved to <cert dir>/stats.dat (a small text file) on shutdown and every
   10 minutes when something changed, and loaded at start, so they survive restarts and
   reboots. Delete the file while the server is stopped to reset all counters. */

time_t stats_since = 0;
static time_t stats_last_save = 0;
static int stats_last_count = -1;

static const struct { const char *name; volatile sig_atomic_t *val; } stats_persist[] = {
    {"count", &count}, {"avg", &avg}, {"rmx", &rmx}, {"tav", &tav}, {"tmx", &tmx}, {"ers", &ers},
    {"tmo", &tmo}, {"cls", &cls}, {"nou", &nou}, {"pth", &pth}, {"nfe", &nfe}, {"ufe", &ufe},
    {"gif", &gif}, {"bad", &bad}, {"txt", &txt}, {"jpg", &jpg}, {"png", &png}, {"swf", &swf},
    {"ico", &ico}, {"sta", &sta}, {"stt", &stt}, {"noc", &noc}, {"rdr", &rdr}, {"pst", &pst},
    {"hed", &hed}, {"opt", &opt}, {"cly", &cly}, {"slh", &slh}, {"slm", &slm}, {"sle", &sle},
    {"slc", &slc}, {"slu", &slu}, {"uca", &uca}, {"ucb", &ucb}, {"uce", &uce}, {"ush", &ush},
    {"kmx", &kmx}, {"krq", &krq}, {"clt", &clt}, {"v13", &v13}, {"v12", &v12}, {"v10", &v10},
    {"zrt", &zrt}
};

void stats_save(const char *dir)
{
    char path[PIXELSERV_MAX_PATH], tmp[PIXELSERV_MAX_PATH];
    FILE *fp;
    size_t i;
    if (!dir) return;
    if (!stats_since) stats_since = time(NULL);
    snprintf(path, sizeof path, "%s/stats.dat", dir);
    snprintf(tmp, sizeof tmp, "%s/stats.dat.tmp", dir);
    if (!(fp = fopen(tmp, "w"))) {
        log_msg(LGG_DEBUG, "cannot write %s: %m", tmp);
        return;
    }
    fprintf(fp, "pixelserv-stats 1\nsince %ld\n", (long)stats_since);
    for (i = 0; i < sizeof stats_persist / sizeof stats_persist[0]; i++)
        fprintf(fp, "%s %ld\n", stats_persist[i].name, (long)*stats_persist[i].val);
    if (fclose(fp) == 0)
        rename(tmp, path);   /* atomic replace: a power cut never leaves a half-written file */
    else
        remove(tmp);
    stats_last_save = time(NULL);
    stats_last_count = count;
}

/* Reset: a random token generated at start is embedded in the statistics page; only a request
   carrying it can reset the counters, so a web page on another site cannot trigger a reset. */
static char stats_token[33];

void stats_token_init(void)
{
    unsigned char r[16];
    int i;
    if (RAND_bytes(r, sizeof r) != 1)
        for (i = 0; i < (int)sizeof r; i++) r[i] = (unsigned char)rand();
    for (i = 0; i < (int)sizeof r; i++)
        snprintf(stats_token + 2 * i, 3, "%02x", r[i]);
}

int stats_reset_requested(const char *path, const char *stats_url)
{
    size_t n = strlen(stats_url);
    if (!stats_token[0] || strncmp(path, stats_url, n) || strncmp(path + n, "?reset=", 7))
        return 0;
    return strcmp(path + n + 7, stats_token) == 0;
}

void stats_reset(const char *dir)
{
    size_t i;
    for (i = 0; i < sizeof stats_persist / sizeof stats_persist[0]; i++)
        *stats_persist[i].val = 0;
    sslctx_tbl_reset_counters();
    stats_since = time(NULL);
    log_msg(LGG_NOTICE, "statistics reset from the statistics page");
    stats_save(dir);
}

void stats_save_periodic(const char *dir)
{
    if (stats_last_count == count || time(NULL) - stats_last_save < 600)
        return;
    stats_save(dir);
}

void stats_load(const char *dir)
{
    char path[PIXELSERV_MAX_PATH], line[96], name[32];
    FILE *fp;
    long v;
    size_t i;
    if (!dir) return;
    snprintf(path, sizeof path, "%s/stats.dat", dir);
    if (!(fp = fopen(path, "r"))) {
        stats_since = time(NULL);
        return;
    }
    if (!fgets(line, sizeof line, fp) || strncmp(line, "pixelserv-stats 1", 17)) {
        log_msg(LGG_WARNING, "ignoring %s: unknown format", path);
        fclose(fp);
        stats_since = time(NULL);
        return;
    }
    while (fgets(line, sizeof line, fp)) {
        if (sscanf(line, "%31s %ld", name, &v) != 2 || v < 0 || v > 2000000000L)
            continue;
        if (!strcmp(name, "since")) { stats_since = (time_t)v; continue; }
        for (i = 0; i < sizeof stats_persist / sizeof stats_persist[0]; i++)
            if (!strcmp(name, stats_persist[i].name)) { *stats_persist[i].val = (sig_atomic_t)v; break; }
    }
    fclose(fp);
    if (!stats_since) stats_since = time(NULL);
    stats_last_count = count;
    stats_last_save = time(NULL);
    log_msg(LGG_NOTICE, "restored statistics from %s (%d requests so far)", path, (int)count);
}

/* ---- HTML statistics page ------------------------------------------------ */

typedef struct { char *p; size_t len, cap; } sbuf;

static void sb_addf(sbuf *b, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (!b->p) return;
    for (;;) {
        va_start(ap, fmt);
        n = vsnprintf(b->p + b->len, b->cap - b->len, fmt, ap);
        va_end(ap);
        if (n < 0) { b->p[b->len] = '\0'; return; }
        if ((size_t)n < b->cap - b->len) { b->len += n; return; }
        size_t ncap = b->cap * 2 + n + 1;
        char *np = realloc(b->p, ncap);
        if (!np) { free(b->p); b->p = NULL; return; }
        b->p = np;
        b->cap = ncap;
    }
}

static void sb_addesc(sbuf *b, const char *s)
{
    for (; s && *s; s++) {
        switch (*s) {
            case '&': sb_addf(b, "&amp;"); break;
            case '<': sb_addf(b, "&lt;"); break;
            case '>': sb_addf(b, "&gt;"); break;
            case '"': sb_addf(b, "&quot;"); break;
            case '\'': sb_addf(b, "&#39;"); break;
            default: sb_addf(b, "%c", *s);
        }
    }
}

/* 1234567 -> "1,234,567" */
static const char *fmt_num(unsigned long v, char *out)
{
    char raw[32];
    int n = snprintf(raw, sizeof raw, "%lu", v), i, o = 0;
    for (i = 0; i < n; i++) {
        if (i > 0 && (n - i) % 3 == 0) out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = '\0';
    return out;
}

static double pct(double part, double whole) { return whole > 0 ? 100.0 * part / whole : 0.0; }

static void html_row(sbuf *b, const char *label, const char *code, unsigned long v, const char *unit)
{
    char num[40];
    sb_addf(b, "<tr><td class=l>%s <span class=c>%s</span></td><td class=v>%s%s</td></tr>",
            label, code, fmt_num(v, num), unit);
}

static const char stats_css[] =
  "<style>"
  ":root{color-scheme:light dark;--bg:#f5f6f8;--card:#fff;--fg:#1c2330;--mute:#66707f;--line:#e5e8ee;--ok:#1f9d55;--warn:#d98a00;--bad:#d64545;--info:#3b7ddd;--acc:#3b7ddd}"
  "@media (prefers-color-scheme:dark){:root:not([data-theme]){--bg:#12161c;--card:#1b212b;--fg:#e6eaf0;--mute:#8d97a6;--line:#2a3240;--ok:#3ecf7a;--warn:#f0b13a;--bad:#f26d6d;--info:#6aa5f5;--acc:#6aa5f5}}"
  ":root[data-theme=light]{color-scheme:light;--bg:#f5f6f8;--card:#fff;--fg:#1c2330;--mute:#66707f;--line:#e5e8ee;--ok:#1f9d55;--warn:#d98a00;--bad:#d64545;--info:#3b7ddd;--acc:#3b7ddd}"
  ":root[data-theme=dark]{color-scheme:dark;--bg:#12161c;--card:#1b212b;--fg:#e6eaf0;--mute:#8d97a6;--line:#2a3240;--ok:#3ecf7a;--warn:#f0b13a;--bad:#f26d6d;--info:#6aa5f5;--acc:#6aa5f5}"
  ":root[data-theme=mono]{color-scheme:light;--bg:#fff;--card:#fff;--fg:#000;--mute:#444;--line:#000;--ok:#000;--warn:#555;--bad:#000;--info:#999;--acc:#000}"
  "[data-theme=mono] .tile,[data-theme=mono] .card,[data-theme=mono] .note{border-color:#000}[data-theme=mono] .good,[data-theme=mono] .mid,[data-theme=mono] .poor{color:#000}[data-theme=mono] .poor{text-decoration:underline}[data-theme=mono] .bar{background:#fff;border:1px solid #000}[data-theme=mono] .s1{background:#000}[data-theme=mono] .s2{background:#777}[data-theme=mono] .s3{background:#bbb}[data-theme=mono] .s4{background:#e2e2e2;box-shadow:inset 0 0 0 1px #000}[data-theme=mono] .s5{background:repeating-linear-gradient(45deg,#000 0 2px,#fff 2px 5px)}[data-theme=mono] .leg b{border:1px solid #000}"
  "@media print{:root{--bg:#fff;--card:#fff;--fg:#000;--line:#000}}"
  "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}"
  "main{max-width:1100px;margin:0 auto;padding:16px}"
  "header{display:flex;flex-wrap:wrap;gap:8px 16px;align-items:baseline;justify-content:space-between;margin:8px 0 16px}"
  "h1{font-size:22px;margin:0}h2{font-size:13px;letter-spacing:.06em;text-transform:uppercase;color:var(--mute);margin:0 0 8px}"
  ".sub{color:var(--mute);font-size:13px;word-break:break-word}"
  ".tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;margin-bottom:16px}"
  ".tile,.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px}"
  ".tile .n{font-size:26px;font-weight:650;line-height:1.15;white-space:nowrap}.tile .n.up{font-size:22px;line-height:1.3}.tile .t{color:var(--mute);font-size:13px}.tile .d{color:var(--mute);font-size:12px;margin-top:2px}"
  ".good{color:var(--ok)}.mid{color:var(--warn)}.poor{color:var(--bad)}"
  ".note{border-left:4px solid var(--warn);background:var(--card);border-radius:8px;padding:10px 14px;margin:0 0 12px;border-top:1px solid var(--line);border-right:1px solid var(--line);border-bottom:1px solid var(--line)}"
  ".note.bad{border-left-color:var(--bad)}.note.ok{border-left-color:var(--ok)}"
  ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(310px,1fr));gap:12px}"
  "table{width:100%;border-collapse:collapse}td{padding:5px 0;border-top:1px solid var(--line)}tr:first-child td{border-top:0}"
  ".l{color:var(--fg)}.c{color:var(--mute);font:11px ui-monospace,Menlo,Consolas,monospace;margin-left:4px}.v{text-align:right;font-variant-numeric:tabular-nums;white-space:nowrap;padding-left:12px}"
  ".s1{background:var(--ok)}.s2{background:var(--info)}.s3{background:var(--warn)}.s4{background:var(--mute)}.s5{background:var(--bad)}"
  ".bar{display:flex;height:12px;border-radius:6px;overflow:hidden;background:var(--line);margin:4px 0 10px}.bar i{display:block;height:100%}"
  ".leg{display:flex;flex-wrap:wrap;gap:4px 14px;font-size:12px;color:var(--mute);margin-bottom:8px}.leg b{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:5px;vertical-align:-1px}"
  "button{font:inherit;font-size:12px;color:var(--fg);background:var(--card);border:1px solid var(--line);border-radius:6px;padding:2px 8px;cursor:pointer}button:hover{border-color:var(--bad)}footer{color:var(--mute);font-size:12px;margin:16px 0 8px;display:flex;flex-wrap:wrap;gap:8px 16px;justify-content:space-between}a{color:var(--acc)}"
  "</style>";

static const char stats_theme_head[] =
  "<script>try{var th=localStorage.getItem('pxtheme');if(th)document.documentElement.setAttribute('data-theme',th)}catch(e){}</script>";

static const char stats_js[] =
  "<script>(function(){var t,c=document.getElementById('ar');function s(){clearTimeout(t);if(c&&c.checked){location.hash='r';t=setTimeout(function(){location.reload()},15000)}else if(location.hash=='#r'){history.replaceState(null,'',location.pathname)}}"
  "if(c){c.checked=location.hash=='#r';c.addEventListener('change',s);s()}"
  "var m=document.getElementById('th');if(m){var v=document.documentElement.getAttribute('data-theme')||'';m.value=v;m.addEventListener('change',function(){"
  "try{if(m.value){localStorage.setItem('pxtheme',m.value);document.documentElement.setAttribute('data-theme',m.value)}else{localStorage.removeItem('pxtheme');document.documentElement.removeAttribute('data-theme')}}catch(e){}})}})()</script>";

char* get_stats_html(const char* version, const char* txt_url)
{
    sbuf b;
    char n1[40], n2[40], n3[40];
    unsigned long req = count, https_total, tls_total, slu_total;
    double ok_pct, hit_pct, t13_pct;
    unsigned int up = process_uptime();
    int sct = sslctx_tbl_get_cnt_total(), sch = sslctx_tbl_get_cnt_hit();
    int scm = sslctx_tbl_get_cnt_miss(), scp = sslctx_tbl_get_cnt_purge();
    int sst = sslctx_tbl_get_sess_cnt(), ssh = sslctx_tbl_get_sess_hit();
    int ssm = sslctx_tbl_get_sess_miss(), ssp = sslctx_tbl_get_sess_purge();
    const char *cls_ok;

    b.cap = 16384; b.len = 0; b.p = malloc(b.cap);
    if (!b.p) return NULL;
    b.p[0] = '\0';

    https_total = (unsigned long)slh + slm + sle + slc + slu;
    tls_total = (unsigned long)v13 + v12 + v10;
    slu_total = (unsigned long)uca + ucb + uce + ush;
    ok_pct = pct(slh, https_total);
    hit_pct = pct(sch, (double)sch + scm);
    t13_pct = pct(v13, tls_total);

    sb_addf(&b, "<!DOCTYPE html><html lang=en><head><meta charset=utf-8>"
                "<meta name=viewport content='width=device-width,initial-scale=1'>"
                "<link rel=icon href='/favicon.ico' type='image/x-icon'><title>pixelserv-tls statistics</title>%s%s</head><body><main>", stats_css, stats_theme_head);

    sb_addf(&b, "<header><h1>pixelserv-tls</h1><div class=sub>");
    sb_addesc(&b, version);
    sb_addf(&b, "</div></header>");

    /* key numbers */
    cls_ok = https_total < 20 ? "" : (ok_pct >= 80 ? " good" : (ok_pct >= 40 ? " mid" : " poor"));
    sb_addf(&b, "<section class=tiles>");
    if (up >= 86400)
        snprintf(n3, sizeof n3, "%dd %dh %dm", (int)(up / 86400), (int)(up % 86400) / 3600, (int)(up % 3600) / 60);
    else if (up >= 3600)
        snprintf(n3, sizeof n3, "%dh %dm", (int)(up / 3600), (int)(up % 3600) / 60);
    else
        snprintf(n3, sizeof n3, "%dm %ds", (int)(up / 60), (int)(up % 60));
    sb_addf(&b, "<div class=tile><div class='n up'>%s</div><div class=t>Uptime</div></div>", n3);
    {
        char since[32] = "";
        time_t t0 = stats_since;
        if (t0) strftime(since, sizeof since, "%d %b %Y", localtime(&t0));
        sb_addf(&b, "<div class=tile><div class=n>%s</div><div class=t>Requests%s%s</div><div class=d>%d ms average, %d ms slowest</div></div>",
                fmt_num(req, n1), since[0] ? " since " : "", since, (int)tav, (int)tmx);
    }
    sb_addf(&b, "<div class=tile><div class='n%s'>%.1f%%</div><div class=t>HTTPS accepted</div><div class=d>%s of %s attempts</div></div>",
            cls_ok, ok_pct, fmt_num(slh, n1), fmt_num(https_total, n2));
    sb_addf(&b, "<div class=tile><div class='n%s'>%.1f%%</div><div class=t>Cert cache hits</div><div class=d>%d certs stored</div></div>",
            (sch + scm) < 20 ? "" : (hit_pct >= 90 ? " good" : " mid"), hit_pct, sct);
    sb_addf(&b, "<div class=tile><div class=n>%.0f%%</div><div class=t>TLS 1.3</div><div class=d>of %s TLS connections</div></div>",
            t13_pct, fmt_num(tls_total, n1));
    sb_addf(&b, "<div class=tile><div class=n>%d / %d</div><div class=t>Threads busy / max</div><div class=d>%.2f requests per thread</div></div>",
            (int)kcc, (int)kmx, kvg);
    sb_addf(&b, "</section>");

    /* attention notes */
    if (https_total >= 50 && ok_pct < 50.0) {
        sb_addf(&b, "<div class='note bad'><b>Most HTTPS connections are being dropped.</b> ");
        if (slu_total > 0 && pct((double)uca + ucb + uce, slu_total) > 50.0)
            sb_addf(&b, "Clients report an unknown CA or certificate (%s times), so those devices or apps do not trust your CA. "
                        "Install the CA certificate (<a href='/ca.crt'>ca.crt</a>) on them, or ignore apps that pin their own certificates.",
                        fmt_num((unsigned long)uca + ucb + uce, n1));
        else
            sb_addf(&b, "Check the TLS handshake breakdown below.");
        sb_addf(&b, "</div>");
    }
    if (clt > 0)
        sb_addf(&b, "<div class=note><b>Requests were dropped because all service threads were busy</b> (%s times). "
                    "Consider a higher thread limit with the -T option.</div>", fmt_num(clt, n1));
    if (ers > 0)
        sb_addf(&b, "<div class=note><b>%s requests were dropped for an unknown reason.</b> Raise the log level with -l 3 or higher to investigate.</div>", fmt_num(ers, n1));

    sb_addf(&b, "<section class=grid>");

    /* HTTPS outcome */
    sb_addf(&b, "<div class=card><h2>HTTPS outcome</h2>");
    if (https_total > 0) {
        sb_addf(&b, "<div class=bar><i class=s1 style='width:%.2f%%'></i><i class=s2 style='width:%.2f%%'></i>"
                    "<i class=s3 style='width:%.2f%%'></i><i class=s4 style='width:%.2f%%'></i><i class=s5 style='width:%.2f%%'></i></div>",
                pct(slh, https_total), pct(slm, https_total), pct(sle, https_total), pct(slc, https_total), pct(slu, https_total));
        sb_addf(&b, "<div class=leg><span><b class=s1></b>accepted</span><span><b class=s2></b>no certificate yet</span>"
                    "<span><b class=s3></b>unusable cert</span><span><b class=s4></b>client left</span><span><b class=s5></b>handshake error</span></div>");
    }
    sb_addf(&b, "<table>");
    html_row(&b, "Accepted", "slh", slh, "");
    html_row(&b, "Rejected, no certificate yet", "slm", slm, "");
    html_row(&b, "Rejected, certificate unusable", "sle", sle, "");
    html_row(&b, "Client left before a request", "slc", slc, "");
    html_row(&b, "Dropped, other handshake errors", "slu", slu, "");
    sb_addf(&b, "</table></div>");

    /* handshake errors */
    sb_addf(&b, "<div class=card><h2>Why handshakes failed (reported by clients)</h2><table>");
    html_row(&b, "Unknown CA", "uca", uca, "");
    html_row(&b, "Bad certificate", "ucb", ucb, "");
    html_row(&b, "Unknown certificate", "uce", uce, "");
    html_row(&b, "Shut down after ServerHello", "ush", ush, "");
    sb_addf(&b, "</table></div>");

    /* TLS versions */
    sb_addf(&b, "<div class=card><h2>TLS versions</h2><table>");
    html_row(&b, "TLS 1.3", "v13", v13, "");
    html_row(&b, "TLS 1.2", "v12", v12, "");
    html_row(&b, "TLS 1.0", "v10", v10, "");
    html_row(&b, "TLS 1.3 early data (0-RTT)", "zrt", zrt, "");
    sb_addf(&b, "</table></div>");

    /* requests */
    sb_addf(&b, "<div class=card><h2>Requests</h2><table>");
    html_row(&b, "Total (HTTP and HTTPS)", "req", req, "");
    html_row(&b, "Average size", "avg", avg, " bytes");
    html_row(&b, "Largest", "rmx", rmx, " bytes");
    html_row(&b, "Average processing time", "tav", tav, " ms");
    html_row(&b, "Slowest processing time", "tmx", tmx, " ms");
    html_row(&b, "Log level (0 to 5)", "log", log_get_verb(), "");
    html_row(&b, "Max requests, one thread", "krq", krq, "");
    sb_addf(&b, "</table></div>");

    /* caches */
    sb_addf(&b, "<div class=card><h2>Caches</h2><table>");
    html_row(&b, "Certificates in cache", "sct", sct, "");
    html_row(&b, "Certificate reuses", "sch", sch, "");
    html_row(&b, "Certificate misses", "scm", scm, "");
    html_row(&b, "Certificate purges", "scp", scp, "");
    html_row(&b, "TLS session reuses", "ssh", sst + ssh, "");
    html_row(&b, "TLS session misses", "ssm", ssm, "");
    html_row(&b, "TLS session purges", "ssp", ssp, "");
    sb_addf(&b, "</table></div>");

    /* what was requested */
    sb_addf(&b, "<div class=card><h2>What was requested (GET)</h2><table>");
    html_row(&b, "Server-side scripting", "nfe", nfe, "");
    html_row(&b, "JavaScript", "txt", txt, "");
    html_row(&b, "GIF", "gif", gif, "");
    html_row(&b, "ICO", "ico", ico, "");
    html_row(&b, "JPG", "jpg", jpg, "");
    html_row(&b, "PNG", "png", png, "");
    html_row(&b, "SWF", "swf", swf, "");
    html_row(&b, "Unknown file extension", "ufe", ufe, "");
    sb_addf(&b, "</table></div>");

    /* methods and responses */
    sb_addf(&b, "<div class=card><h2>Methods and responses</h2><table>");
    html_row(&b, "HTTP 204 responses", "204", noc, "");
    html_row(&b, "OPTIONS", "opt", opt, "");
    html_row(&b, "POST", "pst", pst, "");
    html_row(&b, "HEAD (answered 501)", "hed", hed, "");
    html_row(&b, "Redirects", "rdr", rdr, "");
    html_row(&b, "Empty URL", "nou", nou, "");
    html_row(&b, "Malformed URL", "pth", pth, "");
    html_row(&b, "Unknown request (501)", "bad", bad, "");
    sb_addf(&b, "</table></div>");

    /* dropped */
    sb_addf(&b, "<div class=card><h2>Dropped requests</h2><table>");
    html_row(&b, "Client left, no request sent", "cls", cls, "");
    html_row(&b, "Client left before the response", "cly", cly, "");
    html_row(&b, "All service threads busy", "clt", clt, "");
    html_row(&b, "Unknown reason", "err", ers, "");
    sb_addf(&b, "</table></div>");

    sb_addf(&b, "</section><footer><span>Short codes are the counter names used by the text version");
    if (txt_url && *txt_url) {
        sb_addf(&b, ": <a href='");
        sb_addesc(&b, txt_url);
        sb_addf(&b, "'>text statistics</a>");
    }
    sb_addf(&b, "</span><span><label>Theme <select id=th><option value=''>Auto</option><option value=light>Light</option>"
                "<option value=dark>Dark</option><option value=mono>Black &amp; white</option></select></label> "
                "<label><input type=checkbox id=ar> refresh every 15 s</label> "
                "<button type=button id=rs>Reset statistics</button></span></footer></main>%s"
                "<script>document.getElementById('rs').onclick=function(){if(confirm('Reset all statistics to zero? This cannot be undone.'))location.href=location.pathname+'?reset=%s'}</script></body></html>\r\n",
                stats_js, stats_token);
    (void)scp; (void)ssp;
    return b.p;
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
void print_trace(int sig) {

  void *buf[32];
  char **strings;
  int size, i;
  log_msg(LGG_CRIT, "signal %d\n", sig);
  size = backtrace(buf, 32);
  strings = backtrace_symbols(buf, size);
  log_msg(LGG_CRIT, "backtrace:");
  for (i = 0; i < size; i++)
    log_msg(LGG_CRIT, "%d %s", buf[i], strings[i]);
  free(strings);
  exit(EXIT_FAILURE);
}
#endif
