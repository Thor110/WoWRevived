WoW Engine on macOS
===================

This release is a folder of plain programs (WoWLauncher and WoWEngine) with the SDL3 library and the engine's
files beside them. Keep them together: the programs look for everything in their own folder, and the game is
installed into it.


1. FIRST START: THE PROGRAMS AREN'T SIGNED
------------------------------------------

The programs aren't signed by Apple, so macOS stops them the first time ("can't be opened because Apple cannot
check it for malicious software", or "is damaged"). Either:

- in Terminal, take the download's quarantine off the whole folder (once, after unzipping):

      xattr -dr com.apple.quarantine <the folder>

  (type "xattr -dr com.apple.quarantine " and drag the folder onto the Terminal window), or

- in the Finder, right-click (or Control-click) WoWLauncher, choose Open, then Open again in the box, and the
  same for WoWEngine. On recent macOS the box may only offer "Done": then open System Settings > Privacy &
  Security and choose "Open Anyway" there. If a box then names libSDL3 instead, the Terminal way above is the
  one that works.

Double-clicking a program opens a Terminal window beside the game: that's how macOS starts a program that isn't
an .app. It can be left alone; it closes with the game.


2. INSTALLING AND PLAYING
-------------------------

Start WoWLauncher. It installs the game from the Human and Martian discs or their images into this folder, then
starts it as either side. The music has to come from a .bin/.cue image: macOS doesn't let programs read audio
straight from a CD, and .iso images don't hold the music.


3. THE MOVIES IN MP4
--------------------

WoWRevived's .mp4 movies are played through the ffmpeg program where it's installed (Homebrew: brew install
ffmpeg); without it the game's own .smk movies are played.
