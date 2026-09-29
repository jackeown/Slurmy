# Shared targets for explicit jobpair experiments. No solver limits live here.
SHELL := /usr/bin/env bash
.DEFAULT_GOAL := all
DEG_PAR ?= 1
BATCH_SIZE ?= $(DEG_PAR)
PAIR_ORDER ?= solver-major
SLURMY_HOST ?= datalab
SLURMY_PARTITION ?= CPU-amd
SLURMY_ID ?=
export SLURMY_HOST SLURMY_PARTITION
JOBPAIRS ?= jobpairs.csv
BUILDING ?= building.txt
LIMITER ?= resource_limiter_template.txt
BUILD_RECIPES ?=
AXIOMS ?=

.PHONY: all prepare prepare-submit submit monitor sync stop clean build
all: prepare

# Keep the generated submission complete and current. This target is deliberately
# safe to run repeatedly: it does nothing when the generated files are current,
# and refreshes them automatically after an input or Slurmy template changes.
prepare:
	@if [ ! -f submit.sh ] || [ ! -d submit.sh.files ]; then \
		rm -f -- submit.sh; rm -rf -- submit.sh.files; \
	fi
	@$(MAKE) --no-print-directory submit.sh

submit.sh: Makefile $(JOBPAIRS) $(BUILDING) $(LIMITER) $(AXIOMS) $(BUILD_RECIPES) $(REPO_ROOT)/slurmy.py $(REPO_ROOT)/building-dependencies/slurmy-build.py $(REPO_ROOT)/templates/workflow.mk $(wildcard $(REPO_ROOT)/templates/*.sh)
	@if [ -e submit.sh ] || [ -d submit.sh.files ]; then \
		printf '♻️  Inputs changed; refreshing generated submission files.\n'; \
		rm -f -- submit.sh; rm -rf -- submit.sh.files; \
	fi
	python '$(REPO_ROOT)/slurmy.py' '$(JOBPAIRS)' '$(BUILDING)' '$(LIMITER)' --deg_par '$(DEG_PAR)' --batch-size '$(BATCH_SIZE)' $(if $(AXIOMS),--axioms-file '$(AXIOMS)')
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
	python '$(REPO_ROOT)/slurmy-web.py' --background --host '$(SLURMY_HOST)' --directory '$(CURDIR)'
sync:
	cd '$(REPO_ROOT)' && python ./slurmy-sync.py $(SLURMY_ID) --host '$(SLURMY_HOST)' --follow --interval 2
stop:
	python '$(REPO_ROOT)/slurmy-cancel.py' --host '$(SLURMY_HOST)'
clean:
	rm -f -- submit.sh
	rm -rf -- submit.sh.files
	@printf '✅ Removed generated submission files; input specifications retained.\n'
