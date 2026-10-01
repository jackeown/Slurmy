REPO_ROOT := ../../..
BATCH_SIZE := 3
BUILD_RECIPES := ../example-vampire/build.sh ../example-e/build.sh ../example-drodi/build.sh ../../../build/runsolver/build.sh
include $(REPO_ROOT)/implementation/templates/workflow.mk

jobpairs.csv: Makefile configurations.csv $(REPO_ROOT)/implementation/slurmy-pairs.py $(wildcard ../../example-problems/combined/easy/*.p ../../example-problems/combined/hard/*.p)
	python '$(REPO_ROOT)/implementation/slurmy-pairs.py' --configurations configurations.csv --problems '../../example-problems/combined/easy/*.p' '../../example-problems/combined/hard/*.p' --order '$(PAIR_ORDER)' --output $@

.PHONY: pairs-clean
pairs-clean: clean
	rm -f -- jobpairs.csv
	@printf '✅ Removed the generated all-pairs table.\n'
