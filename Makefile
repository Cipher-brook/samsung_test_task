# The makefile builds kernel module and test application,
# helps to install and uninstall kernel module in kernel

KSRC := ksrc
TEST_APP := test_app
SUBDIRS = $(KSRC) $(TEST_APP)

.PHONY: all
all: subdirs

.PHONY: subdirs
subdirs:
	for n in $(SUBDIRS); do $(MAKE) -C $$n || exit 1; done

.PHONY: ksrc
ksrc:
	$(MAKE) -C $(KSRC) || exit 1

.PHONY: test_app
test_app:
	$(MAKE) -C $(TEST_APP) || exit 1

.PHONY: clean
clean:
	for n in $(SUBDIRS); do $(MAKE) -C $$n clean; done

.PHONY: clean_ksrc
clean_ksrc:
	$(MAKE) -C $(KSRC) clean

.PHONY: clean_test_app
clean_test_app:
	$(MAKE) -C $(TEST_APP) clean

.PHONY: install_kmod
install_kmod:
	$(MAKE) -C $(KSRC) install_kmod

.PHONY: uninstall_kmod
uninstall_kmod:
	$(MAKE) -C $(KSRC) uninstall_kmod
