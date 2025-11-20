#!/bin/bash

# Version Management Script for Dehumidifier Controller
# Supports semantic versioning (MAJOR.MINOR.PATCH)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

VERSION_FILE="VERSION"
CONFIG_FILE="dehumidifier-controller-arduino/config.h"
PRD_FILE=".cursor/rules/prd.mdc"

# Function to read current version
get_version() {
    if [ -f "$VERSION_FILE" ]; then
        cat "$VERSION_FILE" | tr -d ' \n'
    else
        echo "0.0.0"
    fi
}

# Function to parse version components
parse_version() {
    local version=$1
    VERSION_MAJOR=$(echo $version | cut -d. -f1)
    VERSION_MINOR=$(echo $version | cut -d. -f2)
    VERSION_PATCH=$(echo $version | cut -d. -f3)
}

# Function to update config.h with version
update_config_h() {
    local major=$1
    local minor=$2
    local patch=$3
    
    echo -e "${BLUE}Updating config.h...${NC}"
    
    # Use sed to update version defines
    sed -i.bak "s/^#define VERSION_MAJOR.*/#define VERSION_MAJOR $major/" "$CONFIG_FILE"
    sed -i.bak "s/^#define VERSION_MINOR.*/#define VERSION_MINOR $minor/" "$CONFIG_FILE"
    sed -i.bak "s/^#define VERSION_PATCH.*/#define VERSION_PATCH $patch/" "$CONFIG_FILE"
    
    # Remove backup file
    rm -f "${CONFIG_FILE}.bak"
    
    echo -e "${GREEN}✓ Updated config.h${NC}"
}

# Function to update PRD version history
update_prd_version() {
    local version=$1
    local change_type=$2
    
    if [ ! -f "$PRD_FILE" ]; then
        echo -e "${YELLOW}Warning: PRD file not found, skipping PRD update${NC}"
        return
    fi
    
    echo -e "${BLUE}Updating PRD version history...${NC}"
    
    # Get current date
    local date=$(date +"%Y-%m-%d")
    
    # Check if version history section exists
    if grep -q "## 8.2 Version History" "$PRD_FILE"; then
        # Insert new version entry after version history header
        local entry="#### Version $version ($date)\n- $change_type"
        
        # Use sed to insert after the version history header
        # Find the line number of "## 8.2 Version History"
        local line_num=$(grep -n "## 8.2 Version History" "$PRD_FILE" | cut -d: -f1)
        
        # Insert after the header line and before existing entries
        if [ -n "$line_num" ]; then
            # Insert new version entry
            sed -i.bak "${line_num}a\\
\\
#### Version $version ($date)\\
- $change_type
" "$PRD_FILE"
            rm -f "${PRD_FILE}.bak"
            echo -e "${GREEN}✓ Updated PRD version history${NC}"
        fi
    else
        echo -e "${YELLOW}Warning: Version history section not found in PRD${NC}"
    fi
}

# Function to bump version
bump_version() {
    local bump_type=$1
    local current_version=$(get_version)
    
    parse_version "$current_version"
    
    case $bump_type in
        major)
            VERSION_MAJOR=$((VERSION_MAJOR + 1))
            VERSION_MINOR=0
            VERSION_PATCH=0
            ;;
        minor)
            VERSION_MINOR=$((VERSION_MINOR + 1))
            VERSION_PATCH=0
            ;;
        patch)
            VERSION_PATCH=$((VERSION_PATCH + 1))
            ;;
        *)
            echo -e "${RED}Error: Invalid bump type. Use: major, minor, or patch${NC}"
            exit 1
            ;;
    esac
    
    local new_version="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
    
    # Update VERSION file
    echo "$new_version" > "$VERSION_FILE"
    echo -e "${GREEN}✓ Updated VERSION file: $current_version → $new_version${NC}"
    
    # Update config.h
    update_config_h "$VERSION_MAJOR" "$VERSION_MINOR" "$VERSION_PATCH"
    
    # Update PRD
    update_prd_version "$new_version" "$bump_type bump"
    
    echo -e "${GREEN}Version bumped successfully to: $new_version${NC}"
    
    return 0
}

# Function to set version explicitly
set_version() {
    local version=$1
    
    # Validate version format
    if ! [[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo -e "${RED}Error: Invalid version format. Use semantic versioning: MAJOR.MINOR.PATCH${NC}"
        exit 1
    fi
    
    parse_version "$version"
    
    # Update VERSION file
    echo "$version" > "$VERSION_FILE"
    echo -e "${GREEN}✓ Updated VERSION file to: $version${NC}"
    
    # Update config.h
    update_config_h "$VERSION_MAJOR" "$VERSION_MINOR" "$VERSION_PATCH"
    
    echo -e "${GREEN}Version set to: $version${NC}"
}

# Function to show current version
show_version() {
    local version=$(get_version)
    parse_version "$version"
    
    echo -e "${BLUE}Current Version:${NC}"
    echo -e "  Full: ${GREEN}$version${NC}"
    echo -e "  Major: $VERSION_MAJOR"
    echo -e "  Minor: $VERSION_MINOR"
    echo -e "  Patch: $VERSION_PATCH"
}

# Function to create git tag
create_git_tag() {
    local version=$(get_version)
    
    if [ ! -d ".git" ]; then
        echo -e "${YELLOW}Warning: Not a git repository, skipping tag creation${NC}"
        return
    fi
    
    read -p "Create git tag v$version? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        git tag -a "v$version" -m "Version $version"
        echo -e "${GREEN}✓ Created git tag: v$version${NC}"
        echo -e "${YELLOW}Note: Push tags with: git push --tags${NC}"
    fi
}

# Main script logic
case "$1" in
    show|current)
        show_version
        ;;
    bump)
        if [ -z "$2" ]; then
            echo -e "${RED}Error: Specify bump type: major, minor, or patch${NC}"
            echo "Usage: $0 bump [major|minor|patch]"
            exit 1
        fi
        bump_version "$2"
        create_git_tag
        ;;
    set)
        if [ -z "$2" ]; then
            echo -e "${RED}Error: Specify version number${NC}"
            echo "Usage: $0 set MAJOR.MINOR.PATCH"
            exit 1
        fi
        set_version "$2"
        ;;
    *)
        echo -e "${BLUE}Version Management Script${NC}"
        echo ""
        echo "Usage: $0 [command] [options]"
        echo ""
        echo "Commands:"
        echo "  show, current          Show current version"
        echo "  bump [type]           Bump version (major|minor|patch)"
        echo "  set [version]         Set version explicitly (e.g., 1.2.3)"
        echo ""
        echo "Examples:"
        echo "  $0 show               # Show current version"
        echo "  $0 bump patch         # Bump patch version (1.0.0 → 1.0.1)"
        echo "  $0 bump minor         # Bump minor version (1.0.0 → 1.1.0)"
        echo "  $0 bump major         # Bump major version (1.0.0 → 2.0.0)"
        echo "  $0 set 2.1.0          # Set version to 2.1.0"
        echo ""
        exit 1
        ;;
esac

