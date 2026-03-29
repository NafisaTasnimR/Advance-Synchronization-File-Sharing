# -*-Makefile-*-

OBJDIRS += $(USER_OBJDIR)/flockchild

USER_FLOCKCHILD_SRC += $(USER_DIR)/flockchild/flockchild.c
USER_FLOCKCHILD_OBJ := $(patsubst %.c, $(OBJDIR)/%.o, $(USER_FLOCKCHILD_SRC))
USER_FLOCKCHILD_OBJ := $(patsubst %.S, $(OBJDIR)/%.o, $(USER_FLOCKCHILD_OBJ))

KERN_BINFILES += $(USER_OBJDIR)/flockchild/flockchild

flockchild: $(USER_OBJDIR)/flockchild/flockchild

$(USER_OBJDIR)/flockchild/flockchild: $(USER_LIB_OBJ) $(USER_FLOCKCHILD_OBJ)
	@echo + ld[USER/flockchild] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(USER_FLOCKCHILD_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(USER_OBJDIR)/flockchild/%.o: $(USER_DIR)/flockchild/%.c
	@echo + cc[USER/flockchild] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_OBJDIR)/flockchild/%.o: $(USER_DIR)/flockchild/%.S
	@echo + as[USER/flockchild] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
