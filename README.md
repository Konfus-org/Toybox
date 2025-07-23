## About

Toybox is a simple, light-weight, plugin based, open source game engine that is currently in development.
There isn't much here yet but, hopefully, soon there will be!

## How to build

Toybox uses premake to generate project files for different build environments.
The repo contains a "makeproj.bat" script that you can run (if on windows) to generate a visual studio project.
If you are on another platform you will have to manually invoke the premake exe located under "Tools/Premake".
To learn how to use premake look to: https://premake.github.io/.

## What's here so far?

Honestly, not much yet. There is a simple ECS system dubbed TBS (Toy Box System), a plugin system that will find and load plugins at runtime, a math library (which is a thin wrapper around GLM), logging and asserts, a windowing system made using SDL, some graphics primitives, and an incomplete renderer and input system that are being built using SDL. I plan to add all the things a game engine needs! Physics, audio, networking, a UI system, editor and more!

## Contributing

Make a fork with your desired changes then create a pull request to merge it back into the main repo.
I will review, offer comments, and approve if all comments are addressed.
