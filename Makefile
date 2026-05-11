# rv1106-system Makefile
#
# Targets:
#   build        - Build rv1106-system image
#   flash        - Prompt for target, build one system image, and flash it
#   test         - Run E2E tests (depends on flash)
#   dev_release  - Dev release (prerelease) with optional per-SKU testing
#   release      - Production release with optional per-SKU testing
#   release_dry_run - Build, optionally test, and sign without uploading
#   bump-version - Bump version for next release cycle

VERSION := $(shell cat VERSION 2>/dev/null || echo "0.0.0")
VERSION_DEV := $(VERSION)-dev$(shell date -u +%Y%m%d%H%M)

DEVICE_IP ?=
R2_PATH := r2://jetkvm-update/system
SIGNING_KEY_FPR ?=
OTA_ROOT_KEY_FPR := AF5A36A993D828FEFE7C18C2D1B9856C26A79E95

.PHONY: build flash test dev_release release release_dry_run bump-version git_check_dev clean check_device check_remote check_signing_key

# Keep release validation, build/test prompts, and signing ordered even under `make -j`.
.NOTPARALLEL: dev_release release release_dry_run

# -----------------------------------------------------------------------------
# Git checks
# -----------------------------------------------------------------------------
git_check_dev:
	@if [ -n "$$(git status --porcelain)" ]; then \
		echo "Error: Working tree is dirty. Commit or stash changes."; exit 1; \
	fi
	@command -v gh >/dev/null 2>&1 || { echo "Error: gh CLI not installed"; exit 1; }
	@gh auth status >/dev/null 2>&1 || { echo "Error: gh CLI not authenticated. Run 'gh auth login'"; exit 1; }
	@command -v rclone >/dev/null 2>&1 || { echo "Error: rclone not installed"; exit 1; }
	@current_branch="$$(git rev-parse --abbrev-ref HEAD)"; \
	current_commit="$$(git rev-parse HEAD)"; \
	current_short="$$(git rev-parse --short HEAD)"; \
	current_subject="$$(git log -1 --pretty=%s)"; \
	current_version="$$(cat VERSION 2>/dev/null || echo unknown)"; \
	git fetch origin dev; \
	origin_commit="$$(git rev-parse origin/dev)"; \
	origin_short="$$(git rev-parse --short origin/dev)"; \
	if [ "$$current_branch" = "dev" ] && [ "$$current_commit" = "$$origin_commit" ]; then \
		exit 0; \
	fi; \
	echo ""; \
	echo "WARNING: Releasing from the current checkout instead of latest origin/dev"; \
	echo "  Ref:     $$current_branch"; \
	echo "  Commit:  $$current_short"; \
	echo "  Subject: $$current_subject"; \
	echo "  Version: $$current_version"; \
	echo "  origin/dev: $$origin_short"; \
	echo ""; \
	read -p "Continue with this checkout? [y/N] " confirm; \
	[ "$$confirm" = "y" ] || exit 1

check_signing_key:
	@if [ -z "$(SIGNING_KEY_FPR)" ]; then \
		echo "Error: SIGNING_KEY_FPR is required for production releases"; \
		echo "Usage: make release SIGNING_KEY_FPR=<fingerprint> [DEVICE_IP=<ip>] [JETKVM_REMOTE_HOST=<host>]"; \
		exit 1; \
	fi
	@gpg --list-secret-keys --with-colons $(SIGNING_KEY_FPR) >/dev/null 2>&1 || { \
		echo "Error: Signing key $(SIGNING_KEY_FPR) not found in local GPG keyring"; \
		exit 1; \
	}
	@root_fpr="$$(gpg --list-secret-keys --with-colons $(SIGNING_KEY_FPR) | awk -F: '/^fpr:/ { print $$10; exit }')"; \
	if [ -z "$$root_fpr" ]; then \
		echo "Error: Could not determine root fingerprint for signing key $(SIGNING_KEY_FPR)"; \
		exit 1; \
	fi; \
	if [ "$$root_fpr" != "$(OTA_ROOT_KEY_FPR)" ]; then \
		echo "Error: Signing key $(SIGNING_KEY_FPR) belongs to root $$root_fpr, expected $(OTA_ROOT_KEY_FPR)"; \
		exit 1; \
	fi

# -----------------------------------------------------------------------------
# Build / Flash / Test (dependency chain)
# -----------------------------------------------------------------------------
build:
	./scripts/build_system.sh

check_device:
	@DEVICE_IP="$(DEVICE_IP)" ./scripts/check_device.sh

check_remote:
	@echo "Checking remote host connectivity ($(JETKVM_REMOTE_HOST))..."
	@ping -c 1 -W 5 $(shell echo $(JETKVM_REMOTE_HOST) | sed 's/.*@//') > /dev/null 2>&1 || { echo "Error: Cannot reach remote host at $(JETKVM_REMOTE_HOST)"; exit 1; }
	@ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o ConnectTimeout=10 $(JETKVM_REMOTE_HOST) "echo ok" > /dev/null 2>&1 || { echo "Error: SSH failed to $(JETKVM_REMOTE_HOST)"; exit 1; }
	@echo "OK: Remote host reachable"

flash:
ifndef SKIP_BUILD
	PROMPT_VARIANT_TESTS=0 ./scripts/flash_system.sh $(if $(DEVICE_IP),-r $(DEVICE_IP)) --build $(if $(FLASH_SKU),--sku $(FLASH_SKU))
else
	./scripts/flash_system.sh $(if $(DEVICE_IP),-r $(DEVICE_IP)) $(if $(FLASH_SKU),--sku $(FLASH_SKU))
endif

test:
	$(MAKE) check_device
	$(MAKE) check_remote
ifndef SKIP_BUILD
	$(MAKE) flash
else
	$(MAKE) flash SKIP_BUILD=1
endif
	./scripts/run_e2e_tests.sh $(if $(DEVICE_IP),-r $(DEVICE_IP)) --remote-host $(JETKVM_REMOTE_HOST) $(if $(KVM_DIR),--kvm-dir $(KVM_DIR))

# -----------------------------------------------------------------------------
# Dev Release - Prerelease for testing
# -----------------------------------------------------------------------------
dev_release: export BUILD_VERSION := $(VERSION_DEV)
dev_release: git_check_dev build
	@if rclone lsf $(R2_PATH)/$(VERSION_DEV)/ 2>/dev/null | grep -q .; then \
		echo "Error: Version $(VERSION_DEV) already exists in R2"; exit 1; \
	fi
	@if gh release view "release/v$(VERSION_DEV)" --repo jetkvm/rv1106-system >/dev/null 2>&1; then \
		echo "Error: GitHub release release/v$(VERSION_DEV) already exists"; exit 1; \
	fi
	@echo "═══════════════════════════════════════════════════════"
	@echo "  DEV Release (Pre-release)"
	@echo "═══════════════════════════════════════════════════════"
	@echo "  Version: $(VERSION_DEV)"
	@echo "  Tag:     release/v$(VERSION_DEV)"
	@echo "  Branch:  $$(git rev-parse --abbrev-ref HEAD)"
	@echo "  Commit:  $$(git rev-parse --short HEAD)"
	@echo "  Subject: $$(git log -1 --pretty=%s)"
	@echo "  Time:    $$(date -u +%FT%T%z)"
	@echo "═══════════════════════════════════════════════════════"
	@echo ""
	@read -p "Proceed? [y/N] " confirm && [ "$$confirm" = "y" ] || exit 1
	./scripts/release_r2.sh --version $(VERSION_DEV) --unsigned
	./scripts/release_github.sh --version $(VERSION_DEV) --prerelease
	@echo ""
	@echo "OK: Dev release complete: release/v$(VERSION_DEV)"

# -----------------------------------------------------------------------------
# Production Release
# -----------------------------------------------------------------------------
release: export BUILD_VERSION := $(VERSION)
release: check_signing_key git_check_dev build
	@if rclone lsf $(R2_PATH)/$(VERSION)/ 2>/dev/null | grep -q .; then \
		echo "Error: Version $(VERSION) already exists in R2"; exit 1; \
	fi
	@if gh release view "release/v$(VERSION)" --repo jetkvm/rv1106-system >/dev/null 2>&1; then \
		echo "Error: GitHub release release/v$(VERSION) already exists"; exit 1; \
	fi
	@latest_dev=$$(gh release list --repo jetkvm/rv1106-system --limit 10 --json tagName --jq '.[].tagName' | grep "^release/v$(VERSION)-dev" | head -1); \
		if [ -z "$$latest_dev" ]; then \
			echo ""; \
			echo "WARNING: No dev release found for $(VERSION)"; \
			echo ""; \
			read -p "Release production without prior dev release? [y/N] " confirm && [ "$$confirm" = "y" ] || exit 1; \
		else \
			echo "OK: Found prior dev release: $$latest_dev"; \
		fi
	@echo ""
	@echo "═══════════════════════════════════════════════════════"
	@echo "  PRODUCTION Release"
	@echo "═══════════════════════════════════════════════════════"
	@echo "  Version: $(VERSION)"
	@echo "  Tag:     release/v$(VERSION)"
	@echo "  Branch:  $$(git rev-parse --abbrev-ref HEAD)"
	@echo "  Commit:  $$(git rev-parse --short HEAD)"
	@echo "  Subject: $$(git log -1 --pretty=%s)"
	@echo "  Time:    $$(date -u +%FT%T%z)"
	@echo "  Signing: $(SIGNING_KEY_FPR)"
	@echo "═══════════════════════════════════════════════════════"
	@echo ""
	@read -p "Proceed with PRODUCTION release? [y/N] " confirm && [ "$$confirm" = "y" ] || exit 1
	./scripts/release_r2.sh --version $(VERSION) --signing-key $(SIGNING_KEY_FPR)
	./scripts/release_github.sh --version $(VERSION)
	@echo ""
	@echo "OK: Production release complete: release/v$(VERSION)"
	@echo ""
	@echo "Next: Run 'make bump-version' to prepare for next release cycle"

# -----------------------------------------------------------------------------
# Production Release Dry Run
# -----------------------------------------------------------------------------
release_dry_run: export BUILD_VERSION := $(VERSION)
release_dry_run: check_signing_key build
	@echo ""
	@echo "═══════════════════════════════════════════════════════"
	@echo "  PRODUCTION Release Dry Run"
	@echo "═══════════════════════════════════════════════════════"
	@echo "  Version: $(VERSION)"
	@echo "  Tag:     release/v$(VERSION)"
	@echo "  Branch:  $$(git rev-parse --abbrev-ref HEAD)"
	@echo "  Commit:  $$(git rev-parse --short HEAD)"
	@echo "  Subject: $$(git log -1 --pretty=%s)"
	@echo "  Signing: $(SIGNING_KEY_FPR)"
	@echo "═══════════════════════════════════════════════════════"
	@echo ""
	./scripts/release_r2.sh --dry-run --version $(VERSION) --signing-key $(SIGNING_KEY_FPR)
	./scripts/release_github.sh --dry-run --version $(VERSION)
	@echo ""
	@echo "OK: Production release dry run complete: release/v$(VERSION)"

# -----------------------------------------------------------------------------
# Bump Version
# -----------------------------------------------------------------------------
bump-version:
	@next_default=$$(echo $(VERSION) | awk -F. '{print $$1"."$$2"."$$3+1}'); \
		echo "Current version: $(VERSION)"; \
		read -p "Next version [$$next_default]: " next_ver; \
		next_ver=$${next_ver:-$$next_default}; \
		if ! echo "$$next_ver" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$$'; then \
			echo "Error: Invalid version '$$next_ver'. Must be semver format (e.g., 1.2.3)"; \
			exit 1; \
		fi; \
		echo "$$next_ver" > VERSION && \
		git add VERSION && \
		git commit -m "Bump version to $$next_ver" && \
		git push && \
		echo "OK: Version bumped to $$next_ver"

# -----------------------------------------------------------------------------
# Clean build artifacts
# -----------------------------------------------------------------------------
clean:
	@echo "Cleaning build artifacts..."
	sudo rm -rf output/
	./build.sh clean
	rm -rf release-artifacts/
	rm -f buildkit.tar.zst
	@echo "OK: Clean complete"
