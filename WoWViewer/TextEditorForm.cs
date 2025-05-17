using Microsoft.VisualBasic.Logging;
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
        public void parseTEXTOJD() // for parsing the TEXT.ojd file into the listBox for the TextEditorForm
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
                string text = Encoding.ASCII.GetString(data, stringOffset, length - 1); // string length is one less than the byte length
                //MessageBox.Show($"String: {text}"); // debug
                listBox1.Items.Add($"{i:D4} : {text}");
                entries.Add(new WowTextEntry { Name = text, Length = length, Offset = offset }); // re-using WowFileEntry class
                offset += (int)length + 9; // move offset to next entry // not + 10 because length contains the null operator ( hence - 1 above at text )
            }
        }
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
                if (index == 1396)
                {
                    listBox1.SelectedIndex = index - 1; // spoof code
                }
                else
                {
                    listBox1.SelectedIndex = index + 1; // spoof code
                }
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
            parseCheck(); // check what values have changed before saving
        }
        // save the file
        private void parseCheck()
        {
            string inputPath = "TEXT.ojd";
            byte[] data = File.ReadAllBytes(inputPath);
            using (FileStream fs = new FileStream(inputPath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None))
            {
                int offset = 0x293; // first string starts at 0x289
                fs.Seek(0, SeekOrigin.Begin); // move to the beginning of the file
                fs.Write(data, 0, offset); // write first 289 bytes
                int currentOffset = offset; // current offset
                for (int j = 0; j < 1396; j++)
                {
                    if (updatedIndices.Contains((ushort)j))
                    {
                        int stringOffset = entries[updatedIndices.ElementAt(j)].Offset; // get the offset
                        ushort length = (ushort)entries[updatedIndices.ElementAt(j)].Length; // get the length
                        string text = Encoding.ASCII.GetString(data, stringOffset, length); // string length is one less than the byte length
                        ushort theLength = (ushort)(data[currentOffset - 2] | (data[currentOffset - 1] << 8));
                        byte[] newLength = BitConverter.GetBytes((ushort)length + 1); // add extra byte to header character count
                        byte[] newText = Encoding.ASCII.GetBytes(entries[updatedIndices.ElementAt(j)].Name);
                        if (text != entries[updatedIndices.ElementAt(j)].Name) // double check string has actually changed
                        {
                            if(length < (ushort)theLength || length > (ushort)theLength) // if length is different
                            {
                                fs.Seek(offset - 2, SeekOrigin.Begin); // rewind and update the entry length
                                fs.Write(newLength, 0, 2); // update the length
                                fs.Write(newText, 0, length); // update the string // written wrong? // Writing as FF 03 00 04 00 1A 02
                                fs.Write(new byte[] { 0x00 }, 0, 1); // write the null terminator
                                currentOffset += length + 1; // update the offset for writing for the null terminator
                            }
                        }
                        if (j != 1395) // special case for the very last string
                        {
                            fs.Write(data, currentOffset, 9); // write the next header
                            currentOffset += 9; // update the offset for writing
                        }
                        else
                        {
                            fs.Write(data, currentOffset, 1); // write the last byte
                        }
                    }
                    else // write the original string
                    {
                        ushort theLength = (ushort)(data[currentOffset - 2] | (data[currentOffset - 1] << 8)); // bytes 9 and 10 are the string length
                        fs.Write(data, currentOffset, theLength); // update the string
                        currentOffset += 2; // update the offset for writing
                        if (j != 1395) // special case for the very last string
                        {
                            fs.Write(data, currentOffset, 9); // write the next header
                            currentOffset += 9; // update the offset for writing
                        }
                        else
                        {
                            fs.Write(data, currentOffset, 1); // write the last byte
                        }
                    }
                }
            }
            MessageBox.Show("TEXT.ojd updated successfully.", "Save Complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }
}
