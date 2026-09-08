SHELL := /usr/bin/env bash
.DEFAULT_GOAL := all

REPO_ROOT := ../..
include $(REPO_ROOT)/building-dependencies/slurmy-build.mk

.PHONY: all build clean distclean monitor submit sync

all: build
	@rm -f -- submit.sh
	@rm -rf -- submit.sh.files
	@set -eu; \
	solver_dir=$$(mktemp -d); \
	trap 'rm -rf -- "$$solver_dir"' EXIT INT TERM; \
	printf '%s\n' '$(abspath ../example-vampire/bin)' \
		'./vampire --mode casc --cores 1 --time_limit {{cpu-limit}} {{problem}}' \
		> "$$solver_dir/vampire.solver"; \
	printf '%s\n' '$(abspath ../example-e/bin)' \
		'./eprover --auto-schedule --proof-object --cpu-limit={{cpu-limit}} --memory-limit={{mem-limit}} {{problem}}' \
		> "$$solver_dir/e.solver"; \
	printf '%s\n' '$(abspath ../example-drodi/bin)' \
		"./drodi '-cores(1)' '-timeout({{cpu-limit}})' {{problem}}" \
		> "$$solver_dir/drodi.solver"; \
	python ../../slurmy.py \
		--host '$(SLURMY_HOST)' \
		--cpu-limit 3 \
		--wc-limit 6 \
		--mem-limit 1GiB \
		--cpu-request 1-core \
		--memory-request 1200MiB \
		--problems '$(CURDIR)/problems/**/*.p' \
		--solver "$$solver_dir/vampire.solver" \
		--solver "$$solver_dir/e.solver" \
		--solver "$$solver_dir/drodi.solver" \
		--runsolver '$(RUNSOLVER)' \
		--batch-size 5 \
		--max-parallel 6 \
		--worker-overhead 60 \
		--job-name Slurmy-combined-example \
		--sbatch-option=--partition=$(SLURMY_BUILD_PARTITION) \
		--output '$(CURDIR)/submit.sh'
	@printf '\n\033[1;32m✅ Generated one submission with 30 calls: 3 provers × 10 problems.\033[0m\n'
	@printf '   Every call has a 3-second CPU limit and a 6-second wall-clock limit.\n'
	@printf '\033[1;36m👉 Next: run "make submit" to send it to %s.\033[0m\n' '$(SLURMY_HOST)'

build:
	@printf '\033[1;34m🔨 Building all three provers and runsolver with Slurm on %s...\033[0m\n' '$(SLURMY_HOST)'
	@$(MAKE) -C ../example-vampire build SLURMY_HOST='$(SLURMY_HOST)' SLURMY_BUILD_PARTITION='$(SLURMY_BUILD_PARTITION)'
	@$(MAKE) -C ../example-e build SLURMY_HOST='$(SLURMY_HOST)' SLURMY_BUILD_PARTITION='$(SLURMY_BUILD_PARTITION)'
	@$(MAKE) -C ../example-drodi build SLURMY_HOST='$(SLURMY_HOST)' SLURMY_BUILD_PARTITION='$(SLURMY_BUILD_PARTITION)'
	@printf '\033[1;32m✅ All cluster-built artifacts are ready.\033[0m\n'

submit:
	@if [ ! -x submit.sh ] || [ ! -d submit.sh.files ]; then printf '❌ Nothing is ready. Run "make" first.\n' >&2; exit 1; fi
	@printf '\033[1;34m🚀 Sending all three provers and ten problems to %s, then submitting 30 calls...\033[0m\n\n' '$(SLURMY_HOST)'
	@SLURMY_HOST='$(SLURMY_HOST)' ./submit.sh
	@printf '\n\033[1;32m✅ Submitted successfully.\033[0m\n'
	@printf '\033[1;36m👉 Next: run "make sync" to download results until completion.\033[0m\n'

sync:
	@printf '\033[1;34m🔄 Syncing the selected (or latest) Slurmy job from %s...\033[0m\n' '$(SLURMY_HOST)'
	@cd ../.. && python ./slurmy-sync.py $(SLURMY_ID) --host '$(SLURMY_HOST)' --follow --interval 2
	@printf '\n\033[1;32m✅ Results are synchronized under ../../slurmy-results/.\033[0m\n'

monitor:
	@printf '\033[1;34m📊 Opening the Slurmy dashboard for %s...\033[0m\n\n' '$(SLURMY_HOST)'
	@python ../../slurmy-monitor.py --host '$(SLURMY_HOST)'
	@printf '\n👋 Closed the Slurmy dashboard.\n'

clean:
	@rm -f -- submit.sh
	@rm -rf -- submit.sh.files
	@printf '\033[1;32m🧹 Removed the combined submission files.\033[0m\n'

distclean: clean
	@$(MAKE) -C ../example-vampire distclean
	@$(MAKE) -C ../example-e distclean
	@$(MAKE) -C ../example-drodi distclean
	@rm -f -- ../../building-dependencies/runsolver/runsolver ../../building-dependencies/runsolver/.built-on-*
	@printf '\033[1;32m🧹 Also removed every downloaded build artifact.\033[0m\n'
