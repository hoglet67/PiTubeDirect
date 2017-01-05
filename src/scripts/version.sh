#!/bin/bash

# Lookup the last commit ID
VERSION="$(git rev-parse --short HEAD)"

# Check if any uncommitted changes in tracked files
if [ -n "$(git status --untracked-files=no --porcelain)" ]; then 
  VERSION="${VERSION}?"
fi

echo "#define GITVERSION \"${VERSION}\""


