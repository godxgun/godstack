# Peak symbols

Documentation: https://godxgun.github.io/godstack/symbols/peak/
Description: Types, macros, and functions declared by Peak.h.


# Peak symbols

For setup and API workflows, read the [Peak guide](/godstack/docs/peak/). This vault lists individual declarations; it is not a tutorial.

<!-- BEGIN GENERATED API -->

## API reference

Generated from `Peak.h` by `tools/generate_docs.py`. Edit the header comments, not this section.

Declarations from all platform branches are listed; this is not a preprocessed API.

### Types

#### PEAK_HANDLE {#peak_handle}

Source: `Peak.h:208`

```c
typedef void *PEAK_HANDLE;
```

#### PEAK_HANDLE {#peak_handle-1}

Source: `Peak.h:210`

```c
typedef int PEAK_HANDLE;
```

#### PeakCtx {#peakctx}

Source: `Peak.h:222`

```c
typedef struct PeakCtx PeakCtx;
```

█     █
█
█ ███ █ ███
█ █ █ █  █
█ █ █ █  █
█ █ █ █  ██

#### PeakKeyMod {#peakkeymod}

Source: `Peak.h:235`

```c
typedef enum {
    PEAK_KEYMOD_NONE  = 0,
    PEAK_KEYMOD_SHIFT = 1 << 0,
    PEAK_KEYMOD_CTRL  = 1 << 1,
    PEAK_KEYMOD_ALT   = 1 << 2,
    PEAK_KEYMOD_CAPS  = 1 << 3,
    PEAK_KEYMOD_SUPER = 1 << 4,
} PeakKeyMod;
```

█
███ █ █ ███ ███ ███
███ █ █ ███ █ █  █
█   █ █ █   █ █  █
███  █  ███ █ █  ██

#### PeakKeyCode {#peakkeycode}

Source: `Peak.h:244`

```c
typedef enum {
    PEAK_KEY_UNKNOWN = 0,
    PEAK_KEY_UP,
    PEAK_KEY_DOWN,
    PEAK_KEY_LEFT,
    PEAK_KEY_RIGHT,
    PEAK_KEY_SPACE,
    PEAK_KEY_ESCAPE,
    PEAK_KEY_ENTER,
    PEAK_KEY_BACKSPACE,
    PEAK_KEY_TAB,
    PEAK_KEY_DELETE,
    PEAK_KEY_INSERT,
    PEAK_KEY_HOME,
    PEAK_KEY_END,
    PEAK_KEY_PAGEUP,
    PEAK_KEY_PAGEDOWN,

    /* F Keys */
    PEAK_KEY_F1, PEAK_KEY_F2, PEAK_KEY_F3, PEAK_KEY_F4, PEAK_KEY_F5, PEAK_KEY_F6,
    PEAK_KEY_F7, PEAK_KEY_F8, PEAK_KEY_F9, PEAK_KEY_F10, PEAK_KEY_F11, PEAK_KEY_F12,

    /* Numbers */
    PEAK_KEY_0 = '0', // 48
    PEAK_KEY_1 = '1', // 49
    PEAK_KEY_2 = '2', // 50
    PEAK_KEY_3 = '3', // 51
    PEAK_KEY_4 = '4', // 52
    PEAK_KEY_5 = '5', // 53
    PEAK_KEY_6 = '6', // 54
    PEAK_KEY_7 = '7', // 55
    PEAK_KEY_8 = '8', // 56
    PEAK_KEY_9 = '9', // 57

    /* [58;64] */

    /* Letters */
    PEAK_KEY_A = 'A', // 65
    PEAK_KEY_B = 'B', // 66
    PEAK_KEY_C = 'C', // 67
    PEAK_KEY_D = 'D', // 68
    PEAK_KEY_E = 'E', // 69
    PEAK_KEY_F = 'F', // 70
    PEAK_KEY_G = 'G', // 71
    PEAK_KEY_H = 'H', // 72
    PEAK_KEY_I = 'I', // 73
    PEAK_KEY_J = 'J', // 74
    PEAK_KEY_K = 'K', // 75
    PEAK_KEY_L = 'L', // 76
    PEAK_KEY_M = 'M', // 77
    PEAK_KEY_N = 'N', // 78
    PEAK_KEY_O = 'O', // 79
    PEAK_KEY_P = 'P', // 80
    PEAK_KEY_Q = 'Q', // 81
    PEAK_KEY_R = 'R', // 82
    PEAK_KEY_S = 'S', // 83
    PEAK_KEY_T = 'T', // 84
    PEAK_KEY_U = 'U', // 85
    PEAK_KEY_V = 'V', // 86
    PEAK_KEY_W = 'W', // 87
    PEAK_KEY_X = 'X', // 88
    PEAK_KEY_Y = 'Y', // 89
    PEAK_KEY_Z = 'Z', // 90
} PeakKeyCode;
```

#### PeakClip {#peakclip}

Source: `Peak.h:309`

```c
typedef enum {
    PEAK_CLIP_CLIPBOARD = 0, /* Ctrl-C/V, OSC 52 */
    PEAK_CLIP_PRIMARY,       /* mouse select, middle paste */
} PeakClip;
```

#### PeakEventType {#peakeventtype}

Source: `Peak.h:314`

```c
typedef enum {
    PEAK_EVENT_NONE = 0,
    PEAK_EVENT_KEY_DOWN,
    PEAK_EVENT_KEY_UP,
    PEAK_EVENT_WINDOW_CLOSE,
    PEAK_EVENT_WINDOW_RESIZE,
    PEAK_EVENT_POINTER,
    PEAK_EVENT_POINTER_CONNECTED,
    PEAK_EVENT_POINTER_DISCONNECTED,
    PEAK_EVENT_CLIP,
    PEAK_EVENT_TEXT,
    PEAK_EVENT_DROP,
    PEAK_EVENT_FOCUS,
    PEAK_EVENT_EXPOSE,
    PEAK_EVENT_LAST
} PeakEventType;
```

#### PeakPointerState {#peakpointerstate}

Source: `Peak.h:331`

```c
typedef enum {
    PEAK_POINTER_MOVED = 0,
    PEAK_POINTER_PRESSED,
    PEAK_POINTER_RELEASED
} PeakPointerState;
```

#### PeakPointerType {#peakpointertype}

Source: `Peak.h:337`

```c
typedef enum {
    PEAK_POINTER_LEFT = 0,
    PEAK_POINTER_RIGHT,
    PEAK_POINTER_MIDDLE,
    PEAK_POINTER_TOUCH,
    PEAK_POINTER_WHEEL_UP,
    PEAK_POINTER_WHEEL_DOWN,
} PeakPointerType;
```

#### PeakEvent {#peakevent}

Source: `Peak.h:346`

```c
typedef struct {
    PeakEventType type;
    union {
        struct { PeakKeyCode key; PeakKeyMod mod; uint32_t code; } key;
        struct { uint32_t width, height; } resize;
        struct { PeakPointerState state; PeakPointerType type; float x, y; PeakKeyMod mod; } pointer;
        struct { PeakClip which; size_t n; } clip;
        struct { size_t n; } text;
        struct { size_t n; } drop;
        struct { int on; } focus; /* 1 gained, 0 lost */
    };
} PeakEvent;
```

#### PeakWindowFlags {#peakwindowflags}

Source: `Peak.h:366`

```c
enum PeakWindowFlags {
    PEAK_WINDOW_TRANSPARENT = 1 << 0,
    PEAK_WINDOW_FULLSCREEN  = 1 << 1
};
```

█       █
█
█ █ █ █ ███ ███ ███ █ █ █
█ █ █ █ █ █ █ █ █ █ █ █ █
█ █ █ █ █ █ █ █ █ █ █ █ █
█ █  █ █ █ ███ ███  █ █

#### PeakWindowInternal {#peakwindowinternal}

Source: `Peak.h:371`

```c
typedef struct peak_window_internal_t {
    void *w;
} PeakWindowInternal;
```

#### tick {#tick}

Source: `Peak.h:375`

```c
typedef struct PeakWindow {
    PeakCtx *ctx;
    uint64_t generation; /* private lifetime token; do not modify */
    PeakWindowInternal internal;
    int (*tick)(struct PeakWindow *win, void *userdata);
    void *userdata;
    uint32_t *buffer;
    uint32_t width;
    uint32_t height;
    uint32_t bufsize;
    uint16_t *audio;
    int running;
} PeakWindow;
```

#### PeakIOCode {#peakiocode}

Source: `Peak.h:511`

```c
typedef enum {
    PEAK_IO_OK, PEAK_IO_INVALID, PEAK_IO_LIMIT,
    PEAK_IO_NOT_REGULAR, PEAK_IO_UNSUPPORTED, PEAK_IO_SYSTEM
} PeakIOCode;
```

Safe I/O (independent of peak_init): Linux openat2 only; other hosts and
kernels lacking the required resolution flags return PEAK_IO_UNSUPPORTED.
No fallback to ordinary path-based I/O. Native Windows/macOS qualification
is pending. Legacy peak_file_* / peak_filesystem_* retain their semantics.

Ownership: initialize caller storage with *_INIT before first use. Successful
opens/begin fill an empty output; failures leave it empty. These are owning,
non-copyable values, not borrowed descriptors. Do not edit their fields,
close their native handles yourself, or use them concurrently with close.
Close consumes a handle even on error; commit/abort consume both stage
handles on every outcome for a live stage. Separate objects may be used
concurrently; operations on a stage must be externally serialized.

Paths: root_open takes an absolute path ("/" is allowed); all *_at/open/begin
paths are nonempty normalized relative paths. No empty, dot, dot-dot, or
symlink components, trailing slash, or absolute relative paths are allowed.
Limits: 4095 path bytes, 255 bytes/component, 128 components, byte limits
representable as a native nonnegative file offset. No heap allocations.
Every relative lookup is kernel-resolved beneath its owning directory
handle with RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS.
Mount points/hard links are not rejected: these are not authorization,
provenance, or immutable-snapshot primitives. Files are verified regular.

Handles pin directory/file objects, not pathnames. Root or parent rename/
replacement does not redirect an existing handle. A retained directory may
be moved outside the original tree; later operations still target that
object, not the current object at its former path. Reads are bounded but
not snapshots: uncooperative in-place writers can change bytes/size during
a read. size is only the initial observed file size; hash/validation policy
and coordination with writers belong to the caller.

Staging: destination's parent MUST be cooperatively/exclusively controlled
against uncooperative directory writers (including same-user writers).
A named stage is not safe against such writers swapping/unlinking its name
between creation, rename, and cleanup. temporary_leaf is caller-selected,
single-component and distinct from destination's leaf; O_EXCL never reuses
an existing entry. Use an appropriately unpredictable/reserved name and
authorize/serialize the destination yourself. The stage is visible by its
temporary name (mode 0600), so callers must exclude stages from asset lookup.
Commit atomically replaces a non-directory destination entry via renameat;
it does not follow a destination symlink. No no-clobber/version checks,
metadata preservation, backups, or multi-file/bundle transaction policy.
Previous-file preservation on failure applies only within the cooperative
contract; external races are not covered. All stages are same-parent.

Bounded read is positional, reports partial progress on failure and EOF via
read_bytes < bytes. It rejects a currently observed size above limit, but
cannot detect every transient growth or mutation. Stage writes are sequential
and bounded in aggregate; any invalid/failed write poisons publication.
Successful commit without durability only establishes publication. With
FILE_AND_DIRECTORY, fsync(stage) precedes rename and fsync(parent) follows;
durable=1 means both succeeded, subject to filesystem/hardware guarantees;
durable=0 means durability was not established, not that bytes were lost.
This does not fsync ancestor directories or establish durability of newly
created directory hierarchies. Byte bounds are not elapsed-time deadlines.
A post-rename error NEVER means unpublished: inspect published separately.
published is 0 (not attempted/not published), 1 (rename succeeded), or -1
(rename failed ambiguously; e.g. a lost remote FS reply). Destination type/
nonempty-directory errors are definite failures within the cooperative
contract; other rename errors are conservatively reported as uncertain.
Do not interpret -1 as false or true; recovery must inspect/coordinate the
destination. On uncertain publication the temporary name is not removed.
Cleanup reports unlink/close failures independently; a failed pre-publication
cleanup may leave the temporary name for caller-managed recovery. No rollback
after publication. Cleanup success does not establish crash-durable removal
of a temporary name. Invalid finish arguments leave the stage untouched.

#### PeakIOResult {#peakioresult}

Source: `Peak.h:515`

```c
typedef struct { PeakIOCode code; int os_error; } PeakIOResult;
```

#### PeakIODurability {#peakiodurability}

Source: `Peak.h:516`

```c
typedef enum {
    PEAK_IO_DURABILITY_NONE,
    PEAK_IO_DURABILITY_FILE_AND_DIRECTORY
} PeakIODurability;
```

#### PeakIOPublishResult {#peakiopublishresult}

Source: `Peak.h:520`

```c
typedef struct {
    PeakIOResult operation;
    int published;
    int durable;
    PeakIOResult cleanup;
} PeakIOPublishResult;
```

#### PeakIORoot {#peakioroot}

Source: `Peak.h:526`

```c
typedef struct { PEAK_HANDLE handle; } PeakIORoot;
```

#### PeakIOFile {#peakiofile}

Source: `Peak.h:527`

```c
typedef struct {
    PEAK_HANDLE handle;
    uint64_t size, limit;
} PeakIOFile;
```

#### PeakIOStage {#peakiostage}

Source: `Peak.h:531`

```c
typedef struct {
    PEAK_HANDLE handle, parent;
    uint64_t written, limit;
    int failed;
    char destination[256], temporary[256];
} PeakIOStage;
```

#### PeakProc {#peakproc}

Source: `Peak.h:583`

```c
typedef struct PeakProc {
    PEAK_HANDLE fd; /* PEAK_HANDLE_INVALID if closed / failed */
    int pid;        /* 0 if none */
} PeakProc;
```

███ ███ ███ ███
█ █ █   █ █ █
█ █ █   █ █ █
███ █   ███ ███
█
█

#### PeakLogLevel {#peakloglevel}

Source: `Peak.h:703`

```c
typedef enum PeakLogLevel {
    P_LOG_LEVEL_FATAL = 0,
    P_LOG_LEVEL_ERROR,
    P_LOG_LEVEL_WARN,
    P_LOG_LEVEL_INFO,
    P_LOG_LEVEL_DEBUG,
    P_LOG_LEVEL_TRACE,
    P_COUNT_LOG_LEVEL
} PeakLogLevel;
```

█     █
█     █
███ ███ ███ █ █ ███
█ █ ███ █ █ █ █ █ █
█ █ █   █ █ █ █ █ █
███ ███ ███ ███ ███
█
███

#### PeakMemoryDomain {#peakmemorydomain}

Source: `Peak.h:766`

```c
typedef enum {
	PEAK_MEMORY_NON_DRIVER = 0,
	PEAK_MEMORY_DRIVER,
	PEAK_MEMORY_DOMAIN_COUNT
} PeakMemoryDomain;
```

Direct instrumented backing calls only, not libc/DSO or Vulkan internals.
Single-threaded diagnostics; counters persist for the process lifetime.
Each domain has independent fixed live-pointer capacity (512 by default).
Exhaustion leaves request counters valid, but live/release evidence incomplete;
live_blocks/live_bytes/peak_bytes are not exact when tracking is incomplete.
Unknown-pointer operations invalidate BOTH domains' accounting: original
ownership is unknowable. Request counts remain useful but uncertified.
Overflow saturates counters and clears accounting_complete. No reset API.
PEAK_DEBUG_MEMORY_TRACE (default 0) enables per-request output.

#### PeakMemoryStats {#peakmemorystats}

Source: `Peak.h:772`

```c
typedef struct {
	uint64_t allocation_requests; /* successful malloc/calloc/nonzero realloc */
	uint64_t failed_requests;
	uint64_t realloc_requests; /* includes NULL, failure and zero-size release */
	uint64_t released_blocks;
	uint64_t live_blocks;
	uint64_t live_bytes;
	uint64_t peak_bytes; /* requested live bytes, not allocator resident bytes */
	uint64_t domain_errors;
	uint64_t unknown_operations;
	int tracking_complete;
	int accounting_complete;
} PeakMemoryStats;
```

### Macros

#### peak_aligned_alloc {#peak_aligned_alloc}

Source: `Peak.h:555`

```c
#define peak_aligned_alloc(size, alignment) peak_aligned_alloc_impl((size), (alignment))
```

#### peak_aligned_free {#peak_aligned_free}

Source: `Peak.h:558`

```c
#   define peak_aligned_free(p) do { if (p) _aligned_free(p); } while (0)
```

#### peak_aligned_free {#peak_aligned_free-1}

Source: `Peak.h:560`

```c
#   define peak_aligned_free(p) do { if (p) free(p); } while (0)
```

### Functions

#### peak_init {#peak_init}

Source: `Peak.h:226`

```c
PEAK PeakCtx   *peak_init(void);
```

One active runtime. Initialization is lazy with respect to the display.
Quit(NULL), repeated/stale quit are harmless. A context expires at quit.

#### peak_quit {#peak_quit}

Source: `Peak.h:227`

```c
PEAK void       peak_quit(PeakCtx *ctx);
```

#### peak_window_open {#peak_window_open}

Source: `Peak.h:389`

```c
PEAK PeakWindow peak_window_open(PeakCtx *ctx, const char *name, uint32_t width, uint32_t height, uint32_t flags);
```

#### peak_window_close {#peak_window_close}

Source: `Peak.h:390`

```c
PEAK void       peak_window_close(PeakWindow *window);
```

#### peak_window_run {#peak_window_run}

Source: `Peak.h:391`

```c
PEAK void       peak_window_run(PeakWindow *win, int (*peak_tick)(PeakWindow *win, void *userdata), void *userdata);
```

hijack main loop (web)

#### peak_window_epoll {#peak_window_epoll}

Source: `Peak.h:392`

```c
PEAK int        peak_window_epoll(PeakWindow *win, PeakEvent *ev);
```

#### peak_window_fd {#peak_window_fd}

Source: `Peak.h:393`

```c
PEAK int        peak_window_fd(PeakWindow *win);
```

display connection fd, or -1

#### peak_window_pending {#peak_window_pending}

Source: `Peak.h:394`

```c
PEAK int        peak_window_pending(PeakWindow *win);
```

queued window events; 0 if none

#### peak_window_backbuffer {#peak_window_backbuffer}

Source: `Peak.h:395`

```c
PEAK uint32_t  *peak_window_backbuffer(PeakWindow *win, size_t *width, size_t *height);
```

#### peak_window_canvas_selector {#peak_window_canvas_selector}

Source: `Peak.h:397`

```c
PEAK const char *peak_window_canvas_selector(PeakWindow *win);
```

Browser canvas selector borrowed until window close; NULL on native targets.

#### peak_window_clear {#peak_window_clear}

Source: `Peak.h:398`

```c
PEAK void       peak_window_clear(PeakWindow *win, float r, float g, float b, float a);
```

#### peak_window_present {#peak_window_present}

Source: `Peak.h:399`

```c
PEAK void       peak_window_present(PeakWindow *win);
```

#### peak_window_set_title {#peak_window_set_title}

Source: `Peak.h:400`

```c
PEAK void       peak_window_set_title(PeakWindow *win, const char *name);
```

#### peak_window_set_class {#peak_window_set_class}

Source: `Peak.h:401`

```c
PEAK void       peak_window_set_class(PeakWindow *win, const char *name);
```

WM_CLASS / app_id

#### peak_window_set_opacity {#peak_window_set_opacity}

Source: `Peak.h:402`

```c
PEAK void       peak_window_set_opacity(PeakWindow *win, uint8_t alpha);
```

0..255; no-op if the OS has none

#### peak_window_set_size {#peak_window_set_size}

Source: `Peak.h:403`

```c
PEAK void       peak_window_set_size(PeakWindow *win, uint32_t width, uint32_t height);
```

#### peak_window_fullscreen {#peak_window_fullscreen}

Source: `Peak.h:404`

```c
PEAK void       peak_window_fullscreen(PeakWindow *win, int on);
```

#### peak_window_cursor {#peak_window_cursor}

Source: `Peak.h:405`

```c
PEAK void       peak_window_cursor(PeakWindow *win, int on);
```

1 show, 0 hide

#### peak_window_cursor_shape {#peak_window_cursor_shape}

Source: `Peak.h:407`

```c
PEAK void       peak_window_cursor_shape(PeakWindow *win, int shape);
```

0 default, 1 text, 2 hand, 3 wait, 4 crosshair, 5 not-allowed, 6 help

#### peak_window_pointer_relative {#peak_window_pointer_relative}

Source: `Peak.h:408`

```c
PEAK void       peak_window_pointer_relative(PeakWindow *win, int on);
```

1 deltas, 0 absolute

#### peak_window_scale {#peak_window_scale}

Source: `Peak.h:409`

```c
PEAK float      peak_window_scale(PeakWindow *win);
```

framebuffer / window; 1.0 if unknown

#### peak_audio_start {#peak_audio_start}

Source: `Peak.h:418`

```c
PEAK int  peak_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata);
```

█ █
█
███ █ █ ███ █ ███
█ █ █ █ █ █ █ █
███ █ █ █ █ █ █ █
███ ███ ███ █ ███
device pulls interleaved s16le

#### peak_audio_stop {#peak_audio_stop}

Source: `Peak.h:419`

```c
PEAK void peak_audio_stop(void);
```

#### peak_get_time {#peak_get_time}

Source: `Peak.h:430`

```c
PEAK uint64_t peak_get_time(void);
```

nanoseconds

#### peak_sleep_ns {#peak_sleep_ns}

Source: `Peak.h:431`

```c
PEAK void     peak_sleep_ns(int64_t ns);
```

#### peak_file_exists {#peak_file_exists}

Source: `Peak.h:440`

```c
PEAK int   peak_file_exists(const char *path);
```

██ █ █
█    █
███ █ █  ███
█  █ █  ███
█  █ █  █
█  █ ██ ███

#### peak_file_alloc {#peak_file_alloc}

Source: `Peak.h:441`

```c
PEAK void *peak_file_alloc(const char *path, unsigned long *buf_size);
```

#### peak_file_write {#peak_file_write}

Source: `Peak.h:442`

```c
PEAK int   peak_file_write(const char *path, const void *buf, size_t n);
```

create/overwrite

#### peak_io_root_open {#peak_io_root_open}

Source: `Peak.h:541`

```c
PEAK PeakIOResult peak_io_root_open(const char *absolute_path, PeakIORoot *out);
```

#### peak_io_root_open_at {#peak_io_root_open_at}

Source: `Peak.h:542`

```c
PEAK PeakIOResult peak_io_root_open_at(const PeakIORoot *root, const char *relative_path, PeakIORoot *out);
```

#### peak_io_root_close {#peak_io_root_close}

Source: `Peak.h:543`

```c
PEAK PeakIOResult peak_io_root_close(PeakIORoot *root);
```

#### peak_io_file_open {#peak_io_file_open}

Source: `Peak.h:544`

```c
PEAK PeakIOResult peak_io_file_open(const PeakIORoot *root, const char *relative_path, uint64_t max_bytes, PeakIOFile *out);
```

#### peak_io_file_read {#peak_io_file_read}

Source: `Peak.h:545`

```c
PEAK PeakIOResult peak_io_file_read(const PeakIOFile *file, uint64_t offset, void *buf, size_t bytes, size_t *read_bytes);
```

#### peak_io_file_close {#peak_io_file_close}

Source: `Peak.h:546`

```c
PEAK PeakIOResult peak_io_file_close(PeakIOFile *file);
```

#### peak_io_stage_begin {#peak_io_stage_begin}

Source: `Peak.h:547`

```c
PEAK PeakIOResult peak_io_stage_begin(const PeakIORoot *root, const char *destination, const char *temporary_leaf, uint64_t max_bytes, PeakIOStage *out);
```

#### peak_io_stage_write {#peak_io_stage_write}

Source: `Peak.h:548`

```c
PEAK PeakIOResult peak_io_stage_write(PeakIOStage *stage, const void *buf, size_t bytes, size_t *written_bytes);
```

#### peak_io_stage_commit {#peak_io_stage_commit}

Source: `Peak.h:549`

```c
PEAK PeakIOPublishResult peak_io_stage_commit(PeakIOStage *stage, PeakIODurability durability);
```

#### peak_io_stage_abort {#peak_io_stage_abort}

Source: `Peak.h:550`

```c
PEAK PeakIOPublishResult peak_io_stage_abort(PeakIOStage *stage);
```

#### peak_aligned_alloc_impl {#peak_aligned_alloc_impl}

Source: `Peak.h:554`

```c
PEAK void *peak_aligned_alloc_impl(size_t size, size_t alignment);
```

Macros remain replaceable by backing-request instrumentation. The helper
preserves C99 support and accepts sizes that are not alignment multiples.

#### peak_pid {#peak_pid}

Source: `Peak.h:563`

```c
PEAK int peak_pid(void);
```

#### peak_env_set {#peak_env_set}

Source: `Peak.h:564`

```c
PEAK int peak_env_set(const char *name, const char *value);
```

NULL unsets

#### peak_env_get {#peak_env_get}

Source: `Peak.h:565`

```c
PEAK int peak_env_get(const char *name, char *buf, size_t cap);
```

1 if set and fits

#### peak_filesystem_mkdir {#peak_filesystem_mkdir}

Source: `Peak.h:567`

```c
PEAK int peak_filesystem_mkdir(const char *path);
```

one level

#### peak_filesystem_rm {#peak_filesystem_rm}

Source: `Peak.h:568`

```c
PEAK int peak_filesystem_rm(const char *path);
```

unlink, or rmdir if empty

#### peak_filesystem_cwd {#peak_filesystem_cwd}

Source: `Peak.h:569`

```c
PEAK int peak_filesystem_cwd(char *buf, size_t cap);
```

#### peak_filesystem_chdir {#peak_filesystem_chdir}

Source: `Peak.h:570`

```c
PEAK int peak_filesystem_chdir(const char *path);
```

#### peak_filesystem_rename {#peak_filesystem_rename}

Source: `Peak.h:571`

```c
PEAK int peak_filesystem_rename(const char *from, const char *to);
```

#### peak_filesystem_list {#peak_filesystem_list}

Source: `Peak.h:572`

```c
PEAK int peak_filesystem_list(const char *path, int (*fn)(const char *name, void *ud), void *ud);
```

fn 0 stops; 1 if opened

#### peak_filesystem_symlink {#peak_filesystem_symlink}

Source: `Peak.h:573`

```c
PEAK int peak_filesystem_symlink(const char *target, const char *path);
```

#### peak_filesystem_readlink {#peak_filesystem_readlink}

Source: `Peak.h:574`

```c
PEAK int peak_filesystem_readlink(const char *path, char *dst, size_t cap);
```

#### peak_pty_spawn {#peak_pty_spawn}

Source: `Peak.h:589`

```c
PEAK PeakProc peak_pty_spawn(const char *file, const char **argv, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel);
```

Child + PTY. File descriptor is nonblocking. Fails as PEAK_HANDLE_INVALID.

#### peak_pty_resize {#peak_pty_resize}

Source: `Peak.h:590`

```c
PEAK void     peak_pty_resize(PeakProc *pty, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel);
```

#### peak_pty_reap {#peak_pty_reap}

Source: `Peak.h:591`

```c
PEAK int      peak_pty_reap(PeakProc *pty);
```

1 if dead

#### peak_pty_close {#peak_pty_close}

Source: `Peak.h:592`

```c
PEAK void     peak_pty_close(PeakProc *pty);
```

#### peak_job_run {#peak_job_run}

Source: `Peak.h:595`

```c
PEAK PeakProc peak_job_run(const char *cmd, const char *cwd);
```

Off-grid shell.

#### peak_job_reap {#peak_job_reap}

Source: `Peak.h:596`

```c
PEAK int      peak_job_reap(PeakProc *job, int *code);
```

1 if exited

#### peak_job_kill {#peak_job_kill}

Source: `Peak.h:597`

```c
PEAK void     peak_job_kill(PeakProc *job);
```

#### peak_pid_cwd {#peak_pid_cwd}

Source: `Peak.h:598`

```c
PEAK int      peak_pid_cwd(int pid, char *buf, size_t cap);
```

#### peak_child_arm {#peak_child_arm}

Source: `Peak.h:601`

```c
PEAK int         peak_child_arm(void);
```

Dead-child wakeup. fd pollable or INVALID. Missing OS: arm 1, fd INVALID, reap 0.

#### peak_child_disarm {#peak_child_disarm}

Source: `Peak.h:602`

```c
PEAK void        peak_child_disarm(void);
```

#### peak_child_fd {#peak_child_fd}

Source: `Peak.h:603`

```c
PEAK PEAK_HANDLE peak_child_fd(void);
```

#### peak_child_ack {#peak_child_ack}

Source: `Peak.h:604`

```c
PEAK void        peak_child_ack(void);
```

#### peak_child_reap {#peak_child_reap}

Source: `Peak.h:605`

```c
PEAK int         peak_child_reap(int *pid, int *code);
```

1 if one

#### peak_usr1_arm {#peak_usr1_arm}

Source: `Peak.h:608`

```c
PEAK int         peak_usr1_arm(void);
```

SIGUSR1 wakeup. fd pollable or INVALID. Missing OS: arm 1, fd INVALID, ack 0.

#### peak_usr1_disarm {#peak_usr1_disarm}

Source: `Peak.h:609`

```c
PEAK void        peak_usr1_disarm(void);
```

#### peak_usr1_fd {#peak_usr1_fd}

Source: `Peak.h:610`

```c
PEAK PEAK_HANDLE peak_usr1_fd(void);
```

#### peak_usr1_ack {#peak_usr1_ack}

Source: `Peak.h:611`

```c
PEAK int         peak_usr1_ack(void);
```

1 if fired

#### peak_stdout_silence {#peak_stdout_silence}

Source: `Peak.h:613`

```c
PEAK int peak_stdout_silence(void);
```

stdout -> platform null

#### peak_stdout_restore {#peak_stdout_restore}

Source: `Peak.h:614`

```c
PEAK int peak_stdout_restore(void);
```

#### peak_wait {#peak_wait}

Source: `Peak.h:624`

```c
PEAK int peak_wait(PeakWindow *win, const PEAK_HANDLE *fds, uint32_t n, int timeout_ms);
```

█

█ ███
█ █ █
█ █ █
█ ███
Sleep until window (nullable) or any fd is ready. timeout_ms: -1 block, 0 poll. 1 if ready.

#### peak_runtime_dir {#peak_runtime_dir}

Source: `Peak.h:627`

```c
PEAK int         peak_runtime_dir(char *buf, size_t cap, const char *app);
```

Local stream (unix socket / named pipe). listen/accept fds are nonblocking.

#### peak_sock_listen {#peak_sock_listen}

Source: `Peak.h:628`

```c
PEAK PEAK_HANDLE peak_sock_listen(const char *path);
```

#### peak_sock_accept {#peak_sock_accept}

Source: `Peak.h:629`

```c
PEAK PEAK_HANDLE peak_sock_accept(PEAK_HANDLE listen_fd);
```

#### peak_sock_connect {#peak_sock_connect}

Source: `Peak.h:630`

```c
PEAK PEAK_HANDLE peak_sock_connect(const char *path);
```

#### peak_sock_send {#peak_sock_send}

Source: `Peak.h:631`

```c
PEAK int         peak_sock_send(PEAK_HANDLE sock, const void *buf, size_t n, PEAK_HANDLE pass);
```

#### peak_sock_recv {#peak_sock_recv}

Source: `Peak.h:632`

```c
PEAK int         peak_sock_recv(PEAK_HANDLE sock, void *buf, size_t n, PEAK_HANDLE *pass);
```

#### peak_pointer_pid {#peak_pointer_pid}

Source: `Peak.h:633`

```c
PEAK int         peak_pointer_pid(PeakWindow *win);
```

_NET_WM_PID under pointer; 0 if none

#### peak_pointer_local {#peak_pointer_local}

Source: `Peak.h:634`

```c
PEAK int         peak_pointer_local(PeakWindow *win, int *x, int *y);
```

1 if pointer is in this window

#### peak_fd_read {#peak_fd_read}

Source: `Peak.h:637`

```c
PEAK int  peak_fd_read(PEAK_HANDLE fd, void *buf, size_t n);
```

Byte IO on Peak fds (pty, sock, job). -1 would-block, 0 EOF, >0 count.

#### peak_fd_write {#peak_fd_write}

Source: `Peak.h:638`

```c
PEAK int  peak_fd_write(PEAK_HANDLE fd, const void *buf, size_t n);
```

#### peak_fd_close {#peak_fd_close}

Source: `Peak.h:639`

```c
PEAK void peak_fd_close(PEAK_HANDLE fd);
```

#### peak_pipe_capacity {#peak_pipe_capacity}

Source: `Peak.h:641`

```c
PEAK size_t peak_pipe_capacity(PEAK_HANDLE fd);
```

Kernel pipe buffer. 0 if not a pipe. set returns the size in force.

#### peak_pipe_set_capacity {#peak_pipe_set_capacity}

Source: `Peak.h:642`

```c
PEAK size_t peak_pipe_set_capacity(PEAK_HANDLE fd, size_t n);
```

#### peak_page_size {#peak_page_size}

Source: `Peak.h:654`

```c
PEAK size_t peak_page_size(void);
```

█

███ █ ███ ███
█   █ █ █ █ █
█   █ █ █ █ █
█   █ █ █ ███
█
███
Page-mirrored ring: size must be page-aligned. pointer valid for size*2.

#### peak_mirror_map {#peak_mirror_map}

Source: `Peak.h:655`

```c
PEAK void  *peak_mirror_map(size_t size);
```

#### peak_mirror_unmap {#peak_mirror_unmap}

Source: `Peak.h:656`

```c
PEAK void   peak_mirror_unmap(void *p, size_t size);
```

#### peak_vulkan_get_extensions {#peak_vulkan_get_extensions}

Source: `Peak.h:667`

```c
PEAK const char **peak_vulkan_get_extensions(uint32_t *count);
```

██
█
███ ███ █ █
█ █  █   █
█ █  █   █
███  █  █ █
█
███

#### peak_vulkan_create_surface {#peak_vulkan_create_surface}

Source: `Peak.h:668`

```c
PEAK int          peak_vulkan_create_surface(PeakWindow *win, void *instance, const void *allocator, void *out_surface);
```

needs PEAK_VULKAN

#### peak_clip_set {#peak_clip_set}

Source: `Peak.h:685`

```c
PEAK int peak_clip_set(PeakCtx *ctx, PeakWindow *win, PeakClip which, const char *utf8, size_t n);
```

█  █
█
███ █  █ ███
█   █  █ █ █
█   █  █ █ █
███ ██ █ ███
█
█
Window-only methods validate their owned context. Linux additionally validates
native allocation/slot lifetimes, rejecting copied handles after close/reuse.
Other legacy hosts require exclusive window-handle ownership: do not retain
copies after close or quit; stale-copy validation is not provided there.
UTF-8 clipboard. Configured clipboard cap. win NULL: context-local slot. PRIMARY aliases
CLIPBOARD on Win32/macOS/web. request completes as PEAK_EVENT_CLIP; take copies.

#### peak_clip_request {#peak_clip_request}

Source: `Peak.h:686`

```c
PEAK int peak_clip_request(PeakCtx *ctx, PeakWindow *win, PeakClip which);
```

#### peak_clip_take {#peak_clip_take}

Source: `Peak.h:687`

```c
PEAK int peak_clip_take(PeakCtx *ctx, PeakWindow *win, char *dst, size_t cap, size_t *n);
```

#### peak_text_take {#peak_text_take}

Source: `Peak.h:690`

```c
PEAK int peak_text_take(PeakCtx *ctx, PeakWindow *win, char *dst, size_t cap, size_t *n);
```

UTF-8 text / drop path. Configured transfer cap. Completes as PEAK_EVENT_TEXT / DROP; take copies.

#### peak_drop_take {#peak_drop_take}

Source: `Peak.h:691`

```c
PEAK int peak_drop_take(PeakCtx *ctx, PeakWindow *win, char *dst, size_t cap, size_t *n);
```

#### peak_drop_drag {#peak_drop_drag}

Source: `Peak.h:692`

```c
PEAK int peak_drop_drag(PeakCtx *ctx, PeakWindow *win, const char *utf8, size_t n);
```

start OS drag; 0 if none

#### peak_log_printf {#peak_log_printf}

Source: `Peak.h:751`

```c
PEAK void  peak_log_printf(PeakLogLevel level, const char *src, ...);
```

#### peak_debug_malloc_impl {#peak_debug_malloc_impl}

Source: `Peak.h:752`

```c
PEAK void *peak_debug_malloc_impl(size_t size, const char *file, int line, const char *func);
```

#### peak_debug_calloc_impl {#peak_debug_calloc_impl}

Source: `Peak.h:753`

```c
PEAK void *peak_debug_calloc_impl(size_t count, size_t size, const char *file, int line, const char *func);
```

#### peak_debug_free_impl {#peak_debug_free_impl}

Source: `Peak.h:754`

```c
PEAK void  peak_debug_free_impl(void *ptr, const char *file, int line, const char *func);
```

#### peak_debug_realloc_impl {#peak_debug_realloc_impl}

Source: `Peak.h:755`

```c
PEAK void *peak_debug_realloc_impl(void *ptr, size_t size, const char *file, int line, const char *func);
```

#### peak_debug_malloc_domain_impl {#peak_debug_malloc_domain_impl}

Source: `Peak.h:786`

```c
PEAK void *peak_debug_malloc_domain_impl(size_t size, PeakMemoryDomain domain, const char *file, int line, const char *func);
```

#### peak_debug_calloc_domain_impl {#peak_debug_calloc_domain_impl}

Source: `Peak.h:787`

```c
PEAK void *peak_debug_calloc_domain_impl(size_t count, size_t size, PeakMemoryDomain domain, const char *file, int line, const char *func);
```

#### peak_debug_realloc_domain_impl {#peak_debug_realloc_domain_impl}

Source: `Peak.h:788`

```c
PEAK void *peak_debug_realloc_domain_impl(void *ptr, size_t size, PeakMemoryDomain domain, const char *file, int line, const char *func);
```

#### peak_debug_free_domain_impl {#peak_debug_free_domain_impl}

Source: `Peak.h:789`

```c
PEAK void  peak_debug_free_domain_impl(void *ptr, PeakMemoryDomain domain, const char *file, int line, const char *func);
```

#### peak_debug_memory_stats {#peak_debug_memory_stats}

Source: `Peak.h:790`

```c
PEAK PeakMemoryStats peak_debug_memory_stats(PeakMemoryDomain domain);
```

#### peak_debug_memory_report {#peak_debug_memory_report}

Source: `Peak.h:797`

```c
PEAK uint64_t peak_debug_memory_report(void);
```

Prints both domains and leaks; returns successful NON_DRIVER requests.
realloc(NULL,0) returns NULL without a backing request or release; every
realloc wrapper call increments realloc_requests, including zero-size calls.
Original tracked domain is authoritative on free/realloc despite mismatch.
Unknown-pointer operations clear completeness; no release is invented.

<!-- END GENERATED API -->
