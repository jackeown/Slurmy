REPO_ROOT := ../..
DEG_PAR := 3
BUILD_RECIPES := ../example-vampire/build.sh ../example-e/build.sh ../example-drodi/build.sh ../../building-dependencies/runsolver/build.sh
include $(REPO_ROOT)/templates/workflow.mk

jobpairs.csv: Makefile configurations.csv $(REPO_ROOT)/slurmy-pairs.py $(wildcard problems/easy/*.p problems/hard/*.p)
	python '$(REPO_ROOT)/slurmy-pairs.py' --configurations configurations.csv --problems 'problems/easy/*.p' 'problems/hard/*.p' --output $@

.PHONY: pairs-clean
pairs-clean: clean
	rm -f -- jobpairs.csv
	@printf '✅ Removed the generated all-pairs table.\n'
