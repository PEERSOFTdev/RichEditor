## MSVC Build Support

MSVC build support is being developed in the `msvc` branch.

To test MSVC builds:
```bash
git checkout msvc
# Follow instructions in BUILD_MSVC.md
```

The msvc branch contains:
- build_msvc.bat - Automated build script
- BUILD_MSVC.md - Comprehensive build documentation
- test_vswhere.bat - Diagnostic script for VS detection
- DIAGNOSTIC_BUILD.txt - Troubleshooting guide
- DIAGNOSIS_RESULT.txt - Common issues and solutions

Once MSVC build is stable and tested, it will be merged into master.
