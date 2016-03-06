bin_PROGRAMS      = mcjoin
mcjoin_SOURCES    = mcjoin.c
mcjoin_CFLAGS     = -W -Wall -Wextra -std=gnu99
doc_DATA          = README.md LICENSE
EXTRA_DIST        = README.md LICENSE ChangeLog.md

package:
	gbp buildpackage --git-ignore-new --git-ignore-branch			 \
			 --git-no-create-orig --git-upstream-branch=master

## Target to run when building a release
release: dist package
	@for file in $(DIST_ARCHIVES); do	\
		md5sum $$file > ../$$file.md5;	\
	done
	@mv $(DIST_ARCHIVES) ../
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do					\
		printf "$$file    \tDistribution tarball\n";			\
		printf "$$file.md5\t"; cat ../$$file.md5 | cut -f1 -d' ';	\
	done
	@for file in `ls ../$(PACKAGE)_$(VERSION)-*`; do			\
		printf "$$file    \tDebian/Ubuntu file\n";			\
	done
