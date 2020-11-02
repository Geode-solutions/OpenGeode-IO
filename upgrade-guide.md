# Upgrade Guide

## Upgrading from OpenGeode-IO v4.x.x to v5.0.0

### Motivations

This new major release is to formalize PyPI support for OpenGeode.

### Breaking Changes

- **CMake**: CMake minimum requirement has been upgraded to 3.14 to ease import of PyPI modules.


## Upgrading from OpenGeode-IO v3.x.x to v4.0.0

### Motivations

Add a directory "io" to remove confusing include with OpenGeode.

### Breaking Changes

- Include files from OpenGeode-IO should add the path to the files.

## Upgrading from OpenGeode-IO v2.x.x to v3.0.0

### Motivations

Homogenize CMake project and macro names with repository name.

### Breaking Changes

- Replace `OpenGeodeIO` by `OpenGeode-IO`. For example:
`OpenGeodeIO::mesh` is replaced by `OpenGeode-IO::mesh`.
