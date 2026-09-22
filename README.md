# little world

A greenhouse, a few living gardens, and time to tend them.

![Little World](.github/terrarium.png)

Built with [sengine](https://github.com/ImArjunJ/sengine) and [simulates](https://github.com/ImArjunJ/simulates).

```sh
git clone --recurse-submodules https://github.com/ImArjunJ/little-world.git
cd little-world
./vendor/sengine/tools/fetch_filament.sh
./run.sh
```

Linux: Clang with libc++, CMake 3.24+, SDL3 3.2+, FreeType, libpng, and OpenGL 4.1. Assets are included. Saves live in `~/.local/share/little-world`, or `$XDG_DATA_HOME/little-world`.

WASD to walk. E to tend, F to lift, J for the journal. Escape to return.
