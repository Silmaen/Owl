# Security Policy

## Supported Versions

Only the latest minor release gets fixes; nothing is backported to older lines.

| Version                  | Supported |
|--------------------------|-----------|
| 0.2.x (latest release)   | Yes       |
| older                    | No        |

## Reporting a Vulnerability

If you discover a security vulnerability in Owl, please report it privately:

1. **Do not** open a public GitHub issue for security vulnerabilities
2. Use GitHub's private vulnerability reporting (*Security* tab of the repository, *Report a vulnerability*)
3. Include steps to reproduce, potential impact, and any suggested fixes

Owl is maintained on spare time: a report is answered as soon as possible, without a guaranteed delay, and the fix
ships in the next release of the supported line.

## Scope

Owl is a game engine intended for offline/local use. The primary security concerns are:

- **Asset pack integrity**: `.owlpack` files use obfuscation but not cryptographic
  security -- they should not be relied upon for DRM
- **Untrusted asset packs**: since 0.3.0 the pack reader validates every offset, size and
  entry path before use and confines extraction to the target directory; a forged pack is
  rejected with an error. Packs are not signed, so a modified pack can still replace game
  assets
- **Lua sandboxing**: scripts get no file or system access (`io`, `os`, `debug`, `package`
  are not opened, `dofile` and `loadfile` are removed), load text chunks only and run under a
  memory and time quota per instance (see the Lua scripting page); it has not been audited
  against hostile code
- **Save files**: Save data is stored in user-writable directories and is not
  cryptographically signed
