#!/usr/bin/env bash
# Decides whether a push run of build.yml may promote the tests its PR already passed, rather
# than run them again. It prints `promoted=true` only when all of these hold:
#
#   - the event is `push` (a master push or a v* tag push);
#   - SHA is the merge commit of exactly one merged PR into master;
#   - SHA's tree is the tree of that PR's head commit, so the files are the ones it tested;
#   - the most recently completed `build-and-test` check run on that head concluded `success`.
#
# Anything else is `promoted=false`, and so is any answer the GitHub API would not give: an
# error is logged and the script still exits 0, so a failed check means "run everything".
#
# Inputs by environment: GH_TOKEN, REPO (owner/name), SHA (the commit under test) and EVENT
# (github.event_name). It writes `promoted=...` to $GITHUB_OUTPUT and the decision line to
# $GITHUB_STEP_SUMMARY when they are set. It needs gh, and no jq: gh's own --jq does the parsing.
#
# Sourced rather than run, it only defines its functions, so each check can be tried on its own:
#
#   source .github/scripts/promote-check.sh
#   REPO=bilbospocketses/streamflex check_passed <commit>
#   REPO=bilbospocketses/streamflex same_tree <commit> <commit>

set -u

# api <path> <jq filter>: prints gh's answer, or logs the error and fails
api() {
  local out err status
  err=$(mktemp)
  out=$(gh api "$1" --jq "$2" 2>"$err")
  status=$?
  if [ "$status" -ne 0 ]; then
    echo "error: gh api $1 failed (exit $status): $(tr '\n' ' ' < "$err")"
    rm -f "$err"
    return 1
  fi
  rm -f "$err"
  printf '%s\n' "${out//$'\r'/}"
}

is_sha() {
  [[ "$1" =~ ^[0-9a-f]{40}$ ]]
}

# merged_pr <sha>: sets PR_NUMBER and PR_HEAD when sha is the merge commit of exactly one
# merged PR into master
merged_pr() {
  local sha=$1 found count
  PR_NUMBER=""
  PR_HEAD=""
  found=$(api "repos/$REPO/commits/$sha/pulls" \
    ".[] | select(.merged_at != null and .base.ref == \"master\" and .merge_commit_sha == \"$sha\") | \"\(.number) \(.head.sha)\"") \
    || { echo "$found"; return 1; }
  count=$(grep -c . <<< "$found")
  if [ "$count" -ne 1 ]; then
    echo "pull request: ${sha:0:7} is the merge commit of $count merged PRs into master"
    return 1
  fi
  read -r PR_NUMBER PR_HEAD <<< "$found"
  if ! is_sha "$PR_HEAD"; then
    echo "pull request: PR #$PR_NUMBER has no usable head commit ($PR_HEAD)"
    return 1
  fi
  echo "pull request: ${sha:0:7} is the merge commit of PR #$PR_NUMBER, head ${PR_HEAD:0:7}"
}

# tree_of <sha>: prints the commit's tree
tree_of() {
  api "repos/$REPO/commits/$1" ".commit.tree.sha"
}

# same_tree <sha> <sha>: succeeds when both commits have one tree; sets TREE to the first's
same_tree() {
  local a=$1 b=$2 tree_b
  TREE=$(tree_of "$a") || { echo "$TREE"; return 1; }
  tree_b=$(tree_of "$b") || { echo "$tree_b"; return 1; }
  if [ -z "$TREE" ] || [ "$TREE" != "$tree_b" ]; then
    echo "tree: ${a:0:7} has tree ${TREE:0:8} and ${b:0:7} has tree ${tree_b:0:8}, which differ"
    return 1
  fi
  echo "tree: ${a:0:7} and ${b:0:7} both have tree ${TREE:0:8}"
}

# check_passed <sha>: succeeds when the most recently completed build-and-test check run on the
# commit concluded success
check_passed() {
  local sha=$1 last conclusion id completed
  last=$(api "repos/$REPO/commits/$sha/check-runs?check_name=build-and-test&filter=all&per_page=100" \
    '[.check_runs[] | select(.status == "completed" and .app.slug == "github-actions")] | sort_by(.completed_at) | last | if . == null then "none" else "\(.conclusion) \(.id) \(.completed_at)" end') \
    || { echo "$last"; return 1; }
  read -r conclusion id completed <<< "$last"
  if [ "$conclusion" = "none" ]; then
    echo "build-and-test: ${sha:0:7} has no completed build-and-test check run"
    return 1
  fi
  if [ "$conclusion" != "success" ]; then
    echo "build-and-test: not passed on ${sha:0:7}: check run $id concluded $conclusion at $completed"
    return 1
  fi
  echo "build-and-test: passed on ${sha:0:7}: check run $id concluded success at $completed"
}

# decide: sets DECISION and REASON
decide() {
  DECISION=false
  if [ "${EVENT:-}" != "push" ]; then
    echo "event: ${EVENT:-unset}, not push"
    REASON="event is ${EVENT:-unset}; only a push run can promote"
    return
  fi
  echo "event: push"
  if [ -z "${REPO:-}" ] || ! is_sha "${SHA:-}"; then
    REASON="REPO or SHA is missing or malformed (REPO=${REPO:-unset} SHA=${SHA:-unset})"
    return
  fi
  if ! merged_pr "$SHA"; then
    REASON="${SHA:0:7} is not the merge commit of exactly one merged PR into master"
    return
  fi
  if ! same_tree "$SHA" "$PR_HEAD"; then
    REASON="the tree of ${SHA:0:7} is not the tree of PR #$PR_NUMBER head ${PR_HEAD:0:7}, or could not be read"
    return
  fi
  if ! check_passed "$PR_HEAD"; then
    REASON="build-and-test did not pass on PR #$PR_NUMBER head ${PR_HEAD:0:7}"
    return
  fi
  DECISION=true
  REASON="tree ${TREE:0:8} matches PR #$PR_NUMBER head ${PR_HEAD:0:7}, whose build-and-test passed"
}

main() {
  local line
  DECISION=false
  REASON="the checks did not finish"
  decide
  line="promoted=$DECISION: $REASON"
  echo "$line"
  if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    echo "$line" >> "$GITHUB_STEP_SUMMARY"
  fi
  if [ -n "${GITHUB_OUTPUT:-}" ]; then
    echo "promoted=$DECISION" >> "$GITHUB_OUTPUT"
  fi
  return 0
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  main
  exit 0
fi
