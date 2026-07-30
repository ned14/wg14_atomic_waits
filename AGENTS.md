# Agentic coding guidelines

1. All source and header files MUST be kept compatible with the 2011 ISO
C standard, except when testing C++ header-only compilation in `test/`.
C++ source or headers must NOT appear under `include/` or `src/`.
2. Run `clang-format` on every changed header and source file. Do NOT run
`clang-format` on cmake files.
3. When building and testing, extract what to do for the current platform
from `.github/workflows/ci.yml`.
4. C++ is permitted in `test/` solely for compile-testing the public header
and verifying `extern "C"` linkage. Do NOT use C++ in any source or header
file under `include/` or `src/`.
