namespace WoWViewer
{
    public partial class MapEditorForm : Form
    {
        private List<WowTextEntry> entries = new List<WowTextEntry>();
        private byte[]? levelData;
        public MapEditorForm()
        {
            InitializeComponent();
            if (!File.Exists("TEXT.ojd") || !File.Exists("OBJ.ojd"))
            {
                MessageBox.Show("TEXT.ojd or OBJ.ojd not found, please reinstall.");
                this.Close();
                return;
            }
            Reusables loadText = new Reusables();
            entries = loadText.LoadEntries(File.ReadAllBytes("TEXT.ojd"));
            entries = loadText.LoadObjLookup(entries);
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, typeof(Label));
            checkBox1_CheckedChanged(null!, null!); // populate listBox1
        }
        // this is a test method to parse the .nsb map files and log the results to a text file
        public void parseNSB()
        {
            listBox2.Items.Clear();
            listBox3.Items.Clear();
            int length = (levelData!.Length - 8) / 12; // minus header
            int position = 8;
            for(int i = 0; i < length; i++)
            {
                ushort type = (ushort)(levelData[position] | (levelData[position + 1] << 8));
                ushort x = (ushort)(levelData[position + 4] | (levelData[position + 5] << 8));
                ushort y = (ushort)(levelData[position + 8] | (levelData[position + 9] << 8));
                string name = BmolName(type);
                // Only show entries with a resolved OTYPE name; skip internal engine types
                if (!name.StartsWith("#"))
                {
                    listBox2.Items.Add($"{name} X:{x} Y:{y} 0x{position}");
                }
                else
                {
                    listBox3.Items.Add($"{name} X:{x} Y:{y} 0x{position}");
                }
                position += 12;
            }
        }

        //private string BmolName(int id) => entries.FirstOrDefault(e => e.BmolId == (ushort)id)?.Name ?? $"#{id}";
        private string BmolName(int id)
        {
            var entry = entries.FirstOrDefault(e => e.BmolId == (ushort)id);
            if (entry == null) return $"#{id}";
            // Prefer localised text name; fall back to OTYPE_ string; last resort is raw ID
            return !string.IsNullOrEmpty(entry.Name) ? entry.Name
                 : !string.IsNullOrEmpty(entry.OTypeName) ? entry.OTypeName
                 : $"#{id}";
        }
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string levelName = $"DAT\\{listBox1.SelectedIndex + 1}.nsb";
            if (!File.Exists(levelName))
            {
                MessageBox.Show($"DAT\\{listBox1.SelectedIndex + 1}.nsb file missing, reinstall the game.");
                return;
            }
            levelData = File.ReadAllBytes($"DAT\\{listBox1.SelectedIndex + 1}.nsb");
            parseNSB();
        }

        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            // display x / y coordinates.
        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            int index = listBox1.SelectedIndex;
            listBox1.SelectedIndexChanged -= listBox1_SelectedIndexChanged!;
            listBox1.Items.Clear();
            int start = checkBox1.Checked ? 32 : 1; // skip entry 0 (ocean name) in each faction group
            for (int i = 0; i < 30; i++)
            {
                listBox1.Items.Add(entries[start + i].Name);
            }
            listBox1.SelectedIndex = index;
            listBox1.SelectedIndexChanged += listBox1_SelectedIndexChanged!;
        }
    }
}
