
#
# obj.mk
# Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>
#  SPDX-License-Identifier:	GPL2.0+
#

.PHONY: objdir_clean objs_clean

vpath %.S $(dir $(addprefix $(SRCDIR),$(SRCS))) $(SRCDIR)
vpath %.c $(dir $(addprefix $(SRCDIR),$(SRCS))) $(SRCDIR)

CFLAGS+=-MMD

OBJS= $(addprefix $(OBJDIR)/,$(addsuffix .o, $(basename $(notdir $(SRCS)))))
DEPS= $(addprefix $(OBJDIR)/,$(addsuffix .d, $(basename $(notdir $(SRCS)))))

CUSTOBJS= $(addprefix $(OBJDIR)/,$(addsuffix .o, $(basename $(notdir $(OBJSRCS)))))
DEPS+= $(addprefix $(OBJDIR)/,$(addsuffix .d, $(basename $(notdir $(OBJSRCS)))))

-include $(DEPS)

$(OBJDIR)/%.S.o: %.S
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) -o $@ $<

$(OBJDIR)/%.o: %.S
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) -o $@ $<

$(OBJDIR)/%.c.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

objs_clean:
	-rm $(OBJS) $(DEPS) $(CUSTOBJS)

objdir_clean:
	-rmdir $(OBJDIR)

ALL_TARGET+=$(OBJDIR) $(OBJS) $(CUSTOBJS)
CLEAN_TARGET+=objs_clean objdir_clean
