using System.Text;
using WOWViewer;

namespace WoWViewer
{
    public partial class TextEditorForm : Form
    {
        private List<WowTextEntry> entries = new List<WowTextEntry>();
        private HashSet<ushort> updatedIndices = new HashSet<ushort>();
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
            string inputPath = "TEXT.ojd";
            byte[] data = File.ReadAllBytes(inputPath);
            int offset = 0x289; // first string starts at 0x289
            for (int i = 0; i < 1396; i++) // there are only 1396 entries
            {
                byte buttonID = data[offset + 2]; // button type???
                byte category = data[offset + 4];  // Faction: 00 = Martian, 01 = Human, 02 = UI
                byte buttonFunction = data[offset + 6]; // button function??
                ushort length = (ushort)(data[offset + 8] | (data[offset + 9] << 8)); // bytes 9 and 10 are the string length
                int stringOffset = offset + 10; // string offset
                Encoding latin1 = Encoding.GetEncoding("iso-8859-1");
                string text = latin1.GetString(data, stringOffset, length - 1); // string length is one less than the byte length
                text = text.Replace("\\n", "\n"); // <-- this is the actual fix
                listBox1.Items.Add($"{i:D4} : {text}");
                entries.Add(new WowTextEntry { Name = text, Length = length, Offset = offset }); // re-using WowFileEntry class
                offset += (int)length + 9; // move offset to next entry // not + 10 because length contains the null operator ( hence - 1 above at text )
            }
        }
        // list box selected index changed event
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            richTextBox1.Enabled = true; // enable the richTextBox
            richTextBox1.Text = entries[listBox1.SelectedIndex].Name;
        }
        // text updated
        private void richTextBox1_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                int index = listBox1.SelectedIndex;
                string updatedText = richTextBox1.Text;
                if (index == 1396) { listBox1.SelectedIndex = index - 1; } // spoof code
                else { listBox1.SelectedIndex = index + 1; } // spoof code to prevent the listBox from going out of bounds
                entries[index].Name = updatedText;
                entries[index].Length = (ushort)(updatedText.Length); // we'll add null terminator on save
                listBox1.BeginUpdate();
                listBox1.Items.RemoveAt(index);
                listBox1.Items.Insert(index, $"{index:D4} : {updatedText}");
                listBox1.SelectedIndex = index;
                listBox1.EndUpdate();
                this.richTextBox1.Select(this.richTextBox1.Text.Length, 0); // thank you!!! : https://stackoverflow.com/questions/2241862/windows-forms-richtextbox-cursor-position/6457512
                updatedIndices.Add((ushort)index); // Track only this updated index
            }
        }
        // save file button
        private void button1_Click(object sender, EventArgs e)
        {
            string inputPath = "TEXT.ojd";
            byte[] data = File.ReadAllBytes(inputPath);
            using (FileStream fs = new FileStream(inputPath, FileMode.Create, FileAccess.Write, FileShare.None))
            {
                int offset = 0x289; // Original header ends here
                fs.Write(data, 0, offset); // Write original header
                Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
                for (int i = 0; i < entries.Count; i++)
                {
                    bool isUpdated = updatedIndices.Contains((ushort)i);
                    WowTextEntry entry = entries[i];
                    string safeText = entries[i].Name.Replace("\r\n", "\\n");
                    byte[] stringBytes = Encoding.GetEncoding("iso-8859-1").GetBytes(safeText);
                    ushort lengthWithNull = (ushort)(stringBytes.Length + 1);
                    byte[] lengthBytes = BitConverter.GetBytes(lengthWithNull);
                    fs.Write(data, entry.Offset, 8); // Copy header (first 8 bytes untouched)
                    fs.Write(lengthBytes, 0, 2); // Replace the 2 length bytes with the updated length
                    fs.Write(stringBytes, 0, stringBytes.Length); // Write new or original string
                }
                fs.WriteByte(0x00); // write final byte
            }
            MessageBox.Show("TEXT.ojd updated successfully.", "Save Complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }
}
