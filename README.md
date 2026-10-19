# orange

orange is a local, single-learner mathematics study project with a C++20
backend and a planned TypeScript web client. Building the project is also a
hands-on study of C++ and Web technologies.

The current implementation contains HTTP request-head parsing, incremental
Content-Length and chunked request-body decoding, and governance validation
tooling. A runnable service, web client, and mathematics study features are
still pending; the repository does not yet provide an application start command.

| Area | Entry point |
| --- | --- |
| Project design and V1 outcomes | [Architecture baseline](docs/architecture/ADD-0001-orange-v1-baseline.md) |
| C++ implementation and tests | [`service/`](service/); [toolchain and verification commands](docs/development/cpp-development-standard.md#baseline-practices) |
| Governance validator and its test commands | [Validator README](tools/governance-validator/README.md) |
| Documentation and standards | [Documentation index](docs/README.md) |
| Current implementation decisions | [Decision-record index](docs/adr/INDEX.md) |

The [local-runtime proposal](docs/adr/ADR-0003-local-runtime-path.md) describes
the planned startup path; its index entry records the current approval and
implementation status.
