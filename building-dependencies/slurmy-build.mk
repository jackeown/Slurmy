# Shared remote-build settings for the examples.
SLURMY_HOST ?= datalab
SLURMY_BUILD_PARTITION ?= CPU-amd
SLURMY_BUILD_TAG := $(subst :,_,$(subst /,_,$(SLURMY_HOST)))
BUILDING_DEPENDENCIES := $(abspath $(REPO_ROOT)/building-dependencies)
SLURMY_BUILD_DRIVER := $(BUILDING_DEPENDENCIES)/slurmy-build.py
RUNSOLVER_DIRECTORY := $(BUILDING_DEPENDENCIES)/runsolver
RUNSOLVER := $(RUNSOLVER_DIRECTORY)/runsolver
RUNSOLVER_STAMP := $(RUNSOLVER_DIRECTORY)/.built-on-$(SLURMY_BUILD_TAG)

$(RUNSOLVER_STAMP): $(RUNSOLVER_DIRECTORY)/build.sh $(SLURMY_BUILD_DRIVER)
	@printf '\033[1;34m🔨 Building runsolver in a Slurm job on %s...\033[0m\n' '$(SLURMY_HOST)'
	python '$(SLURMY_BUILD_DRIVER)' \
		--host '$(SLURMY_HOST)' \
		--name runsolver \
		--recipe '$(RUNSOLVER_DIRECTORY)/build.sh' \
		--output '$(RUNSOLVER_DIRECTORY)' \
		--artifact runsolver \
		--cpus-per-task 4 \
		--memory 4GiB \
		--time 00:15:00 \
		--sbatch-option=--partition=$(SLURMY_BUILD_PARTITION)
	@touch '$@'
	@printf '\033[1;32m✅ Built runsolver on %s and downloaded it.\033[0m\n' '$(SLURMY_HOST)'

$(RUNSOLVER): $(RUNSOLVER_STAMP)
	@test -x '$@' || { printf '❌ Remote build did not produce %s.\n' '$@' >&2; exit 1; }
