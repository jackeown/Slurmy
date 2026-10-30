REPO_ROOT := ../../..
BATCH_SIZE := 2
BUILD_RECIPES := ../example-e/build.sh ../../../implementation/build/runsolver/build.sh
include $(REPO_ROOT)/implementation/templates/workflow.mk

.PHONY: refresh-problems pairs-clean
jobpairs.csv: Makefile configurations.csv problem-globs.txt refresh-problems
	"$(SLURMY_PYTHON)" '$(REPO_ROOT)/implementation/slurmy-pairs.py' --configurations configurations.csv --problem-globs-file problem-globs.txt --order '$(PAIR_ORDER)' --output $@

pairs-clean: clean
	rm -f -- jobpairs.csv
