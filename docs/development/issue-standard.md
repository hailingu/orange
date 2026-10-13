<!-- Governance proposal: docs/adr/ADR-0001-orange-issue-governance.md -->
# Issue Standard

**Status**: Active. [The governing Full ADR](../adr/ADR-0001-orange-issue-governance.md)
is `Accepted` (2026-10-13) under the approval contract in
[AGENTS.md](../../AGENTS.md); the mandatory language below now governs issue
creation, classification, and closure in the `hailingu/orange` tracker.

**Applies to**: every issue opened in the `hailingu/orange` GitHub
tracker, by humans and agents alike. Like the other standards under
`docs/development/`, this file is written in English for agent
interoperability; issue bodies themselves SHOULD follow the project
documentation language (Chinese).

**Last reviewed**: 2026-10-13

## Required Reading

- [AGENTS.md](../../AGENTS.md) — Decision Records, Version-Control Safety,
  and Verification And Completion Evidence govern implementation approval,
  external writes, and evidence. This standard defines issue severity and
  disposition locally; an issue does not authorize implementation or operation.

## Title Format

Every issue title MUST use exactly this shape, mirroring the Conventional
Commits format used for commits in this repository, with the P1–P3 tag
replacing the type word:

    P<1|2|3>(<scope>): <summary>

- `P<1|2|3>` — defect severity, or the P3 planning category for new features,
  defined below. This classification is carried only by the tag and its
  matching label added during maintainer triage (see Labels); urgency words
  (紧急, 严重, blocker) MUST NOT
  appear in the summary.
- `<scope>` — one area token identifying the affected orange capability
  or repository area (for example `graph`, `matrix`, `learning`, or `docs`).
  Reuse an existing commit scope when applicable; do not bundle unrelated areas.
- `<summary>` — one line in the issue's natural language, no trailing
  period. It states the problem, boundary, or requested capability rather
  than an implementation plan.

Conventional-Commit type words (`feat`, `fix`, `refactor`, `docs`, …)
MUST NOT replace the leading `P<1|2|3>` tag. This prohibition applies only
to that prefix position; these words MAY appear as scope tokens or in the
summary. For example, `P3(docs): ...` conforms and `docs(graph): ...` does not.
Free-form prefixes without a scope (for example `P3 已知边界：…`) do not
conform either.

Examples:

    P1(learning): 答题记录保存失败导致学习进度丢失
    P2(graph): 不连续点附近偶发显示跨越间断的连线
    P3(docs): 矩阵运算错误说明缺少示例
    P3(matrix): 希望支持矩阵特征值计算

## Severity

This section defines the severity calibration for orange issues:

- **P1** — user data loss, corruption, or exposure that is realistically
  reachable through normal use, common dirty data, or known system
  behavior; or a core workflow that is unusable (crash on launch, save
  always failing). Fix before merging any related task. Retaining a P1 defect
  as a known boundary is not permitted.
- **P2** — existing functional defects and contract inconsistencies that do
  not meet P1, whether consistently reproducible or limited to rare dirty
  data, narrow race windows, or extreme values. A workaround is not required.
  Fix or register as a known boundary, at the repository owner's discretion.
- **P3** — further preventive hardening, documentation improvements, and
  quality debt that do not remedy an existing functional defect or contract
  inconsistency.
  Also the planning category for requests for new capabilities that do not
  remedy an existing defect; it does not assert a defect or risk severity.
  Recorded and scheduled at the owner's discretion; P3 scheduling alone does
  not block a merge.

Regardless of issue classification or disposition, mandatory AGENTS.md,
Accepted ADR, security, and contract requirements continue to gate merge and
task completion. In particular, intent-bearing documentation for a newly
added public symbol MUST precede its implementation body. A P3 label or
known-boundary decision MUST NOT waive that requirement.

For defects and known boundaries, a severity claim MUST be backed by a
trigger description or reproduction steps in the body. When that severity
is uncertain, open at the higher severity and let triage lower it.

A feature or improvement that introduces a new capability without remedying
an existing defect MUST use P3 and the Feature / improvement body structure.
Its Goal MUST state the unmet user need and expected benefit, and Acceptance
Criteria MUST state observable outcomes. Reproduction steps are not required.
A request that remedies an existing defect MUST instead use that defect's
severity and the Defect / regression structure. Every issue requires a
P1–P3 title tag at creation; matching labels are a maintainer triage requirement
after creation, as defined below.

## Labels

Reporters MUST supply the required title and body, but labels are not a
prerequisite for opening an issue. A reporter without label permissions MAY
submit an unlabeled issue. Maintainers MUST NOT reject it solely for missing
labels; it awaits triage.

Label maintenance belongs to a maintainer with at least triage access. Before
recording triage as complete or closing an issue on behalf of the repository,
that maintainer MUST ensure the rules below are satisfied. If required labels
do not exist, a maintainer with write access MUST provision them before triage
can complete; the issue remains open awaiting triage. Pending label work does
not waive P1 or other mandatory requirements. The prohibition on assigning
`known-boundary` to P1 applies at every stage. These permission boundaries
follow [GitHub's label management rules](https://docs.github.com/en/issues/using-labels-and-milestones-to-track-work/managing-labels).

- Exactly one classification label — `P1`, `P2`, or `P3` — MUST be present and
  MUST match the title tag.
- `known-boundary` is permitted only for P2/P3 and MUST be added when the
  repository owner explicitly decides to retain such a behavior or limitation.
  P1 issues MUST NOT carry this label. Record the retention decision under
  Disposition with the owner identity, rationale, evidence, and reopening
  criterion; a review round alone is not a disposition.
- Area labels MAY mirror the title scope.

## Body Structure

One issue reports one problem; do not batch unrelated defects into a
single issue. Use the matching structure:

Defect / regression:

    ## 环境 (Environment)
    ## 复现步骤 (Steps to Reproduce)
    ## 期望结果 (Expected)
    ## 实际结果 (Actual)
    ## 证据 (Evidence)
    ## 回归来源 (Regression Source)

Known boundary:

    ## 边界描述 (Boundary)
    ## 触发条件 (Trigger)
    ## 现有防线 (Existing Defense)
    ## 处置 (Disposition)

Feature / improvement:

    ## 背景与目标 (Goal)
    ## 方案草案 (Draft Approach)
    ## 验收标准 (Acceptance Criteria)

Evidence cites stable artifacts — commit identifiers, command output,
logs, screenshots — per the AGENTS.md verification rules; mutable line
numbers are supplementary only. `Regression Source` cites the commit that
introduced the defect when known.

### Known Boundary Content

Only P2/P3 issues are eligible for this disposition; the P1 fix requirement
in Severity still applies even when the owner is willing to retain the risk.

Boundary and Trigger MUST identify the retained behavior and the conditions
that expose it. Existing Defense MUST record the current controls or state
that none are known, and describe the residual user impact. Disposition MUST
identify the repository owner who made the decision, cite their explicit
decision to retain the boundary, and record its rationale, supporting evidence,
and an observable criterion for reopening. Recording a boundary does not
prove it is fixed or authorize an implementation or contract change.

### Defect Report Content

The rules and reusable template below define the defect-report format locally.
Keep the six required headings in the order above.

- **Environment** records the relevant platform, configuration, entry point,
  and version. Distinguish the inspected code revision from the running app
  version when they have not been matched. Explain the severity using the
  observed impact and any verified workaround.
- **Steps to Reproduce** gives numbered actions and the precise trigger.
  State whether the report was independently reproduced and whether the
  trigger is intermittent. Label constructed minimal examples explicitly;
  they MUST NOT be presented as captured requests, responses, or logs.
- **Expected** describes the observable successful behavior and existing
  guarantees that a fix must preserve, such as user confirmation before
  applying changes. Proposed behavior beyond an established contract MUST
  be identified as a proposal rather than an existing requirement.
- **Actual** records the observed failure, relevant diagnostic text, and
  effect on the user's work. Separate user-reported behavior from what a
  screenshot or local reproduction directly demonstrates.
- **Evidence** connects stable code references and symbols to the behavior,
  separates confirmed causes from hypotheses, and states missing evidence.
  Record verification commands with their working directories, results, and
  limits; explicitly say when tests or live reproduction were not run.
  Passing existing tests MUST NOT be described as proof that the reported
  defect does not exist. Include focused repair suggestions and observable
  acceptance criteria here when available; proposals do not authorize an
  implementation or a contract change.
- **Regression Source** identifies the introducing commit only when known.
  Otherwise say it is unknown; an analysis baseline MUST NOT be described
  as the introducing commit without evidence.

Screenshots and logs SHOULD be attached or linked through a location readers
can access. If evidence exists only in the reporting conversation, state
that limitation and describe the relevant observation. A local temporary
file path MUST NOT be presented as a GitHub-accessible attachment. Exclude
secrets and unnecessary sensitive data from all evidence.

Scale detail to the defect. Unknown information should be marked as such;
creating an actionable issue does not require a completed investigation,
new tests, or a speculative root cause. Within Evidence, use short lead-in
paragraphs; use subheadings only when needed for readability.

### Reusable Defect Template

Replace the placeholders with supported facts. Keep required headings and
remove inapplicable optional detail rather than inventing evidence.

````markdown
## 环境 (Environment)

- 平台、相关配置与操作入口：<已知信息；未知项注明待确认>。
- 应用运行版本：<版本或待确认>；分析代码版本：<如已检查，填写 commit SHA>。
- 严重性：P<1|2|3>。<触发条件、影响，以及已验证的绕过方式（如有）>。

## 复现步骤 (Steps to Reproduce)

1. <准备状态与入口>
2. <触发操作或输入>
3. <观察位置>

复现状态：<用户报告 / 本地已复现；频率或尚未确认的条件>。
<如附最小输入，注明是实际捕获还是构造示例。>

## 期望结果 (Expected)

<用户可观察的正确结果，以及必须保留的既有保证。>

## 实际结果 (Actual)

- <观察到的失败现象与相关错误文案>
- <对当前操作、画布或数据的影响>

## 证据 (Evidence)

<截图、日志或录屏的可访问链接；无法提供附件时说明证据来源与限制。>

固定版本代码证据（如已检查）：

- [<符号或模块>](<包含 commit SHA 的代码链接>)：<该处行为与问题的关系>。

原因分析：<已确认的原因；待验证的推测及缺失证据分别说明>。

验证情况：<工作目录、实际执行命令、结果和未执行项；未运行则明确说明>。

建议修复与验收（如已有建议）：

- <针对本问题的修复方向，标明仍需确认的方案>
- <可观察、可验证的成功条件与回归场景>
- <应保留的行为或需要另行确定范围的契约变更>

## 回归来源 (Regression Source)

<已确认的引入提交及依据；未知则注明尚未定位，分析基线不代表引入版本。>
````

## Closing

An issue resolved by a code change closes only with the resolving commit
referenced in a comment and verification evidence for the reported outcome.
An issue closed without a code change MUST have a comment naming the
disposition (for example duplicate, no longer reproducible, or retained
known boundary), its rationale, supporting evidence, and an observable
criterion for reopening. A duplicate disposition MUST link the issue that
tracks the same problem. Retained-boundary closure is permitted only for
P2/P3 and MUST record the repository owner decision required by Known Boundary
Content. When retention is proposed for a P1 issue, it MUST remain open until
resolved or evidence-backed triage establishes a non-P1 classification.
Willingness to retain P1 impact is not evidence for lowering severity.
Bulk-closing without a disposition record is not permitted.
