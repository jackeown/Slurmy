SHELL := /usr/bin/env bash
.DEFAULT_GOAL := run

REPO_ROOT := ../..
SLURMY_HOST ?= datalab
# Leave empty to sync the newest job, or pass SLURMY_ID=name_timestamp_pid.
SLURMY_ID ?=

.PHONY: run sync

run:
	@printf '\033[1;34m🧭 Opening the interactive Slurmy workflow generator...\033[0m\n'
	@python ./example-generator.py

sync:
	@printf '\033[1;34m🔄 Syncing the selected (or latest) Slurmy job from %s...\033[0m\n' '$(SLURMY_HOST)'
	@cd '$(REPO_ROOT)' && python ./slurmy-sync.py $(SLURMY_ID) --host '$(SLURMY_HOST)' --follow --interval 2
	@printf '\n\033[1;32m✅ Results are synchronized under %s.\033[0m\n' '$(abspath $(REPO_ROOT)/slurmy-results)'
