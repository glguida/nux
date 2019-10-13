.PHONY: all includes incdir incs
ALL_TARGET+= includes

ifneq ($(INCDIR)z,z)
ALL_TARGET+= incdir
incdir:
	install -d ${INSTALLINCDIR}/${INCDIR}
else
incdir:
endif

ifneq ($(INCS)z,z)
ALL_TARGET+= incs
incs:
	install -m 0644 ${INCS} ${INSTALLINCDIR}/${INCDIR}/ 
else
incs:
endif

ifneq ($(INCSUBDIRS)z,z)
includes:
	for dir in $(INCSUBDIRS); do $(MAKE) -C $$dir includes incdir incs; done
else
includes:
endif
