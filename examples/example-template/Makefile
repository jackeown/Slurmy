SHELL := /usr/bin/env bash
.DEFAULT_GOAL := all

REPO_ROOT := ../..

# -----------------------------------------------------------------------------
# Workflow variables
# Copy this directory to example-NAME and edit these values, or run the
# interactive ../example-maker/example-maker.py application to create them.
# A generated workflow.mk may override any value without changing this template.
# -----------------------------------------------------------------------------
# SSH alias or host name used by every build, submit, monitor, and sync command.
SLURMY_HOST ?= datalab
# Slurm partition used for both compilation and theorem-prover experiments.
SLURMY_BUILD_PARTITION ?= CPU-amd
# Short lowercase name used in local stamps, remote build IDs, and Slurm jobs.
EXAMPLE_NAME ?= my-solver
# Executable path produced below bin/ by BUILD_RECIPE.
SOLVER_ARTIFACT ?= my-solver
# Solver root and invocation description consumed by slurmy.py.
SOLVER_DESCRIPTION ?= solver.solver
# Optional newline-delimited files containing existing solver-description paths
# and problem globs. The interactive maker uses these so paths may contain spaces.
SOLVER_PATHS_FILE ?=
PROBLEM_GLOBS_FILE ?=
# Bash recipe executed by slurmy-build.py on a compute node.
BUILD_RECIPE ?= build.sh

# Every quoted glob contributes problems. Spaces in glob paths are unsupported.
PROBLEM_GLOBS ?= $(CURDIR)/problems/*.p

# Per-solver-call limits enforced by runsolver.
CPU_LIMIT ?= 60
WALL_LIMIT ?= 70
MEMORY_LIMIT ?= 2GiB
# Resources requested for each Slurm array task.
CPU_REQUEST ?= 1-core
MEMORY_REQUEST ?= 2300MiB
# Calls run sequentially per array task, then the cluster-wide concurrency cap.
BATCH_SIZE ?= 20
MAX_PARALLEL ?= 100
# Extra seconds for each array task to stage and save results around solver calls.
WORKER_OVERHEAD ?= 60
# Additional sbatch directives applied to the theorem-prover job.
SUBMIT_SBATCH_OPTIONS ?= --partition=$(SLURMY_BUILD_PARTITION)
# Leave empty to make sync use the newest job, or pass SLURMY_ID=name_timestamp_pid.
SLURMY_ID ?=

# Resources for the separate Slurm job that compiles the solver.
BUILD_CPUS ?= 8
BUILD_MEMORY ?= 8GiB
BUILD_TIME ?= 00:30:00
# Additional sbatch directives applied to the solver build job.
BUILD_SBATCH_OPTIONS ?= --partition=$(SLURMY_BUILD_PARTITION)

-include workflow.mk
include $(REPO_ROOT)/building-dependencies/slurmy-build.mk

SOLVER := $(abspath bin/$(SOLVER_ARTIFACT))
SOLVER_STAMP := $(abspath bin/.$(EXAMPLE_NAME)-built-on-$(SLURMY_BUILD_TAG))

ifeq ($(strip $(SOLVER_PATHS_FILE)),)
WORKFLOW_BUILD_TARGETS := $(SOLVER)
WORKFLOW_BUILD_MESSAGE := $(EXAMPLE_NAME) and runsolver were built on $(SLURMY_HOST) and are ready.
else
WORKFLOW_BUILD_TARGETS :=
WORKFLOW_BUILD_MESSAGE := Existing solver descriptions and runsolver are ready.
endif

.PHONY: all build clean distclean monitor submit sync

all: build
	@rm -f -- submit.sh
	@rm -rf -- submit.sh.files
	@set -euo pipefail; \
	slurmy_args=( \
		--host '$(SLURMY_HOST)' \
		--cpu-limit '$(CPU_LIMIT)' \
		--wc-limit '$(WALL_LIMIT)' \
		--mem-limit '$(MEMORY_LIMIT)' \
		--cpu-request '$(CPU_REQUEST)' \
		--memory-request '$(MEMORY_REQUEST)' \
		--runsolver '$(RUNSOLVER)' \
		--batch-size '$(BATCH_SIZE)' \
		--max-parallel '$(MAX_PARALLEL)' \
		--worker-overhead '$(WORKER_OVERHEAD)' \
		--job-name 'Slurmy-$(EXAMPLE_NAME)' \
		$(foreach option,$(SUBMIT_SBATCH_OPTIONS),--sbatch-option='$(option)') \
		--output '$(CURDIR)/submit.sh' \
	); \
	if [[ -n '$(SOLVER_PATHS_FILE)' ]]; then \
		while IFS= read -r solver; do [[ -z "$$solver" ]] || slurmy_args+=(--solver "$$solver"); done < '$(SOLVER_PATHS_FILE)'; \
	else \
		slurmy_args+=(--solver '$(abspath $(SOLVER_DESCRIPTION))'); \
	fi; \
	if [[ -n '$(PROBLEM_GLOBS_FILE)' ]]; then \
		while IFS= read -r pattern; do [[ -z "$$pattern" ]] || slurmy_args+=(--problems "$$pattern"); done < '$(PROBLEM_GLOBS_FILE)'; \
	else \
		$(foreach pattern,$(PROBLEM_GLOBS),slurmy_args+=(--problems '$(pattern)');) \
	fi; \
	python '$(abspath $(REPO_ROOT)/slurmy.py)' "$${slurmy_args[@]}"
	@printf '\n\033[1;32m✅ Generated the %s submission.\033[0m\n' '$(EXAMPLE_NAME)'
	@printf '   Inspect submit.sh and submit.sh.files/ before submitting.\n'
	@printf '\033[1;36m👉 Next: run "make submit" to send it to %s.\033[0m\n' '$(SLURMY_HOST)'

build: $(WORKFLOW_BUILD_TARGETS) $(RUNSOLVER)
	@printf '\033[1;32m✅ %s\033[0m\n' '$(WORKFLOW_BUILD_MESSAGE)'

$(SOLVER_STAMP): $(BUILD_RECIPE) $(SLURMY_BUILD_DRIVER)
	@printf '\033[1;34m🔨 Building %s in a Slurm job on %s...\033[0m\n' '$(EXAMPLE_NAME)' '$(SLURMY_HOST)'
	python '$(SLURMY_BUILD_DRIVER)' \
		--host '$(SLURMY_HOST)' \
		--name '$(EXAMPLE_NAME)' \
		--recipe '$(abspath $(BUILD_RECIPE))' \
		--output '$(abspath bin)' \
		--artifact '$(SOLVER_ARTIFACT)' \
		--cpus-per-task '$(BUILD_CPUS)' \
		--memory '$(BUILD_MEMORY)' \
		--time '$(BUILD_TIME)' \
		$(foreach option,$(BUILD_SBATCH_OPTIONS),--sbatch-option='$(option)')
	@touch '$@'
	@printf '\033[1;32m✅ Built %s on %s and downloaded it.\033[0m\n' '$(EXAMPLE_NAME)' '$(SLURMY_HOST)'

$(SOLVER): $(SOLVER_STAMP)
	@test -x '$@' || { printf '❌ Remote build did not produce executable %s.\n' '$@' >&2; exit 1; }

submit:
	@if [ ! -x submit.sh ] || [ ! -d submit.sh.files ]; then printf '❌ Nothing is ready. Run "make" first.\n' >&2; exit 1; fi
	@printf '\033[1;34m🚀 Sending %s to %s and submitting its array...\033[0m\n\n' '$(EXAMPLE_NAME)' '$(SLURMY_HOST)'
	@SLURMY_HOST='$(SLURMY_HOST)' ./submit.sh
	@printf '\n\033[1;32m✅ Submitted successfully.\033[0m\n'
	@printf '\033[1;36m👉 Next: run "make monitor" or "make sync".\033[0m\n'

sync:
	@printf '\033[1;34m🔄 Syncing the selected (or latest) Slurmy job from %s...\033[0m\n' '$(SLURMY_HOST)'
	@cd '$(REPO_ROOT)' && python ./slurmy-sync.py $(SLURMY_ID) --host '$(SLURMY_HOST)' --follow --interval 2
	@printf '\n\033[1;32m✅ Results are synchronized under %s.\033[0m\n' '$(abspath $(REPO_ROOT)/slurmy-results)'

monitor:
	@printf '\033[1;34m📊 Opening the Slurmy dashboard for %s...\033[0m\n\n' '$(SLURMY_HOST)'
	@python '$(abspath $(REPO_ROOT)/slurmy-monitor.py)' --host '$(SLURMY_HOST)'
	@printf '\n👋 Closed the Slurmy dashboard.\n'

clean:
	@rm -f -- submit.sh
	@rm -rf -- submit.sh.files
	@printf '\033[1;32m🧹 Removed the generated submission files.\033[0m\n'

distclean: clean
	@rm -rf -- bin
	@printf '\033[1;32m🧹 Also removed downloaded solver artifacts.\033[0m\n'
