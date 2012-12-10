
# Extra all
EXTRA_DIST=wfxp.html favicon.ico img/*.png img/*.gif css/*.css js/*.js

install-data-hook:
	for dir in wfxp wfxp/css wfxp/img wfxp/js; do \
	$(mkinstalldirs) "$(datarootdir)/$$dir" ; \
	done
	for file in $(EXTRA_DIST); do \
	cp -f "$(srcdir)/$$file" "$(datarootdir)/wfxp/$$file"; \
	done

