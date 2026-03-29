# -*-Makefile-*-

OBJDIRS += $(USER_OBJDIR)/flocktest

USER_FLOCKTEST_SRC += $(USER_DIR)/flocktest/flocktest.c
USER_FLOCKTEST_OBJ := $(patsubst %.c, $(OBJDIR)/%.o, $(USER_FLOCKTEST_SRC))
USER_FLOCKTEST_OBJ := $(patsubst %.S, $(OBJDIR)/%.o, $(USER_FLOCKTEST_OBJ))
KERN_BINFILES += $(USER_OBJDIR)/flocktest/flocktest

flocktest: $(USER_OBJDIR)/flocktest/flocktest

$(USER_OBJDIR)/flocktest/flocktest: $(USER_LIB_OBJ) $(USER_FLOCKTEST_OBJ)
	@echo + ld[USER/flocktest] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(USER_FLOCKTEST_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(USER_OBJDIR)/flocktest/%.o: $(USER_DIR)/flocktest/%.c
	@echo + cc[USER/flocktest] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_OBJDIR)/flocktest/%.o: $(USER_DIR)/flocktest/%.S
	@echo + as[USER/flocktest] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
