#
# obj.mk
# Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>
#  SPDX-License-Identifier:	BSD-2-Clause
#

.PHONY: objdir_clean objs_clean

OBJDIRSTAMP=$(OBJDIR)/.stamp

vpath %.S $(dir $(addprefix $(SRCDIR),$(SRCS))) $(SRCDIR)
vpath %.c $(dir $(addprefix $(SRCDIR),$(SRCS))) $(SRCDIR)
vpath %.cc $(dir $(addprefix $(SRCDIR),$(SRCS))) $(SRCDIR)

CFLAGS+=-MMD

OBJS+= $(addprefix $(OBJDIR)/,$(addsuffix .o, $(basename $(notdir $(SRCS)))))
DEPS+= $(addprefix $(OBJDIR)/,$(addsuffix .d, $(basename $(notdir $(SRCS)))))

CUSTOBJS= $(addprefix $(OBJDIR)/,$(addsuffix .o, $(basename $(notdir $(OBJSRCS)))))
DEPS+= $(addprefix $(OBJDIR)/,$(addsuffix .d, $(basename $(notdir $(OBJSRCS)))))

-include $(DEPS)

$(OBJDIR)/%.S.o: %.S $(OBJDIRSTAMP)
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) -o $@ $<

$(OBJDIR)/%.o: %.S $(OBJDIRSTAMP)
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) -o $@ $<

$(OBJDIR)/%.c.o: %.c $(OBJDIRSTAMP)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: %.c $(OBJDIRSTAMP)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.cc.o: %.cc $(OBJDIRSTAMP)
	$(CXX) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: %.cc $(OBJDIRSTAMP)
	$(CXX) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIRSTAMP):
	-mkdir -p $(OBJDIR)
	touch $(OBJDIRSTAMP)

objs_clean:
	-rm $(OBJS) $(DEPS) $(CUSTOBJS)

objdir_clean:
	rm $(OBJDIRSTAMP)

ALL_TARGET+= $(OBJS) $(CUSTOBJS)
CLEAN_TARGET+=objs_clean objdir_clean
