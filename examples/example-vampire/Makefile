SHELL := /usr/bin/env bash
.DEFAULT_GOAL := all

REPO_ROOT := ../..
include $(REPO_ROOT)/slurmy-build.mk

.PHONY: all build clean distclean monitor submit

VAMPIRE := $(abspath bin/vampire)
VAMPIRE_STAMP := $(abspath bin/.vampire-built-on-$(SLURMY_BUILD_TAG))

all: build
	@rm -f -- submit.sh
	@rm -rf -- submit.sh.files
	@set -eu; \
	solver_file=$$(mktemp); \
	trap 'rm -f -- "$$solver_file"' EXIT INT TERM; \
	printf '%s\n' '$(CURDIR)/bin' \
		'./vampire --mode casc --cores 1 --time_limit {{cpu-limit}} {{problem}}' \
		> "$$solver_file"; \
	python ../../slurmy.py \
		--host '$(SLURMY_HOST)' \
		--cpu-limit 8 \
		--wc-limit 12 \
		--mem-limit 1GiB \
		--cpu-request 1-core \
		--memory-request 1200MiB \
		--problems '$(CURDIR)/problems/*.p' \
		--solver "$$solver_file" \
		--runsolver '$(RUNSOLVER)' \
		--batch-size 2 \
		--max-parallel 2 \
		--worker-overhead 60 \
		--job-name Slurmy-vampire-example \
		--sbatch-option=--partition=$(SLURMY_BUILD_PARTITION) \
		--output '$(CURDIR)/submit.sh'
	@printf '\n\033[1;32m✅ Generated the Vampire submission.\033[0m\n'
	@printf '   Inspect submit.sh and submit.sh.files/ if you want to review it.\n'
	@printf '\033[1;36m👉 Next: run "make submit" to send it to %s.\033[0m\n' '$(SLURMY_HOST)'

build: $(VAMPIRE) $(RUNSOLVER)
	@printf '\033[1;32m✅ Vampire and runsolver were built on %s and are ready.\033[0m\n' '$(SLURMY_HOST)'

$(VAMPIRE_STAMP): build.sh $(SLURMY_BUILD_DRIVER)
	@printf '\033[1;34m🔨 Building Vampire in a Slurm job on %s...\033[0m\n' '$(SLURMY_HOST)'
	python '$(SLURMY_BUILD_DRIVER)' \
		--host '$(SLURMY_HOST)' \
		--name vampire \
		--recipe '$(abspath build.sh)' \
		--output '$(abspath bin)' \
		--artifact vampire \
		--cpus-per-task 8 \
		--memory 8GiB \
		--time 00:45:00 \
		--sbatch-option=--partition=$(SLURMY_BUILD_PARTITION)
	@touch '$@'
	@printf '\033[1;32m✅ Built Vampire on %s and downloaded it.\033[0m\n' '$(SLURMY_HOST)'

$(VAMPIRE): $(VAMPIRE_STAMP)
	@test -x '$@' || { printf '❌ Remote build did not produce %s.\n' '$@' >&2; exit 1; }

submit:
	@if [ ! -x submit.sh ] || [ ! -d submit.sh.files ]; then printf '❌ Nothing is ready. Run "make" first.\n' >&2; exit 1; fi
	@printf '\033[1;34m🚀 Transferring the prepared files to %s and submitting the array...\033[0m\n\n' '$(SLURMY_HOST)'
	@SLURMY_HOST='$(SLURMY_HOST)' ./submit.sh
	@printf '\n\033[1;32m✅ Submitted successfully.\033[0m\n'
	@printf '\033[1;36m👉 Next: run "make monitor" to watch its progress.\033[0m\n'

monitor:
	@printf '\033[1;34m📊 Opening the Slurmy dashboard for %s...\033[0m\n\n' '$(SLURMY_HOST)'
	@python ../../slurmy-monitor.py --host '$(SLURMY_HOST)'
	@printf '\n👋 Closed the Slurmy dashboard.\n'

clean:
	@rm -f -- submit.sh
	@rm -rf -- submit.sh.files
	@printf '\033[1;32m🧹 Removed the generated submission files.\033[0m\n'

distclean: clean
	@rm -rf -- source bin
	@printf '\033[1;32m🧹 Also removed old local source data and downloaded Vampire artifacts.\033[0m\n'
