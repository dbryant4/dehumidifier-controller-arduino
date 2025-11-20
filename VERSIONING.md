# Version Management

This project uses [Semantic Versioning](https://semver.org/) (SemVer) for version numbering.

## Version Format

Versions follow the format: `MAJOR.MINOR.PATCH`

- **MAJOR**: Incremented for incompatible API changes or major feature additions
- **MINOR**: Incremented for backwards-compatible feature additions
- **PATCH**: Incremented for backwards-compatible bug fixes

## Version File

The current version is stored in the `VERSION` file at the project root. This file is the single source of truth for the version number.

## Version Management Script

The `version.sh` script provides commands to manage versions:

### Show Current Version
```bash
./version.sh show
# or
./version.sh current
```

### Bump Version
```bash
# Bump patch version (1.0.0 → 1.0.1)
./version.sh bump patch

# Bump minor version (1.0.0 → 1.1.0)
./version.sh bump minor

# Bump major version (1.0.0 → 2.0.0)
./version.sh bump major
```

### Set Version Explicitly
```bash
./version.sh set 2.1.0
```

## What Gets Updated

When you run the version script, it automatically updates:

1. **VERSION file** - The main version file
2. **config.h** - Updates `VERSION_MAJOR`, `VERSION_MINOR`, and `VERSION_PATCH` defines
3. **PRD (prd.mdc)** - Adds entry to version history section
4. **Git tag** - Optionally creates a git tag (prompts for confirmation)

## Version Usage in Code

The version is used in several places:

- **config.h**: Defines `VERSION_MAJOR`, `VERSION_MINOR`, `VERSION_PATCH`, and `VERSION_BUILD`
- **getVersionString()**: Returns formatted version string (e.g., "1.0.0")
- **Web Interface**: Displays version on main page and JSON API
- **Serial Output**: Shows version during OTA updates
- **Display**: Shows version during firmware updates

## Workflow

### For Bug Fixes (Patch)
```bash
./version.sh bump patch
git add VERSION dehumidifier-controller-arduino/config.h .cursor/rules/prd.mdc
git commit -m "Bump version to $(cat VERSION)"
git tag v$(cat VERSION)
git push && git push --tags
```

### For New Features (Minor)
```bash
./version.sh bump minor
git add VERSION dehumidifier-controller-arduino/config.h .cursor/rules/prd.mdc
git commit -m "Bump version to $(cat VERSION)"
git tag v$(cat VERSION)
git push && git push --tags
```

### For Breaking Changes (Major)
```bash
./version.sh bump major
git add VERSION dehumidifier-controller-arduino/config.h .cursor/rules/prd.mdc
git commit -m "Bump version to $(cat VERSION) - Breaking changes"
git tag v$(cat VERSION)
git push && git push --tags
```

## Build Integration

The build script (`build.sh`) automatically reads and displays the version when building:

```bash
./build.sh
# Output: Building dehumidifier-controller-arduino v1.0.0 for ESP32-S2...
```

## Version History

Version history is maintained in:
- **PRD**: `.cursor/rules/prd.mdc` (Section 8.2)
- **Git Tags**: Created automatically by version script

## Best Practices

1. **Always bump version before release**: Use the version script to ensure consistency
2. **Commit version changes**: Include VERSION, config.h, and PRD updates in the same commit
3. **Create git tags**: Use tags to mark releases (the script prompts for this)
4. **Update PRD**: The script automatically updates the PRD version history
5. **Document changes**: Add meaningful commit messages describing what changed

## Troubleshooting

### Version file not found
If you see "VERSION file not found", create it:
```bash
echo "1.0.0" > VERSION
```

### Config.h not updating
Make sure the version script has execute permissions:
```bash
chmod +x version.sh
```

### PRD not updating
The PRD update requires the version history section to exist. If it's missing, the script will warn you but continue.

