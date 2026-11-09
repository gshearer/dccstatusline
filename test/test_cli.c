// dccstatusline — MIT
// test_cli: the real binary, spawned with pipes — the program never forks,
// but its test harness may

#define _GNU_SOURCE   // glibc declares memmem only under it

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "check.h"

extern char **environ;

static const char *dcc_bin;
static const char *ref_path;

static const char *const names[] =   // --themes' rows: the stock line, then every preset
  { "none", "catppuccin", "nord", "gruvbox", "tokyonight", "dracula",
    "onedark", "rosepine", "kanagawa", "solarized" };

static bool run_bin(const char *const *, const char *, size_t, char *, size_t,
                    size_t *);
static bool run_caught(const char *const *, const char *, size_t, char *, size_t,
                       size_t *, char *, size_t, size_t *);
static bool has(const char *, size_t, const char *);
static void expect_line(const char *, const char *, const char *);
static void expect_fallback(const char *, size_t, const char *);
static void test_styled(void);
static void test_cwd_shortened(void);
static void test_failure_modes(void);
static void test_print_config(void);
static void test_reference_is_default(void);
static void test_themes(void);
static void test_themes_overlay(void);
static size_t styles(const char *, size_t);
static void test_themes_palette(void);
static void test_flags(void);

// Every section the binary can fill without a repository under the cwd.
static const char full_payload[] =
  "{\"cwd\":\"/nonexistent/acme/platform\","
  "\"model\":{\"id\":\"claude-opus-5-5\",\"display_name\":\"Opus\"},"
  "\"effort\":{\"level\":\"high\"},"
  "\"context_window\":{\"total_input_tokens\":84213,"
  "\"context_window_size\":200000},"
  "\"rate_limits\":{\"five_hour\":{\"used_percentage\":23.5},"
  "\"seven_day\":{\"used_percentage\":41.0}}}";

// args are the command-line arguments after the program name, NULL-terminated;
// run_bin(NULL, ...) runs the bare binary.
static bool
run_bin(const char *const *args, const char *in, size_t inlen,
        char *out, size_t outcap, size_t *outlen)
{
  posix_spawn_file_actions_t fa;
  pid_t pid;
  int inpipe[2], outpipe[2], status;
  char *argv[12] = { (char *)dcc_bin };
  size_t used = 0, a;

  for(a = 0; args && args[a]; a++)
  {
    if(a + 2 >= sizeof argv / sizeof argv[0]) return(false);   // keep the NULL

    argv[a + 1] = (char *)args[a];
  }

  if(pipe(inpipe) != 0 || pipe(outpipe) != 0) return(false);

  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_adddup2(&fa, inpipe[0], 0);
  posix_spawn_file_actions_adddup2(&fa, outpipe[1], 1);
  posix_spawn_file_actions_addclose(&fa, inpipe[1]);
  posix_spawn_file_actions_addclose(&fa, outpipe[0]);

  if(posix_spawn(&pid, dcc_bin, &fa, NULL, argv, environ) != 0)
  {
    posix_spawn_file_actions_destroy(&fa);

    return(false);
  }

  posix_spawn_file_actions_destroy(&fa);
  close(inpipe[0]);
  close(outpipe[1]);

  {
    size_t done = 0;

    while(done < inlen)
    {
      ssize_t put = write(inpipe[1], in + done, inlen - done);

      if(put < 0)
      {
        if(errno == EINTR) continue;

        break;   // EPIPE when the child stopped reading: fine
      }

      done += (size_t)put;
    }
  }

  close(inpipe[1]);

  for(;;)
  {
    ssize_t got = read(outpipe[0], out + used, outcap - used);

    if(got < 0 && errno == EINTR) continue;

    if(got <= 0) break;

    used += (size_t)got;

    if(used == outcap) break;
  }

  close(outpipe[0]);
  *outlen = used;

  if(waitpid(pid, &status, 0) != pid) return(false);

  return(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// run_bin with the child's stderr caught in err. False when the capture failed,
// as well as when the run did: a read that failed is not a silent stderr.
static bool
run_caught(const char *const *args, const char *in, size_t inlen,
           char *out, size_t outcap, size_t *outlen,
           char *err, size_t errcap, size_t *errlen)
{
  const int saved = dup(2);
  FILE *const f = fopen("cli_stderr.txt", "w+");
  const bool captured = saved >= 0 && f && dup2(fileno(f), 2) == 2;
  const bool ran = captured && run_bin(args, in, inlen, out, outcap, outlen);
  bool readok = false;

  if(saved >= 0)   // the child has the file; CHECK's own reports must not
  {
    dup2(saved, 2);
    close(saved);
  }

  *errlen = 0;

  if(f)
  {
    rewind(f);
    *errlen = fread(err, 1, errcap, f);
    readok = !ferror(f);
    fclose(f);
  }

  return(captured && ran && readok);
}

static bool
has(const char *buf, size_t n, const char *needle)
{
  return(memmem(buf, n, needle, strlen(needle)) != NULL);
}

static void
expect_line(const char *payload, const char *want, const char *what)
{
  char out[8192];
  size_t n = 0;

  CHECK(run_bin(NULL, payload, strlen(payload), out, sizeof out, &n),
        "%s: exit 0", what);
  CHECK(n == strlen(want) && memcmp(out, want, n) == 0,
        "%s: got \"%.*s\" want \"%s\"", what, (int)n, out, want);
}

static void
expect_fallback(const char *payload, size_t len, const char *what)
{
  static const char prefix[] = "\x1b[0;2mdccstatusline v";
  char out[8192];
  size_t n = 0;

  CHECK(run_bin(NULL, payload, len, out, sizeof out, &n), "%s: exit 0", what);
  CHECK(n >= sizeof prefix - 1 && memcmp(out, prefix, sizeof prefix - 1) == 0,
        "%s: fallback line (got %zu bytes)", what, n);
  CHECK(n && out[n - 1] == '\n', "%s: newline-terminated", what);
}

static void
test_styled(void)
{
  static const char ini[] =
    "[statusline]\n"
    "sections = context plan_short\n"
    "separator = \" | \"\n"
    "[context]\n"
    "format = {used}/{ceiling} {pct}\n"
    "fg = default\n"
    "used_fg = default\n"
    "pct_fg = default\n"
    "[plan_short]\n"
    "format = {window} {pct}\n"
    "fg = default\n"
    "pct_fg = red\n";
  static const char payload[] =
    "{\"context_window\":{\"total_input_tokens\":50000,"
    "\"context_window_size\":200000},"
    "\"rate_limits\":{\"five_hour\":{\"used_percentage\":23.5}}}";
  FILE *f = fopen("cli_config.ini", "w");

  CHECK(f && fputs(ini, f) >= 0 && fclose(f) == 0, "fixture config written");
  CHECK(setenv("DCCSTATUSLINE_CONFIG", "cli_config.ini", 1) == 0, "env");

  expect_line(payload,
              "\x1b[0m50,000"
              "\x1b[0m/"
              "\x1b[0m200,000"
              "\x1b[0m "
              "\x1b[0m25%"
              "\x1b[0;90m | "
              "\x1b[0m5h"
              "\x1b[0m "
              "\x1b[0;31m24%"
              "\x1b[0m\n",
              "styled end-to-end");
}

// The whole cwd pipeline through the real binary: shrink, then a column cap.
static void
test_cwd_shortened(void)
{
  static const char ini[] =
    "[statusline]\n"
    "sections = cwd\n"
    "[cwd]\n"
    "style   = shrink\n"
    "depth   = 2\n"
    "max_len = 20\n"
    "fg = default\n"
    "path_fg = default\n";
  static const char payload[] =
    "{\"cwd\":\"/mnt/volumes/source/acme/platform/terraform/projects\"}";
  FILE *f = fopen("cli_cwd_config.ini", "w");

  CHECK(f && fputs(ini, f) >= 0 && fclose(f) == 0, "cwd fixture config written");
  CHECK(setenv("DCCSTATUSLINE_CONFIG", "cli_cwd_config.ini", 1) == 0, "env");

  expect_line(payload, "\x1b[0m\xe2\x80\xa6/terraform/projects\x1b[0m\n",
              "cwd shortened end-to-end");
}

static void
test_failure_modes(void)
{
  static char big[100 * 1024];

  expect_fallback("this is not json at all", 23, "garbage stdin");
  expect_fallback("", 0, "empty stdin");
  expect_fallback("{}", 2, "empty object");
  expect_fallback("[1,2,3]", 7, "non-object json");

  memset(big, 'x', sizeof big);
  expect_fallback(big, sizeof big, "oversized stdin");
}

// --print-config is what the plugin's configure skill reads as the dialect, so
// it must be examples/config to the byte, multibyte glyphs included.
static void
test_print_config(void)
{
  static char want[16 * 1024], got[16 * 1024];
  FILE *f = fopen(ref_path, "rb");
  size_t wn = 0, gn = 0;

  CHECK(f && (wn = fread(want, 1, sizeof want, f)) > 0 && feof(f),
        "reference config read whole");

  if(f) fclose(f);

  CHECK(run_bin((const char *const[]){ "--print-config", NULL }, "", 0, got,
                sizeof got, &gn),
        "--print-config: exit 0");
  CHECK(gn == wn && memcmp(got, want, wn) == 0,
        "--print-config: %zu bytes, reference has %zu", gn, wn);
}

// The reference promises it is the compiled-in defaults, and the skill tells
// Claude that defaults need no restating. Hold it to that through the binary.
static void
test_reference_is_default(void)
{
  char dflt[8192], ref[8192];
  size_t dn = 0, rn = 0;
  FILE *f = fopen("cli_empty.ini", "w");

  CHECK(f && fclose(f) == 0, "empty config written");
  CHECK(setenv("DCCSTATUSLINE_CONFIG", "cli_empty.ini", 1) == 0, "env");
  CHECK(run_bin(NULL, full_payload, sizeof full_payload - 1, dflt, sizeof dflt,
                &dn), "defaults: exit 0");
  CHECK(setenv("DCCSTATUSLINE_CONFIG", ref_path, 1) == 0, "env");
  CHECK(run_bin(NULL, full_payload, sizeof full_payload - 1, ref, sizeof ref, &rn),
        "reference: exit 0");
  CHECK(dn > 0 && dn == rn && memcmp(dflt, ref, dn) == 0,
        "reference renders as the defaults: \"%.*s\" vs \"%.*s\"",
        (int)rn, ref, (int)dn, dflt);
}

// --themes is what the plugin's theme skill reads the theme names from, one
// per line, name first, the stock line's none ahead of the presets.
static void
test_themes(void)
{
  char out[16 * 1024];
  size_t n = 0, i, pos = 0;

  CHECK(run_bin((const char *const[]){ "--themes", NULL }, "", 0, out, sizeof out, &n),
        "--themes: exit 0");

  for(i = 0; i < sizeof names / sizeof names[0]; i++)
  {
    const size_t len = strlen(names[i]);
    const char *const eol = memchr(out + pos, '\n', n - pos);

    CHECK(eol && n - pos > len && memcmp(out + pos, names[i], len) == 0
          && out[pos + len] == ' ',
          "--themes: line %zu is %s", i + 1, names[i]);

    if(!eol) return;

    pos = (size_t)(eol - out) + 1;
  }

  CHECK(pos == n, "--themes: one line per theme, nothing after");
}

// An overlay is the plugin picker's preview of its options, laid over every
// row, the stock line's too, as keys below a theme line are.
static void
test_themes_overlay(void)
{
  static const char overlay[] =
    "[statusline]\nseparator = \" \xe2\x80\xa2 \"\n"   // " • "
    "[git]\nformat = {branch}\n"                         // glyphs off
    "[plan_short]\nformat = {window} {pct} {resets}\n"
    "[plan_long]\nformat = {window} {pct} {resets}\n";
  static const char bullet[] = " \xe2\x80\xa2 ";
  static const char *const glyph[] = { "\xe2\x8e\x87", "\xe2\x86\xbb" };   // ⎇ ↻
  char out[16 * 1024];
  size_t n = 0, pos = 0, lines = 0;

  CHECK(run_bin((const char *const[]){ "--themes", overlay, NULL }, "", 0, out,
                sizeof out, &n),
        "--themes overlay: exit 0");

  while(pos < n)
  {
    const char *const eol = memchr(out + pos, '\n', n - pos);
    const size_t end = eol ? (size_t)(eol - out) : n;
    size_t seps = 0, glyphs = 0, j, g;

    for(j = pos; j < end; j++)
    {
      seps += end - j >= sizeof bullet - 1
              && memcmp(out + j, bullet, sizeof bullet - 1) == 0;

      for(g = 0; g < sizeof glyph / sizeof glyph[0]; g++)
        glyphs += end - j >= 3 && memcmp(out + j, glyph[g], 3) == 0;
    }

    CHECK(seps == 5 && glyphs == 0,
          "--themes overlay: line %zu has %zu bullets, %zu glyphs",
          lines + 1, seps, glyphs);

    lines++;
    pos = end + 1;
  }

  CHECK(lines == sizeof names / sizeof names[0],
        "--themes overlay: %zu lines", lines);
}

// The distinct styles one line paints in, the closing reset aside.
static size_t
styles(const char *line, size_t n)
{
  const char *seen[16];
  size_t len[16], count = 0, i, j, k;

  for(i = 0; i + 1 < n; i++)
  {
    if(line[i] != '\x1b' || line[i + 1] != '[') continue;

    for(j = i + 2; j < n && line[j] != 'm'; j++)
      ;

    if(j - i - 2 == 1 && line[i + 2] == '0') continue;

    for(k = 0; k < count; k++)
      if(len[k] == j - i - 2 && memcmp(seen[k], line + i + 2, len[k]) == 0) break;

    if(k == count && count < sizeof seen / sizeof seen[0])
    {
      seen[count] = line + i + 2;
      len[count++] = j - i - 2;
    }

    i = j;
  }

  return(count);
}

// A palette overlay lays every preset again in it: the muted color and one,
// two or three of the preset's own. The stock line takes no palette, so its
// row reads the same under each.
static void
test_themes_palette(void)
{
  static const struct
  {
    const char *overlay;
    size_t styles;
  } palettes[] =
  {
    { "[statusline]\npalette = monochrome\n", 2 },
    { "[statusline]\npalette = duotone\n",    3 },
    { "[statusline]\npalette = tritone\n",    4 },
  };
  char out[16 * 1024], err[256], stock[2048];
  size_t p, n = 0, en = 0, sn = 0;

  for(p = 0; p < sizeof palettes / sizeof palettes[0]; p++)
  {
    const char *const palette = palettes[p].overlay + sizeof "[statusline]\npalette = " - 1;
    const int pl = (int)strcspn(palette, "\n");
    size_t pos = 0, lines = 0;

    CHECK(run_caught((const char *const[]){ "--themes", palettes[p].overlay, NULL },
                     "", 0, out, sizeof out, &n, err, sizeof err, &en),
          "--themes %.*s: exit 0, stderr captured", pl, palette);
    CHECK(!en, "--themes %.*s: stderr \"%.*s\"", pl, palette, (int)en, err);

    for(; pos < n && lines < sizeof names / sizeof names[0]; lines++)
    {
      const char *const eol = memchr(out + pos, '\n', n - pos);
      const size_t end = eol ? (size_t)(eol - out) : n;
      const size_t want = palettes[p].styles;

      if(lines == 0 && p == 0 && end - pos <= sizeof stock)
        memcpy(stock, out + pos, sn = end - pos);

      if(lines == 0)
        CHECK(sn && end - pos == sn && memcmp(out + pos, stock, sn) == 0,
              "--themes %.*s: the stock line as under the first palette", pl, palette);

      else
        CHECK(styles(out + pos, end - pos) == want,
              "--themes %.*s: %s paints %zu styles, want %zu", pl, palette, names[lines],
              styles(out + pos, end - pos), want);

      pos = end + 1;
    }

    CHECK(lines == sizeof names / sizeof names[0] && pos == n, "--themes %.*s: %zu lines",
          pl, palette, lines);
  }
}

// --theme, --palette, --profile and --glyphs stand in for the config keys of
// the same name: the theme, in the palette --palette names, as if it were the
// config's first line, the profile and its glyphs over whatever the config set.
// A value the binary does not know costs a note, never the line. The payload
// sits in a repository, so the ⎇ can show.
static void
test_flags(void)
{
  static const char repo_payload[] =
    "{\"cwd\":\"cli_repo\","
    "\"model\":{\"id\":\"claude-opus-5-5\",\"display_name\":\"Opus\"},"
    "\"effort\":{\"level\":\"high\"},"
    "\"context_window\":{\"total_input_tokens\":84213,"
    "\"context_window_size\":200000}}";
  // The same with a far-off reset, so the detailed line has a reset to mark.
  static const char repo_payload_resets[] =
    "{\"cwd\":\"cli_repo\","
    "\"model\":{\"id\":\"claude-opus-5-5\",\"display_name\":\"Opus\"},"
    "\"effort\":{\"level\":\"high\"},"
    "\"rate_limits\":{\"five_hour\":{\"used_percentage\":23.5,"
    "\"resets_at\":4102444800}}}";
  static const char flagged[] =
    "[statusline]\n"
    "theme = dracula\n"   // yields to --theme, without a word
    "palette = monochrome\n"   // and this to --palette
    "sections = context\n"
    "[cwd]\nfg = red\n"
    "[context]\nformat = {pct}\n";
  static const char keyed[] =
    "[statusline]\n"
    "theme = nord\n"
    "palette = duotone\n"
    "profile = basic\n"
    "glyphs = on\n"
    "sections = context\n"
    "[cwd]\nfg = red\n"
    "[context]\nformat = {pct}\n";
  static const struct
  {
    const char *args[3];
    const char *note;
  } fallbacks[] =
  {
    { { "--theme", "nosuch", NULL },   "dccstatusline: --theme: unknown theme: nosuch\n" },
    { { "--profile", "sparse", NULL }, "dccstatusline: --profile: bad profile: sparse\n" },
    { { "--glyphs", "yes", NULL },     "dccstatusline: --glyphs: bad glyphs: yes\n" },
    { { "--palette", "rainbow", NULL }, "dccstatusline: --palette: bad palette: rainbow\n" },
    { { "--theme", NULL },             "dccstatusline: ignored argument: --theme\n" },
  };
  char a[8192], b[8192], base[8192], got[8192], err[256];
  size_t an = 0, bn = 0, basen = 0, gn = 0, en = 0, i;
  FILE *f;

  CHECK((mkdir("cli_repo", 0755) == 0 || errno == EEXIST)
        && (mkdir("cli_repo/.git", 0755) == 0 || errno == EEXIST), "repo made");
  f = fopen("cli_repo/.git/HEAD", "w");
  CHECK(f && fputs("ref: refs/heads/main\n", f) >= 0 && fclose(f) == 0, "HEAD written");
  f = fopen("cli_flagged.ini", "w");
  CHECK(f && fputs(flagged, f) >= 0 && fclose(f) == 0, "flagged config written");
  f = fopen("cli_keyed.ini", "w");
  CHECK(f && fputs(keyed, f) >= 0 && fclose(f) == 0, "keyed config written");

  CHECK(setenv("DCCSTATUSLINE_CONFIG", "cli_flagged.ini", 1) == 0, "env");
  CHECK(run_caught((const char *const[]){ "--theme", "nord", "--palette", "duotone",
                                          "--profile", "basic", "--glyphs", "on", NULL },
                   repo_payload, sizeof repo_payload - 1, a, sizeof a, &an,
                   err, sizeof err, &en), "flags: exit 0, stderr captured");
  CHECK(!en, "flags: stderr \"%.*s\"", (int)en, err);

  CHECK(setenv("DCCSTATUSLINE_CONFIG", "cli_keyed.ini", 1) == 0, "env");
  CHECK(run_caught(NULL, repo_payload, sizeof repo_payload - 1, b, sizeof b, &bn,
                   err, sizeof err, &en), "keys: exit 0, stderr captured");
  CHECK(!en, "keys: stderr \"%.*s\"", (int)en, err);

  CHECK(an > 0 && an == bn && memcmp(a, b, an) == 0,
        "flags render as the keys: \"%.*s\" vs \"%.*s\"",
        (int)an, a, (int)bn, b);
  CHECK(has(a, an, "129;161;193m") && !has(a, an, "94;129;172m") && has(a, an, "\x1b[0;31m")
        && has(a, an, "\xe2\x8e\x87 ") && !has(a, an, "84,213"),
        "duotone nord under the red cwd, the basic line with its glyph: \"%.*s\"",
        (int)an, a);

  CHECK(setenv("DCCSTATUSLINE_CONFIG", "cli_empty.ini", 1) == 0, "env");
  CHECK(run_bin(NULL, repo_payload, sizeof repo_payload - 1, base, sizeof base,
                &basen), "no flags: exit 0");

  // The Nerd Font glyphs in place of both, and only those.
  CHECK(run_caught((const char *const[]){ "--profile", "detailed",
                                          "--glyphs", "nerdfont", NULL },
                   repo_payload_resets, sizeof repo_payload_resets - 1,
                   got, sizeof got, &gn, err, sizeof err, &en),
        "--glyphs nerdfont: exit 0, stderr captured");
  CHECK(!en, "--glyphs nerdfont: stderr \"%.*s\"", (int)en, err);
  CHECK(has(got, gn, "\xee\x82\xa0 ") && has(got, gn, "\xef\x80\xa1 ")
        && !has(got, gn, "\xe2\x8e\x87") && !has(got, gn, "\xe2\x86\xbb"),
        "--glyphs nerdfont: U+E0A0 and U+F021 alone: \"%.*s\"", (int)gn, got);
  CHECK(has(got, gn, "\xee\x82\xb6") && has(got, gn, "\xee\x82\xb4")
        && has(got, gn, "Opus 5.5") && has(got, gn, "high")
        && !has(got, gn, "\xc2\xb7"),
        "--glyphs nerdfont: the model pill, the effort, no dot: \"%.*s\"", (int)gn, got);

  // Without a profile, the glyphs are the formats' own: the flag changes nothing.
  CHECK(run_caught((const char *const[]){ "--glyphs", "on", NULL },
                   repo_payload, sizeof repo_payload - 1, got, sizeof got, &gn,
                   err, sizeof err, &en), "--glyphs alone: exit 0, stderr captured");
  CHECK(gn == basen && memcmp(got, base, gn) == 0 && !en,
        "--glyphs alone renders as no flags: \"%.*s\", stderr \"%.*s\"",
        (int)gn, got, (int)en, err);
  CHECK(run_caught((const char *const[]){ "--glyphs", "nerdfont", NULL },
                   repo_payload, sizeof repo_payload - 1, got, sizeof got, &gn,
                   err, sizeof err, &en), "--glyphs nerdfont alone: exit 0, stderr captured");
  CHECK(gn == basen && memcmp(got, base, gn) == 0 && !en,
        "--glyphs nerdfont alone renders as no flags: \"%.*s\", stderr \"%.*s\"",
        (int)gn, got, (int)en, err);

  for(i = 0; i < sizeof fallbacks / sizeof fallbacks[0]; i++)
  {
    const char *const what = fallbacks[i].note + sizeof "dccstatusline: " - 1;

    CHECK(run_caught(fallbacks[i].args, repo_payload, sizeof repo_payload - 1,
                     got, sizeof got, &gn, err, sizeof err, &en),
          "%s: exit 0, stderr captured", what);
    CHECK(gn == basen && memcmp(got, base, gn) == 0,
          "%s: renders as no flags: \"%.*s\"", what, (int)gn, got);
    CHECK(en == strlen(fallbacks[i].note) && memcmp(err, fallbacks[i].note, en) == 0,
          "%s: stderr \"%.*s\"", what, (int)en, err);
  }
}

int
main(int argc, char **argv)
{
  if(argc != 3)
  {
    fprintf(stderr, "usage: %s <path-to-dccstatusline> <reference-config>\n",
            argv[0]);
    return(1);
  }

  dcc_bin = argv[1];
  ref_path = argv[2];
  signal(SIGPIPE, SIG_IGN);   // the oversized-stdin case writes into a wall

  test_styled();
  test_cwd_shortened();
  test_failure_modes();
  test_print_config();
  test_reference_is_default();
  test_themes();
  test_themes_overlay();
  test_themes_palette();
  test_flags();

  return(check_failures ? 1 : 0);
}
