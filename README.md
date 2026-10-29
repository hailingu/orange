# orange

orange is a local, single-learner mathematics study project with a C++20
backend and a TypeScript web client. Building the project is also a
hands-on study of C++ and Web technologies.

## Run it locally

One documented path builds both sides and serves the two fixed loopback
origins ([ADR-0003](docs/adr/ADR-0003-local-runtime-path.md)):

```sh
scripts/dev.sh
```

The stable origin is **https://127.0.0.1:8443** (open it in a browser and
accept the self-signed certificate once per browser profile); the plaintext
comparison endpoint is http://127.0.0.1:8080. Both listeners bind loopback
only. The certificate is generated into the gitignored `run/` directory on
first use and reused across restarts, so the origin keeps its identity.
`Ctrl-C` stops both listeners cleanly.

### Prerequisites (macOS, the declared Darwin toolchain)

- Apple clang (Xcode Command Line Tools), CMake 3.21+
- Node.js with npm (`web/` pins its exact toolchain in `package.json`)
- OpenSSL 3 (`brew install openssl@3`); `vcpkg.json` additionally pins the
  dependency for toolchain-file builds of the vcpkg registry state recorded
  in its baseline

A busy port fails startup with a coded `orange:` diagnostic instead of a
silent alternative port.

| Area | Entry point |
| --- | --- |
| Project design and V1 outcomes | [Architecture baseline](docs/architecture/ADD-0001-orange-v1-baseline.md) |
| C++ implementation and tests | [`service/`](service/); [toolchain and verification commands](docs/development/cpp-development-standard.md#baseline-practices) |
| Web client and its commands | [`web/`](web/); [TypeScript standard](docs/development/typescript-development-standard.md#baseline-practices) |
| Governance validator and its test commands | [Validator README](tools/governance-validator/README.md) |
| Documentation and standards | [Documentation index](docs/README.md) |
| Current implementation decisions | [Decision-record index](docs/adr/INDEX.md) |
