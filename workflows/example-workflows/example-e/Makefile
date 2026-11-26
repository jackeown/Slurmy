REPO_ROOT := ../../..
BATCH_SIZE := 2
BUILD_RECIPES := build.sh ../../../implementation/build/runsolver/build.sh
include $(REPO_ROOT)/implementation/templates/workflow.mk

solver-problem-pairs.csv: Makefile solvers.csv $(REPO_ROOT)/implementation/slurmy-pairs.py $(wildcard ../../example-problems/basic/*.p)
	"$(SLURMY_PYTHON)" '$(REPO_ROOT)/implementation/slurmy-pairs.py' --configurations solvers.csv --problems '../../example-problems/basic/*.p' --order '$(PAIR_ORDER)' --output $@

.PHONY: pairs-clean
pairs-clean: clean
	rm -f -- solver-problem-pairs.csv
	@printf '✅ Removed the generated all-pairs table.\n'
