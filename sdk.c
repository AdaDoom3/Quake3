#define NOB_IMPLEMENTATION
#include "nobuild.h"
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

#define BUILD "build/"
#define SPVD  BUILD "shaders/"
#define MAIN  "q3.c"

// Validation layers: enabled for debug builds, stripped for production.
// Production builds patch the generated C source to zero out the layer
// count and comment out the debug utils extension — no #ifdefs needed.
#define VALIDATION_LAYERS_ENABLED  true
#define VALIDATION_LAYERS_DISABLED false

// ── shader extraction ──────────────────────────────────────────
//
// Embedded shaders can appear anywhere in q3.c at the top level
// (outside of any C brace-delimited block) as:
//
//     glsl <stage> <Name> {        ← shader definition (body extracted)
//     ...GLSL source...
//     }
//
//     glsl <stage> <Name>;         ← forward declaration (skipped)
//
// nob walks the file tracking C brace depth.  A `glsl` line
// at depth 0 is recognised as a shader block; anything nested inside
// a C function, struct, etc. is ignored.  The C-only output written
// to build/q3.c has every shader block excised; each block's
// GLSL body is extracted to its own .glsl file for SPIR-V compilation.
// Forward declarations (ending in `;`) are also excised from the
// generated C source.

typedef struct {
    char        name[64];
    char        stage[16];
    const char *code;
    size_t      code_len;
    const char *block_start; // first char of `glsl ...` line
    const char *block_end;   // first char after the closing `}\n`
} Shader_Block;

// Region to excise from C output (forward declarations or shader blocks)
typedef struct {
    const char *start;
    const char *end;
} Excise_Region;

#define MAX_SHADERS 16
#define MAX_EXCISE  64

// Walk the source tracking C brace depth and extract top-level
// `glsl <stage> <Name>` blocks.  Forward declarations (`glsl <stage> <Name>;`)
// are recorded as excise regions but not as shader blocks.
// Returns the number of shader blocks found.
static int extract_shaders(const char *src, Shader_Block *out, int max,
                           Excise_Region *excise, int *excise_count, int excise_max) {
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

        // Look for `glsl ` at depth 0
        if (depth == 0 and strncmp(p, "glsl ", 5) == 0) {
            // Verify it's at the start of a line (only whitespace before it)
            const char *scan = p;
            while (scan > src and scan[-1] != '\n') scan--;
            bool line_ok = true;
            for (const char *c = scan; c < p; c++)
                if (*c != ' ' and *c != '\t') { line_ok = false; break; }
            if (not line_ok) { p++; continue; }

            // Scan backwards further to capture any preceding comment block
            // (lines starting with // immediately above the glsl line)
            const char *blk = scan;
            while (blk > src) {
                // Move to the start of the previous line
                const char *prev_end = blk - 1; // points to '\n' before blk
                if (prev_end < src) break;
                const char *prev_start = prev_end;
                while (prev_start > src and prev_start[-1] != '\n') prev_start--;
                // Check if that line is a comment (skip whitespace then //)
                const char *c = prev_start;
                while (c < prev_end and (*c == ' ' or *c == '\t')) c++;
                if (c + 1 < prev_end and c[0] == '/' and c[1] == '/') {
                    blk = prev_start;  // include this comment line
                } else {
                    break;             // not a comment, stop
                }
            }

            p += 5;

            // Stage (comes first in new syntax)
            const char *ss = p;
            while (*p and *p != ' ' and *p != '\n' and *p != ';') p++;
            size_t sl = (size_t)(p - ss);
            if (sl >= sizeof out[0].stage) sl = sizeof out[0].stage - 1;

            while (*p == ' ') p++;

            // Name
            const char *ns = p;
            while (*p and *p != ' ' and *p != '{' and *p != ';' and *p != '\n') p++;
            size_t nl = (size_t)(p - ns);
            if (nl >= sizeof out[0].name) nl = sizeof out[0].name - 1;

            while (*p == ' ') p++;

            // Check for forward declaration (semicolon)
            if (*p == ';') {
                p++;
                if (*p == '\n') p++;
                // Record as an excise region (remove from C output)
                if (*excise_count < excise_max) {
                    excise[*excise_count].start = blk;
                    excise[*excise_count].end   = p;
                    (*excise_count)++;
                }
                continue;
            }

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

            // Skip past the closing brace, optional comment, and trailing newline
            p = code_end + 1;
            // Skip optional trailing comment like `} // Denoise`
            while (*p == ' ') p++;
            if (p[0] == '/' and p[1] == '/') {
                while (*p and *p != '\n') p++;
            }
            if (*p == '\n') p++;

            memcpy(out[count].stage, ss, sl);
            out[count].stage[sl] = '\0';
            memcpy(out[count].name, ns, nl);
            out[count].name[nl] = '\0';
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

// Write the C-only source: copy `src` but skip every shader block and
// forward-declaration region.  Injects the generated Shader_Path() macro
// so q3.c can reference shader .spv files by their glsl block name.
// When `production` is true, also patch out validation layers and the
// debug utils extension so the binary runs clean without them.
static bool write_c_source(const char *path, const char *src, size_t src_len,
                            const Shader_Block *sh, int nsh,
                            const Excise_Region *ex, int nex,
                            bool production) {

    // Shader_Path macro header injected at the top of the generated source
    const char *hdr =
        "// Generated by sdk.c — Shader_Path resolves glsl shader names to .spv paths\n"
        "#define Shader_Path(name) \"" SPVD "\" #name \".spv\"\n\n";
    size_t hdr_len = strlen(hdr);

    // Merge shader blocks and excise regions into a sorted skip list
    int total_skip = nsh + nex;
    typedef struct { const char *start; const char *end; } Skip;
    Skip *skips = malloc((size_t)total_skip * sizeof(Skip));
    for (int i = 0; i < nsh; i++) { skips[i].start = sh[i].block_start; skips[i].end = sh[i].block_end; }
    for (int i = 0; i < nex; i++) { skips[nsh + i].start = ex[i].start; skips[nsh + i].end = ex[i].end; }
    // Sort by start position
    for (int i = 0; i < total_skip - 1; i++)
        for (int j = i + 1; j < total_skip; j++)
            if (skips[j].start < skips[i].start) { Skip t = skips[i]; skips[i] = skips[j]; skips[j] = t; }

    // Build the full C source into a buffer so we can patch it
    size_t buf_cap = hdr_len + src_len + 256;
    char  *buf     = malloc(buf_cap);
    size_t buf_len = 0;

    // Write the generated header first
    memcpy(buf, hdr, hdr_len);
    buf_len = hdr_len;

    // Copy the source, skipping all excise regions
    const char *cursor = src;
    const char *end    = src + src_len;
    for (int i = 0; i < total_skip; i++) {
        if (skips[i].start > cursor) {
            size_t chunk = (size_t)(skips[i].start - cursor);
            memcpy(buf + buf_len, cursor, chunk);
            buf_len += chunk;
        }
        cursor = skips[i].end;
    }
    if (cursor < end) {
        size_t chunk = (size_t)(end - cursor);
        memcpy(buf + buf_len, cursor, chunk);
        buf_len += chunk;
    }
    buf[buf_len] = '\0';
    free(skips);

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
// The `glsl <stage> <Name> { ... }` blocks in q3.c define embedded shaders.
// sdk.c generates a `Shader_Path(name)` macro into build/q3.c that resolves
// shader names to .spv paths.
//
// This function validates that every Shader_Path(xxx) call in the source
// maps to an extracted glsl shader block, and vice versa.  Mismatches
// produce compiler-style errors with line numbers.

// Find next Shader_Path reference, handling optional whitespace: Shader_Path(x) or Shader_Path (x)
static const char *find_shader_path(const char *p, const char **name_out, size_t *name_len_out) {
    while (*p) {
        const char *sp = strstr(p, "Shader_Path");
        if (not sp) return NULL;
        const char *q = sp + 11;  // skip "Shader_Path"
        while (*q == ' ') q++;    // skip optional whitespace
        if (*q != '(') { p = q; continue; }
        q++;  // skip '('
        while (*q == ' ') q++;
        *name_out = q;
        while (*q and *q != ')' and *q != ' ' and *q != '\n') q++;
        *name_len_out = (size_t)(q - *name_out);
        return sp;
    }
    return NULL;
}

// Find all `glsl <stage> <name>;` forward declarations in the source and collect them
typedef struct { char name[64]; char stage[16]; int line; } Shader_Fwd_Decl;

static int collect_forward_decls(const char *src, Shader_Fwd_Decl *out, int max) {
    int count = 0;
    const char *p = src;
    int line = 1;
    while (*p and count < max) {
        if (*p == '\n') { line++; p++; continue; }
        if (strncmp(p, "glsl ", 5) == 0) {
            // Verify line start (only whitespace before)
            const char *ls = p;
            while (ls > src and ls[-1] != '\n') ls--;
            bool ok = true;
            for (const char *c = ls; c < p; c++)
                if (*c != ' ' and *c != '\t') { ok = false; break; }
            if (ok) {
                const char *q = p + 5;
                // Stage
                const char *ss = q;
                while (*q and *q != ' ' and *q != ';' and *q != '\n') q++;
                size_t sl = (size_t)(q - ss);
                while (*q == ' ') q++;
                // Name
                const char *ns = q;
                while (*q and *q != ' ' and *q != '{' and *q != ';' and *q != '\n') q++;
                size_t nl = (size_t)(q - ns);
                while (*q == ' ') q++;
                if (*q == ';') {
                    // This is a forward declaration
                    if (sl < sizeof out[0].stage) { memcpy(out[count].stage, ss, sl); out[count].stage[sl] = '\0'; }
                    if (nl < sizeof out[0].name)  { memcpy(out[count].name, ns, nl);  out[count].name[nl] = '\0'; }
                    out[count].line = line;
                    count++;
                    p = q + 1;
                    continue;
                }
            }
        }
        p++;
    }
    return count;
}

static bool validate_shader_declarations(const char *src, const Shader_Block *sh, int n) {
    bool ok = true;

    // ── Forward declaration ↔ body consistency ──────────────────
    // Every `glsl <stage> <name>;` forward declaration must have a matching `glsl <stage> <name> { ... }` body,
    // and vice versa. Mismatched names (like Primary_Miss vs Ray_Miss) are caught here.
    Shader_Fwd_Decl fwd[64];
    int fwd_count = collect_forward_decls(src, fwd, 64);

    // Check: every forward declaration must have a matching body
    for (int f = 0; f < fwd_count; f++) {
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (strcmp(sh[i].name, fwd[f].name) == 0) {
                // Also verify the stage matches
                if (strcmp(sh[i].stage, fwd[f].stage) != 0) {
                    nob_log(NOB_ERROR,
                        "q3.c:%d: forward declaration 'glsl %s %s;' has stage mismatch — "
                        "body is 'glsl %s %s'",
                        fwd[f].line, fwd[f].stage, fwd[f].name, sh[i].stage, sh[i].name);
                    ok = false;
                }
                found = true;
                break;
            }
        }
        if (not found) {
            nob_log(NOB_ERROR,
                "q3.c:%d: forward declaration 'glsl %s %s;' has no matching body — "
                "add a 'glsl %s %s { ... }' block",
                fwd[f].line, fwd[f].stage, fwd[f].name, fwd[f].stage, fwd[f].name);
            ok = false;
        }
    }

    // Check: every body must have a matching forward declaration
    for (int i = 0; i < n; i++) {
        bool found = false;
        for (int f = 0; f < fwd_count; f++) {
            if (strcmp(sh[i].name, fwd[f].name) == 0) { found = true; break; }
        }
        if (not found) {
            nob_log(NOB_ERROR,
                "q3.c: shader body 'glsl %s %s { ... }' has no forward declaration — "
                "add 'glsl %s %s;' in the §12 Shaders section",
                sh[i].stage, sh[i].name, sh[i].stage, sh[i].name);
            ok = false;
        }
    }

    // ── Shader_Path() ↔ body consistency ────────────────────────
    // Forward check: every Shader_Path(xxx) call must match an extracted block
    const char *p = src;
    const char *name;
    size_t name_len;
    while ((p = find_shader_path(p, &name, &name_len)) != NULL) {
        const char *call_pos = p;
        p = name + name_len;
        if (name_len == 0 or name_len > 63) continue;

        char expected[64];
        memcpy(expected, name, name_len);
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
                "q3.c:%d: Shader_Path(%s) has no matching "
                "'glsl %s ...' block", line, expected, expected);
            ok = false;
        }
    }

    // Reverse check: every extracted shader must have a Shader_Path() reference
    for (int i = 0; i < n; i++) {
        // Search for Shader_Path(name) or Shader_Path (name) with the shader's name
        const char *s = src;
        const char *sname;
        size_t sname_len;
        bool found = false;
        while ((s = find_shader_path(s, &sname, &sname_len)) != NULL) {
            s = sname + sname_len;
            if (sname_len == strlen(sh[i].name) and memcmp(sname, sh[i].name, sname_len) == 0) {
                found = true;
                break;
            }
        }
        if (not found) {
            nob_log(NOB_ERROR,
                "q3.c: glsl '%s' extracted but never loaded — "
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
    const char *src_file = BUILD "q3.c";

    switch (plat) {

    // Windows: mingw cross-compiler targeting x86_64
    case PLATFORM_WINDOWS:
        nob_cmd_append(cmd,
            "x86_64-w64-mingw32-gcc",
            "-O3", "-ffast-math", "-flto",
            "-Wall", "-Wextra",
            "-o", BUILD "release/q3.exe",
            src_file,
            "-I/usr/x86_64-w64-mingw32/include/SDL2",
            "-lmingw32", "-lSDL2main", "-lSDL2",
            "-lvulkan-1", "-lm", "-lz", "-lOpenAL32",
            "-mwindows");
        break;

    // macOS: Homebrew paths, MoltenVK, framework OpenAL
    case PLATFORM_MACOS:
        nob_cmd_append(cmd,
            "cc",
            "-O3", "-ffast-math", "-flto",
            "-Wall", "-Wextra",
            production ? "-DNDEBUG" : "-g",
            "-o", production ? BUILD "release/q3_macos" : BUILD "q3",
            src_file,
            "-I/opt/homebrew/include/SDL2", "-I/opt/homebrew/include",
            "-D_REENTRANT",
            "-L/opt/homebrew/lib",
            "-lSDL2", "-lvulkan", "-lm", "-lz",
            "-framework", "OpenAL");
        break;

    // Debian / Ubuntu: standard system paths
    case PLATFORM_DEBIAN:
        nob_cmd_append(cmd,
            "cc",
            "-O3", "-march=x86-64-v2", "-ffast-math", "-flto",
            "-Wall", "-Wextra",
            production ? "-DNDEBUG" : "-g",
            "-o", production ? BUILD "release/q3_debian" : BUILD "q3",
            src_file,
            "-I/usr/include/SDL2", "-D_REENTRANT",
            "-lSDL2", "-lvulkan", "-lm", "-lz",
            "-lopenal");
        break;

    // Arch Linux: same layout as Debian
    case PLATFORM_ARCH:
        nob_cmd_append(cmd,
            "cc",
            "-O3", "-march=x86-64-v2", "-ffast-math", "-flto",
            "-Wall", "-Wextra",
            production ? "-DNDEBUG" : "-g",
            "-o", production ? BUILD "release/q3_arch" : BUILD "q3",
            src_file,
            "-I/usr/include/SDL2", "-D_REENTRANT",
            "-lSDL2", "-lvulkan", "-lm", "-lz",
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
            "-o", production ? BUILD "release/q3" : BUILD "q3",
            src_file,
            "-I/usr/include/SDL2", "-D_REENTRANT",
            "-lSDL2", "-lvulkan", "-lm", "-lz",
            "-lopenal");
        break;
    }
}

// ── shader tuning defines ─────────────────────────────────────────
//
// q3.c contains a block between SHADER_TUNING_BEGIN / SHADER_TUNING_END
// with `#define NAME VALUE` lines.  These are extracted and passed as
// `-DNAME=VALUE` flags to glslangValidator so shaders can reference them.
//
// Command-line overrides: ./sdk -DVNDF_ALPHA_FLOOR=0.02 -DCAS_AMOUNT=0.5

#define MAX_DEFINES 64

typedef struct {
    char name[64];
    char value[64];
} Define_Entry;

// Extract #define lines from the SHADER_TUNING_BEGIN..END region of q3.c
static int extract_tuning_defines(const char *src, Define_Entry *out, int max) {
    int count = 0;
    const char *begin = strstr(src, "SHADER_TUNING_BEGIN");
    const char *end   = strstr(src, "SHADER_TUNING_END");
    if (not begin or not end or end <= begin) return 0;

    const char *p = begin;
    while (p < end and count < max) {
        const char *def = strstr(p, "#define ");
        if (not def or def >= end) break;
        def += 8;
        while (*def == ' ') def++;

        // Parse name
        const char *name_start = def;
        while (*def and *def != ' ' and *def != '\t' and *def != '\n') def++;
        size_t name_len = (size_t)(def - name_start);
        if (name_len == 0 or name_len >= 64) { p = def; continue; }

        // Skip whitespace
        while (*def == ' ' or *def == '\t') def++;

        // Parse value (up to whitespace or comment)
        const char *val_start = def;
        while (*def and *def != ' ' and *def != '\t' and *def != '\n'
               and not (def[0] == '/' and def[1] == '/')) def++;
        size_t val_len = (size_t)(def - val_start);
        if (val_len == 0 or val_len >= 64) { p = def; continue; }

        memcpy(out[count].name,  name_start, name_len);
        out[count].name[name_len] = '\0';
        memcpy(out[count].value, val_start,  val_len);
        out[count].value[val_len] = '\0';
        count++;
        p = def;
    }
    return count;
}

// Apply a -DNAME=VALUE override: update existing entry or append new one
static int apply_define_override(Define_Entry *defs, int count, int max,
                                 const char *name, size_t nlen,
                                 const char *value) {
    for (int i = 0; i < count; i++) {
        if (strlen(defs[i].name) == nlen and memcmp(defs[i].name, name, nlen) == 0) {
            snprintf(defs[i].value, sizeof defs[i].value, "%s", value);
            return count;
        }
    }
    if (count < max) {
        memcpy(defs[count].name, name, nlen);
        defs[count].name[nlen] = '\0';
        snprintf(defs[count].value, sizeof defs[count].value, "%s", value);
        return count + 1;
    }
    return count;
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
    int  cli_overrides = 0;              // Count of -D overrides for logging
    Define_Entry cli_defs[MAX_DEFINES];  // Temporary storage for -D args
    int cli_def_count = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--production") == 0 or strcmp(argv[i], "--prod") == 0)
            production = true;
        else if (strcmp(argv[i], "--windows") == 0)  target = PLATFORM_WINDOWS;
        else if (strcmp(argv[i], "--macos") == 0)    target = PLATFORM_MACOS;
        else if (strcmp(argv[i], "--debian") == 0)   target = PLATFORM_DEBIAN;
        else if (strcmp(argv[i], "--arch") == 0)     target = PLATFORM_ARCH;
        else if (strcmp(argv[i], "--all") == 0)      build_all = true;
        else if (strncmp(argv[i], "-D", 2) == 0 and cli_def_count < MAX_DEFINES) {
            const char *d = argv[i] + 2;
            const char *eq = strchr(d, '=');
            if (eq and (size_t)(eq - d) < 64) {
                memcpy(cli_defs[cli_def_count].name, d, (size_t)(eq - d));
                cli_defs[cli_def_count].name[eq - d] = '\0';
                snprintf(cli_defs[cli_def_count].value, 64, "%s", eq + 1);
                cli_def_count++;
                cli_overrides++;
            }
        }
    }

    // Read the single-source file
    Nob_String_Builder sb = {0};
    if (not nob_read_entire_file(MAIN, &sb)) return 1;
    nob_sb_append_null(&sb);

    // Extract shader tuning defines from SHADER_TUNING_BEGIN..END region
    Define_Entry shader_defs[MAX_DEFINES];
    int shader_def_count = extract_tuning_defines(sb.items, shader_defs, MAX_DEFINES);

    // Apply command-line -D overrides (last write wins)
    for (int i = 0; i < cli_def_count; i++)
        shader_def_count = apply_define_override(shader_defs, shader_def_count, MAX_DEFINES,
                                                  cli_defs[i].name, strlen(cli_defs[i].name),
                                                  cli_defs[i].value);

    // Pre-build -DNAME=VALUE argument strings for glslangValidator
    char *define_args[MAX_DEFINES];
    for (int i = 0; i < shader_def_count; i++) {
        define_args[i] = malloc(140);
        snprintf(define_args[i], 140, "-D%s=%s", shader_defs[i].name, shader_defs[i].value);
    }
    nob_log(NOB_INFO, "shader tuning: %d defines (%d overridden from cli)", shader_def_count, cli_overrides);

    // Extract top-level shader blocks and forward declarations
    Shader_Block shaders[MAX_SHADERS];
    Excise_Region excise[MAX_EXCISE];
    int excise_count = 0;
    int n = extract_shaders(sb.items, shaders, MAX_SHADERS, excise, &excise_count, MAX_EXCISE);
    nob_log(NOB_INFO, "extracted %d shader(s), %d forward decl(s) from %s", n, excise_count, MAIN);

    // Validate that every Shader_Path() call matches an extracted glsl shader block
    if (not validate_shader_declarations(sb.items, shaders, n)) return 1;

    // Write the C-only source (shader blocks + forward decls excised, Shader_Path macro injected)
    if (not write_c_source(BUILD "q3.c", sb.items, sb.count - 1,
                           shaders, n, excise, excise_count, production)) return 1;

    // Write extracted shaders to build/shaders/ (preprocessing and/or/not to GLSL operators)
    char glsl_paths[MAX_SHADERS][256], spv_paths[MAX_SHADERS][256];
    for (int i = 0; i < n; i++) {
        snprintf(glsl_paths[i], sizeof glsl_paths[i], SPVD "%s.glsl", shaders[i].name);
        snprintf(spv_paths[i],  sizeof spv_paths[i],  SPVD "%s.spv",  shaders[i].name);
        if (not write_shader_glsl(glsl_paths[i], shaders[i].code, shaders[i].code_len)) return 1;
    }

    // Compile each shader to SPIR-V (injecting tuning defines as -D flags)
    bool force_recompile = (cli_overrides > 0);  // -D overrides bypass staleness check
    Nob_Cmd cmd = {0};
    for (int i = 0; i < n; i++) {
        if (not force_recompile and not spv_stale(spv_paths[i], MAIN)) continue;
        nob_cmd_append(&cmd, "glslangValidator",
            "-V", "--target-env", "vulkan1.3",
            "-S", shaders[i].stage,
            "-o", spv_paths[i]);
        for (int d = 0; d < shader_def_count; d++)
            nob_cmd_append(&cmd, define_args[d]);
        nob_cmd_append(&cmd, glsl_paths[i]);
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
