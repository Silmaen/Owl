# Security Policy

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| 0.2.x   | Yes                |
| 0.1.x   | Yes                |
| < 0.1   | No                 |

## Reporting a Vulnerability

If you discover a security vulnerability in Owl, please report it responsibly:

1. **Do not** open a public GitHub issue for security vulnerabilities
2. Send an email to the project maintainer with details of the vulnerability
3. Include steps to reproduce, potential impact, and any suggested fixes

We will acknowledge receipt within 48 hours and aim to provide a fix or mitigation
within a reasonable timeframe depending on severity.

## Scope

Owl is a game engine intended for offline/local use. The primary security concerns are:

- **Asset pack integrity**: `.owlpack` files use obfuscation but not cryptographic
  security -- they should not be relied upon for DRM
- **Untrusted asset packs**: since 0.3.0 the pack reader validates every offset, size and
  entry path before use and confines extraction to the target directory; a forged pack is
  rejected with an error. Packs are not signed, so a modified pack can still replace game
  assets
- **Lua sandboxing**: The Lua scripting environment removes `io`, `os`, `dofile`, and
  `loadfile` but is not a security sandbox for untrusted code
- **Save files**: Save data is stored in user-writable directories and is not
  cryptographically signed
