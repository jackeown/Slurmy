REPO_ROOT := ../..
DEG_PAR := 3
include $(REPO_ROOT)/templates/workflow.mk

jobpairs.csv: configurations.csv $(wildcard problems/easy/*.p problems/hard/*.p)
	python '$(REPO_ROOT)/slurmy-pairs.py' --configurations configurations.csv --problems 'problems/easy/*.p' 'problems/hard/*.p' --output $@

.PHONY: pairs-clean
pairs-clean: clean
	rm -f -- jobpairs.csv
	@printf '✅ Removed the generated all-pairs table.\n'
