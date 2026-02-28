# Golden RNG Enterprise - Technical Documentation

## Algorithm: AUR3ES (Golden Animated Universal Randomized Diffusion with Enhanced Security)

### Overview

AUR3ES is a cryptographically secure pseudo-random number generator (CSPRNG) designed for high-performance security applications. It combines multiple cryptographic primitives to ensure unpredictability and statistical quality.

### State Structure

```
Internal State: 512 bits (8 × 64-bit words)
├── state[0-7]: Main state words
├── counter:    Block counter (prevents output patterns)
└── reseed_counter: Reseed interval tracker
```

### Key Derivation (SHA-512)

1. Input seed is processed through SHA-512
2. Key is stretched using XOR feedback
3. State is initialized with derived key
4. Initial mixing rounds applied (12 rounds)

### Generation Algorithm

```
For each 256-bit output block:
1. Copy current state to working buffer
2. Mix working state with counter
3. Apply 4 rounds of full_state_mix()
4. Generate 4 × 64-bit output words:
   - Mix using golden constants
   - Apply rotations
   - XOR with state
5. Update internal state
6. Increment counter
7. Check for reseed
```

### Mixing Functions

#### full_state_mix()

- **Round 1**: Linear mixing (add, rotate)
- **Round 2**: Cross diffusion (mix64)
- **Round 3**: Constant mixing (golden constants)
- **Round 4**: Final mixing (rotate, XOR)

#### mix64()

Based on MurmurHash3 finalizer:

```c
h ^= h >> 33;
h *= 0xff51afd7ed558ccdULL;
h ^= h >> 33;
h *= 0xc4ceb9fe1a85ec53ULL;
h ^= h >> 33;
```

### Security Properties

| Property          | Status  | Description               |
| ----------------- | ------- | ------------------------- |
| Key Size          | 512-bit | SHA-512 key derivation    |
| State Size        | 512-bit | 8 × 64-bit words          |
| Output            | 256-bit | 4 × 64-bit per block      |
| Forward Secrecy   | ✅      | State changes each output |
| Backward Secrecy  | ✅      | Reseed breaks patterns    |
| Timing Resistance | ✅      | Constant-time operations  |

### Performance Characteristics

| Metric                | Value                          |
| --------------------- | ------------------------------ |
| State Size            | 512 bits                       |
| Output per Block      | 256 bits (32 bytes)            |
| Cycles per Byte       | ~5-10 (architecture dependent) |
| Sequential Throughput | ~257-280 MB/s                  |
| Parallel Throughput   | ~893 MB/s (4 threads, 3.32x)   |

## Entropy Sources (CSPRNG Module)

The project uses a professional CSPRNG module (`csprng_entropy.c`) with multiple entropy sources:

### Primary Sources (OS-level)

| Source                | Platform   | Quality |
| --------------------- | ---------- | ------- |
| `/dev/urandom`        | Linux/Unix | High    |
| `getrandom()` syscall | Linux      | High    |
| `sysctl KERN_ARND`    | BSD/macOS  | High    |
| `BCryptGenRandom()`   | Windows    | High    |

### Hardware Sources (when available)

| Source | Platform      | Quality |
| ------ | ------------- | ------- |
| RDRAND | Intel x86/x64 | Medium  |
| RDSEED | Intel x86/x64 | High    |

### Software Sources (always available)

| Source        | Description                        |
| ------------- | ---------------------------------- |
| CPU Jitter    | Timing variations in CPU execution |
| Timer Entropy | High-resolution timer variations   |

### Security: No Predictable Fallback

**Important**: Since v5.0.1, the RNG does NOT use predictable entropy fallback:

- If system entropy fails, `rng_create()` returns `NULL`
- Error messages are logged to stderr
- The application must handle this error case

This guarantees that the RNG is always properly initialized with secure entropy.

### Reseeding

- Automatic: Every 70,000 blocks
- Manual: Via `rng_reseed()`
- Entropy collected from multiple sources
- 12 rounds of mixing after reseed

## Security Considerations

### Memory Security

- Secure zeroing on destroy (with memory barriers)
- mlock() to prevent swapping
- Stack canaries enabled
- Memory locking can be disabled in config

### Timing Attacks

- Constant-time comparison
- No branching on secret data
- Prevents cache timing attacks

### Thread Safety

- Mutex-protected state access
- Thread-local RNG instances for parallel generation
- Deterministic mode is thread-safe
- Built-in monitoring module for runtime health checks

### Runtime Monitoring

The project includes an integrated monitoring module (`rng_monitoring.c`) that provides:

| Feature             | Description                                        |
| ------------------- | -------------------------------------------------- |
| Statistics          | Total bytes/blocks generated, reseed count, uptime |
| Entropy Monitoring  | Source availability, quality assessment            |
| Performance Metrics | Current/average/peak throughput, operations/sec    |
| Health Checks       | RNG initialization, entropy adequacy, memory usage |
| Callback System     | Event-driven health notifications                  |

## Testing

### NIST SP 800-22 Tests

All tests pass with α = 0.01:

| Test                | Status  |
| ------------------- | ------- |
| Frequency (Monobit) | ✅ PASS |
| Block Frequency     | ✅ PASS |
| Runs                | ✅ PASS |
| Binary Matrix Rank  | ✅ PASS |
| Serial              | ✅ PASS |
| Entropy             | ✅ PASS |

### Comprehensive Statistical Test Suites

Golden RNG has been thoroughly tested using industry-standard test suites:

| Test Suite         | Tests | Result                      |
| ------------------ | ----- | --------------------------- |
| Dieharder          | 17    | ✅ ALL PASSED               |
| PractRand          | 2098+ | ✅ ALL PASSED (up to 64 GB) |
| TestU01 SmallCrush | 15    | ✅ ALL PASSED               |
| TestU01 BigCrush   | 160   | ✅ ALL PASSED               |
| Advanced Built-in  | 10    | ✅ ALL PASSED               |
| Monte Carlo Pi     | 3     | ✅ ALL PASSED               |

**Total: 2,300+ statistical tests - ALL PASSED**

### Advanced Tests

| Test             | Result                     |
| ---------------- | -------------------------- |
| Avalanche Effect | ✅ PASS (50.0%)            |
| Diffusion        | ✅ PASS (49.84%)           |
| Correlation      | ✅ PASS (0.0015)           |
| Autocorrelation  | ✅ PASS (0 failed lags)    |
| Serial Test      | ✅ PASS                    |
| Runs Test        | ✅ PASS (0.08% error)      |
| Monobit          | ✅ PASS (0.498327)         |
| Block Frequency  | ✅ PASS                    |
| Matrix Rank      | ✅ PASS                    |
| Entropy          | ✅ PASS (7.9998 bits/byte) |

### Monte Carlo Pi Estimation

| Points     | Estimation | Error   | Status  |
| ---------- | ---------- | ------- | ------- |
| 100,000    | 3.14996000 | 0.266%  | ✅ PASS |
| 1,000,000  | 3.14172400 | 0.004%  | ✅ PASS |
| 10,000,000 | 3.14133960 | 0.008%  | ✅ PASS |
| 50,000,000 | 3.14154600 | 0.0015% | ✅ PASS |

**All Monte Carlo Pi tests: 100% PASSED**

### Test Environment

- **Platform:** Linux (x64)
- **TestU01 Version:** 1.2.3
- **Dieharder Version:** 3.31.1
- **PractRand Version:** 0.95

### Determinism Verification

- Same seed produces same sequence (verified)
- Different seeds produce different sequences (verified)
- All KAT (Known Answer Tests) pass

## API Reference

See `rng_engine.h` for complete API documentation.

## Version History

- v5.0.1 - Security improvements
  - Removed predictable entropy fallback
  - Added CSPRNG professional module integration
- v5.0.0 - Initial enterprise release
  - AUR3ES algorithm
  - SIMD optimizations
  - Full NIST test suite
  - Multi-platform support
  - Integrated monitoring module (rng_monitoring.c)
