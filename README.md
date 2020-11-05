[![Build Status](https://travis-ci.org/koekeishiya/chunkwm.svg?branch=master)](https://travis-ci.org/koekeishiya/chunkwm)

### chunkwm - Tiling WM for macOS High Sierra & Mojave

**chunkwm** was a tiling window manager for macOS High Sierra and Mojave developed by [koekeishiya](https://github.com/koekeishiya/). The project is **no longer in development** and has been superseded by **[yabai](#)** for macOS Big Sur and onwards.

The original repository has since been deleted, and the last known commits/accepted pull requests have been merged here.

---

#### Installation and configuration

- Installation guides can be foundin the *docs* folder of the repository:
  - chunkwm must be built from the source code ([./docs/docs/source.html](./docs/docs/source.html)).
    - Supports macOS High Sierra and Mojave.
    - Requires the **[skhd](https://github.com/koekeishiya/skhd)** daemon for keyboard shortcuts.
    - Requires *xcode-8* command line tools.
- Configuration examples can be found in the *examples* folder of the repository.
- User guides can be found in the *docs* and *src* folders of the repository:
  - Introduction to chunkwm ([./docs/docs/userguide.html](./docs/docs/userguide.html))
  - Tiling reference ([./src/plugins/tiling/README.md](./src/plugins/border/README.md))
  - Border reference ([./src/plugins/border/README.md](./src/plugins/border/README.md))
  - FFM reference ([./src/plugins/ffm/README.md](./src/plugins/border/README.md))

---

#### Deprecation notice from [koekeishiya](https://github.com/koekeishiya/)

**chunkwm is no longer in development because of a C99 re-write, [yabai](https://github.com/koekeishiya/yabai).** 

This was originally supposed to be the first RC version of **chunkwm**. However, due to major architectural changes, supported systems, and changes to functionality, it has been released separately. There are multiple reasons behind these changes, based on the experience gained while experimenting with, designing, and using both kwm and chunkwm. Some of these changes are performance related, while others have been made with the user experience, operating system integration, error reporting, and customisability in mind.
