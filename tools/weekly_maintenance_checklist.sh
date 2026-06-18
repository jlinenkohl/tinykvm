#!/usr/bin/env bash
set -euo pipefail

# Weekly branch maintenance assistant for TinyKVM.
# Phase 1: read-only audit.
# Phase 2: optional, prompt-gated execution of recommended actions.

ALLOW_DIRTY=0
RECOMMEND_ONLY=1
EXECUTE_MODE=0
COLOR_MODE="never"
COLOR_ENABLED=0
REPO_PATH=""

CLR_RED=""
CLR_GREEN=""
CLR_YELLOW=""
CLR_BLUE=""
CLR_CYAN=""
CLR_BOLD=""
CLR_RESET=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --allow-dirty) ALLOW_DIRTY=1 ;;
    --recommend-only) RECOMMEND_ONLY=1 ;;
    --execute) RECOMMEND_ONLY=0; EXECUTE_MODE=1 ;;
    --repo-path)
      shift
      if [[ $# -eq 0 ]]; then
        echo "Error: --repo-path requires a path argument" >&2
        exit 1
      fi
      REPO_PATH="$1"
      ;;
    --repo-path=*) REPO_PATH="${1#*=}" ;;
    --color=always) COLOR_MODE="always" ;;
    --color=auto) COLOR_MODE="auto" ;;
    --color=never) COLOR_MODE="never" ;;
    --color)
      echo "Error: use --color=always|auto|never" >&2
      exit 1
      ;;
    -h|--help)
      cat <<'EOF'
Usage: bash tools/weekly_maintenance_checklist.sh [options]

Options:
  --allow-dirty     Allow execution prompts even when working tree is dirty.
  --recommend-only  Audit + recommendations only (default mode).
  --execute         Enable prompt-gated execution of recommended actions.
  --repo-path PATH  Run checks against PATH (default: current working directory).
  --color=WHEN      Colorize output. WHEN is one of: never (default), auto, always.
  -h, --help        Show this help.

Behavior:
  1) Runs read-only checks first.
  2) Generates recommendations.
  3) In --execute mode: prompts for explicit authorization before each action.
EOF
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
  shift
done

case "$COLOR_MODE" in
  always) COLOR_ENABLED=1 ;;
  auto)
    if [[ -t 1 && "${TERM:-dumb}" != "dumb" ]]; then
      COLOR_ENABLED=1
    fi
    ;;
  never) COLOR_ENABLED=0 ;;
  *)
    echo "Error: invalid --color value: $COLOR_MODE" >&2
    exit 1
    ;;
esac

if (( COLOR_ENABLED == 1 )); then
  CLR_RED=$'\033[31m'
  CLR_GREEN=$'\033[32m'
  CLR_YELLOW=$'\033[33m'
  CLR_BLUE=$'\033[34m'
  CLR_CYAN=$'\033[36m'
  CLR_BOLD=$'\033[1m'
  CLR_RESET=$'\033[0m'
fi

style() {
  local color="$1"
  local text="$2"
  if (( COLOR_ENABLED == 1 )); then
    printf "%s%s%s" "$color" "$text" "$CLR_RESET"
  else
    printf "%s" "$text"
  fi
}

pad_right() {
  local text="$1"
  local width="$2"
  printf "%-*s" "$width" "$text"
}

branch_ab_color() {
  local ahead="$1"
  local behind="$2"
  if [[ "$behind" == "0" && "$ahead" == "0" ]]; then
    printf "%s" "$CLR_GREEN"
  elif [[ "$behind" != "0" && "$ahead" == "0" ]]; then
    printf "%s" "$CLR_RED"
  elif [[ "$behind" != "0" && "$ahead" != "0" ]]; then
    printf "%s" "$CLR_YELLOW"
  else
    printf "%s" "$CLR_CYAN"
  fi
}

if [[ -z "$REPO_PATH" ]]; then
  REPO_PATH="$PWD"
fi

if [[ ! -d "$REPO_PATH" ]]; then
  echo "Error: repository path does not exist: $REPO_PATH" >&2
  exit 1
fi

if ! git -C "$REPO_PATH" rev-parse --git-dir >/dev/null 2>&1; then
  echo "Error: target path is not a git repository: $REPO_PATH" >&2
  echo "Hint: pass --repo-path PATH to target a specific repository." >&2
  exit 1
fi

REPO_TOP="$(git -C "$REPO_PATH" rev-parse --show-toplevel)"
cd "$REPO_TOP"

BASE_REF="upstream/master"
if ! git show-ref --verify --quiet "refs/remotes/$BASE_REF"; then
  if git show-ref --verify --quiet "refs/remotes/upstream/main"; then
    BASE_REF="upstream/main"
  elif git show-ref --verify --quiet "refs/remotes/origin/master"; then
    BASE_REF="origin/master"
  elif git show-ref --verify --quiet "refs/remotes/origin/main"; then
    BASE_REF="origin/main"
  else
    echo "Error: could not resolve base ref. Expected one of upstream/master, upstream/main, origin/master, origin/main." >&2
    exit 1
  fi
fi

CURRENT_BRANCH="$(git branch --show-current 2>/dev/null || true)"
if [[ -z "$CURRENT_BRANCH" ]]; then
  CURRENT_BRANCH="(detached)"
fi

STATUS_SHORT="$(git status --short)"
DIRTY_COUNT=0
if [[ -n "$STATUS_SHORT" ]]; then
  DIRTY_COUNT="$(printf "%s\n" "$STATUS_SHORT" | wc -l | awk '{print $1}')"
fi

echo "$(style "$CLR_BOLD" "== Weekly Maintenance Audit (Read-Only) ==")"
echo "Repository : $(basename "$(git rev-parse --show-toplevel)")"
echo "Current    : $CURRENT_BRANCH"
echo "Base ref   : $BASE_REF"
echo "Dirty files: $DIRTY_COUNT"
if [[ -n "$STATUS_SHORT" ]]; then
  echo ""
  echo "Working tree changes:"
  echo "$STATUS_SHORT"
fi

echo ""
echo "$(style "$CLR_BOLD" "Local branches (type, ahead/behind vs $BASE_REF, last commit UTC, tracking):")"
printf "%-34s %-11s %-12s %-20s %-8s %-26s\n" "BRANCH" "TYPE" "AHEAD/BEHIND" "LAST_COMMIT_UTC" "HEAD" "UPSTREAM"
printf "%-34s %-11s %-12s %-20s %-8s %-26s\n" "------" "----" "------------" "---------------" "----" "--------"

mapfile -t LOCAL_BRANCHES < <(git for-each-ref --format='%(refname:short)' refs/heads)

declare -A BR_TYPE
declare -A BR_AHEAD
declare -A BR_BEHIND
declare -A BR_UPSTREAM
declare -A BR_AHEAD_UP
declare -A BR_BEHIND_UP
declare -A BR_LAST_COMMIT_UTC
declare -A BR_HEAD

classify_branch() {
  local b="$1"
  if [[ "$b" == pr-* ]]; then
    echo "pr"
  elif [[ "$b" == "master" || "$b" == "main" ]]; then
    echo "trunk"
  elif [[ "$b" == "vanilla" ]]; then
    echo "vanilla"
  elif [[ "$b" == *ifunc* || "$b" == *cabi* || "$b" == *rework* || "$b" == *track* ]]; then
    echo "integration"
  else
    echo "other"
  fi
}

for b in "${LOCAL_BRANCHES[@]}"; do
  t="$(classify_branch "$b")"
  BR_TYPE["$b"]="$t"

  # left-right: left is branch-only (ahead), right is base-only (behind)
  read -r ahead behind < <(git rev-list --left-right --count "$b...$BASE_REF")
  BR_AHEAD["$b"]="$ahead"
  BR_BEHIND["$b"]="$behind"

  up="$(git for-each-ref --format='%(upstream:short)' "refs/heads/$b")"
  BR_UPSTREAM["$b"]="$up"
  BR_AHEAD_UP["$b"]="-"
  BR_BEHIND_UP["$b"]="-"
  BR_LAST_COMMIT_UTC["$b"]="$(TZ=UTC git log -1 --date=format-local:'%Y-%m-%dT%H:%M:%SZ' --format='%cd' "$b")"
  BR_HEAD["$b"]="$(git rev-parse --short "$b")"
  if [[ -n "$up" ]] && git rev-parse --verify --quiet "$up" >/dev/null; then
    read -r aup bup < <(git rev-list --left-right --count "$b...$up")
    BR_AHEAD_UP["$b"]="$aup"
    BR_BEHIND_UP["$b"]="$bup"
  fi

  ab="${ahead}/${behind}"
  ab_padded="$(pad_right "$ab" 12)"
  last_commit_utc="${BR_LAST_COMMIT_UTC[$b]}"
  head_short="${BR_HEAD[$b]}"
  up_show="${up:--}"
  ab_color="$(branch_ab_color "$ahead" "$behind")"
  ab_styled="$(style "$ab_color" "$ab_padded")"
  printf "%-34s %-11s %s %-20s %-8s %-26s\n" "$b" "$t" "$ab_styled" "$last_commit_utc" "$head_short" "$up_show"
done

echo ""
echo "$(style "$CLR_BOLD" "PR-branch quick health (relative to $BASE_REF):")"
for b in "${LOCAL_BRANCHES[@]}"; do
  if [[ "${BR_TYPE[$b]}" != "pr" ]]; then
    continue
  fi
  commit_count="$(git rev-list --count "$BASE_REF..$b")"
  merged="no"
  if git merge-base --is-ancestor "$b" "$BASE_REF"; then
    merged="yes"
  fi
  if [[ "$merged" == "yes" ]]; then
    line_color="$CLR_GREEN"
  else
    line_color="$CLR_YELLOW"
  fi
  echo "$(style "$line_color" "- $b: commits_above_base=$commit_count merged_into_base=$merged")"
done

# Recommendations and optional executable actions

declare -a ACTION_DESCS
declare -a ACTION_CMDS

action_add() {
  ACTION_DESCS+=("$1")
  ACTION_CMDS+=("$2")
}

echo ""
echo "$(style "$CLR_BOLD" "== Recommendations ==")"

if (( DIRTY_COUNT > 0 )); then
  echo "$(style "$CLR_RED" "- Working tree is dirty. Recommendation: avoid branch-switching maintenance until WIP is committed/stashed.")"
fi

echo "$(style "$CLR_BLUE" "- Always fetch remotes before making merge/rebase decisions (recommended weekly).")"
action_add "Fetch latest remote refs (origin + upstream)" "git fetch --all --prune"

if [[ -n "${BR_AHEAD[master]:-}" && -n "${BR_BEHIND[master]:-}" ]]; then
  if [[ "${BR_AHEAD[master]}" == "0" && "${BR_BEHIND[master]}" != "0" ]]; then
    echo "$(style "$CLR_YELLOW" "- master is behind $BASE_REF and has no local-only commits: fast-forward recommended.")"
    action_add "Fast-forward master from $BASE_REF" "git switch master && git merge --ff-only $BASE_REF"
  elif [[ "${BR_AHEAD[master]}" != "0" && "${BR_BEHIND[master]}" != "0" ]]; then
    echo "$(style "$CLR_RED" "- master has diverged from $BASE_REF: inspect manually before any update.")"
  fi
fi

for b in "${LOCAL_BRANCHES[@]}"; do
  t="${BR_TYPE[$b]}"
  ahead="${BR_AHEAD[$b]}"
  behind="${BR_BEHIND[$b]}"

  if [[ "$t" == "integration" && "$behind" != "0" ]]; then
    echo "$(style "$CLR_YELLOW" "- $b is behind $BASE_REF by $behind commit(s): consider rebasing when ready to resume that track.")"
    action_add "Rebase integration branch $b onto $BASE_REF" "git switch $b && git rebase $BASE_REF"
  fi

  if [[ "$t" == "pr" ]]; then
    if [[ "$behind" != "0" ]]; then
      echo "$(style "$CLR_YELLOW" "- $b is behind $BASE_REF by $behind commit(s): do not auto-update unless reviewer asks; keep PR stable by default.")"
    fi
    if [[ "$ahead" == "0" ]]; then
      echo "$(style "$CLR_YELLOW" "- $b has no commits above $BASE_REF: candidate for cleanup if obsolete.")"
    fi
  fi

  if [[ "$t" == "vanilla" ]]; then
    echo "$(style "$CLR_BLUE" "- vanilla policy reminder: keep pristine and avoid active feature work on this branch.")"
  fi
done

if (( RECOMMEND_ONLY == 1 )); then
  echo ""
  if (( EXECUTE_MODE == 0 )); then
    echo "Recommend-only mode is the default; no executable prompts shown."
    echo "Use --execute to enable prompt-gated actions."
  else
    echo "Recommend-only mode enabled; no executable prompts shown."
  fi
  exit 0
fi

if (( ${#ACTION_CMDS[@]} == 0 )); then
  echo ""
  echo "No executable recommendations were generated."
  exit 0
fi

if (( DIRTY_COUNT > 0 && ALLOW_DIRTY == 0 )); then
  echo ""
  echo "Execution is blocked because the working tree is dirty."
  echo "Use --allow-dirty to override, or run again after committing/stashing WIP."
  exit 0
fi

echo ""
echo "== Authorized Execution =="
echo "Each action requires explicit approval."

auto_all=0

for i in "${!ACTION_CMDS[@]}"; do
  n=$((i + 1))
  desc="${ACTION_DESCS[$i]}"
  cmd="${ACTION_CMDS[$i]}"

  echo ""
  echo "[$n/${#ACTION_CMDS[@]}] $desc"
  echo "Command: $cmd"

  if (( auto_all == 0 )); then
    read -r -p "Authorize this action? [y]es/[n]o/[a]ll/[q]uit: " reply
    case "${reply,,}" in
      y|yes)
        ;;
      a|all)
        auto_all=1
        ;;
      q|quit)
        echo "Stopping by user request."
        break
        ;;
      *)
        echo "Skipped."
        continue
        ;;
    esac
  fi

  set +e
  bash -lc "$cmd"
  rc=$?
  set -e
  if (( rc != 0 )); then
    echo "Action failed with exit code $rc."
    read -r -p "Continue with remaining actions? [y/N]: " cont
    case "${cont,,}" in
      y|yes) ;;
      *)
        echo "Stopping after failure."
        break
        ;;
    esac
  else
    echo "Action completed."
  fi
done

echo ""
echo "Done. Final status:"
git status --short
