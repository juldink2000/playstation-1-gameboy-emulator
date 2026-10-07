# playstation-1-gameboy-emulator

This is a Game Boy emulator for the PlayStation 1. It is still in early development. There is currently no sound, and I haven't been able to get it running above 20 FPS or achieve a stable frame rate yet.

You can burn it to a CD-R and run it on a real PlayStation 1.

If you want to add a new game, read the README for the rest of the instructions.

## Adding a Game

To add a new Game Boy game, first place the `.gb` ROM in the location used by the emulator.

Then rebuild the project.

Open a terminal in the project folder and run:

```text
taskkill /f /im duckstation-qt-x64-ReleaseLTCG.exe
cmake -S . -B build
del build\gbps1.bin
cmake --build build

After building, gbps1.bin will be recreated with the new game.

You can then open the new build in DuckStation to test it, or burn the resulting .bin/.cue to a CD-R and run it on a real PlayStation 1.

Notes
The ROM must be a Game Boy .gb file.
Make sure the ROM is placed in the correct folder before building.
The emulator is still in early development, so some games may not work correctly.
Performance is currently limited to around 20 FPS and the frame rate is not stable.
There is currently no sound.
