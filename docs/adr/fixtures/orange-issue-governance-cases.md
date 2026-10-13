<!-- Governance proposal: docs/adr/ADR-0001-orange-issue-governance.md -->
# Fixed issue-governance review fixtures

These constructed inputs make the Proposed ADR's AC-03 through AC-05,
AC-07, and AC-08 reproducible. They are not real issues, captured logs,
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
