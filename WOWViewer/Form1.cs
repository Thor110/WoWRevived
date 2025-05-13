using System.Text;

namespace WOWViewer
{
    public partial class WOWViewer : Form
    {
        private string filePath = string.Empty;
        private string outputPath = string.Empty; // temp testing
        private int fileCount = 0;
        private List<WowFileEntry> entries = new List<WowFileEntry>();
        public WOWViewer()
        {
            InitializeComponent();
        }
        // open file
        private void button1_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.InitialDirectory = "C:\\Program Files (x86)\\Jeff Wayne's 'The War Of The Worlds'";
                openFileDialog.Filter = "WoW Container (*.wow)|*.wow|All Files (*.*)|*.*";
                openFileDialog.FilterIndex = 1;
                openFileDialog.RestoreDirectory = true;
                openFileDialog.Title = "Select a Container (.wow) file";

                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    filePath = openFileDialog.FileName;
                    textBox1.Text = filePath;
                    parseFileCount();
                }
            }
        }
        // extract selected file
        private void button2_Click(object sender, EventArgs e)
        {
            if (listBox1.SelectedIndex == -1)
            {
                MessageBox.Show("Please select a file from the list.");
                return;
            }

            WowFileEntry selected = entries[listBox1.SelectedIndex];
            ExtractFile(selected);
            MessageBox.Show($"Extracted: {selected.Name}.wav");
        }
        // select file output folder
        private void button3_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog folderBrowserDialog = new FolderBrowserDialog();
            if (folderBrowserDialog.ShowDialog() == DialogResult.OK)
            {
                outputPath = folderBrowserDialog.SelectedPath;
                if (!outputPath.EndsWith("\\")) // If Not Root Directory
                {
                    outputPath += "\\"; // Complete Directory String
                }
                textBox2.Text = outputPath;
                button2.Enabled = true; // Enable extract button
                button4.Enabled = true; // Enable extract all button
            }
        }
        // extract all files
        private void button4_Click(object sender, EventArgs e)
        {
            foreach (var entry in entries)
            {
                ExtractFile(entry);
            }
            MessageBox.Show("All files extracted successfully.");
        }
        private void parseFileCount()
        {
            using (BinaryReader br = new BinaryReader(File.OpenRead(filePath)))
            {
                if (!ReadHeader(br))
                {
                    MessageBox.Show("Invalid or unsupported file.");
                    return;
                }
            }
        }
        private bool ReadHeader(BinaryReader br)
        {
            string magic = new string(br.ReadChars(4));
            if (magic != "KAT!" && magic != "SfxL")
                return false;

            fileCount = br.ReadInt32();
            label1.Text = "File Count : " + fileCount.ToString();

            entries.Clear();

            if (magic == "KAT!")
            {
                label2.Text = "Data or Maps";
                // files are stored differently in these files
            }
            else if (magic == "SfxL")
            {
                label2.Text = "Container Type : " + "Sound Effects Library";
                // filenames are up to 8 bytes in these file
                for (int i = 0; i < fileCount; i++)
                {
                    byte[] nameBytes = br.ReadBytes(8); // filename (ASCII padded)
                    int length = br.ReadInt32();        // likely the size
                    int offset = br.ReadInt32();        // likely the offset

                    string name = Encoding.ASCII.GetString(nameBytes).TrimEnd('\0');
                    //listBox1.Items.Add($"{name} - Offset: {offset} (0x{offset:X}), Size: {length} bytes");
                    listBox1.Items.Add($"{name}");

                    entries.Add(new WowFileEntry { Name = name, Length = length, Offset = offset });
                }
            }

            return true;
        }
        public static byte[] CreateWavHeader(int dataSize, int sampleRate = 22050, short bitsPerSample = 16, short channels = 1)
        {
            int byteRate = sampleRate * channels * bitsPerSample / 8;
            short blockAlign = (short)(channels * bitsPerSample / 8);
            int chunkSize = 36 + dataSize;

            using (MemoryStream ms = new MemoryStream(44))
            using (BinaryWriter bw = new BinaryWriter(ms))
            {
                bw.Write(Encoding.ASCII.GetBytes("RIFF"));
                bw.Write(chunkSize);
                bw.Write(Encoding.ASCII.GetBytes("WAVE"));
                bw.Write(Encoding.ASCII.GetBytes("fmt "));
                bw.Write(16); // PCM
                bw.Write((short)1); // format = PCM
                bw.Write(channels);
                bw.Write(sampleRate);
                bw.Write(byteRate);
                bw.Write(blockAlign);
                bw.Write(bitsPerSample);
                bw.Write(Encoding.ASCII.GetBytes("data"));
                bw.Write(dataSize);
                return ms.ToArray();
            }
        }
        private void ExtractFile(WowFileEntry entry)
        {
            using (BinaryReader br = new BinaryReader(File.OpenRead(filePath)))
            {
                br.BaseStream.Seek(entry.Offset, SeekOrigin.Begin);
                byte[] rawData = br.ReadBytes(entry.Length);

                byte[] wavHeader = CreateWavHeader(entry.Length);
                string outputFilePath = Path.Combine(outputPath, entry.Name + ".wav");

                using (FileStream fs = new FileStream(outputFilePath, FileMode.Create, FileAccess.Write))
                {
                    fs.Write(wavHeader, 0, wavHeader.Length);
                    fs.Write(rawData, 0, rawData.Length);
                }
            }
        }
    }
}
