#define NOB_IMPLEMENTATION
#include "nob.h"
#include <iso646.h>

// ── system dependencies ──────────────────────────────────────────
//
// On Debian/Ubuntu:
//   apt-get install -y libsdl2-dev libvulkan-dev libopenal-dev \
//                      glslang-tools mesa-vulkan-drivers sway
//
// On Arch Linux:
//   pacman -S sdl2 vulkan-icd-loader vulkan-headers openal \
//             glslang vulkan-swrast sway
//
// Mesa's lavapipe (mesa-vulkan-drivers / vulkan-swrast) provides
// software Vulkan ray tracing when no discrete GPU is available.
// sway provides a headless Wayland compositor for --run mode.
//
// For Windows cross-compilation (--windows):
//   apt-get install -y mingw-w64

// ── settings ─────────────────────────────────────────────────────

#define SRC   "./"
#define BUILD "build/"
#define SPVD  BUILD "shaders/"
#define MAIN  SRC "quake3.c"

// Validation layers: enabled for debug builds, stripped for production.
// Production builds patch the generated C source to zero out the layer
// count and comment out the debug utils extension — no #ifdefs needed.
#define VALIDATION_LAYERS_ENABLED  true
#define VALIDATION_LAYERS_DISABLED false

// ── shader extraction ──────────────────────────────────────────
//
// Embedded shaders can appear anywhere in quake3.c at the top level
// (outside of any C brace-delimited block) as:
//
//     glsl shader <Name> <stage> {
//     ...GLSL source...
//     }
//
// nob walks the file tracking C brace depth.  A `glsl shader` line
// at depth 0 is recognised as a shader block; anything nested inside
// a C function, struct, etc. is ignored.  The C-only output written
// to build/quake3.c has every shader block excised; each block's
// GLSL body is extracted to its own .glsl file for SPIR-V compilation.

typedef struct {
    char        name[64];
    char        stage[16];
    const char *code;
    size_t      code_len;
    const char *block_start; // first char of `glsl shader ...` line
    const char *block_end;   // first char after the closing `}\n`
} Shader_Block;

#define MAX_SHADERS 16

// Walk the source tracking C brace depth and extract top-level
// `glsl shader` blocks.  Returns the number of blocks found.
static int extract_shaders(const char *src, Shader_Block *out, int max) {
    int count = 0;
    int depth = 0;           // C brace nesting depth
    const char *p = src;

    while (*p and count < max) {

        // Skip C line comments
        if (p[0] == '/' and p[1] == '/') {
            while (*p and *p != '\n') p++;
            continue;
        }

        // Skip C block comments
        if (p[0] == '/' and p[1] == '*') {
            p += 2;
            while (*p and not (p[0] == '*' and p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }

        // Skip string literals
        if (*p == '"') {
            p++;
            while (*p and *p != '"') { if (*p == '\\') p++; p++; }
            if (*p) p++;
            continue;
        }

        // Skip character literals
        if (*p == '\'') {
            p++;
            while (*p and *p != '\'') { if (*p == '\\') p++; p++; }
            if (*p) p++;
            continue;
        }

        // Track C brace depth
        if (*p == '{' and depth >= 0) { depth++; p++; continue; }
        if (*p == '}' and depth >  0) { depth--; p++; continue; }

        // Look for `glsl shader ` at depth 0, possibly with leading whitespace
        if (depth == 0 and strncmp(p, "glsl shader ", 12) == 0) {
            // Verify it's at the start of a line (only whitespace before it)
            const char *scan = p;
            while (scan > src and scan[-1] != '\n') scan--;
            bool line_ok = true;
            for (const char *c = scan; c < p; c++)
                if (*c != ' ' and *c != '\t') { line_ok = false; break; }
            if (not line_ok) { p++; continue; }

            const char *blk = scan;  // block starts at beginning of line
            p += 12;

            // Name
            const char *ns = p;
            while (*p and *p != ' ' and *p != '\n') p++;
            size_t nl = (size_t)(p - ns);
            if (nl >= sizeof out[0].name) nl = sizeof out[0].name - 1;
            memcpy(out[count].name, ns, nl);
            out[count].name[nl] = '\0';

            while (*p == ' ') p++;

            // Stage
            const char *ss = p;
            while (*p and *p != ' ' and *p != '{' and *p != '\n') p++;
            size_t sl = (size_t)(p - ss);
            while (sl > 0 and ss[sl - 1] == ' ') sl--;
            if (sl >= sizeof out[0].stage) sl = sizeof out[0].stage - 1;
            memcpy(out[count].stage, ss, sl);
            out[count].stage[sl] = '\0';

            // Opening brace
            while (*p and *p != '{') p++;
            if (not *p) break;
            p++;
            if (*p == '\n') p++;

            // Shader body — brace-count to find the matching close
            const char *code_start = p;
            const char *code_end   = NULL;
            int sdepth = 1;
            while (*p and sdepth > 0) {
                if      (*p == '{') sdepth++;
                else if (*p == '}') { if (--sdepth == 0) { code_end = p; break; } }
                p++;
            }
            if (not code_end) break;

            // Skip past the closing brace and trailing newline
            p = code_end + 1;
            if (*p == '\n') p++;

            out[count].code        = code_start;
            out[count].code_len    = (size_t)(code_end - code_start);
            out[count].block_start = blk;
            out[count].block_end   = p;
            count++;
            continue;
        }

        p++;
    }
    return count;
}

// Write the C-only source: copy `src` but skip every shader block region.
// Injects the generated Shader_Path() macro so quake3.c can reference
// shader .spv files by their glsl block name.
// When `production` is true, also patch out validation layers and the
// debug utils extension so the binary runs clean without them.
static bool write_c_source(const char *path, const char *src, size_t src_len,
                            const Shader_Block *sh, int n, bool production) {

    // Shader_Path macro header injected at the top of the generated source
    const char *hdr =
        "// Generated by nob.c — Shader_Path resolves glsl shader names to .spv paths\n"
        "#define Shader_Path(name) \"" SPVD "\" #name \".spv\"\n\n";
    size_t hdr_len = strlen(hdr);

    // Build the full C source into a buffer so we can patch it
    size_t buf_cap = hdr_len + src_len + 256;
    char  *buf     = malloc(buf_cap);
    size_t buf_len = 0;

    // Write the generated header first
    memcpy(buf, hdr, hdr_len);
    buf_len = hdr_len;

    // Copy the source, skipping shader block regions
    const char *cursor = src;
    const char *end    = src + src_len;
    for (int i = 0; i < n; i++) {
        if (sh[i].block_start > cursor) {
            size_t chunk = (size_t)(sh[i].block_start - cursor);
            memcpy(buf + buf_len, cursor, chunk);
            buf_len += chunk;
        }
        cursor = sh[i].block_end;
    }
    if (cursor < end) {
        size_t chunk = (size_t)(end - cursor);
        memcpy(buf + buf_len, cursor, chunk);
        buf_len += chunk;
    }
    buf[buf_len] = '\0';

    // Production: force VALIDATION_LAYER_COUNT to 0 so validation is never requested
    if (production) {
        char *vl = strstr(buf, "VALIDATION_LAYER_COUNT = 1");
        if (vl) vl[25] = '0';
        nob_log(NOB_INFO, "production: validation layers stripped");
    }

    FILE *f = fopen(path, "wb");
    if (not f) { nob_log(NOB_ERROR, "could not open %s for writing", path); free(buf); return false; }
    fwrite(buf, 1, buf_len, f);
    fclose(f);
    free(buf);
    return true;
}

// ── helpers ──────────────────────────────────────────────────────

static bool write_file(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (not f) { nob_log(NOB_ERROR, "could not open %s for writing", path); return false; }
    fwrite(data, 1, len, f);
    fclose(f);
    return true;
}

static bool spv_stale(const char *spv, const char *src) {
    return not nob_file_exists(spv) or nob_needs_rebuild1(spv, src);
}

// Check if a character is part of an identifier
static bool is_ident(char c) {
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z')
        or (c >= '0' and c <= '9') or c == '_';
}

// Write a GLSL shader file, preprocessing `and`/`or`/`not` keywords
// to their GLSL operator equivalents `&&`/`||`/`!`.
static bool write_shader_glsl(const char *path, const char *code, size_t len) {
    size_t cap = len * 2 + 1;
    char  *out = malloc(cap);
    size_t pos = 0;
    const char *p   = code;
    const char *end = code + len;

    while (p < end) {

        // Skip string literals
        if (*p == '"') {
            out[pos++] = *p++;
            while (p < end and *p != '"') { if (*p == '\\' and p + 1 < end) out[pos++] = *p++; out[pos++] = *p++; }
            if (p < end) out[pos++] = *p++;
            continue;
        }

        // Skip line comments
        if (p + 1 < end and p[0] == '/' and p[1] == '/') {
            while (p < end and *p != '\n') out[pos++] = *p++;
            continue;
        }

        // Skip block comments
        if (p + 1 < end and p[0] == '/' and p[1] == '*') {
            out[pos++] = *p++; out[pos++] = *p++;
            while (p < end and not (p[0] == '*' and p + 1 < end and p[1] == '/')) out[pos++] = *p++;
            if (p < end) { out[pos++] = *p++; out[pos++] = *p++; }
            continue;
        }

        // Replace and/or/not keywords at word boundaries
        bool at_boundary = (p == code or not is_ident(p[-1]));
        if (at_boundary) {
            if (p + 3 <= end and memcmp(p, "and", 3) == 0 and (p + 3 >= end or not is_ident(p[3]))) {
                out[pos++] = '&'; out[pos++] = '&'; p += 3; continue;
            }
            if (p + 2 <= end and memcmp(p, "or", 2) == 0 and (p + 2 >= end or not is_ident(p[2]))) {
                out[pos++] = '|'; out[pos++] = '|'; p += 2; continue;
            }
            if (p + 3 <= end and memcmp(p, "not", 3) == 0 and (p + 3 >= end or not is_ident(p[3]))) {
                out[pos++] = '!'; p += 3; continue;
            }
        }

        out[pos++] = *p++;
    }

    bool ok = write_file(path, out, pos);
    free(out);
    return ok;
}

// ── shader forward declaration validation ────────────────────────
//
// The `glsl shader <Name> <stage> { ... }` blocks in quake3.c serve as
// forward declarations.  nob.c generates a `Shader_Path(name)` macro
// into build/quake3.c that resolves shader names to .spv paths.
//
// This function validates that every Shader_Path(xxx) call in the source
// maps to an extracted glsl shader block, and vice versa.  Mismatches
// produce compiler-style errors with line numbers.

static bool validate_shader_declarations(const char *src, const Shader_Block *sh, int n) {
    bool ok = true;

    // Forward check: every Shader_Path(xxx) call must match an extracted block
    const char *p = src;
    while ((p = strstr(p, "Shader_Path(")) != NULL) {
        const char *call_pos = p;
        p += 12;  // skip past "Shader_Path("
        const char *name_start = p;
        while (*p and *p != ')' and *p != '\n') p++;
        size_t name_len = (size_t)(p - name_start);
        if (name_len == 0 or name_len > 63) continue;

        char expected[64];
        memcpy(expected, name_start, name_len);
        expected[name_len] = '\0';

        // Find a matching extracted shader block
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (strcmp(sh[i].name, expected) == 0) {
                found = true;
                break;
            }
        }

        if (not found) {
            int line = 1;
            for (const char *c = src; c < call_pos; c++)
                if (*c == '\n') line++;
            nob_log(NOB_ERROR,
                "quake3.c:%d: Shader_Path(%s) has no matching "
                "'glsl shader %s ...' block", line, expected, expected);
            ok = false;
        }
    }

    // Reverse check: every extracted shader must have a Shader_Path() reference
    for (int i = 0; i < n; i++) {
        char pattern[128];
        snprintf(pattern, sizeof pattern, "Shader_Path(%s)", sh[i].name);
        if (not strstr(src, pattern)) {
            nob_log(NOB_ERROR,
                "quake3.c: glsl shader '%s' extracted but never loaded — "
                "add Shader_Path(%s) to use it", sh[i].name, sh[i].name);
            ok = false;
        }
    }

    return ok;
}

// ── windows dll bundling ─────────────────────────────────────────
//
// After cross-compiling with mingw, bundle the required DLLs into
// build/release/ so the .exe is self-contained and downloadable.
// DLLs are first searched in the mingw sysroot, then downloaded
// from official release archives if not found locally.

#define DLL_CACHE BUILD "dlls/"

// Download a file using curl, caching in build/dlls/
static bool download_file(const char *url, const char *dest) {
    if (nob_file_exists(dest)) return true;
    nob_log(NOB_INFO, "downloading %s", url);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "curl", "-fsSL", "--create-dirs", "-o", dest, url);
    return nob_cmd_run(&cmd);
}

// Search common mingw sysroot paths for a DLL
static bool find_local_dll(const char *dll, char *out, size_t out_sz) {
    const char *search_dirs[] = {
        "/usr/x86_64-w64-mingw32/bin/",
        "/usr/x86_64-w64-mingw32/lib/",
        "/usr/lib/gcc/x86_64-w64-mingw32/12-posix/",
        "/usr/lib/gcc/x86_64-w64-mingw32/12-win32/",
        "/usr/lib/gcc/x86_64-w64-mingw32/13-posix/",
        "/usr/lib/gcc/x86_64-w64-mingw32/13-win32/",
    };
    for (int i = 0; i < (int)(sizeof search_dirs / sizeof search_dirs[0]); i++) {
        snprintf(out, out_sz, "%s%s", search_dirs[i], dll);
        if (nob_file_exists(out)) return true;
    }
    return false;
}

// Copy a DLL from local sysroot or download cache into build/release/
static bool bundle_dll(const char *dll, const char *dest_dir) {
    char src_path[512], dst_path[512];
    snprintf(dst_path, sizeof dst_path, "%s%s", dest_dir, dll);

    // Already bundled from a previous build
    if (nob_file_exists(dst_path)) return true;

    // Try local sysroot first
    if (find_local_dll(dll, src_path, sizeof src_path)) {
        nob_log(NOB_INFO, "bundling %s (local)", src_path);
        return nob_copy_file(src_path, dst_path);
    }

    // Try download cache
    char cache_path[512];
    snprintf(cache_path, sizeof cache_path, DLL_CACHE "%s", dll);
    if (nob_file_exists(cache_path)) {
        nob_log(NOB_INFO, "bundling %s (cached)", dll);
        return nob_copy_file(cache_path, dst_path);
    }

    nob_log(NOB_WARNING, "%s not found locally or in cache", dll);
    return true;
}

// Download SDL2 and OpenAL Soft Windows DLLs from official releases
static bool download_windows_dlls(void) {
    if (not nob_mkdir_if_not_exists(DLL_CACHE)) return false;

    // SDL2 x64 DLL
    char sdl2_zip[256];
    snprintf(sdl2_zip, sizeof sdl2_zip, DLL_CACHE "SDL2-win64.zip");
    if (not nob_file_exists(DLL_CACHE "SDL2.dll")) {
        if (not download_file(
                "https://github.com/libsdl-org/SDL/releases/download/release-2.30.10/SDL2-2.30.10-win32-x64.zip",
                sdl2_zip)) return false;
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "unzip", "-jo", sdl2_zip, "SDL2.dll", "-d", DLL_CACHE);
        if (not nob_cmd_run(&cmd))
            nob_log(NOB_WARNING, "failed to extract SDL2.dll from archive");
    }

    // OpenAL Soft x64 DLL
    char oal_zip[256];
    snprintf(oal_zip, sizeof oal_zip, DLL_CACHE "openal-soft-win64.zip");
    if (not nob_file_exists(DLL_CACHE "OpenAL32.dll")) {
        if (not download_file(
                "https://github.com/kcat/openal-soft/releases/download/1.24.1/openal-soft-1.24.1-bin.zip",
                oal_zip)) return false;
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "unzip", "-jo", oal_zip, "*/bin/Win64/soft_oal.dll", "-d", DLL_CACHE);
        if (not nob_cmd_run(&cmd))
            nob_log(NOB_WARNING, "failed to extract OpenAL DLL from archive");
        // OpenAL Soft ships as soft_oal.dll — rename to OpenAL32.dll
        char soft_path[256], oal_path[256];
        snprintf(soft_path, sizeof soft_path, DLL_CACHE "soft_oal.dll");
        snprintf(oal_path, sizeof oal_path, DLL_CACHE "OpenAL32.dll");
        if (nob_file_exists(soft_path)) rename(soft_path, oal_path);
    }

    return true;
}

static bool bundle_windows_dlls(void) {
    // Download any DLLs we don't have locally
    if (not download_windows_dlls()) return false;

    const char *dlls[] = {
        "SDL2.dll",
        "OpenAL32.dll",
    };
    int count = (int)(sizeof dlls / sizeof dlls[0]);
    for (int i = 0; i < count; i++) {
        if (not bundle_dll(dlls[i], BUILD "release/")) return false;
    }
    return true;
}

// ── platform builds ──────────────────────────────────────────────

typedef enum {
    PLATFORM_NATIVE,   // current host (default)
    PLATFORM_WINDOWS,  // mingw cross-compile
    PLATFORM_MACOS,    // macOS (MoltenVK)
    PLATFORM_DEBIAN,   // Debian / Ubuntu
    PLATFORM_ARCH,     // Arch Linux
    PLATFORM_COUNT
} Platform;

static const char *platform_names[] = {
    "native", "windows", "macos", "debian", "arch"
};

// Append platform-specific C compiler flags and libraries to `cmd`.
static void platform_compile(Nob_Cmd *cmd, Platform plat, bool production) {
    const char *src_file = BUILD "quake3.c";

    switch (plat) {

    // Windows: mingw cross-compiler targeting x86_64
    case PLATFORM_WINDOWS:
        nob_cmd_append(cmd,
            "x86_64-w64-mingw32-gcc",
            "-O3", "-ffast-math", "-flto",
            "-Wall", "-Wextra",
            "-o", BUILD "release/quake3.exe",
            src_file,
            "-I/usr/x86_64-w64-mingw32/include/SDL2",
            "-lmingw32", "-lSDL2main", "-lSDL2",
            "-lvulkan-1", "-lm", "-lOpenAL32",
            "-mwindows");
        break;

    // macOS: Homebrew paths, MoltenVK, framework OpenAL
    case PLATFORM_MACOS:
        nob_cmd_append(cmd,
            "cc",
            "-O3", "-ffast-math", "-flto",
            "-Wall", "-Wextra",
            production ? "-DNDEBUG" : "-g",
            "-o", production ? BUILD "release/quake3_macos" : BUILD "quake3",
            src_file,
            "-I/opt/homebrew/include/SDL2", "-I/opt/homebrew/include",
            "-D_REENTRANT",
            "-L/opt/homebrew/lib",
            "-lSDL2", "-lvulkan", "-lm",
            "-framework", "OpenAL");
        break;

    // Debian / Ubuntu: standard system paths
    case PLATFORM_DEBIAN:
        nob_cmd_append(cmd,
            "cc",
            "-O3", "-march=x86-64-v2", "-ffast-math", "-flto",
            "-Wall", "-Wextra",
            production ? "-DNDEBUG" : "-g",
            "-o", production ? BUILD "release/quake3_debian" : BUILD "quake3",
            src_file,
            "-I/usr/include/SDL2", "-D_REENTRANT",
            "-lSDL2", "-lvulkan", "-lm",
            "-lopenal");
        break;

    // Arch Linux: same layout as Debian
    case PLATFORM_ARCH:
        nob_cmd_append(cmd,
            "cc",
            "-O3", "-march=x86-64-v2", "-ffast-math", "-flto",
            "-Wall", "-Wextra",
            production ? "-DNDEBUG" : "-g",
            "-o", production ? BUILD "release/quake3_arch" : BUILD "quake3",
            src_file,
            "-I/usr/include/SDL2", "-D_REENTRANT",
            "-lSDL2", "-lvulkan", "-lm",
            "-lopenal");
        break;

    // Native: host compiler with -march=native
    case PLATFORM_NATIVE:
    default:
        nob_cmd_append(cmd,
            "cc",
            "-O3", "-march=native", "-mtune=native",
            "-ffast-math", "-flto",
            production ? "-DNDEBUG" : "-g",
            "-Wall", "-Wextra",
            "-o", production ? BUILD "release/quake3" : BUILD "quake3",
            src_file,
            "-I/usr/include/SDL2", "-D_REENTRANT",
            "-lSDL2", "-lvulkan", "-lm",
            "-lopenal");
        break;
    }
}

// ── main ─────────────────────────────────────────────────────────

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    if (not nob_mkdir_if_not_exists(BUILD))            return 1;
    if (not nob_mkdir_if_not_exists(SPVD))             return 1;
    if (not nob_mkdir_if_not_exists(BUILD "release/"))  return 1;

    // Parse command-line flags
    bool production = false;
    Platform target = PLATFORM_NATIVE;
    bool build_all  = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--production") == 0 or strcmp(argv[i], "--prod") == 0)
            production = true;
        else if (strcmp(argv[i], "--windows") == 0)  target = PLATFORM_WINDOWS;
        else if (strcmp(argv[i], "--macos") == 0)    target = PLATFORM_MACOS;
        else if (strcmp(argv[i], "--debian") == 0)   target = PLATFORM_DEBIAN;
        else if (strcmp(argv[i], "--arch") == 0)     target = PLATFORM_ARCH;
        else if (strcmp(argv[i], "--all") == 0)      build_all = true;
    }

    // Read the single-source file
    Nob_String_Builder sb = {0};
    if (not nob_read_entire_file(MAIN, &sb)) return 1;
    nob_sb_append_null(&sb);

    // Extract top-level shader blocks
    Shader_Block shaders[MAX_SHADERS];
    int n = extract_shaders(sb.items, shaders, MAX_SHADERS);
    nob_log(NOB_INFO, "extracted %d shader(s) from %s", n, MAIN);

    // Validate that every Shader_Path() call matches an extracted glsl shader block
    if (not validate_shader_declarations(sb.items, shaders, n)) return 1;

    // Write the C-only source (shader blocks excised, Shader_Path macro injected)
    if (not write_c_source(BUILD "quake3.c", sb.items, sb.count - 1, shaders, n, production)) return 1;

    // Write extracted shaders to build/shaders/ (preprocessing and/or/not to GLSL operators)
    char glsl_paths[MAX_SHADERS][256], spv_paths[MAX_SHADERS][256];
    for (int i = 0; i < n; i++) {
        snprintf(glsl_paths[i], sizeof glsl_paths[i], SPVD "%s.glsl", shaders[i].name);
        snprintf(spv_paths[i],  sizeof spv_paths[i],  SPVD "%s.spv",  shaders[i].name);
        if (not write_shader_glsl(glsl_paths[i], shaders[i].code, shaders[i].code_len)) return 1;
    }

    // Compile each shader to SPIR-V
    Nob_Cmd cmd = {0};
    for (int i = 0; i < n; i++) {
        if (not spv_stale(spv_paths[i], MAIN)) continue;
        nob_cmd_append(&cmd, "glslangValidator",
            "-V", "--target-env", "vulkan1.3",
            "-S", shaders[i].stage,
            "-o", spv_paths[i], glsl_paths[i]);
        if (not nob_cmd_run(&cmd)) return 1;
    }

    // Build for all platforms (production) or just the selected target
    if (build_all) {
        for (int p = 0; p < PLATFORM_COUNT; p++) {
            nob_log(NOB_INFO, "building production release for %s", platform_names[p]);
            platform_compile(&cmd, (Platform)p, true);
            if (not nob_cmd_run(&cmd))
                nob_log(NOB_WARNING, "%s build failed (toolchain may not be installed)", platform_names[p]);
        }

        // Bundle DLLs for the Windows build
        if (not bundle_windows_dlls())
            nob_log(NOB_WARNING, "some Windows DLLs could not be bundled");
    } else {
        platform_compile(&cmd, target, production);
        if (not nob_cmd_run(&cmd)) return 1;

        // Bundle DLLs when targeting Windows
        if (target == PLATFORM_WINDOWS) {
            if (not bundle_windows_dlls()) return 1;
        }
    }

    return 0;
}
