WoW Engine - custom campaigns
=============================

A custom campaign is a small text file that says what kind of campaign it is and how it starts: a Human
or Martian campaign, or a Human or Martian Civil War, from the game's own start or from a start file of
your own, with or without the Martians' pods.


1. PLAYING ONE
--------------

Main Menu > Custom Campaigns (under the Civil War entry). (The Civil War entry itself asks which side to play;
a custom civil war's file says which sectors are yours, so it doesn't ask.)

The menu lists the custom campaigns for the side you started the game as: a Human game shows the Human
campaigns and Human Civil Wars, a Martian game the Martian ones. Pick one, choose a slot and a name as for
any new campaign, and it starts on the war map (there is no intro movie).

"Resume Custom Campaign" at the top of the menu lists your custom campaigns' saves. The main menu's own
Resume Campaign lists the regular and Civil War saves only.

Three examples come with the engine:

    Quiet Skies (no pods)                  the Human campaign without the Martians' pods
    Human Civil War: The Highlands Alone   a harder Human Civil War: only the far north is yours
    Stranded (no pods)                     the Martian campaign without the pods


2. WHERE THE FILES GO
---------------------

In a folder called Campaigns in the main folder, the one with Human.000 and Martian.000 in it:

    <game folder>\Campaigns\Highlands Alone.txt     a campaign (it must end in .txt)
    <game folder>\Campaigns\Highlands.000           a start file it uses (optional, any name)

The files are read each time the Custom Campaigns menu opens, so you can change one, go back to the main
menu and open Custom Campaigns again to see the change; there's no need to restart. A change is used by
campaigns started after it: campaigns already begun keep what they started with.


3. WRITING ONE
--------------

A campaign file is plain text, one setting per line, as "key = value". Anything after a # is a comment.

    name = Quiet Skies (no pods)     # what the menu shows
    kind = human
    pods = off

The settings:

    name    What the menu shows. Without it, the file's name (without .txt).

    kind    One of:   human   martian   human civil war   martian civil war
            Required: a file without a kind isn't shown.

    start   A start file in the same Campaigns folder, by its name only (no folder), for example
                start = Highlands.000
            Without it the campaign starts from the game's own Human.000 or Martian.000.

    pods    on or off: whether the Martians' pods land (human and martian kinds only).
            Optional: without it they're on. A civil war never has pods.

A civil war also takes the same layout lines as DAT\CivilWar.txt (that file explains them), which say
how the start is split between the two sides:

    player  = 1 2 3 4    # these sectors' objects stay yours
    other   = rest       # these go to the other side ("rest": every sector not named elsewhere)
    neutral =            # these are emptied and belong to no one
    copy    = 30         # your home is given a copy of this sector's base (or: none)
    copy at = 468 540    # where on the war map the copy goes

Lines left out take the standard Civil War layout built into the engine. If you give any of player,
other or neutral, they replace the standard split together, so give every one you need. Your home is
always the Grampian Mountains (3) in a Human Civil War and London (30) in a Martian one. Sectors the other
faction held in the start file are always emptied.

Sector numbers:

     1 North Western Highlands    2 Isle of Skye               3 Grampian Mountains
     4 Strathmore                 5 Isle of Man                6 Argyll
     7 Southern Uplands           8 Edinburgh                  9 Galloway
    10 Northumberland            11 Newcastle                 12 Lake District
    13 Pennines                  14 Whitby                    15 Liverpool
    16 Nottingham                17 The Wolds                 18 North Wales
    19 Cotswolds                 20 Northampton               21 Fens
    22 South Wales               23 Bristol                   24 Oxford
    25 Essex                     26 Norfolk                   27 Cornwall
    28 Southampton               29 Dover                     30 London


4. START FILES
--------------

A start file is a saved campaign in the game's own form, so you can make one with a save editor
(WoWViewer's) from a copy of Human.000 or Martian.000, or copy one of your own saves (SaveGame\Human.003,
say) to begin from where it stood. Any name and extension will do.

It must be your side's: a Human start file for a Human campaign or Human Civil War, a Martian one for the
Martians'. For a civil war, the layout lines above are applied to it as the campaign starts.


5. SAVES
--------

Custom campaigns are saved as SaveGame\Custom.001, Custom.002 and so on, up to Custom.999. Human and
Martian custom saves share the numbers, and neither side is ever offered the other's slot to replace.

A save carries its whole campaign. Everything a campaign is (the war map, the research, its kind and
whether the pods land) is in the save. The campaign file and start file are only read when a campaign
starts. So:

- you can pass a Custom.nnn save to someone else and they can play it without the campaign file that
  made it: they put it in their SaveGame folder as Custom.nnn, with a number they aren't using;
- changing or deleting a campaign file doesn't change campaigns already begun.
- with the Modern Gameplay option "Bridges stay broken", the bridges destroyed so far are kept beside the save
  as Custom.nnn.bridges: pass that on with the save to keep them down.

Custom saves need this engine (version 0.2.5 from update 246, or later). The original game doesn't list
them and leaves them alone.


6. IF A CAMPAIGN DOESN'T APPEAR
-------------------------------

WoWEngine.log (beside the program) has a line for anything in a campaign file the engine couldn't read:

    Custom campaign Mine.txt: line 3: kind is human, martian, human civil war or martian civil war, not 'humans'
    Custom campaign Mine.txt: no kind: the file isn't offered

Check that:
- the file is in the Campaigns folder of the main folder and its name ends in .txt;
- it has a kind line;
- you started the game as the side it's for (a Martian campaign only shows in a Martian game).

When a campaign starts from its own start file, the log names the file it used:

    Custom campaign's start file: ...\Campaigns\Highlands.000
