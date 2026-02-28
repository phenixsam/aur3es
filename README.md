# Golden RNG Enterprise v5.0 (AUR3ES)

[![Build Status](https://github.com/phenixsam/aur3es/actions/workflows/ci.yml/badge.svg)](https://github.com/phenixsam/aur3es/actions)
[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.html)
[![Version](https://img.shields.io/badge/version-v5.0-blue)](https://github.com/phenixsam/aur3es)

A high-performance cryptographic random number generator (CSPRNG) implementing the AUR3ES algorithm with SHA-512 key derivation.

**Website**: https://phenixsam.github.io/aur3es/
**GitHub**: https://github.com/phenixsam/aur3es

## Main Features

- **Cryptographically Secure**: AUR3ES algorithm with SHA-512 key derivation
- **Multi-platform**: Support for Linux, Windows, macOS
- **SIMD Optimized**: Support for SSE2, AVX2, AVX-512
- **Parallel Generation**: Multi-threaded with local RNG instances per thread
- **Statistical Tests**: Complete NIST SP 800-22 test suite
- **Monitoring Module**: Real-time statistics, RNG health, performance metrics
- **Enhanced Security**: Constant-time operations, memory locking, secure erasure

## Entropy Sources

The system uses multiple entropy sources to guarantee high-quality randomness:

| Source             | Description                         | Platform   |
| ------------------ | ----------------------------------- | ---------- |
| `/dev/urandom`     | Kernel entropy                      | Linux/Unix |
| `getrandom()`      | Entropy syscall                     | Linux      |
| `sysctl KERN_ARND` | System entropy                      | BSD/macOS  |
| `RDRAND`           | Intel Hardware RNG                  | x86/x64    |
| `RDSEED`           | Intel Hardware RNG (higher quality) | x86/x64    |
| CPU Jitter         | CPU timing variations               | All        |
| Timer              | High-resolution timers              | All        |

## Building

### Prerequisites

- GCC or Clang compiler
- OpenSSL development libraries
- POSIX threads (Linux/macOS)

### Build Commands

```bash
# Release build (default)
make

# Debug build
make BUILD_TYPE=debug

# With sanitizers
make BUILD_TYPE=sanitize

# With profiling
make BUILD_TYPE=profile

# With code coverage
make BUILD_TYPE=coverage

# With Link-Time Optimization
make BUILD_TYPE=lto
```

## Usage

### Basic Generation

```bash
# Generate random bytes to stdout
./goldenrng

# Generate to file
./goldenrng -m file -o random.bin -s 10M
```

### Testing

```bash
# Run all tests
make test

# Quick test
make test-quick

# Unit tests only
./goldenrng -m unittests

# Known Answer Tests
./goldenrng -m kat

# Determinism verification
./goldenrng -m determinism

# Advanced statistical tests
./goldenrng -m advanced

# SIMD benchmark
./goldenrng -m simd -s 100M
```

### Benchmarking

```bash
# Parallel benchmark
./goldenrng -m benchmark -s 100M -t 4

# SIMD benchmark
./goldenrng -m simd -s 100M
```

## Command Line Options

| Option              | Description                              |
| ------------------- | ---------------------------------------- |
| `-m, --mode MODE`   | Operation mode                           |
| `-d, --dist DIST`   | Distribution: bytes, uniform, normal     |
| `-o, --output FILE` | Output file                              |
| `-s, --size SIZE`   | Size (1G, 100M, 500K)                    |
| `-S, --seed SEED`   | Seed for deterministic mode              |
| `-t, --threads NUM` | Number of threads for parallel benchmark |
| `-v, --verbose`     | Verbose mode                             |
| `-V, --version`     | Show version                             |
| `-h, --help`        | Show help                                |

### Operation Modes

- `stdout` - Generate to stdout (default)
- `file` - Generate to file
- `stats` - Show statistics
- `test` - Basic random number test
- `simulate` - Monte Carlo Pi simulation
- `benchmark` - Parallel benchmark
- `simd` - SIMD benchmark and CPU detection
- `kat` - Known Answer Tests
- `unittests` - Complete unit test suite
- `determinism` - Verify determinism
- `advanced` - Advanced statistical tests

## Algorithm

The AUR3ES algorithm (Golden Animated Universal Randomized Diffusion with Enhanced Security) is a cryptographically secure PRNG that combines:

- **Internal state of 512 bits** (8 × 64-bit words)
- **SHA-512** for key derivation
- **4 rounds of non-linear mixing** per block
- **Golden constants mixing** for enhanced diffusion
- **Counter mode** for output transformation
- **Automatic reseed** with system entropy

### Technical Details

```
State: 8 × uint64_t = 512 bits
Constants: Golden Ratio (high and low parts)
Mixing rounds: 4 per block
Reseed interval: 70,000 blocks
Key derivation: SHA-512
Output per block: 256 bits (4 × 64-bit)
```

## Performance

- Sequential: ~257-280 MB/s
- Parallel (4 threads): ~893 MB/s (3.32x speedup)
- SIMD: Up to 2x additional on supported CPUs
- Optimized for maximum throughput

## Security Features

- ✅ Secure memory erasure after use
- ✅ Memory locking (mlock) to prevent swap
- ✅ Constant-time operations
- ✅ Multi-source entropy validation
- ✅ Thread-safe operation with mutex protection
- ✅ Input validation on all public APIs
- ✅ **No predictable entropy fallback** (since v5.0.1)

### Security Considerations

**Important**: Starting from version 5.0.1, the RNG **does not use predictable entropy fallback**. If the system cannot provide adequate entropy:

1. The `rng_create()` function returns `NULL`
2. An error message is logged to stderr
3. The program must handle this error case

This ensures the RNG is always properly initialized with secure entropy.

## Testing

The project includes comprehensive tests:

- Unit tests (core functions, statistical, determinism, SIMD)
- Known Answer Tests (KAT)
- NIST SP 800-22 statistical tests
- Avalanche effect test
- Diffusion test
- Correlation analysis
- Monte Carlo Pi estimation
- TestU01 BigCrush integration (optional)

### Statistical Test Results

Golden RNG has been exhaustively tested using industry-standard test suites:

| Test Suite                | Tests | Result                      |
| ------------------------- | ----- | --------------------------- |
| Dieharder                 | 17    | ✅ ALL PASSED               |
| PractRand                 | 2098+ | ✅ ALL PASSED (up to 64 GB) |
| TestU01 SmallCrush        | 15    | ✅ ALL PASSED               |
| TestU01 BigCrush          | 160   | ✅ ALL PASSED               |
| Integrated Advanced Tests | 10    | ✅ ALL PASSED               |
| Monte Carlo Pi            | 3     | ✅ ALL PASSED               |

**Total: 2,300+ statistical tests - ALL PASSED**

## Installation

```bash
# Install to /usr/local
sudo make install

# Uninstall
sudo make uninstall
```

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

### Third-Party Libraries

This project uses the following third-party libraries:

- **OpenSSL**: For SHA-512 key derivation (Apache 2.0 license)
- **TestU01** (optional): For statistical testing (BSD license)

## Version

v5.0 - Golden RNG Enterprise
