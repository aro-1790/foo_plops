# foo_plops- PLaylist OPerationS for foobar2000

> **Disclaimer first:** AI-assisted (that's a nice way of saying generated) with mostly Minimax M3.1 Flash on NIM and finished up with Freebuff, at a rightfully low version number.
> Still, it has a very small surface and a very small task. Don't be afraid to report issues, however!

### What it is

A foobar2000 component for replacing some functions from a couple of plugins that are no longer available for modern fb2k users.

- Playlist context menu entries are added, inspired by my usage of [foo_utils](https://foosion.foobar2000.org/components/?id=utils) for just this purpose: a **Playlist Operations** group holding **Add to...**, **Move to...**, and **Remove from all playlists...** Move strips the selected songs from the current playlist and sends them to the target, while Add duplicates them there and leaves the source alone. The Remove command is self-explanatory: it finds the selected songs in every playlist and removes them there too, reporting what it changed. foobar2000 can already do much of that in one step, so I'm not sure how valuable that one is.
- The Edit main menu gets its own entry, inspired by a function that old builds of [marc2k3](https://marc2k3.github.io/)'s perpetually-deleting-shit ass foo_run_main provided: **Playlist Operations → Go to top of active playlist**, which selects the first item and scrolls it into view. I may add more functions in the future, upon need or inspiration.

Locked playlists- autoplaylists included- are skipped rather than overridden, and undo stays per-playlist like the rest of foobar2000, so don't expect one undo to cover both playlists. There is no configuration UI and not much else hiding in here yet, and I hope to keep it that way.

### Building

Visual Studio (I'm using the 2022 set installed with [PortableMSVC](https://github.com/tgbender/portablemsvc)) with the C++ toolchain; the SDK is fetchable from the build script.

> ```
> build.bat fetch     fetch the pinned dependency (the SDK)
> build.bat build     build release\foo_plops.dll and release\x64\foo_plops.dll
> ```

### Credits

The absolute fools who have unleashed hell on this planet so I can develop a new dumb addiction that doesn't really benefit anyone, probably? Well, that, and myself... I guess. And free model availability, as I've spent enough money already. Go donate to and celebrate wholesale the actual coders out there. There simply aren't enough of them to fulfill silly niche projects like this.
