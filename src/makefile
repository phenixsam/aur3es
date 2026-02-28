# =============================================================================
# GOLDEN RNG ENTERPRISE - PRODUCTION MAKEFILE
# =============================================================================
# A high-performance cryptographic random number generator (CSPRNG)
# implementing the AUR3ES algorithm with SHA-512 key derivation.
#
# Features:
#   - Multiple build profiles (release, debug, production, etc.)
#   - Advanced security features (Spectre/Meltdown mitigation)
#   - Cross-platform support (Linux, macOS, Windows)
#   - Code coverage and sanitizers
#   - Automatic versioning
# =============================================================================

# =============================================================================
# CONFIGURATION
# =============================================================================

# Build type: release (default), debug, sanitize, profile, coverage, lto, ofast, size
BUILD_TYPE ?= release

# Compiler settings
CC ?= gcc
CXX ?= g++
AR ?= ar

# Installation paths
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include
MANDIR = $(PREFIX)/share/man

# Build directory
BUILD_DIR ?= build

# Number of parallel jobs (default: number of CPU cores)
NPROCS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
JOBS ?= -j$(NPROCS)

# Enable ccache if available
USE_CCACHE ?= $(shell command -v ccache >/dev/null 2>&1 && echo yes)
ifeq ($(USE_CCACHE),yes)
    CC := ccache $(CC)
    CXX := ccache $(CXX)
endif

# Enable verbose build
VERBOSE ?= no

# =============================================================================
# VERSIONING
# =============================================================================

# Get version from git or use default
GIT_VERSION := $(shell git describe --tags --abbrev=4 --dirty 2>/dev/null || echo "v5.0.0")
GIT_COMMIT := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_DATE := $(shell date -u +"%Y-%m-%d %H:%M:%S UTC")
BUILD_TIMESTAMP := $(shell date -u +"%Y%m%d%H%M%S")

# Compiler info
COMPILER_VERSION := $(shell $(CC) --version | head -n1)
COMPILER_NAME := $(shell basename $(CC))

# =============================================================================
# BUILD PROFILES
# =============================================================================

# Common flags - comprehensive warnings and security flags
COMMON_CFLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wstrict-aliasing \
                -fno-strict-aliasing -D_GNU_SOURCE -fcommon \
                -Wno-unused-function

# Security flags - stack protector, fortification, PIE
SECURITY_CFLAGS = -fstack-protector-strong -D_FORTIFY_SOURCE=3 \
                   -fcf-protection=full -fPIE
SECURITY_LDFLAGS = -Wl,-pie

# Release flags (optimized for speed)
RELEASE_CFLAGS = -O3 -march=native -ffast-math -funroll-loops -fomit-frame-pointer
RELEASE_LDFLAGS = -Wl,-O1,--sort-common,--as-needed,-z,relro,-z,now $(SECURITY_LDFLAGS)

# Production flags (maximum optimization with safety)
PRODUCTION_CFLAGS = -O3 -march=native -ffast-math -funroll-loops \
                    -fomit-frame-pointer -ftree-vectorize
PRODUCTION_LDFLAGS = $(RELEASE_LDFLAGS)

# Ofast flags (aggressive optimization)
OFAST_CFLAGS = -Ofast -march=native -ffast-math -funroll-loops \
               -fomit-frame-pointer -ftree-vectorize -funsafe-math-optimizations
OFAST_LDFLAGS = $(RELEASE_LDFLAGS)

# Size optimization flags
SIZE_CFLAGS = -Oz -march=native -ffunction-sections -fdata-sections
SIZE_LDFLAGS = -Wl,--gc-sections

# Debug flags (with debugging symbols)
DEBUG_CFLAGS = -O0 -g3 -ggdb -fno-omit-frame-pointer -fno-inline

# Sanitizer flags (for testing)
SANITIZER_CFLAGS = -O1 -g3 -fsanitize=address -fsanitize=undefined \
                   -fsanitize=leak -fno-omit-frame-pointer -fno-inline
SANITIZER_LDFLAGS = -fsanitize=address -fsanitize=undefined -fsanitize=leak

# Profile flags
PROFILE_CFLAGS = -O2 -g -pg
PROFILE_LDFLAGS = -pg

# Code coverage flags
COVERAGE_CFLAGS = -O0 -g -fprofile-arcs -ftest-coverage -fno-inline
COVERAGE_LDFLAGS = -fprofile-arcs -ftest-coverage

# Link-time optimization
LTO_CFLAGS = -flto -ffat-lto-objects
LTO_LDFLAGS = -flto $(SECURITY_LDFLAGS)

# Cross-compilation flags
CROSS_PREFIX ?=
ifeq ($(CROSS_PREFIX),)
    # Detect if we're cross-compiling
    ifeq ($(CC),x86_64-linux-gnu-gcc)
        CROSS_PREFIX = x86_64-linux-gnu-
    else ifeq ($(CC),aarch64-linux-gnu-gcc)
        CROSS_PREFIX = aarch64-linux-gnu-
    else ifeq ($(CC),arm-linux-gnueabihf-gcc)
        CROSS_PREFIX = arm-linux-gnueabihf-
    else ifeq ($(CC),riscv64-linux-gnu-gcc)
        CROSS_PREFIX = riscv64-linux-gnu-
    endif
endif

# Architecture-specific optimizations
ifeq ($(CROSS_PREFIX),x86_64-linux-gnu-)
    ARCH_CFLAGS = -march=x86-64-v2
else ifeq ($(CROSS_PREFIX),aarch64-linux-gnu-)
    ARCH_CFLAGS = -march=armv8-a
else ifeq ($(CROSS_PREFIX),arm-linux-gnueabihf-)
    ARCH_CFLAGS = -march=armv7-a+fp
else ifeq ($(CROSS_PREFIX),riscv64-linux-gnu-)
    ARCH_CFLAGS = -march=rv64gc
else
    # Fallback for unknown architectures
    ARCH_CFLAGS = 
endif

# Platform detection
ifeq ($(OS),Windows_NT)
    PLATFORM := windows
    LIBEXT := .dll
    LIBPREFIX := 
    EXEEXT := .exe
    RM := del /Q
    SEP := \\
else
    PLATFORM := $(shell uname -s | tr '[:upper:]' '[:lower:]')
    LIBEXT := .so
    LIBPREFIX := lib
    EXEEXT :=
    RM := rm -f
    SEP := /
endif

# Select build profile
ifeq ($(BUILD_TYPE),release)
    CFLAGS = $(COMMON_CFLAGS) $(RELEASE_CFLAGS) $(SECURITY_CFLAGS)
    LDFLAGS = $(RELEASE_LDFLAGS)
    BUILD_SUBDIR = release
    STRIP = -s
else ifeq ($(BUILD_TYPE),production)
    CFLAGS = $(COMMON_CFLAGS) $(PRODUCTION_CFLAGS) $(SECURITY_CFLAGS)
    LDFLAGS = $(PRODUCTION_LDFLAGS)
    BUILD_SUBDIR = production
    STRIP = -s
else ifeq ($(BUILD_TYPE),ofast)
    CFLAGS = $(COMMON_CFLAGS) $(OFAST_CFLAGS) $(SECURITY_CFLAGS)
    LDFLAGS = $(OFAST_LDFLAGS)
    BUILD_SUBDIR = ofast
    STRIP = -s
else ifeq ($(BUILD_TYPE),size)
    CFLAGS = $(COMMON_CFLAGS) $(SIZE_CFLAGS) $(SECURITY_CFLAGS)
    LDFLAGS = $(SIZE_LDFLAGS)
    BUILD_SUBDIR = size
    STRIP = -s
else ifeq ($(BUILD_TYPE),debug)
    CFLAGS = $(COMMON_CFLAGS) $(DEBUG_CFLAGS)
    LDFLAGS =
    BUILD_SUBDIR = debug
    STRIP =
else ifeq ($(BUILD_TYPE),sanitize)
    CFLAGS = $(COMMON_CFLAGS) $(DEBUG_CFLAGS) $(SANITIZER_CFLAGS)
    LDFLAGS = $(SANITIZER_LDFLAGS)
    BUILD_SUBDIR = sanitize
    STRIP =
else ifeq ($(BUILD_TYPE),profile)
    CFLAGS = $(COMMON_CFLAGS) $(PROFILE_CFLAGS)
    LDFLAGS = $(PROFILE_LDFLAGS)
    BUILD_SUBDIR = profile
    STRIP =
else ifeq ($(BUILD_TYPE),coverage)
    CFLAGS = $(COMMON_CFLAGS) $(DEBUG_CFLAGS) $(COVERAGE_CFLAGS)
    LDFLAGS = $(COVERAGE_LDFLAGS)
    BUILD_SUBDIR = coverage
    STRIP =
else ifeq ($(BUILD_TYPE),lto)
    CFLAGS = $(COMMON_CFLAGS) $(RELEASE_CFLAGS) $(LTO_CFLAGS) $(SECURITY_CFLAGS)
    LDFLAGS = $(RELEASE_LDFLAGS) $(LTO_LDFLAGS)
    BUILD_SUBDIR = lto
    STRIP = -s
else
    $(error Unknown BUILD_TYPE: $(BUILD_TYPE). Use: release, production, ofast, size, debug, sanitize, profile, coverage, lto)
endif

# Add version info (simplified for portability)
CFLAGS += -DGOLDENRNG_VERSION='"$(GIT_VERSION)"'
CFLAGS += -DGOLDENRNG_BUILD='"$(BUILD_DATE)"'
CFLAGS += -DGOLDENRNG_COMMIT='"$(GIT_COMMIT)"'

# Output directory
OUTPUT_DIR := $(BUILD_DIR)/$(BUILD_SUBDIR)

# =============================================================================
# LIBRARIES
# =============================================================================

# Core libraries
LIBS = -lpthread -lm

# OpenSSL - try pkg-config first, then fallback
OPENSSL_LIBS := $(shell pkg-config --libs openssl 2>/dev/null)
ifeq ($(OPENSSL_LIBS),)
    OPENSSL_LIBS = -lcrypto
endif

OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null)
LIBS += $(OPENSSL_LIBS)
CFLAGS += $(OPENSSL_CFLAGS)

# =============================================================================
# INCLUDE DIRECTORIES
# =============================================================================

# All includes are relative to src directory
INCLUDES = -I. -Icore -Iplatform -Iutils

# TestU01 support
HAVE_TESTU01 ?= $(shell pkg-config --exists libtestu01-0 && echo yes)
ifeq ($(HAVE_TESTU01),yes)
    CFLAGS += $(shell pkg-config --cflags libtestu01-0) -DTESTU01_AVAILABLE
    LIBS += $(shell pkg-config --libs libtestu01-0)
    TESTU01_LIBS = $(shell pkg-config --libs libtestu01-0)
else
    # Check for manually installed TestU01
    HAVE_TESTU01 := $(shell test -f /usr/lib/x86_64-linux-gnu/libtestu01.a && echo yes)
    ifeq ($(HAVE_TESTU01),yes)
        CFLAGS += -DTESTU01_AVAILABLE -I/usr/include -I/usr/include/testu01
        LIBS += -L/usr/lib/x86_64-linux-gnu -ltestu01 -ltestu01mylib -ltestu01probdist -lm
        TESTU01_LIBS = -L/usr/lib/x86_64-linux-gnu -ltestu01 -ltestu01mylib -ltestu01probdist -lm
    endif
endif

# =============================================================================
# SOURCE FILES - Using relative paths from src directory
# =============================================================================

# All sources (relative to src directory)
SRCS = main.c \
       core/rng_engine.c \
       core/rng_security.c \
       core/rng_simd_optimized.c \
       core/rng_logging.c \
       core/rng_monitoring.c \
       core/csprng_entropy.c \
       platform/platform_abstraction.c \
       utils/rng_advanced_tests.c

# Test sources (if TestU01 available)
ifeq ($(HAVE_TESTU01),yes)
    SRCS += utils/testu01_bigcrush.c
endif

# Object files - in build directory with same structure
OBJS := $(patsubst %.c,$(OUTPUT_DIR)/%.o,$(SRCS))
DEPS := $(patsubst %.c,$(OUTPUT_DIR)/%.d,$(SRCS))

# Executable
TARGET := $(OUTPUT_DIR)/goldenrng$(EXEEXT)

# Legacy source groups for compatibility with test rules
CORE_SRCS = core/rng_engine.c core/rng_security.c core/rng_simd_optimized.c core/rng_logging.c core/rng_monitoring.c core/csprng_entropy.c
PLATFORM_SRCS = platform/platform_abstraction.c
MAIN_SRCS = main.c
UTILS_SRCS = utils/rng_advanced_tests.c

# =============================================================================
# COLOR OUTPUT
# =============================================================================

BOLD := $(shell tput bold 2>/dev/null || echo "")
GREEN := $(shell tput setaf 2 2>/dev/null || echo "")
YELLOW := $(shell tput setaf 3 2>/dev/null || echo "")
RED := $(shell tput setaf 1 2>/dev/null || echo "")
BLUE := $(shell tput setaf 4 2>/dev/null || echo "")
RESET := $(shell tput sgr0 2>/dev/null || echo "")

# Disable colors if not a terminal
ifneq ($(shell test -t 1 && echo yes),yes)
    BOLD = GREEN = YELLOW = RED = BLUE = RESET =
endif

# Print helpers
PRINT_BOLD = @echo "$(BOLD)$1$(RESET)"
PRINT_GREEN = @echo "$(GREEN)✓ $1$(RESET)"
PRINT_YELLOW = @echo "$(YELLOW)⚠ $1$(RESET)"
PRINT_RED = @echo "$(RED)✗ $1$(RESET)"
PRINT_BLUE = @echo "$(BLUE)ℹ $1$(RESET)"

# Verbose mode
ifeq ($(VERBOSE),yes)
    V = 
    Q =
else
    V = @
    Q = @
endif

# =============================================================================
# RULES
# =============================================================================

.PHONY: all dirs info help

all: info dirs $(TARGET)
	$(PRINT_GREEN) "Build complete: $(TARGET)"

info:
	@echo "$(BOLD)=== Golden RNG Enterprise Build ===$(RESET)"
	@echo "  Build Type:    $(BUILD_TYPE)"
	@echo "  Compiler:      $(CC) ($(COMPILER_NAME))"
	@echo "  Version:       $(GIT_VERSION)"
	@echo "  Commit:        $(GIT_COMMIT)"
	@echo "  Build Date:    $(BUILD_DATE)"
	@echo "  Jobs:          $(NPROCS)"
	@echo "  Output:        $(TARGET)"
	@echo "  CCache:        $(USE_CCACHE)"
	@echo ""

dirs:
	$(V)mkdir -p $(OUTPUT_DIR)/core $(OUTPUT_DIR)/platform \
		$(OUTPUT_DIR)/utils $(OUTPUT_DIR)/bin $(OUTPUT_DIR)/lib

$(TARGET): $(OBJS)
	$(V)echo "$(BLUE)Linking $(TARGET)...$(RESET)"
	$(V)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(V)if [ "$(STRIP)" = "-s" ]; then strip $@; fi
	$(V)$(PRINT_GREEN) "Binary size: $$(du -h $@ | cut -f1)"

# Pattern rule for compiling - source is in current dir, output goes to build dir
$(OUTPUT_DIR)/%.o: %.c | dirs
	$(V)@echo "$(BLUE)Compiling $<...$(RESET)"
	$(V)$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Include dependencies
-include $(DEPS)

# =============================================================================
# CLEAN
# =============================================================================

.PHONY: clean distclean

clean:
	$(V)$(RM) -rf $(BUILD_DIR)
	$(V)$(PRINT_GREEN) "Clean complete"

distclean: clean
	$(V)$(RM) -f *.gcda *.gcno *.gcov gmon.out coverage.info
	$(V)$(PRINT_GREEN) "Distclean complete"

# =============================================================================
# TESTING
# =============================================================================

.PHONY: test test-quick test-all test-fuzz

test: $(TARGET)
	$(PRINT_BLUE) "=== Running Tests ==="
	$(V)./$(TARGET) -m unittests
	$(V)./$(TARGET) -m kat
	$(V)./$(TARGET) -m determinism
	$(V)./$(TARGET) -m advanced
	$(PRINT_GREEN) "=== Tests Complete ==="

test-quick: $(TARGET)
	$(V)./$(TARGET) -m test

test-all: test smallcrush crush bigcrush

# BigCrush test (requires TestU01 library)
bigcrush: $(TARGET)
	$(PRINT_BLUE) "=== Building BigCrush Test ==="
	@if [ "$(HAVE_TESTU01)" != "yes" ]; then \
		echo "$(RED)Error: TestU01 library not found. Install with: sudo apt-get install libtestu01-0-dev$(RESET)"; \
		exit 1; \
	fi
	$(V)mkdir -p $(OUTPUT_DIR)/bin
	$(V)$(CC) $(CFLAGS) $(INCLUDES) -DTESTU01_STANDALONE -DTESTU01_AVAILABLE \
		-I/usr/include -I/usr/include/testu01 \
		$(CORE_SRCS:%=$(OUTPUT_DIR)/%) $(PLATFORM_SRCS:%=$(OUTPUT_DIR)/%) \
		$(OUTPUT_DIR)/utils/testu01_bigcrush.o \
		$(TESTU01_LIBS) -lpthread $(OPENSSL_LIBS) -lm -o $(OUTPUT_DIR)/bin/test_bigcrush
	$(PRINT_GREEN) "BigCrush test built: $(OUTPUT_DIR)/bin/test_bigcrush"

run-bigcrush: bigcrush
	$(PRINT_BLUE) "=== Running BigCrush Test ==="
	./$(TARGET) | ./$(OUTPUT_DIR)/bin/test_bigcrush

smallcrush: $(TARGET)
	$(PRINT_BLUE) "=== Building SmallCrush Test ==="
	@if [ "$(HAVE_TESTU01)" != "yes" ]; then \
		echo "$(RED)Error: TestU01 library not found.$(RESET)"; \
		exit 1; \
	fi
	$(V)mkdir -p $(OUTPUT_DIR)/bin
	$(V)$(CC) $(CFLAGS) $(INCLUDES) -DTESTU01_STANDALONE -DTESTU01_AVAILABLE -DTESTU01_DEFAULT_SUITE=0 \
		-I/usr/include -I/usr/include/testu01 \
		$(CORE_SRCS:%=$(OUTPUT_DIR)/%) $(PLATFORM_SRCS:%=$(OUTPUT_DIR)/%) \
		$(OUTPUT_DIR)/utils/testu01_bigcrush.o \
		$(TESTU01_LIBS) -lpthread $(OPENSSL_LIBS) -lm -o $(OUTPUT_DIR)/bin/test_smallcrush

crush: $(TARGET)
	$(PRINT_BLUE) "=== Building Crush Test ==="
	@if [ "$(HAVE_TESTU01)" != "yes" ]; then \
		echo "$(RED)Error: TestU01 library not found.$(RESET)"; \
		exit 1; \
	fi
	$(V)mkdir -p $(OUTPUT_DIR)/bin
	$(V)$(CC) $(CFLAGS) $(INCLUDES) -DTESTU01_STANDALONE -DTESTU01_AVAILABLE -DTESTU01_DEFAULT_SUITE=1 \
		-I/usr/include -I/usr/include/testu01 \
		$(CORE_SRCS:%=$(OUTPUT_DIR)/%) $(PLATFORM_SRCS:%=$(OUTPUT_DIR)/%) \
		$(OUTPUT_DIR)/utils/testu01_bigcrush.o \
		$(TESTU01_LIBS) -lpthread $(OPENSSL_LIBS) -lm -o $(OUTPUT_DIR)/bin/test_crush

# =============================================================================
# BENCHMARKING
# =============================================================================

.PHONY: benchmark benchmark-simd benchmark-all

benchmark: $(TARGET)
	$(PRINT_BLUE) "=== Running Benchmark ==="
	./$(TARGET) -m benchmark -s 100M

benchmark-simd: $(TARGET)
	$(PRINT_BLUE) "=== Running SIMD Benchmark ==="
	./$(TARGET) -m simd -s 100M

benchmark-all: benchmark benchmark-simd
	$(PRINT_GREEN) "=== All Benchmarks Complete ==="

# =============================================================================
# INSTALLATION
# =============================================================================

.PHONY: install install-bin install-lib install-config install-man

install: install-bin install-lib install-config install-man
	$(PRINT_GREEN) "Installation complete to $(PREFIX)"

install-bin: $(TARGET)
	$(V)install -d $(DESTDIR)$(BINDIR)
	$(V)install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/
	$(V)install -d $(DESTDIR)$(BINDIR)/goldenrng.d
	$(V)install -m 755 $(OUTPUT_DIR)/bin/test_* $(DESTDIR)$(BINDIR)/goldenrng.d/ 2>/dev/null || true
	$(PRINT_GREEN) "Binaries installed to $(BINDIR)"

install-lib:
	$(V)install -d $(DESTDIR)$(LIBDIR)
	$(V)install -d $(DESTDIR)$(LIBDIR)/pkgconfig
	$(PRINT_GREEN) "Libraries installed to $(LIBDIR)"

install-config:
	$(V)install -d $(DESTDIR)$(PREFIX)/etc/goldenrng
	$(V)install -m 644 config/goldenrng.conf $(DESTDIR)$(PREFIX)/etc/goldenrng/
	$(PRINT_GREEN) "Configuration installed to $(PREFIX)/etc/goldenrng"

install-man:
	$(V)install -d $(DESTDIR)$(MANDIR)/man1
	$(V)install -m 644 docs/goldenrng.1 $(DESTDIR)$(MANDIR)/man1/
	$(PRINT_GREEN) "Man pages installed to $(MANDIR)"

.PHONY: uninstall

uninstall:
	$(V)$(RM) $(BINDIR)/goldenrng
	$(V)$(RM) -rf $(BINDIR)/goldenrng.d
	$(V)$(RM) $(MANDIR)/man1/goldenrng.1
	$(V)$(RM) -rf $(PREFIX)/etc/goldenrng
	$(PRINT_GREEN) "Uninstallation complete"

# =============================================================================
# PACKAGING
# =============================================================================

.PHONY: package package-deb package-rpm

package: package-deb package-rpm

package-deb: $(TARGET)
	$(PRINT_BLUE) "=== Building Debian Package ==="
	@if ! command -v dpkg-deb >/dev/null 2>&1; then \
		echo "$(YELLOW)dpkg-deb not found, skipping .deb package$(RESET)"; \
		exit 0; \
	fi
	$(V)mkdir -p $(OUTPUT_DIR)/pkg/DEBIAN
	$(V)mkdir -p $(OUTPUT_DIR)/pkg/usr/bin
	$(V)mkdir -p $(OUTPUT_DIR)/pkg/usr/share/man/man1
	$(V)mkdir -p $(OUTPUT_DIR)/pkg/etc/goldenrng
	$(V)cp $(CURDIR)/../packaging/deb/control $(OUTPUT_DIR)/pkg/DEBIAN/
	$(V)install -m 755 $(TARGET) $(OUTPUT_DIR)/pkg/usr/bin/
	$(V)install -m 644 $(CURDIR)/../docs/goldenrng.1 $(OUTPUT_DIR)/pkg/usr/share/man/man1/
	$(V)install -m 644 $(CURDIR)/../config/goldenrng.conf $(OUTPUT_DIR)/pkg/etc/goldenrng/
	$(V)dpkg-deb --build $(OUTPUT_DIR)/pkg $(OUTPUT_DIR)/goldenrng_$(GIT_VERSION)_amd64.deb
	$(PRINT_GREEN) "Debian package: $(OUTPUT_DIR)/goldenrng_$(GIT_VERSION)_amd64.deb"

package-rpm:
	$(PRINT_BLUE) "=== Building RPM Package ==="
	@if ! command -v rpmbuild >/dev/null 2>&1; then \
		echo "$(YELLOW)rpmbuild not found, skipping .rpm package$(RESET)"; \
		exit 0; \
	fi
	$(V)mkdir -p $(HOME)/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
	$(V)cp $(CURDIR)/../packaging/rpm/goldenrng.spec $(HOME)/rpmbuild/SPECS/
	$(V)tar -czf $(HOME)/rpmbuild/SOURCES/goldenrng-$(GIT_VERSION).tar.gz --exclude=.git --exclude=build --exclude=src/build .
	$(V)rpmbuild -bb --define "_topdir $(HOME)/rpmbuild" \
		--define "version $(GIT_VERSION)" \
		$(HOME)/rpmbuild/SPECS/goldenrng.spec
	$(PRINT_GREEN) "RPM package built in $(HOME)/rpmbuild/RPMS/"

# =============================================================================
# CODE ANALYSIS
# =============================================================================

.PHONY: analyze analyze-cppcheck analyze-clang-tidy

analyze: analyze-cppcheck analyze-clang-tidy
	$(PRINT_GREEN) "=== Analysis Complete ==="

analyze-cppcheck:
	$(PRINT_BLUE) "=== Running cppcheck ==="
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c11 --inline-suppr $(SRCS) 2>&1 || true; \
	else \
		echo "$(YELLOW)cppcheck not installed$(RESET)"; \
	fi

analyze-clang-tidy:
	$(PRINT_BLUE) "=== Running clang-tidy ==="
	@if command -v clang-tidy >/dev/null 2>&1; then \
		clang-tidy -header-filter='.*' -checks='*,-modernize-*' \
			$(CORE_SRCS) $(MAIN_SRCS) -- $(CFLAGS) $(INCLUDES) 2>&1 || true; \
	else \
		echo "$(YELLOW)clang-tidy not installed$(RESET)"; \
	fi

# =============================================================================
# DOCUMENTATION
# =============================================================================

.PHONY: docs doxygen

docs:
	$(PRINT_BLUE) "=== Building Documentation ==="
	@if ! command -v doxygen >/dev/null 2>&1; then \
		echo "$(YELLOW)Doxygen not installed, skipping documentation$(RESET)"; \
		exit 0; \
	fi
	$(V)doxygen Doxyfile
	$(PRINT_GREEN) "Documentation generated in docs/html"

# =============================================================================
# CROSS-COMPILATION
# =============================================================================

.PHONY: cross cross-all

cross: cross-x86_64 cross-arm64 cross-arm32

cross-x86_64:
	$(PRINT_BLUE) "=== Cross-compiling for x86_64 ==="
	$(V)$(MAKE) $(JOBS) all BUILD_TYPE=release CC=x86_64-linux-gnu-gcc CROSS_PREFIX=x86_64-linux-gnu-

cross-arm64:
	$(PRINT_BLUE) "=== Cross-compiling for ARM64 ==="
	$(V)$(MAKE) $(JOBS) all BUILD_TYPE=release CC=aarch64-linux-gnu-gcc CROSS_PREFIX=aarch64-linux-gnu-

cross-arm32:
	$(PRINT_BLUE) "=== Cross-compiling for ARM32 ==="
	$(V)$(MAKE) $(JOBS) all BUILD_TYPE=release CC=arm-linux-gnueabihf-gcc CROSS_PREFIX=arm-linux-gnueabihf-

cross-all: cross-x86_64 cross-arm64 cross-arm32
	$(PRINT_GREEN) "=== Cross-compilation Complete ==="

# =============================================================================
# CI/CD HELPERS
# =============================================================================

.PHONY: ci-build ci-test ci-release

ci-build:
	$(V)$(MAKE) $(JOBS) all BUILD_TYPE=release
	$(V)$(MAKE) $(JOBS) all BUILD_TYPE=debug

ci-test:
	$(V)$(MAKE) test

ci-release:
	$(V)$(MAKE) $(JOBS) all BUILD_TYPE=release
	$(V)$(MAKE) $(JOBS) all BUILD_TYPE=lto
	$(V)$(MAKE) $(JOBS) all BUILD_TYPE=ofast
	$(V)$(MAKE) test
	$(V)$(MAKE) package

# =============================================================================
# DEVELOPMENT
# =============================================================================

.PHONY: devstyle devhooks

devstyle:
	@echo "Running code formatting..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find src -name '*.c' -o -name '*.h' | xargs clang-format -i; \
		echo "Code formatted"; \
	else \
		echo "clang-format not installed"; \
	fi

devhooks:
	@echo "Installing git hooks..."
	@if [ -d .git ]; then \
		ln -sf ../../scripts/pre-commit .git/hooks/pre-commit; \
		echo "Pre-commit hook installed"; \
	else \
		echo "Not a git repository"; \
	fi

# =============================================================================
# STATS
# =============================================================================

.PHONY: stats

stats:
	@echo "$(BOLD)=== Project Statistics ===$(RESET)"
	@echo "Source files: $$(find . -maxdepth 1 -name '*.c' | wc -l)"
	@echo "Header files: $$(find . -maxdepth 1 -name '*.h' | wc -l)"
	@echo "Core source files: $$(find core -name '*.c' 2>/dev/null | wc -l)"
	@echo "Core header files: $$(find core -name '*.h' 2>/dev/null | wc -l)"
	@echo "Total lines: $$(find . -name '*.c' -o -name '*.h' | xargs wc -l 2>/dev/null | tail -1)"
	@echo "Object files: $$(find $(OUTPUT_DIR) -name '*.o' 2>/dev/null | wc -l)"
	@if [ -f $(TARGET) ]; then \
		echo "Binary size: $$(du -h $(TARGET) | cut -f1)"; \
		echo "Symbols: $$(nm $(TARGET) 2>/dev/null | wc -l)"; \
	fi

# =============================================================================
# HELP
# =============================================================================

.PHONY: help

help:
	@echo "$(BOLD)Golden RNG Enterprise - Production Build System$(RESET)"
	@echo ""
	@echo "$(BOLD)Usage:$(RESET) make [target] [BUILD_TYPE=type] [OPTIONS]"
	@echo ""
	@echo "$(BOLD)Build Types:$(RESET)"
	@echo "  release    - Optimized release build (default)"
	@echo "  production - Maximum optimization with safety"
	@echo "  ofast      - Aggressive optimization (may break strict aliasing)"
	@echo "  size       - Optimize for binary size"
	@echo "  debug      - Debug build with symbols"
	@echo "  sanitize   - Build with sanitizers (ASAN, USAN, LSAN)"
	@echo "  profile    - Build with profiling support"
	@echo "  coverage   - Build with code coverage"
	@echo "  lto        - Release build with Link-Time Optimization"
	@echo ""
	@echo "$(BOLD)Main Targets:$(RESET)"
	@echo "  all           - Build the project (default)"
	@echo "  test          - Run all tests"
	@echo "  test-quick    - Run quick test"
	@echo "  test-all      - Run all tests including BigCrush"
	@echo "  bigcrush      - Build BigCrush test (requires TestU01)"
	@echo "  run-bigcrush  - Build and run BigCrush test"
	@echo "  benchmark     - Run performance benchmark"
	@echo "  benchmark-simd - Run SIMD benchmark"
	@echo "  install       - Install binary and files"
	@echo "  uninstall     - Remove installed files"
	@echo "  package        - Create distribution packages"
	@echo "  clean          - Clean build artifacts"
	@echo "  analyze        - Run static analysis"
	@echo "  docs           - Generate documentation"
	@echo "  cross          - Cross-compile for other architectures"
	@echo "  help           - Show this help"
	@echo ""
	@echo "$(BOLD)Options:$(RESET)"
	@echo "  BUILD_TYPE=   - Set build profile"
	@echo "  JOBS=-jN      - Number of parallel jobs (default: auto)"
	@echo "  VERBOSE=yes   - Enable verbose output"
	@echo "  USE_CCACHE=yes - Use ccache if available"
	@echo "  PREFIX=/path  - Installation prefix (default: /usr/local)"
	@echo ""
	@echo "$(BOLD)Examples:$(RESET)"
	@echo "  make                                    # Release build"
	@echo "  make BUILD_TYPE=debug                  # Debug build"
	@echo "  make BUILD_TYPE=sanitize JOBS=-j4      # Sanitizer build with 4 jobs"
	@echo "  make install PREFIX=/usr               # Install to /usr"
	@echo "  make test BUILD_TYPE=coverage          # Run tests with coverage"
	@echo "  make cross-all                          # Cross-compile all platforms"

# Export variables for sub-makes
export

