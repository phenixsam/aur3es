Name:           golden-rng
Version:        5.0.0
Release:        1%{?dist}
Summary:        Golden RNG Enterprise - Cryptographic Random Number Generator

License:        Proprietary
URL:            https://github.com/goldenrng/enterprise
Source0:        %{name}-%{version}.tar.gz

Requires:      glibc >= 2.17, openssl >= 1.1.0

%description
Golden RNG Enterprise is a high-performance cryptographic random number 
generator (CSPRNG) implementing the AUR3ES algorithm with SHA-512 key derivation.

Features:
- Cryptographically secure random number generation
- Multi-platform support (Linux, Windows, macOS)
- SIMD optimized (SSE2, AVX2, AVX-512)
- Multi-threaded parallel generation
- Full NIST SP 800-22 statistical test suite

%prep
%setup -c

%build
# Binary already built, skip build step
# cd src
# make %{?_smp_mflags} BUILD_TYPE=release

%install
# Install the pre-built binary
mkdir -p %{buildroot}%{_bindir}
install -m 755 src/goldenrng %{buildroot}%{_bindir}/goldenrng

%files
%doc README.md LICENSE
%{_bindir}/goldenrng

%changelog
* Mon Feb 23 2026 Golden RNG Team <surffero2077@proton.me> - 5.0.0-1
- Initial RPM release
- aur3es cryptographic CSPRNG implementation
- SIMD optimizations (SSE2, AVX2)
- Full NIST SP 800-22 test suite

