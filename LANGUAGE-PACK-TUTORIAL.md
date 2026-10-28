## Language Pack Tutorial

How to create a language pack - rough guide.

First, create a backup of your TEXT.OJD file, just to be safe.

Export the TEXT.OJD file to a .txt file or edit them in the text editor line by line.

When translating a TEXT.OJD.TXT file, please keep the format the same, because importing properly relies on this.

That is, that it gets the entire length of the string after the third space and it relies on there being one line per string (1397 lines, or 1396 for an original TEXT.OJD without the added Credits entry).

0000 [FACTION] : STRING

So in this case it would return "STRING"

Everything is initially read from the TEXT.OJD file and then the strings are updated when you import a file.

Some lines are separated by "\n" new line separation, this is replaced with a real new line when importing or using the text editor.

But it is corrected when exporting or saving, so make sure to keep them as they are because the game reads them that way

After you are done translating then import the file and click save to update the TEXT.OJD file.

Saving writes a new TEXT.OJD and keeps the previous one as TEXT.OJD.bak. If a string contains a character the game's ISO-8859-1 text can't hold, the save is refused and the message names the string to fix.

Language packs should also include a `language.txt` file containing just the language name on its first line (English, French, German, Italian or Spanish). The launcher, Smackw32.dll and _inmm.dll use it to pick the launcher text, credits timing and Discord text, and fall back to the size of credits.png when it is missing.

If you create a language pack please reach out to me as I would like to include them in this repository for future use.