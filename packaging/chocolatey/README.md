# Peek Chocolatey package

This directory contains the Chocolatey Community Repository package for Peek.

Build the package from this directory:

```powershell
choco pack peek.nuspec
```

Test it in an isolated Windows environment before publishing:

```powershell
choco install peek --source . --version 0.1.0
choco uninstall peek
```

The package downloads the versioned x64 executable from the corresponding GitHub Release and verifies its SHA-256 checksum. Update the version, release URL, checksum, package source URL, icon URL, and release notes URL together for each release.
