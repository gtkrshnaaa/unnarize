# Makefile for generating documentation listing
# Run: make list - Generate listing.txt with all markdown files
# Run: make clean - Remove listing.txt

.PHONY: list clean

# Find all .md files and generate listing.txt
list:
	@echo "Generating listing.txt..."
	@rm -f listing.txt
	@find . -name "*.md" -type f | sort | while read file; do \
		echo "=== $$file ===" >> listing.txt; \
		echo "" >> listing.txt; \
		cat "$$file" >> listing.txt; \
		echo "" >> listing.txt; \
		echo "" >> listing.txt; \
	done
	@echo "Done! Created listing.txt with $$(find . -name '*.md' -type f | wc -l) files"

clean:
	rm -f listing.txt
