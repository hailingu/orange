<!-- Governed by docs/adr/ADR-0002-governance-validator-adoption.md and docs/adr/ADR-0005-required-approver-match.md -->
# Governance Validator

The validator enforces the repository's deterministic ADD, ADR, and OCR
structure and lifecycle contracts under `AGENTS.md`. Run it with the commands
documented in `docs/README.md`:

- `npm test --prefix tools/governance-validator`
- `npm run validate --prefix tools/governance-validator`

Both are executed from the repository root. The validator is source work
under its AGENTS.md routing row: every behavioral edit lands its failing test
first and dependencies stay exactly locked.

For Accepted ADRs/OCRs and Current ADDs, the active Approver must exactly match
the concrete Required Approver. A different valid actor fails with exit 1 and
`APPROVAL_ACTOR_NOT_AUTHORIZED`, alongside the record path. Actor equality
does not bypass author separation, timestamp or exact `Approve` evidence checks.

The existing metadata gate accepts only a concrete actor for Required Approver.
Descriptive approver rules remain unsupported by the validator even though the
templates permit that form; this correction does not add rule interpretation.

Adopted and localized by
`docs/adr/ADR-0002-governance-validator-adoption.md` from a proven
same-contract implementation owned by the repository owner. The adoption
baseline contained 208 tests; this is a historical count. Run `npm test` for
the current suite total. The source system's
2026-08-14 point-in-time decomposition review is not maintained in this
repository; re-measure locally before citing any unit-size limit.

The 2026-10-16 peer-review repairs were directly authorized by the owner with
a one-task waiver of ADR preapproval and serialization. The assessment,
authorization, implementation boundaries and verification evidence are in
[`docs/reviews/2026-10-16-peer-assessment-repairs.md`](../../docs/reviews/2026-10-16-peer-assessment-repairs.md).

Record filenames must agree with their ADR, OCR or architecture directory,
including service and archive roots (`RECORD_LOCATION_INVALID`). Explicit
`ADR:`, `OCR:` and `Governed by` leading source-comment markers in supported text
formats must resolve to retained records (`PROVENANCE_RECORD_MISSING`). This
checks existing markers; it does not require every source file to have one.
Generated dependency directories remain outside repository traversal.

Real fenced Mermaid blocks in instantiated Markdown are syntax-checked.
Authoring templates retain their declared variables and are not executable
diagrams. ADD completeness and ID-coverage checks apply only to ADDs.
Unresolved variables in instantiated documents are checked outside HTML
comments and fenced examples. Template declaration/use checks retain their
raw authoring input.

Deferred candidates require a non-placeholder reason in their reason/evidence
column. Older tables may retain `None — <reason>` in the unused ADR-path cell.
Risk-dimension validation uses the structured matrix rather than subsequent
legend tables. Required OCR eligibility clauses may be reordered or accompanied
by additional clauses; each required clause must be present and checked, and
an unchecked duplicate still fails. Record references use one shared path
reader for plain, backtick and Markdown-link forms, preserving service roots.


The follow-up assessment and evidence are in
[`docs/reviews/2026-10-16-peer-assessment-followup.md`](../../docs/reviews/2026-10-16-peer-assessment-followup.md).
All repository test fixtures now require their owning test context;
`temporaryDirectory(t, prefix)` registers exact-path teardown before population.
The same ownership applies to the outside-root symlink fixture. Earlier shared
temporary directories are outside this task's cleanup scope.

Active concrete record paths in Markdown prose, inline code and inline links
must resolve to contained regular files (`RECORD_REFERENCE_INVALID`). Explicit
repository identities use repository-root resolution; other inline link
targets use the document's directory. URL fragments do not change file identity.
External URLs, filename grammar, authoring templates, comments and fenced
examples are inert. Untriggered Archival sections of active records are future
instructions; references in retired records and ordinary Archival sections
remain checked. This checks record-file existence, not arbitrary hyperlinks
or fragment-heading existence.
