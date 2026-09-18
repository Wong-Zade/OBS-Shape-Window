# OBS Shape Window V1

A Windows x64 OBS source that displays another OBS video source through a live circle or rounded-rectangle mask.

## V1

- Select an existing OBS video source
- Circle mask
- Rounded rectangle mask
- Adjustable corner radius
- Normal OBS source transform/resize
- GitHub Actions Windows x64 build

## Build

The included GitHub Actions workflow fetches the official OBS plugin template build system on the GitHub Windows runner, then builds and packages the plugin. No Visual Studio or OBS SDK is required on the user's PC.

The current V1 dependency pin follows the official OBS plugin template's published OBS 31.1.1 / obs-deps 2025-07-11 Windows x64 hashes.
