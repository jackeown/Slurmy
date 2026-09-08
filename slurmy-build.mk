# Shared remote-build settings for the examples.
SLURMY_HOST ?= datalab
SLURMY_BUILD_PARTITION ?= CPU-amd
SLURMY_BUILD_TAG := $(subst :,_,$(subst /,_,$(SLURMY_HOST)))
SLURMY_BUILD_DRIVER := $(abspath $(REPO_ROOT)/slurmy-build.py)
RUNSOLVER := $(abspath $(REPO_ROOT)/runsolver-build/runsolver)
RUNSOLVER_STAMP := $(abspath $(REPO_ROOT)/runsolver-build/.built-on-$(SLURMY_BUILD_TAG))

$(RUNSOLVER_STAMP): $(REPO_ROOT)/runsolver-build/build.sh $(SLURMY_BUILD_DRIVER)
	@printf '\033[1;34m🔨 Building runsolver in a Slurm job on %s...\033[0m\n' '$(SLURMY_HOST)'
	python '$(SLURMY_BUILD_DRIVER)' \
		--host '$(SLURMY_HOST)' \
		--name runsolver \
		--recipe '$(abspath $(REPO_ROOT)/runsolver-build/build.sh)' \
		--output '$(abspath $(REPO_ROOT)/runsolver-build)' \
		--artifact runsolver \
		--cpus-per-task 4 \
		--memory 4GiB \
		--time 00:15:00 \
		--sbatch-option=--partition=$(SLURMY_BUILD_PARTITION)
	@touch '$@'
	@printf '\033[1;32m✅ Built runsolver on %s and downloaded it.\033[0m\n' '$(SLURMY_HOST)'

$(RUNSOLVER): $(RUNSOLVER_STAMP)
	@test -x '$@' || { printf '❌ Remote build did not produce %s.\n' '$@' >&2; exit 1; }
