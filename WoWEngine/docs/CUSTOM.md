# Custom campaigns

The players' version of this page ships with the release as `README-CUSTOM-CAMPAIGNS.txt`
(`readme/README-CUSTOM-CAMPAIGNS.txt` in the source); a change to the rules goes in both.

MODERN (the owner's idea at update 245, built in update 246). A custom campaign is a text file that says what kind
of campaign it is and how it starts. The engine lists them on the main menu and plays them as it plays the regular
campaign and the civil wars.

## Where they are chosen

The main menu has **Custom Campaigns** under the side's civil war (command 0x9005, `FrontEnd::CustomCampaignsMenu`).
It lists the custom campaigns for the side the game was started as: a Human game lists the Human ones and the Human
Civil War ones, a Martian game the Martian ones and the Martian Civil War ones. The list shows five to a page, with
"Next Campaigns" and "Previous Campaigns" rows when there are more. With none, the heading says "No custom
campaigns".

- Choosing one goes on as Start New Campaign does: the slots ("Select Campaign to Replace"), the campaign's name, then
  straight to the war map. There is no intro movie, as in the civil wars.
- **Resume Custom Campaign**, at the top of the menu when there are custom saves for the side, is a Load menu of
  them only. The main menu's Resume Campaign lists the regular and civil war saves, not the custom ones.

## The files

Each custom campaign is a `.txt` file in a **Campaigns** folder in the main folder (the game's folder, beside
`Human.000` and `Martian.000`; since update 247, `dat\Campaigns` before). The engine also reads its own Campaigns
folder, beside the program or, in a build run where it was made, the source folder's (the places its loose `dat`
comes from, so not with `--no-loose`); a release ships its examples in the main folder's.

A file in the main folder's Campaigns hides one of the same name in the engine's. The files are read each time the menu
opens, so a change is seen without starting the program again. Lines the engine can't read are noted in
WoWEngine.log ("Custom campaign <file>: line n: ...") and left out. A file without a kind isn't offered.

Anything after a `#` is a comment. The keys:

| Key | Value | Default |
|---|---|---|
| `name` | what the menu shows | the file's name without `.txt` |
| `kind` | `human`, `martian`, `human civil war` or `martian civil war` | none: the file isn't offered |
| `start` | a start file in the same Campaigns folder (a name only, no folder; any extension: `Highlands.000`) | the side's own: `Human.000` or `Martian.000` |
| `pods` | `on` or `off`: whether the Martians' pods land (the Human and Martian kinds) | `on` |

A civil war (`human civil war`, `martian civil war`) also takes CivilWar.txt's layout lines, which say which of the
start file's sectors stay the player's, go to the other side or are emptied, and where the player's copy of a base
comes from and goes: `player`, `other`, `neutral`, `copy`, `copy at` (their rules are in `dat\CivilWar.txt`). What
the file doesn't give is the civil war's draft (`CivilWarLayout::Default`), not CivilWar.txt's.

- A civil war never has pods (they are the Martians' against the Humans: in a Human civil war they would give the
  Martians' machines to a Human side). `pods = on` there is noted and left out.
- The layout lines in a Human or Martian campaign are noted and left out.
- The **start file** is any save in the game's own form: a copy of `Human.000` or `Martian.000` changed with a save
  editor (WoWViewer's), or a campaign saved in `SaveGame` (`Human.003`, say) to begin from where it stood. It must
  be the side's: a Human start file for a Human campaign or Human civil war. A civil war is made from it as from
  the side's own start file (`MakeCivilWarStart`), so its H lists must hold the player's faction.

Examples are shipped in the Campaigns folder (`WoWEngine/Campaigns` in the source):

```
# Quiet Skies.txt: the Human campaign without the Martians' pods
name = Quiet Skies (no pods)
kind = human
pods = off
```

```
# Highlands Alone.txt: a harder Human Civil War
name = Human Civil War: The Highlands Alone
kind = human civil war
player  = 1 2 3 4                # North Western Highlands, Isle of Skye, Grampian Mountains, Strathmore
other   = rest                   # London's: every other Human sector of the start file
copy    = 30                     # London's base, copied to the Grampians
copy at = 468 540                # where the Martians' base was
```

and `Stranded.txt`, the Martian campaign without pods.

## The saves

`SaveGame\Custom.001` to `Custom.999`, one set of numbers for both sides (the Save menu never offers a number the
other side's custom save has). Each has the engine's tag (core/Campaign.h): `WOWE`, the tag's version (1), and the
campaign word, whose low byte is the kind (0 regular, 1 Human Civil War, 2 Martian Civil War) and whose bits above
are flags: 0x100 a custom campaign's, 0x200 no pods, 0x400 the player Martian (for the regular kind; a civil war's
side is its own). The original's save follows. The kind and flags are all a save needs: the definition is read only
when a campaign starts, so changing or deleting its file doesn't change a campaign already begun.

**A save carries its campaign.** Everything a campaign is (the war map, the research, the kind and the pods) is in
its save, so a `Custom.nnn` can be passed to another player and played without the definition or start file that
made it: it goes in their `SaveGame` folder as `Custom.nnn` with a number they don't use, and needs the engine
from update 246 on.

Numbered from 001 as every save is (the menus' slots start at 1); `.000` is the game's name for its start files.

An engine from before update 246 refuses a save with flags ("unknown campaign"); the original game never lists
`Custom.nnn`.

## In play

A custom campaign plays as its kind does: a Human or Martian one as the regular campaign (its endings and movies
included) but for the pods, a civil war as the civil wars (docs/MODERN.md "The civil wars": no pods, no movie at the
end). `WarMap::DropPod` asks `HasPods(campaign, flags)`.

## Not done yet

- A definition can't yet set the date, the money or the research, or add units: the start file is the way to change
  those (a save editor). `add` / `remove` lines for the layout are the obvious next step, as for CivilWar.txt.
- The Single Player Options menu (with the network version enabled) has no Custom Campaigns entry, as it has no
  civil war entry.
- The definition's name isn't kept in the save; the Load menu shows the campaign's own name.
