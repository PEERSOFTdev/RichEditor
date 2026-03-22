# Executable Size Optimization — Reference

Comprehensive analysis of RichEditor binary size, performed March 2026.
All measurements use the MXE cross-compiler (GCC 11.5.0, x86_64-w64-mingw32.static).

This document is the definitive reference for size-related decisions.  Do not
repeat the research; consult this document instead.

---

## Current State (after optimizations applied)

| Build | Size |
|---|---|
| MinGW debug (with `-g`) | ~1,005 KB |
| MinGW stripped (`make strip`) | **348,160 bytes (340 KB)** |
| MSVC release (`build_msvc.bat`) | ~294.5 KB |

### Why MSVC is ~46 KB smaller

1. **`/OPT:ICF`** (Identical COMDAT Folding) — MSVC's linker merges
   byte-identical functions.  GNU ld has no equivalent.
2. **No pthread overhead** — MSVC uses native Win32 threading.  MXE's GCC
   links ~12 KB of libwinpthread because it was built with
   `--enable-threads=posix`.
3. **Leaner CRT** — MSVC's static CRT (`/MT`) is smaller than MinGW's
   libstdc++ + libgcc + libwinpthread combination.
4. **More aggressive LTO** — MSVC's `/LTCG` + `/OPT:REF` + `/OPT:ICF`
   eliminates more dead code than GCC's `-flto` + `--gc-sections`.

---

## MinGW Binary Anatomy

### PE section breakdown (stripped)

| Section | Size | Purpose |
|---|---|---|
| `.text` | 231.6 KB | Executable code |
| `.rdata` | 39.5 KB | Read-only data (strings, vtables) |
| `.pdata` | 14.9 KB | x86-64 unwind descriptors (mandatory) |
| `.xdata` | 13.2 KB | x86-64 unwind data (mandatory) |
| `.rsrc` | 23.9 KB | Resources (dialogs, string tables, system icon) |
| `.idata` | 10.0 KB | Import tables |
| `.data` | 1.7 KB | Initialized data |
| `.reloc` | 1.2 KB | Base relocations (ASLR) |
| `.bss` | 2,064 KB | Uninitialized globals (zero file space) |

### Code breakdown (`.text` section)

| Category | Approx. Size | Share |
|---|---|---|
| Application code | ~124 KB | ~53% |
| libstdc++ (std::wstring, std::vector, etc.) | ~67 KB | ~29% |
| pthread (from MXE posix threading model) | ~12 KB | ~5% |
| CRT + exception runtime + misc | ~32 KB | ~13% |

Application code is only about half of `.text`.  The rest is statically linked
library overhead, most of which cannot be eliminated without replacing
std::wstring/std::vector with C equivalents (impractical).

---

## Optimizations Applied (this session)

### 1. Fix `--gc-sections` not reaching the linker — saved 5.0 KB

`-Wl,--gc-sections` was in `CFLAGS` but not in `LDFLAGS`.  During compilation
with `-c`, linker flags are silently ignored.  The link step never received
`--gc-sections`, so dead library functions prepared by `-ffunction-sections
-fdata-sections` were never removed.

Also added `-flto -Os` to `LDFLAGS` so the LTO pass uses the correct
optimization level.

### 2. Add `-fno-exceptions` — saved 2.0 KB

The source code contains zero `throw`, `catch`, or `try` statements (7 grep
hits are all in comments).  With `-fno-exceptions`, allocation failure in
std::wstring/std::vector calls `abort()` instead of throwing `std::bad_alloc`.
This is acceptable for a desktop application.

The flag must appear in both `CFLAGS` (compile step) and `LDFLAGS` (link step)
for LTO to honour it.

### 3. Remove redundant MSVC flags — no size change

Removed `/Os` and `/Gy` from `build_msvc.bat` release CFLAGS.  Both are
already implied by `/O1` (`/O1` = `/Og /Os /Oy /Ob2 /GF /Gy`).  No binary
size change; purely cosmetic cleanup.

---

## Optimizations Tested — Zero Measured Impact

These flags were tested and showed **0 KB savings** because LTO already
subsumes their effect:

| Flag | What it does | Why no effect |
|---|---|---|
| `-fno-rtti` | Remove C++ RTTI metadata | LTO already eliminates unreachable typeinfo |
| `-fmerge-all-constants` | Merge identical constants aggressively | LTO + linker already merge constants |
| `-fno-stack-protector` | Remove stack canary checks | No canary code was emitted for this program |
| `-fno-threadsafe-statics` | Remove guard variables for function-local statics | No guarded statics exist |
| `-fno-ident` | Remove `.comment` section | Stripped away by `strip` already |

These flags are harmless to add but provide no measurable benefit.

---

## Optional / Risky Optimizations (not applied)

### `-fno-unwind-tables -fno-asynchronous-unwind-tables` — saves 3.5 KB

Reduces `.pdata` by 1.6 KB and `.xdata` by 2.0 KB by omitting unwind info for
app functions compiled with these flags.

**Risk:** On x86-64 Windows, `.pdata`/`.xdata` are used by SEH for stack
unwinding.  Without unwind info, a hardware exception (access violation, divide
by zero) in app code will not unwind cleanly.  For a desktop text editor this
is arguably acceptable since such crashes are already fatal, but it violates
the x64 ABI contract.

**Verdict:** Not recommended unless binary size is critically constrained.

### Eliminate pthread overhead — saves ~12 KB (not actionable)

MXE's GCC was built with `--enable-threads=posix`, which statically links
libwinpthread (~12 KB of `.text`).  The application is single-threaded and does
not need POSIX threads.

**Cannot be fixed with compiler flags.**  Would require rebuilding the
cross-compiler with `--enable-threads=win32`, or using a different MinGW-w64
distribution.  Not actionable within the current toolchain.

### Replace std::wstring / std::vector with C equivalents — saves ~20-40 KB

libstdc++ accounts for ~67 KB of `.text`.  Replacing `std::wstring` (35 uses)
and `std::vector` with manual Win32 buffer management would eliminate most of
this overhead.

**Not practical.**  The refactor would touch hundreds of lines, reduce code
clarity, and introduce manual memory management bugs.  The size saving does not
justify the maintenance cost.

---

## MSVC Build — Already Optimal

The MSVC release build (`build_msvc.bat`) is already at its ceiling.  Every
relevant optimization is enabled:

| Flag | Purpose |
|---|---|
| `/O1` | Minimize size (implies `/Os /Oy /Ob2 /GF /Gy`) |
| `/GL` + `/LTCG` | Whole program optimization |
| `/OPT:REF` | Remove unreferenced sections |
| `/OPT:ICF` | Identical COMDAT folding |
| `/GS-` | No buffer security checks |
| `/GR-` | No RTTI |
| `/Zc:inline` | Remove unreferenced COMDATs |
| `/Zc:threadSafeInit-` | No thread-safe statics |
| `/MT` | Static CRT |

**Tested and confirmed no further savings:**

- `/Gw` (global data optimization) — tested, no size reduction.
- `/MERGE:.rdata=.text` (section merging) — tested, no size reduction.

The MSVC linker's `/OPT:ICF` + `/LTCG` combination is already more aggressive
than anything available in GNU ld.

---

## Things That Cannot Be Reduced

| Item | Size | Why |
|---|---|---|
| `.pdata` + `.xdata` | 28.1 KB | Mandatory on x86-64 Windows (SEH/ABI) |
| `.rsrc` (resources) | 23.9 KB | Bilingual string tables + dialogs; system icon only, no custom icon files |
| `.idata` (imports) | 10.0 KB | Fixed cost of Win32 API imports |
| `.reloc` | 1.2 KB | Required for ASLR; removing disables ASLR |
| `.bss` (globals) | 2,064 KB runtime | Zero file space; large due to EXTENDED_PATH_MAX (32,767) buffers for UNC path support |

---

## Code-Level Opportunities (estimated, not yet implemented)

These would require source code changes and have not been tested.  Estimates
are based on code analysis.

### Duplicated code blocks — estimated 5-10 KB

| Pattern | Repetitions | Est. Savings |
|---|---|---|
| Template variable expansion (date/time format blocks in `ExpandTemplateVariables`) | 8 nearly identical blocks | 2-3 KB |
| `LoadSettings()` read-default-parse pattern | ~20 repetitions | 1-2 KB |
| `CreateRichEditControl()` DLL fallback blocks | 5 similar blocks | 1-2 KB |
| `AddToFindHistory()` / `AddToReplaceHistory()` identical logic | 2 copies | 0.5 KB |
| Strip trailing newline pattern | 3 copies | 0.3 KB |

### SendMessage wrapper functions — estimated 2-4 KB

187 `SendMessage` calls to RichEdit controls.  The top candidates for wrappers:

| Message | Count | Boilerplate per call |
|---|---|---|
| `EM_EXGETSEL` / `EM_EXSETSEL` | 45 combined | CHARRANGE struct setup |
| `EM_GETTEXTRANGE` | 17 | TEXTRANGE struct setup |
| `EM_GETTEXTLENGTHEX` | 8 | GETTEXTLENGTHEX struct setup |

Introducing 3-4 inline helper functions would deduplicate the struct setup.

### String constant deduplication — estimated 0-2 KB

`L"Settings"` appears 64 times.  The linker likely merges identical wide string
literals, but defining `static const WCHAR*` named constants would guarantee
dedup and improve readability.  Actual savings depend on whether the linker
already merges them (LTO may handle this).

---

## Guidelines for Keeping the Binary Small

1. **Do not add C++ exception handling** (`throw`/`catch`/`try`).
   `-fno-exceptions` is now a build flag.

2. **Prefer Win32 APIs over C++ standard library** for new code.  Each new
   `std::` type instantiation pulls in additional libstdc++ code.

3. **Avoid new static libraries.**  Each statically linked library adds its own
   overhead even if only one function is used (though LTO mitigates this).

4. **Refactor duplicated code** rather than copy-pasting.  Helper functions
   compress better under LTO and reduce `.text` section growth.

5. **Keep resources minimal.**  String tables are efficient; avoid embedding
   large bitmaps or custom icons unless necessary.

6. **LTO is the single most important optimization.**  It reduces the binary
   from ~2.4 MB to ~340 KB.  Never remove `-flto` from the build.

7. **`--gc-sections` must be in LDFLAGS, not CFLAGS.**  The linker flag is
   ignored during compilation (`-c`).

8. **The MSVC build is the size reference.**  At ~294.5 KB it represents the
   practical floor for this codebase.  The ~46 KB gap from MinGW is structural
   (pthread, libstdc++, no ICF) and cannot be closed without changing the
   cross-compiler or rewriting C++ standard library usage.

---

## Test Results Log

All tests performed on Ubuntu 24.04 with MXE GCC 11.5.0, March 2026.
Sizes are after `strip`.  Baseline is the pre-optimization Makefile.

| Build Configuration | Stripped Size | Delta |
|---|---|---|
| Baseline (old Makefile) | 355,328 | — |
| Fix LDFLAGS (`--gc-sections -flto -Os`) | 350,208 | **-5,120** |
| + `-fno-exceptions` | 348,160 | **-7,168** |
| + `-fno-rtti` | 348,160 | -7,168 (no change) |
| + `-fmerge-all-constants -fno-ident` | 348,160 | -7,168 (no change) |
| + `-fno-stack-protector -fno-threadsafe-statics` | 348,160 | -7,168 (no change) |
| + `-fno-unwind-tables -fno-async-unwind-tables` | 344,576 | -10,752 |
| No LTO, with `--gc-sections` | 2,464,256 | +2,108,928 |
| No LTO, no `--gc-sections` (naive) | 357,376 | +2,048 |
