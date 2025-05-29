using System.Text;

namespace WoWViewer
{
    public partial class MapEditorForm : Form
    {
        public MapEditorForm()
        {
            InitializeComponent();
        }
        // Load Maps Button
        private void button1_Click(object sender, EventArgs e)
        {
            parseNSB();
        }
        // this is a test method to parse the .nsb map files and log the results to a text file
        public void parseNSB()
        {
            for (int i = 1; i < 31; i++)
            {
                string inputPath = $"DAT\\{i}.nsb";
                string outputPath = $"DAT\\{i}.nsb.txt";
                byte[] data = File.ReadAllBytes(inputPath);
                using (StreamWriter log = new StreamWriter(outputPath, false, Encoding.UTF8))
                {
                    int offset = 8; // skip the 8-byte header
                    int index = 0;
                    while (offset + 4 <= data.Length)
                    {
                        uint block = BitConverter.ToUInt32(data, offset);
                        log.WriteLine($"[{index:D4}] Offset {offset:X4}: Value = 0x{block:X8} ({block})");
                        offset += 4;
                        index++;
                    }
                    log.WriteLine($"Total 4-byte blocks: {index}");
                }
            }
            MessageBox.Show("NSB Parsing complete. Check the DAT folder for the output files.", "Parsing Complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }
}
