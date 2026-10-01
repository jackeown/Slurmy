REPO_ROOT := ../../..
BATCH_SIZE := 2
BUILD_RECIPES := build.sh ../../../build/runsolver/build.sh
include $(REPO_ROOT)/implementation/templates/workflow.mk

jobpairs.csv: Makefile configurations.csv $(REPO_ROOT)/implementation/slurmy-pairs.py $(wildcard ../../example-problems/basic/*.p)
	python '$(REPO_ROOT)/implementation/slurmy-pairs.py' --configurations configurations.csv --problems '../../example-problems/basic/*.p' --order '$(PAIR_ORDER)' --output $@

.PHONY: pairs-clean
pairs-clean: clean
	rm -f -- jobpairs.csv
	@printf '✅ Removed the generated all-pairs table.\n'
