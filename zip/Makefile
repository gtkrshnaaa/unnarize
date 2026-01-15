# Makefile for achieving docs and examples
# Run: make - Generate zip files in this directory

.PHONY: zip docs examples clean

zip: docs examples

docs:
	@echo "Zipping docs/mddocs..."
	# Generate listing.txt first
	$(MAKE) -C ../docs/mddocs list
	# Create zip file, excluding hidden files/git
	cd .. && zip -r zip/mddocs.zip docs/mddocs -x "*/.*" -x "*/Makefile"
	@echo "Created zip/mddocs.zip"

examples:
	@echo "Zipping examples..."
	# Generate listing.txt first
	$(MAKE) -C ../examples list
	# Create zip file, excluding hidden files/git
	cd .. && zip -r zip/examples.zip examples -x "*/.*" -x "*/Makefile"
	@echo "Created zip/examples.zip"

clean:
	rm -f *.zip
