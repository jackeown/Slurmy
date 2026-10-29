# Shared targets for explicit solver-problem pair experiments. No solver limits live here.
SHELL := /usr/bin/env bash
.DEFAULT_GOAL := all
BATCH_SIZE ?= 1
PAIR_ORDER ?= solver-major
SLURMY_HOST ?= datalab
SLURMY_PARTITION ?= CPU-amd
SLURMY_RESERVATION ?=
SLURMY_MAX_OUTSTANDING ?= 32
SLURMY_ID ?=
export SLURMY_HOST SLURMY_PARTITION SLURMY_MAX_OUTSTANDING SLURMY_RESERVATION
JOBPAIRS ?= jobpairs.csv
BUILDING ?= building.txt
LIMITER ?= resource_limiter_template.txt
BUILD_RECIPES ?=
AXIOMS ?=

.PHONY: all prepare prepare-submit submit monitor sync stop resume clean build
all: prepare

# Keep the generated submission complete and current. This target is deliberately
# safe to run repeatedly: it does nothing when the generated files are current,
# and refreshes them automatically after an input or Slurmy template changes.
prepare:
	@if [ ! -f submit.sh ] || [ ! -d submit.sh.files ]; then \
		rm -f -- submit.sh; rm -rf -- submit.sh.files; \
	fi
	@$(MAKE) --no-print-directory submit.sh

submit.sh: Makefile $(JOBPAIRS) $(BUILDING) $(LIMITER) $(AXIOMS) $(BUILD_RECIPES) $(REPO_ROOT)/implementation/slurmy.py $(REPO_ROOT)/implementation/job_names.py $(REPO_ROOT)/implementation/check_threads.py $(REPO_ROOT)/implementation/ssh_transport.py $(REPO_ROOT)/implementation/package_inputs.py $(REPO_ROOT)/implementation/submit_coordinator.py $(REPO_ROOT)/implementation/build/slurmy-build.py $(REPO_ROOT)/implementation/templates/workflow.mk $(wildcard $(REPO_ROOT)/implementation/templates/*.sh)
	@if [ -e submit.sh ] || [ -d submit.sh.files ]; then \
		printf '♻️  Inputs changed; refreshing generated submission files.\n'; \
		rm -f -- submit.sh; rm -rf -- submit.sh.files; \
	fi
	@python '$(REPO_ROOT)/implementation/check_threads.py' '$(JOBPAIRS)' --host '$(SLURMY_HOST)' --partition '$(SLURMY_PARTITION)'
	python '$(REPO_ROOT)/implementation/slurmy.py' '$(JOBPAIRS)' '$(BUILDING)' '$(LIMITER)' --batch-size '$(BATCH_SIZE)' --pair-order '$(PAIR_ORDER)' --max-outstanding '$(SLURMY_MAX_OUTSTANDING)' $(if $(AXIOMS),--axioms-file '$(AXIOMS)')
	@printf '\n✅ Prepared the explicit jobpairs. Inspect submit.sh.files/.\n👉 make prepare-submit prepares again and dispatches; make submit dispatches these prepared files as-is.\n'

build: prepare
	bash ./submit.sh.files/builds/run.sh
	@printf '✅ Built and downloaded the declared resources.\n'

prepare-submit: prepare
	@$(MAKE) --no-print-directory submit

# Dispatch exactly the prepared submission; never regenerate submit.sh here.
submit:
	@test -f submit.sh && test -d submit.sh.files || { printf 'No prepared submission found. Run make prepare or make prepare-submit first.\n' >&2; exit 1; }
	bash ./submit.sh
	@printf '✅ Submitted. Next: make monitor, make sync, or make stop.\n'

monitor:
	python '$(REPO_ROOT)/web-app/main.py' --background --host '$(SLURMY_HOST)' --directory '$(CURDIR)'
sync:
	cd '$(REPO_ROOT)' && python ./implementation/slurmy-sync.py $(SLURMY_ID) --host '$(SLURMY_HOST)' --follow --interval 2
stop:
	@test -n '$(SLURMY_ID)' || { printf 'Set SLURMY_ID to the Slurmy job ID to stop.\n' >&2; exit 1; }
	python '$(REPO_ROOT)/implementation/slurmy-job.py' stop '$(SLURMY_ID)' --host '$(SLURMY_HOST)'
resume:
	@test -n '$(SLURMY_ID)' || { printf 'Set SLURMY_ID to the stopped Slurmy job ID to resume.\n' >&2; exit 1; }
	python '$(REPO_ROOT)/implementation/slurmy-job.py' resume '$(SLURMY_ID)' --host '$(SLURMY_HOST)'
clean:
	rm -f -- submit.sh
	rm -rf -- submit.sh.files
	@printf '✅ Removed generated submission files; input specifications retained.\n'
