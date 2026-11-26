REPO_ROOT := ../../..
BATCH_SIZE := 3
BUILD_RECIPES := ../example-vampire/build.sh ../example-e/build.sh ../example-drodi/build.sh ../../../implementation/build/runsolver/build.sh
include $(REPO_ROOT)/implementation/templates/workflow.mk

solver-problem-pairs.csv: Makefile solvers.csv $(REPO_ROOT)/implementation/slurmy-pairs.py $(wildcard ../../example-problems/combined/easy/*.p ../../example-problems/combined/hard/*.p)
	"$(SLURMY_PYTHON)" '$(REPO_ROOT)/implementation/slurmy-pairs.py' --configurations solvers.csv --problems '../../example-problems/combined/easy/*.p' '../../example-problems/combined/hard/*.p' --order '$(PAIR_ORDER)' --output $@

.PHONY: pairs-clean
pairs-clean: clean
	rm -f -- solver-problem-pairs.csv
	@printf '✅ Removed the generated all-pairs table.\n'
