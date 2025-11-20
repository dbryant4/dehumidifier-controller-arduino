# Release Builds

This directory contains versioned firmware binaries for GitHub releases.

## Building a Release

To build a release binary:

```bash
# Build using version from VERSION file
./build.sh

# Or specify version explicitly
./build.sh 1.0.0
```

The build script will:
1. Read the version from `VERSION` file (or use the provided version)
2. Compile the firmware
3. Create two binary files:
   - `dehumidifier-controller-arduino.ino.bin` - For OTA uploads (in project root)
   - `releases/dehumidifier-controller-arduino-v1.0.0.bin` - For GitHub releases

## GitHub Release Workflow

1. **Update Version**:
   ```bash
   ./version.sh bump [major|minor|patch]
   ```

2. **Build Release Binary**:
   ```bash
   ./build.sh
   ```

3. **Create Git Tag** (optional, script will prompt):
   ```bash
   ./version.sh bump patch
   # Script will prompt to create tag
   ```

4. **Create GitHub Release**:
   - Go to GitHub repository → Releases → Draft a new release
   - Tag: `v1.0.0` (matches version)
   - Title: `Version 1.0.0`
   - Upload: `releases/dehumidifier-controller-arduino-v1.0.0.bin`
   - Add release notes from PRD version history

## File Naming Convention

Release binaries follow the pattern:
```
dehumidifier-controller-arduino-vMAJOR.MINOR.PATCH.bin
```

Example:
- `dehumidifier-controller-arduino-v1.0.0.bin`
- `dehumidifier-controller-arduino-v1.1.0.bin`
- `dehumidifier-controller-arduino-v2.0.0.bin`

## Notes

- Release binaries are stored in the `releases/` directory
- The `releases/` directory is gitignored except for `.gitkeep`
- Versioned binaries are intended for GitHub releases only
- Use the unversioned `.bin` file in the project root for OTA updates

