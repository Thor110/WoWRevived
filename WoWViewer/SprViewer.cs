using System.Text;

namespace WoWViewer
{
    public partial class SprViewer : Form
    {
        private List<WowFileEntry> entries;
        private string selectedEntry; // selected SPR file on enter
        private string lastSelectedEntry;
        byte[] rawData;
        public static int PaletteOffset(int paletteIndex) => 768 + paletteIndex * 768;

        public SprViewer(List<WowFileEntry> entryList, string entryName)
        {
            InitializeComponent();
            entries = entryList;
            selectedEntry = entryName;
            PopulateList();
        }
        private void PopulateList()
        {
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".SPR", StringComparison.OrdinalIgnoreCase)).ToList()) { listBox1.Items.Add(entry.Name); }
            foreach (var entry in entries.Where(e => e.Name.EndsWith(".PAL", StringComparison.OrdinalIgnoreCase)).ToList()) { listBox2.Items.Add(entry.Name); }
            listBox1.SelectedIndex = listBox1.FindStringExact(selectedEntry); // set selectedEntry
            listBox2.SelectedIndex = 0; // set to first palette file
        }
        // SPR list box index changed
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (listBox1.SelectedItem!.ToString()! == lastSelectedEntry) { return; } // prevent selecting the same entry
            selectedEntry = listBox1.SelectedItem!.ToString()!; // update selected entry
            lastSelectedEntry = selectedEntry; // update last selected entry
            rawData = entries.First(e => e.Name.Equals(selectedEntry, StringComparison.OrdinalIgnoreCase))!.Data!; // find selected entry data
            if (Encoding.ASCII.GetString(rawData, 0, 4) == "FFUH") { rawData = FfuhDecoder.Decompress(rawData); } // decompress the selected entry data if it is compressed
            //
            RenderCurrent();
        }
        // PAL list box index changed
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string palName = listBox2.SelectedItem!.ToString()!;
            byte[] rawData = entries.First(e => e.Name.Equals(palName, StringComparison.OrdinalIgnoreCase)).Data!;
            palData = FfuhDecoder.Decompress(rawData);
            numericUpDown1.Maximum = SprDecoder.PaletteCount(palData) - 1;
            numericUpDown1.Value = 0;
            RenderCurrent();
        }
        private byte[] palData;

        private void RenderCurrent()
        {
            if (rawData == null || palData == null) { return; } // this won't ever fire
            int paletteOffset = SprDecoder.PaletteOffset((int)numericUpDown1.Value);
            pictureBox1.Image = SprDecoder.Render(rawData, palData, paletteOffset);
            label1.Text = SprDecoder.ReadInfo(rawData).ToString();
        }

        private void numericUpDown1_ValueChanged(object sender, EventArgs e)
        {
            // numericUpDown1 = raw byte offset, step 3 or 768
            numericUpDown1.Maximum = palData.Length - 768;
            numericUpDown1.Increment = 768; // step by one RGB entry at a time for fine tuning
            RenderCurrent();
        }
    }
}
