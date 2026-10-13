<!-- Governance proposal: docs/adr/ADR-0001-orange-issue-governance.md -->
# Fixed issue-governance review fixtures

These constructed inputs make the Proposed ADR's AC-03 through AC-05,
AC-07 through AC-12 reproducible. They are not real issues, captured logs,
owner decisions, commits, or test results, and activate no policy.

Evaluate the exact inputs below against the cited sections of
[the issue standard](../../development/issue-standard.md). Use only the
fixture premises; do not substitute live issues or execute fixture commands.
For a variant, copy its named base and apply only the specified replacement
or omission. `Conforming` and `Nonconforming` describe the artifact under the
evaluated clauses; a check passes only when every outcome and reason agrees
with the final case table. Record each case ID, actual outcome, clause, and
the standard and fixture Git blobs. This is structured document inspection,
not a replacement for the configured governance validator.

Unless a case specifies another stage, issue-label checks evaluate completed
maintainer triage with the named labels available; closure actions are by a
maintainer on behalf of the repository. A newly submitted external report
awaiting triage is evaluated separately in the L cases.

## D — Defect issue

Repository: `hailingu/orange`. Title: `P2(graph): 旧请求结果覆盖当前视口`.
Labels: `P2`. This is also the fixed input for a request to remedy an existing
defect; it is not a request for a new capability.

```markdown
## 环境 (Environment)
构造场景：网页图形工作区，版本 fixture-v1。P2：仅在两个请求回复倒序到达时
出现旧图形；重试当前视口可恢复，不丢失输入。本样例没有真实运行版本。
## 复现步骤 (Steps to Reproduce)
1. 请求 A 计算 sin(x)，视口 [-1, 1]。
2. A 尚未返回时请求 B，视口 [2, 3]。
3. 先送达 B，再送达 A；观察显示的视口。
这些步骤和回复顺序是构造输入，未独立或在线复现。
## 期望结果 (Expected)
拟议行为：A 到达后仍显示 B 的 [2, 3] 结果；不是已确认的现有保证。
## 实际结果 (Actual)
构造场景中 A 覆盖 B，显示 [-1, 1]；用户需重试才能恢复当前视口。
## 证据 (Evidence)
证据限于本文件中的构造序列，不是捕获的日志。原因未确认，未检查源代码，
未运行测试或实时复现。拟议修复验收：按上述顺序送达时忽略 A 并保留 B。
## 回归来源 (Regression Source)
未知；fixture-v1 是样例标识，不是引入缺陷的提交。
```

## O — Reproducible non-core defect without a workaround

Repository: `hailingu/orange`. Title: `P2(graph): 图形 PNG 导出始终失败`.
Labels: `P2`. Synthetic premise: PNG export is an existing non-core feature;
graphing, learning, and saving inputs remain usable, and no data is lost,
corrupted, or exposed. There is no other export route or workaround. These
are fixed premises, not claims about orange's current implementation.

```markdown
## 环境 (Environment)
构造场景：网页图形工作区，版本 fixture-v1。P2：现有 PNG 导出功能每次失败，
没有替代导出方法；图形显示、学习和输入保存仍可用，用户数据不受影响。
## 复现步骤 (Steps to Reproduce)
1. 绘制 sin(x)，视口 [-1, 1]。
2. 点击已有的“导出 PNG”按钮。
3. 重复点击一次；两次均未下载文件。这是构造序列，未在线复现。
## 期望结果 (Expected)
构造场景的现有导出契约：每次点击下载当前图形的 PNG 文件。
## 实际结果 (Actual)
每次点击均显示“导出失败”，没有文件；无其他导出途径，重试不能恢复。
## 证据 (Evidence)
证据仅为上述构造输入；没有真实日志、源代码检查或测试结果，原因未确认。
拟议修复验收：每次点击均下载当前视口的 PNG，并保留输入 sin(x)。
## 回归来源 (Regression Source)
未知；fixture-v1 是样例标识，不是引入缺陷的提交。
```

## B — Retained boundary issue

Repository: `hailingu/orange`. Title: `P2(graph): 倒序回复时需重试当前视口`.
Labels: `P2`, `known-boundary`. Within this constructed world only,
`@fixture-owner` is the repository owner and has made the decision below.

```markdown
## 边界描述 (Boundary)
构造边界：旧请求可能覆盖当前视口；保留该限制，不声称问题已修复。
## 触发条件 (Trigger)
按 D 的 A、B 顺序发送请求，按 B、A 顺序送达回复。
## 现有防线 (Existing Defense)
用户可以重试当前视口。残余影响：重试前显示旧图形；输入不丢失。
## 处置 (Disposition)
Owner: @fixture-owner（仅为构造场景中的仓库所有者）。
Owner decision: 保留此限制，待图形请求处理任务规划时重新评估。
Rationale: 重试可恢复，输入不丢失；不据此降低缺陷严重性。
Evidence: 本文件 D 的构造序列和上述防线；没有真实所有者审批或运行日志。
Reopen criterion: 按 D 的步骤触发后，重试一次仍不能恢复 [2, 3] 视口。
```

## F — Pure feature request

Repository: `hailingu/orange`. Title: `P3(matrix): 希望支持矩阵特征值计算`.
Labels: `P3`. Premise: this asks for an absent capability, not a remedy for
an existing failure. There are no reproduction steps.

```markdown
## 背景与目标 (Goal)
新功能请求：希望查看矩阵的特征值，帮助学习二维线性变换；不报告现有故障。
## 方案草案 (Draft Approach)
提议增加特征值结果展示，具体设计与实施授权另行处理。
## 验收标准 (Acceptance Criteria)
输入 [[2, 0], [0, 3]] 时返回无序集合 {2, 3}，并保留原始矩阵输入。
```

## R — Wrong body for a defect-remedy request

Use D's repository, title, labels, and existing-failure premise, but replace
its entire body with this exact feature-style body:

```markdown
## 背景与目标 (Goal)
请求解决 D 中已存在的倒序回复故障，减少用户重试。
## 方案草案 (Draft Approach)
提议忽略旧回复；本样例没有实施授权。
## 验收标准 (Acceptance Criteria)
按 D 的回复顺序送达后保留 [2, 3] 视口。
```

## M — Mandatory documentation gate on a P3 feature task

Issue input: F, with its P3 title, label, and feature body unchanged. Evaluate
only the merge/completion disposition, not implementation or other gates.
Within this synthetic world, all other gates are satisfied. The proposed
implementation adds the new public function `orange::eigenvalues` with an
implementation body and no intent-bearing documentation before it. This is
constructed review input, not source code or an actual implementation claim.

Fixed disposition: block merge and task completion until intent-bearing
documentation is added before the new public function's implementation body.
The P3 issue may remain scheduled at the owner's discretion; scheduling does
not waive the documentation gate.

## C — Code-resolution closure comment

Issue input: D. Synthetic premises: commit
`1111111111111111111111111111111111111111` exists in the fixture repository
and fixes D. The verification block below is invented fixture evidence,
not an executed command, real result, or claim about orange source.

```markdown
Resolving commit: 1111111111111111111111111111111111111111.
Verification evidence:
- Constructed command: fixture-check graph-current-result.
- Constructed working directory: /fixture/orange.
- Constructed result: exit 0; B arrives before A; final viewport [2, 3];
  A is ignored and input sin(x) is retained.
```

## Fixture issue 202

The synthetic duplicate target has issue number 202 and exactly D's title,
labels, repository, and body. The local link below resolves to this fixed
issue artifact, not to an actual GitHub issue.

## U — Duplicate closure comment

Issue input: D, assigned synthetic issue number 203.

```markdown
Disposition: duplicate.
Duplicate issue: [fixture issue 202](#fixture-issue-202).
Rationale: issues 202 and 203 describe the same D request/reply order and stale viewport.
Supporting evidence: both fixed issue bodies are D; neither adds another trigger.
Reopen criterion: issue 203 is shown to occur with a different request/reply order.
```

## K — Retained-boundary closure comment

Issue input: B. The owner decision remains a synthetic premise only.

```markdown
Disposition: retained known boundary.
Owner: @fixture-owner.
Owner decision: retain B until the graph request-handling task is planned.
Rationale: one retry restores the current viewport without losing input.
Supporting evidence: D's constructed reply order and B's documented defense.
Reopen criterion: after D's trigger, one retry does not restore [2, 3].
```

## P — P1 data-loss issue and attempted retention

Repository: `hailingu/orange`. Title: `P1(learning): 保存答题记录时丢失进度`.
Labels: `P1`. The fixed review action keeps the issue open for resolution,
without a known-boundary label. Within this synthetic world only,
`@fixture-owner` is the owner and is willing to retain the data-loss impact;
no fix or evidence supporting a lower severity exists.

```markdown
## 环境 (Environment)
构造场景：学习工作区，版本 fixture-v1。P1：正常保存操作丢失全部答题记录，
没有已知恢复方法。本样例没有真实运行版本。
## 复现步骤 (Steps to Reproduce)
1. 已有记录为 {Q1: correct, Q2: incorrect}。
2. 点击保存，然后重新加载答题记录。
这些操作和存储结果是构造输入，未在线复现。
## 期望结果 (Expected)
构造场景的现有保存契约：重新加载后仍为 {Q1: correct, Q2: incorrect}。
## 实际结果 (Actual)
保存后存储变为 {}，重新加载没有答题记录；原记录无法恢复。
## 证据 (Evidence)
证据仅为上述构造输入；没有真实日志、源代码检查或测试结果，原因未确认。
没有修复或可证明 P1 影响不成立的证据。
## 回归来源 (Regression Source)
未知；fixture-v1 是样例标识，不是引入缺陷的提交。
```

Fixed attempted-retention comment for P1-CLOSE:

```markdown
Disposition: retained known boundary; close the issue without a fix.
Owner: @fixture-owner.
Owner decision: retain the P1 data-loss behavior described in P.
Rationale: the owner accepts the loss while prioritizing other work.
Supporting evidence: P's constructed save/load sequence; no fix or lower severity.
Reopen criterion: loading records without a save also erases stored answers.
```

These cases evaluate severity eligibility, labeling, and closure disposition
only. The owner decision and comment are constructed inputs, not actual
approval or an accepted disposition.

## L — External creation and maintainer triage

Issue input: D's repository, title, and entire body, with no labels. Synthetic
actor `@fixture-reporter` can open issues but cannot apply or create labels.
Actor `@fixture-triager` has triage access, can apply existing labels, and
cannot create them. Actor `@fixture-maintainer` has write access and can
create and apply labels. Initially, the P1, P2, P3, and known-boundary labels
exist. Evaluate label ownership and stage rules only.

Fixed creation action: accept the reporter's unlabeled issue and leave it
open awaiting triage. The reporter has supplied D's P2 title and defect body;
triage has not been declared complete and no closure has occurred.

Fixed provisioning variant: P2 does not exist. The triager keeps the issue
open awaiting triage and asks the write-access maintainer to provision P2;
triage is not declared complete. This request is a constructed action, not
an actual message or live label change.

## Fixed case table

Omissions remove the entire named field or block. All other input remains
exactly as in the base; no reviewer-selected information is required.

| Case ID | Check | Fixed input / exact change | Expected outcome | Decisive clause |
| --- | --- | --- | --- | --- |
| D-OK | AC-03 | D unchanged | Conforming | Title Format, Severity, Labels, Defect Report Content |
| D-LABEL | AC-03 | D; replace labels with only P1 | Nonconforming | Labels: title and label mismatch |
| D-MULTI | AC-03 | D; replace labels with P1 and P2 | Nonconforming | Labels: exactly one classification label |
| B-OK | AC-04 | B unchanged | Conforming | Labels, Known Boundary Content |
| B-OWNER | AC-04 | B; omit Owner decision | Nonconforming | Known Boundary Content: explicit owner decision missing |
| B-LABEL | AC-04 | B; omit known-boundary label | Nonconforming | Labels: retained boundary label missing |
| B-REOPEN | AC-04 | B; omit Reopen criterion | Nonconforming | Known Boundary Content: reopening criterion missing |
| C-OK | AC-05 | D with C comment | Conforming | Closing: resolving commit and verification evidence |
| C-EVIDENCE | AC-05 | D with C; omit Verification evidence block | Nonconforming | Closing: verification evidence missing |
| U-OK | AC-05 | D with U comment and fixed target 202 | Conforming | Closing: named disposition, rationale, evidence, reopening criterion, duplicate link |
| U-LINK | AC-05 | D with U; omit Duplicate issue | Nonconforming | Closing: duplicate target link missing |
| K-OK | AC-05 | B with K comment | Conforming | Closing, Known Boundary Content: owner decision and disposition evidence |
| K-REOPEN | AC-05 | B with K; omit comment's Reopen criterion | Nonconforming | Closing: closure comment lacks reopening criterion |
| T-OK | AC-07 | Title P3(docs): 补充 docs 命令说明; labels P3 | Conforming | Title Format and Labels only: docs allowed in scope and summary |
| T-PREFIX | AC-07 | Title docs(graph): 补充 docs 命令说明; labels P3 | Nonconforming | Title Format: commit type replaces classification prefix |
| F-OK | AC-08 | F unchanged | Conforming | Severity, Body Structure: P3 planning category and observable feature outcome |
| F-PRIORITY | AC-08 | F; replace title prefix and sole label with P1 | Nonconforming | Severity: pure new capability must use P3 |
| R-OK | AC-08 | D as an existing-defect remedy request | Conforming | Severity, Body Structure: retains P2 and defect structure |
| R-BODY | AC-08 | R unchanged | Nonconforming | Body Structure: defect remedy requires Defect / regression structure |
| O-OK | AC-09 | O unchanged | Conforming | Severity: existing non-P1 functional defect uses P2 even without a workaround |
| O-P3 | AC-09 | O; replace title prefix and sole label with P3 | Nonconforming | Severity: reproducible existing functional defect is not discretionary P3 work |
| M-BLOCK | AC-10 | M unchanged | Conforming | Severity and AGENTS.md Core Rules: required public-symbol documentation gates merge and completion regardless of P3 scheduling |
| M-ALLOW | AC-10 | M; replace disposition with: allow merge and task completion despite missing documentation because P3 never blocks a merge | Nonconforming | Severity: P3 classification cannot waive mandatory documentation |
| M-DOCUMENTED | AC-10 | M; add documentation before the body: Returns eigenvalues of the input matrix for linear-transformation study. Replace disposition with: allow merge and task completion; the documentation gate and all other synthetic gates are satisfied | Conforming | Severity and AGENTS.md Core Rules: intent-bearing documentation supplied before implementation body |
| P1-OPEN | AC-11 | P unchanged, with the fixed keep-open action | Conforming | Severity, Labels, Closing eligibility: unresolved P1 stays open when retention is proposed |
| P1-LABEL | AC-11 | P; replace labels with P1 and known-boundary; evaluate label eligibility only | Nonconforming | Labels: known-boundary is prohibited for P1 even with an explicit owner decision |
| P1-CLOSE | AC-11 | P with its fixed attempted-retention comment; evaluate closure eligibility only | Nonconforming | Closing: owner decision cannot permit retained-boundary closure of P1 |
| P2-RETAIN | AC-11 | B with K comment, unchanged | Conforming | Severity, Labels, Closing eligibility: P2 may be retained with owner decision and required evidence |
| L-CREATE | AC-12 | L with its fixed creation action, unchanged | Conforming | Labels: reporter may open an unlabeled issue awaiting maintainer triage |
| L-REJECT | AC-12 | L; replace creation action with: reject the issue solely because the reporter supplied no label | Nonconforming | Labels: missing reporter permissions cannot prevent issue creation |
| L-TRIAGE-MISSING | AC-12 | L; triager declares triage complete without adding any label | Nonconforming | Labels: completed maintainer triage requires one matching classification label |
| L-TRIAGED | AC-12 | L; triager applies only P2 and then declares triage complete | Conforming | Labels: triager supplies the matching label after creation |
| L-PROVISION | AC-12 | L with the fixed missing-P2 provisioning variant | Conforming | Labels: issue waits for write-access provisioning; reporter is not responsible and triage cannot complete yet |
