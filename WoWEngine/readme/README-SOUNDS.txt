WoW Engine - replacing the game's sounds
========================================

Any sound effect or voice line can be replaced with a WAV or OGG file of your own. Unlike the picture overrides,
this doesn't need Enhanced assets: a sound file is used as soon as it's there.


1. WHERE THE FILES GO
---------------------

In the game's folders, named after the sound they replace:

    Sound effects:              SFX\<name>.wav  or  SFX\<name>.ogg

    Voices, Human game:         VOX\HUMAN\<name>.wav     (or VOX\<name>.wav; .ogg the same)
    Voices, Martian game:       VOX\MARTIAN\<name>.wav   (or VOX\<name>.wav; .ogg the same)

When both are there, the .wav is used.

Upper or lower case doesn't matter. A sound without a file of its own plays as the game's.


2. FINDING A SOUND'S NAME
-------------------------

The easiest way: add these two lines to WoWRevived.ini

    [Debug]
    Dave=1

and play. WoWEngine.log (beside the program) then has a line for each sound the first time it plays:

    Dave: sound MCLICK2

so MCLICK2.wav in SFX replaces it. (Take the lines out again afterwards.)

WoWViewer also lists every sound in the game's sound libraries (SFX\SFX.WoW, VOX\HUMAN.WoW and
VOX\MARTIAN.WoW) and can export them as WAV files to start from.


3. THE FILES
------------

- WAV files: 8, 16, 24 or 32-bit, or 32-bit float; mono or stereo; any sample rate. They're converted to
  the game's form as they're read.
- OGG files (Ogg Vorbis): mono or stereo, any sample rate, converted the same way. Opus in an .ogg isn't
  read.
- Every sound in the game is 22050 Hz, so a WAV at 22050 Hz is used exactly as it is; any other rate is
  converted. Mono is right for nearly every sound; a few are stereo (the war room's background sounds, the
  briefings, the title, victory and defeat music, the epilogues), and a mono file is used for both sides
  of those.
- WAVs exported by WoWViewer put back unchanged sound as the game's. (Before WoWViewer's update in 2026,
  its exports of the short stereo sounds, the briefings, victory, defeat and OPTION, were labelled mono and
  play at half speed: export those again.)
- Compressed WAVs (ADPCM, MP3 inside a WAV) can't be read: save as plain PCM.
- A .snd file is read too: the libraries' own form, raw 16-bit at 22050 Hz with no header (mono, or
  interleaved stereo for the stereo sounds).


4. CHECKING IT WORKED
---------------------

WoWEngine.log names each file as it's first used:

    Sound file: ...\SFX\MCLICK2.wav

or says why it wasn't used:

    Sound file ...\SFX\MCLICK2.wav: format 2 at 4 bits isn't read (PCM 8, 16, 24 or 32-bit, or 32-bit float); not used


5. THE GAME'S LIBRARIES FIRST
-----------------------------

To play the game's own sounds and use loose files only for sounds the libraries don't have, add to
WoWRevived.ini:

    [Sound]
    Search Libraries First=1
