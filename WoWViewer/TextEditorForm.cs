using System.Text;
using WOWViewer;

namespace WoWViewer
{
    public partial class TextEditorForm : Form
    {
        private List<WowTextEntry> entries = new List<WowTextEntry>();
        private List<WowTextBackup> backup = new List<WowTextBackup>();
        private static readonly Encoding Latin1 = Encoding.GetEncoding("iso-8859-1");
        private static string inputPath = "TEXT.ojd";
        public TextEditorForm()
        {
            InitializeComponent();
            listBox1.DrawMode = DrawMode.OwnerDrawFixed;
            listBox1.DrawItem += ListBox1_DrawItem!;
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(ListBox), typeof(Label) });
            if (!checkSaveFileExists()) { return; } // user doesn't have TEXT.ojd file.
            parseTEXTOJD();
        }
        private bool checkSaveFileExists()
        {
            if (!File.Exists(inputPath))
            {
                MessageBox.Show("TEXT.ojd file not found, please check the game directory.", "File Not Found", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
            return true;
        }
        // for the listBox draw item event to change the color of the text if an entry is edited
        private void ListBox1_DrawItem(object sender, DrawItemEventArgs e)
        {
            if (e.Index < 0 || e.Index >= listBox1.Items.Count) { return; } // Check if index is valid before drawing
            e.DrawBackground();
            string itemText = listBox1.Items[e.Index].ToString()!;
            int entryIndex = int.Parse(itemText.AsSpan(0, 4));
            // Determine proper color
            Color textColor = (e.State & DrawItemState.Selected) == DrawItemState.Selected
                ? SystemColors.HighlightText
                : entries[entryIndex].Edited ? Color.Red
                : SystemColors.WindowText;
            // Use TextRenderer instead of Graphics.DrawString for better alignment and kerning
            TextRenderer.DrawText(e.Graphics, itemText, e.Font, e.Bounds, textColor, TextFormatFlags.VerticalCenter);
            e.DrawFocusRectangle();
        }
        // for initial parsing the TEXT.ojd file into the listBox for the TextEditorForm
        public void parseTEXTOJD()
        {
            byte[] data = File.ReadAllBytes(inputPath); // read the file into a byte array
            int offset = 0x289; // first string starts at 0x289
            for (int i = 0; i < 1396; i++) // there are only 1396 entries
            {
                byte category = data[offset + 4];  // Faction: 00 = Martian, 01 = Human, 02 = UI
                ushort length = (ushort)(data[offset + 8] | (data[offset + 9] << 8)); // bytes 9 and 10 are the string length
                int stringOffset = offset + 10; // string offset
                string text = Latin1.GetString(data, stringOffset, length - 1).Replace("\\n", "\n");
                // string length is one less than the ushort length as length contains the null operator // replaces \n with actual new line
                listBox1.Items.Add($"{i:D4} : [{getFaction(category)}] : {text}");
                entries.Add(new WowTextEntry { Name = text, Faction = category, Index = (ushort)i });
                backup.Add(new WowTextBackup { Name = text }); // create backup for entries to undo changes
                offset += (int)length + 9; // move offset to next entry // not + 10 because length contains the null operator ( hence - 1 above at text )
            }
        }
        // list box selected index changed event
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            int index = getRealIndex();
            richTextBox1.Enabled = true; // enable the richTextBox
            richTextBox1.Text = entries[index].Name; // update the richTextBox with the selected entry
            if (!entries[index].Edited) { checkForEdits(); } // check for any other edits
            else { button4.Enabled = true; } // enable the undo button if the currently selected entry is edited
        }
        // faction type or user interface
        private static string getFaction(byte category) => category == 0x00 ? "Martian" : category == 0x01 ? "Human" : category == 0x02 ? "UI" : "Unknown";
        // get the real index from the listBox text
        private int getRealIndex() => int.Parse(listBox1.Text.AsSpan(0, 4));
        // text updated on key down event
        private void richTextBox1_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                e.SuppressKeyPress = true; // suppress the enter key from adding a new line
                int index = getRealIndex(); // get the real index
                string updatedText = richTextBox1.Text; // get the text from the rich text box
                entries[index].Name = updatedText; // update the entry name
                RefreshListBoxEntry(index, updatedText);
                this.richTextBox1.Select(this.richTextBox1.Text.Length, 0); // thank you!!! : https://stackoverflow.com/questions/2241862/windows-forms-richtextbox-cursor-position/6457512
                if (updatedText == backup[index].Name)
                {
                    entries[index].Edited = false;
                    checkForEdits(); // check for any other edits
                    return;
                }
                entries[index].Edited = true; // mark the entry as edited
                button1.Enabled = true; // enable the save button
                label2.Text = "Status : Changes Made";
                button4.Enabled = true; // enable the undo button
            }
        }
        // save file button
        private void button1_Click(object sender, EventArgs e)
        {
            if(!checkSaveFileExists()) { return; } // user deleted the file while editing
            byte[] data = File.ReadAllBytes(inputPath); // read the file into a byte array
            using (FileStream fs = new FileStream(inputPath, FileMode.Create, FileAccess.Write, FileShare.None))
            {
                int offset = 0x289; // Original header starts here
                fs.Write(data, 0, offset); // Write original header 0x0 - 0x289
                for (int i = 0; i < 1396; i++) // total entry count of 1395
                {
                    byte[] stringBytes = Latin1.GetBytes(entries[i].Name.Replace("\r\n", "\\n").Replace("\r", "\\n").Replace("\n", "\\n")); // replace actual new line with \n so the game can read it
                    fs.Write(data, offset, 8);
                    fs.Write(BitConverter.GetBytes((ushort)(stringBytes.Length + 1)), 0, 2); // write the new string length (2 bytes)
                    fs.Write(stringBytes, 0, stringBytes.Length); // Write new or original string
                    if (entries[i].Edited) // update backup entries
                    {
                        backup[i].Name = entries[i].Name;
                        entries[i].Edited = false;
                    }
                    offset = offset + backup[i].Name.Length + 9;
                }
                fs.WriteByte(0x00); // write final byte which is always 0x00
            }
            reFilter(); // re filter to redraw the list box and remove the changes caused by the edited flag
            button1.Enabled = false;
            button4.Enabled = false;
            MessageBox.Show("TEXT.ojd updated successfully.", "Save Complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        // filter by faction
        private void radioButton1_CheckedChanged(object sender, EventArgs e) { filterByFaction(0x00); } // Martian
        private void radioButton2_CheckedChanged(object sender, EventArgs e) { filterByFaction(0x01); } // Human
        private void radioButton3_CheckedChanged(object sender, EventArgs e) { filterByFaction(0x02); } // UI
        private void radioButton4_CheckedChanged(object sender, EventArgs e) { filterByFaction(0x03); } // show all
        private void filterByFaction(byte faction)
        {
            richTextBox1.Text = "Select a string."; // clear the richTextBox
            richTextBox1.Enabled = false; // disable the richTextBox
            listBox1.BeginUpdate();
            listBox1.Items.Clear();
            foreach (WowTextEntry entry in entries)
            {
                if (entry.Faction == faction || faction == 0x03)
                {
                    listBox1.Items.Add($"{entry.Index:D4} : [{getFaction(entry.Faction)}] : {entry.Name}");
                }
            }
            listBox1.EndUpdate();
        }
        private void reFilter()
        {
            if (radioButton1.Checked) { filterByFaction(0x00); }
            else if (radioButton2.Checked) { filterByFaction(0x01); }
            else if (radioButton3.Checked) { filterByFaction(0x02); }
            else if (radioButton4.Checked) { filterByFaction(0x03); }
        }
        // export to text file
        private void button2_Click(object sender, EventArgs e)
        {
            using (StreamWriter log = new StreamWriter("TEXT.OJD.txt", false, Latin1))
            {
                for (int i = 0; i < 1396; i++) // there are only 1396 entries
                {
                    log.WriteLine($"{i:D4} [{getFaction(entries[i].Faction)}] : {entries[i].Name.Replace("\r\n", "\\n").Replace("\r", "\\n").Replace("\n", "\\n")}");
                }
            }
            MessageBox.Show("TEXT.OJD.TXT file saved in the game directory.");
        }
        // import strings from a text file
        private void button3_Click(object sender, EventArgs e)
        {
            string filePath;
            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.InitialDirectory = Directory.GetCurrentDirectory();
                openFileDialog.Filter = "Text Files (*.txt)|*.txt";
                openFileDialog.FilterIndex = 1;
                openFileDialog.RestoreDirectory = true;
                openFileDialog.Title = "Import TEXT.OJD.TXT file";
                if (openFileDialog.ShowDialog() != DialogResult.OK) { return; }
                else { filePath = openFileDialog.FileName; }
            }
            var lines = File.ReadLines(filePath, Latin1);
            if (lines.Count() < 1396) // check the line count matches before updating so there is no need to reparse the original TEXT.ojd file
            {
                MessageBox.Show("Error importing TEXT.OJD.TXT file, please check the file and try again.", "Import Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            int count = 0;
            foreach (var line in lines)
            {
                string name = string.Join(" ", line.Split(' ').Skip(3)).Replace("\\n", "\n");
                if(name != entries[count].Name) { entries[count].Edited = true; } // mark the entry as edited
                entries[count].Name = name; // update the entry name
                count++;
            }
            button1.Enabled = true; // enable the save button
            label2.Text = "Status : Changes Made"; // here we assume changes have been made when importing a TEXT.OJD.txt file
            reFilter(); // re-sort the listBox incase a filter is ticked already and repopulate the list
            MessageBox.Show("Text file imported, now just hit save!", "Import", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        // undo changes to selected string
        private void button4_Click(object sender, EventArgs e)
        {
            int index = getRealIndex();
            string updatedText = backup[index].Name;
            entries[index].Name = updatedText;
            entries[index].Edited = false;
            RefreshListBoxEntry(index, updatedText);
            richTextBox1.Text = updatedText;
            checkForEdits(); // check for any other edits
        }
        // check for any edits
        private void checkForEdits()
        {
            button4.Enabled = false; // disable the undo button
            if (entries.Any(e => e.Edited)) { return; } // check if any entry is edited
            button1.Enabled = false; // disable the save button
            label2.Text = "Status : No Changes Made";
        }
        // refresh the list box entry
        private void RefreshListBoxEntry(int index, string text)
        {
            int selectedIndex = listBox1.SelectedIndex; // get the selected index
            listBox1.SelectedIndexChanged -= listBox1_SelectedIndexChanged!; // remove event handler before changing selected index
            if (selectedIndex == listBox1.Items.Count) { listBox1.SelectedIndex = selectedIndex - 1; } // spoof code
            else { listBox1.SelectedIndex = selectedIndex + 1; } // spoof code to prevent the listBox from going out of bounds
            listBox1.BeginUpdate();
            listBox1.Items.RemoveAt(selectedIndex);
            listBox1.Items.Insert(selectedIndex, $"{index:D4} : [{getFaction(entries[index].Faction)}] : {text}");
            listBox1.SelectedIndex = selectedIndex;
            listBox1.SelectedIndexChanged += listBox1_SelectedIndexChanged!; // add event handler after changing selected index
            listBox1.EndUpdate();
        }
        // on close prompt
        private void TextEditorForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (entries.Any(e => e.Edited))
            {
                var result = MessageBox.Show(
                    "You have unsaved changes. Do you want to save before exiting?",
                    "Unsaved Changes",
                    MessageBoxButtons.YesNoCancel,
                    MessageBoxIcon.Warning);
                if (result == DialogResult.Cancel) { e.Cancel = true; } // Prevent closing
                else if (result == DialogResult.Yes) { button1.PerformClick(); } // Trigger the save button
            }
        }
    }
}