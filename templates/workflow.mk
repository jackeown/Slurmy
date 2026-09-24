# Shared targets for explicit jobpair experiments. No solver limits live here.
SHELL := /usr/bin/env bash
.DEFAULT_GOAL := all
DEG_PAR ?= 1
SLURMY_HOST ?= datalab
SLURMY_PARTITION ?= CPU-amd
SLURMY_ID ?=
export SLURMY_HOST SLURMY_PARTITION
JOBPAIRS ?= jobpairs.csv
BUILDING ?= building.txt
LIMITER ?= resource_limiter_template.txt

.PHONY: all prepare submit monitor sync stop clean build
all: prepare

# Keep the generated submission complete and current. This target is deliberately
# safe to run repeatedly: it does nothing when the generated files are current,
# and refreshes them automatically after an input or Slurmy template changes.
prepare:
	@if [ ! -f submit.sh ] || [ ! -d submit.sh.files ]; then \
		rm -f -- submit.sh; rm -rf -- submit.sh.files; \
	fi
	@$(MAKE) --no-print-directory submit.sh

submit.sh: $(JOBPAIRS) $(BUILDING) $(LIMITER) $(REPO_ROOT)/slurmy.py $(wildcard $(REPO_ROOT)/templates/*.sh)
	@if [ -e submit.sh ] || [ -d submit.sh.files ]; then \
		printf '♻️  Inputs changed; refreshing generated submission files.\n'; \
		rm -f -- submit.sh; rm -rf -- submit.sh.files; \
	fi
	python '$(REPO_ROOT)/slurmy.py' '$(JOBPAIRS)' '$(BUILDING)' '$(LIMITER)' --deg_par '$(DEG_PAR)'
	@printf '\n✅ Prepared the explicit jobpairs. Inspect submit.sh.files/.\n👉 make submit builds resources on the cluster, fetches them, and submits the calls.\n'

build: prepare
	bash ./submit.sh.files/builds/run.sh
	@printf '✅ Built and downloaded the declared resources.\n'

submit: prepare
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
