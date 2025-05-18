using System.Text;
using System.Windows.Forms;
using WOWViewer;

namespace WoWViewer
{
    public partial class TextEditorForm : Form
    {
        private List<WowTextEntry> entries = new List<WowTextEntry>();
        private static readonly Encoding Latin1 = Encoding.GetEncoding("iso-8859-1");
        private static string inputPath = "TEXT.ojd";
        private static string temporaryString = string.Empty;
        public TextEditorForm()
        {
            InitializeComponent();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(ListBox), typeof(Label) });
            parseTEXTOJD();
        }
        // for parsing the TEXT.ojd file into the listBox for the TextEditorForm
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
                entries.Add(new WowTextEntry { Name = text, Length = length, Offset = offset, Faction = category, Index = (ushort)i });
                offset += (int)length + 9; // move offset to next entry // not + 10 because length contains the null operator ( hence - 1 above at text )
            }
        }
        // list box selected index changed event
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            richTextBox1.Enabled = true; // enable the richTextBox
            richTextBox1.Text = entries[getRealIndex()].Name; // update the richTextBox with the selected entry
            temporaryString = richTextBox1.Text;
        }
        // faction type or user interface
        private static string getFaction(byte category) => category == 0x00 ? "Martian" : category == 0x01 ? "Human" : category == 0x02 ? "UI" : "Unknown";
        // get the real index from the listBox text
        private int getRealIndex() => int.Parse(listBox1.Text.Substring(0, 4));
        // text updated on key down event
        private void richTextBox1_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                e.SuppressKeyPress = true; // suppress the enter key from adding a new line
                int index = getRealIndex(); // get the real index
                int selectedIndex = listBox1.SelectedIndex; // get the selected index
                string updatedText = richTextBox1.Text; // get the text from the rich text box
                if (selectedIndex == listBox1.Items.Count) { listBox1.SelectedIndex = selectedIndex - 1; } // spoof code
                else { listBox1.SelectedIndex = selectedIndex + 1; } // spoof code to prevent the listBox from going out of bounds
                entries[index].Name = updatedText; // update the entry name
                entries[index].Length = (ushort)(updatedText.Length); // we'll add null terminator on save
                listBox1.BeginUpdate();
                listBox1.Items.RemoveAt(selectedIndex);
                listBox1.Items.Insert(selectedIndex, $"{index:D4} : [{getFaction(entries[index].Faction)}] : {updatedText}");
                listBox1.SelectedIndex = selectedIndex;
                listBox1.EndUpdate();
                this.richTextBox1.Select(this.richTextBox1.Text.Length, 0); // thank you!!! : https://stackoverflow.com/questions/2241862/windows-forms-richtextbox-cursor-position/6457512
            }
        }
        // save file button
        private void button1_Click(object sender, EventArgs e)
        {
            byte[] data = File.ReadAllBytes(inputPath); // read the file into a byte array
            using (FileStream fs = new FileStream(inputPath, FileMode.Create, FileAccess.Write, FileShare.None))
            {
                int offset = 0x289; // Original header starts here
                fs.Write(data, 0, offset); // Write original header 0x0 - 0x289
                for (int i = 0; i < 1396; i++) // total entry count of 1395
                {
                    byte[] stringBytes = Latin1.GetBytes(entries[i].Name.Replace("\r\n", "\\n")); // replace actual new line with \n so the game can read it
                    fs.Write(data, entries[i].Offset, 8); // Copy header (first 8 bytes untouched)
                    fs.Write(BitConverter.GetBytes((ushort)(stringBytes.Length + 1)), 0, 2); // write the new string length (2 bytes)
                    fs.Write(stringBytes, 0, stringBytes.Length); // Write new or original string
                }
                fs.WriteByte(0x00); // write final byte which is always 0x00
            }
            MessageBox.Show("TEXT.ojd updated successfully.", "Save Complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        // filter by faction
        private void radioButton1_CheckedChanged(object sender, EventArgs e) { filterByFaction(0x00); } // Martian
        private void radioButton2_CheckedChanged(object sender, EventArgs e) { filterByFaction(0x01); } // Human
        private void radioButton3_CheckedChanged(object sender, EventArgs e) { filterByFaction(0x02); } // UI
        private void radioButton4_CheckedChanged(object sender, EventArgs e) { filterByFaction(0x03); } // show all
        private void filterByFaction(byte faction)
        {
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
        // export to text file
        private void button2_Click(object sender, EventArgs e)
        {
            byte[] data = File.ReadAllBytes(inputPath);
            //string outputPath = "TEXT.OJD.txt";
            using (StreamWriter log = new StreamWriter("TEXT.OJD.txt", false, Latin1))
            {
                int offset = 0x289; // first string starts at 0x289
                //int count = 0; // count checker for total number of entries
                for (int i = 0; i < 1396; i++) // there are only 1396 entries
                {
                    byte category = data[offset + 4];  // Faction: 00 = Martian, 01 = Human, 02 = UI
                    ushort length = (ushort)(data[offset + 8] | (data[offset + 9] << 8)); // bytes 9 and 10 are the string length
                    int stringOffset = offset + 10; // string offset
                    string text = Latin1.GetString(data, stringOffset, length - 1).Replace("\n", "").Replace("\r\n", "\\n"); // string length is one less than the byte length
                    log.WriteLine($"{i:D4} [{getFaction(category)}] : {text}");
                    offset += length + 9; // move offset to next entry // not + 10 because length contains the null operator ( hence - 1 above at text )
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
            var lines = File.ReadLines(filePath);
            if (lines.Count() < 1396) // check the line count matches before updating so there is no need to reparse the original TEXT.ojd file
            {
                MessageBox.Show("Error importing TEXT.OJD.TXT file, please check the file and try again.", "Import Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            int count = 0;
            foreach (var line in lines)
            {
                string name = string.Join(" ", line.Split(' ').Skip(3));
                entries[count].Length = name.Length;
                entries[count].Name = name;
                count++;
            }
            // re-sort the listBox incase a filter is ticked already and repopulate the list
            if (radioButton1.Checked) { filterByFaction(0x00); }
            else if (radioButton2.Checked) { filterByFaction(0x01); }
            else if (radioButton3.Checked) { filterByFaction(0x02); }
            else if (radioButton4.Checked) { filterByFaction(0x03); }
            MessageBox.Show("Text file imported, now just hit save!", "Import", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        // undo changes to selected string
        private void button4_Click(object sender, EventArgs e)
        {
            if (temporaryString != richTextBox1.Text) { richTextBox1.Text = temporaryString; return; }
            MessageBox.Show("No changes to undo.", "Undo", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }
}
