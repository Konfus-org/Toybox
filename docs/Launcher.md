# Toybox Launcher

Toybox now starts apps through a single generic launcher executable instead of per-example launcher projects.

## Supported Arguments

- `ToyboxLauncher.exe --app=<AppModuleName>` loads the requested app module from the launcher executable directory.
- `--working-dir=<dir>` is optional. When omitted, it defaults to the launcher executable directory.
- Logs always write beneath `<launcher exe dir>/logs`, resolved automatically by `tbx::Log`.
- `--settings=<settings_path>` is optional and overrides the default `Settings.json` asset.
- `--register-assets=<asset_dir>` is optional and appends extra asset dirs to be registered to the asset manager.
- `--inject-plugins=<plugin_name, plugin_name, ...>` is optional and appends extra runtime plugins.
- `--live-together-die-together=<pid>` is optional and ties the engine's lifetime to a launching process: the engine monitors that process id and shuts itself down once it exits, so a host (e.g. Toybox Studio) never leaves an orphaned engine behind — even if the host crashes. Omit it to run independently.

## Apps

- Launchable apps derive from `tbx::App`.
- The launcher only performs app discovery, command-line parsing, and log-directory setup.
- Each app owns service-provider creation, settings loading, runtime plugin loading, window creation, the frame loop, and shutdown.
- Each app receives the resolved working directory from the launcher and uses it to configure its `IFileOps` service.

## Settings

- `tbx::AppSettings` owns the app name, graphics/world/physics settings, and the startup plugin list.
- Runtime plugin dependencies live in the `plugins` array inside the app settings asset.
