# rv1106-system Makefile
#
# Targets:
#   build        - Build rv1106-system image
#   flash        - Flash system image to device (depends on build)
#   test         - Run E2E tests (depends on flash)
#   dev_release  - Dev release (prerelease) with testing
#   release      - Production release with testing
#   bump-version - Bump version for next release cycle

VERSION := $(shell cat VERSION 2>/dev/null || echo "0.0.0")
VERSION_DEV := $(VERSION)-dev$(shell date -u +%Y%m%d%H%M)

DEVICE_IP ?= 192.168.1.77
R2_PATH := r2://jetkvm-update/system

.PHONY: build flash test dev_release release bump-version git_check_dev clean

# -----------------------------------------------------------------------------
# Git checks
# -----------------------------------------------------------------------------
git_check_dev:
	@if [ "$$(git rev-parse --abbrev-ref HEAD)" != "dev" ]; then \
		echo "Error: Must be on 'dev' branch"; exit 1; \
	fi
	@if [ -n "$$(git status --porcelain)" ]; then \
		echo "Error: Working tree is dirty. Commit or stash changes."; exit 1; \
	fi
	@git fetch origin dev
	@if [ "$$(git rev-parse HEAD)" != "$$(git rev-parse origin/dev)" ]; then \
		echo "Error: Local dev is not up-to-date with origin/dev"; exit 1; \
	fi
	@command -v gh >/dev/null 2>&1 || { echo "Error: gh CLI not installed"; exit 1; }
	@gh auth status >/dev/null 2>&1 || { echo "Error: gh CLI not authenticated. Run 'gh auth login'"; exit 1; }
	@command -v rclone >/dev/null 2>&1 || { echo "Error: rclone not installed"; exit 1; }

# -----------------------------------------------------------------------------
# Build / Flash / Test (dependency chain)
# -----------------------------------------------------------------------------
build: clean
	./scripts/build_system.sh

flash: build
	./scripts/flash_system.sh -r $(DEVICE_IP)

test: flash
	./scripts/run_e2e_tests.sh -r $(DEVICE_IP)

# -----------------------------------------------------------------------------
# Dev Release - Prerelease for testing
# -----------------------------------------------------------------------------
dev_release: export BUILD_VERSION := $(VERSION_DEV)
dev_release: git_check_dev test
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
	@echo "  Time:    $$(date -u +%FT%T%z)"
	@echo "═══════════════════════════════════════════════════════"
	@echo ""
	@read -p "Proceed? [y/N] " confirm && [ "$$confirm" = "y" ] || exit 1
	./scripts/release_r2.sh --version $(VERSION_DEV)
	./scripts/release_github.sh --version $(VERSION_DEV) --prerelease
	@echo ""
	@echo "OK: Dev release complete: release/v$(VERSION_DEV)"

# -----------------------------------------------------------------------------
# Production Release
# -----------------------------------------------------------------------------
release: export BUILD_VERSION := $(VERSION)
release: git_check_dev test
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
	@echo "  Time:    $$(date -u +%FT%T%z)"
	@echo "═══════════════════════════════════════════════════════"
	@echo ""
	@read -p "Proceed with PRODUCTION release? [y/N] " confirm && [ "$$confirm" = "y" ] || exit 1
	./scripts/release_r2.sh --version $(VERSION)
	./scripts/release_github.sh --version $(VERSION)
	@echo ""
	@echo "OK: Production release complete: release/v$(VERSION)"
	@echo ""
	@echo "Next: Run 'make bump-version' to prepare for next release cycle"

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
	rm -f buildkit.tar.zst
	@echo "OK: Clean complete"
