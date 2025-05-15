using System.Text;

namespace WOWViewer
{
    public partial class SaveEditorForm : Form
    {
        private bool saveChanging;
        private WowSaveEntry selectedSave = new WowSaveEntry();
        public SaveEditorForm()
        {
            InitializeComponent();
            InitializeSaveLoader();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, typeof(Label));
        }
        // initialize save loader and count saves
        private void InitializeSaveLoader() { for (int i = 1; i <= 5; i++) { SaveLoader("Human", i); } for (int i = 1; i <= 5; i++) { SaveLoader("Martian", i); } }
        // save loader
        private void SaveLoader(string type, int number) { if (File.Exists("SaveGame\\" + $"{type}.00{number}")) { listBox1.Items.Add($"{type}.00{number}"); } }
        // save file button
        private void button1_Click(object sender, EventArgs e)
        {
            string fileName = fileSafetyCheck(); // check the file still exists
            if (fileName == null) { return; } // check if the file exists
            using (var fs = new FileStream(fileName, FileMode.Open, FileAccess.Write))
            {
                // only write values that have changed
                if (textBox1.Text != selectedSave.Name)
                {
                    byte[] nameBytes = new byte[36];
                    byte[] newNameBytes = Encoding.ASCII.GetBytes(textBox1.Text);
                    Array.Copy(newNameBytes, nameBytes, Math.Min(36, newNameBytes.Length));
                    fs.Seek(0x0C, SeekOrigin.Begin);
                    fs.Write(nameBytes, 0, 36);
                    selectedSave.Name = textBox1.Text; // update the selected save object
                }
                if (dateTimePicker1.Value != selectedSave.dateTime)
                {
                    DateTime dt = dateTimePicker1.Value;
                    float totalHours = dt.Hour + (dt.Minute / 60f) + (dt.Second / 3600f);
                    float tickFloat = totalHours * 20.055f;
                    byte[] timeBytes = BitConverter.GetBytes(tickFloat);
                    fs.Seek(0x4C, SeekOrigin.Begin);
                    fs.Write(timeBytes, 0, 4);
                    selectedSave.dateTime = dt; // update the selected save object
                }
            }
            MessageBox.Show("Save Game Updated!");
            button1.Enabled = false; // disable the save button
        }
        // save selected in list box
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (listBox1.SelectedItem == null) { return; } // prevents the list box from triggering twice when a save file is deleted while the program is open
            saveChanging = true; // prevents the text box from triggering text changed event when switching saves
            parseSaveFile();
        }
        private void parseSaveFile()
        {
            string fileName = fileSafetyCheck(); // check the file still exists
            if (fileName == null) { return; } // check if the file exists
            using (var br = new BinaryReader(File.OpenRead(fileName)))
            {
                br.BaseStream.Seek(0x0C, SeekOrigin.Begin); // start after "....GAME(..."
                byte[] nameBytes = br.ReadBytes(36);
                string saveName = Encoding.ASCII.GetString(nameBytes).TrimEnd('\0');
                textBox1.Text = saveName;
                // update selectedSave object
                selectedSave.Name = saveName;
                // current date and time
                br.BaseStream.Seek(0x4C, SeekOrigin.Begin);
                byte[] timeBytes = br.ReadBytes(4);
                float tickFloat = BitConverter.ToSingle(timeBytes, 0);
                float totalHours = tickFloat / 20.055f;
                int hours = (int)totalHours;
                float fractionalHour = totalHours - hours;
                int minutes = (int)(fractionalHour * 60);
                int seconds = (int)((fractionalHour * 60 - minutes) * 60);
                br.BaseStream.Seek(0x5A, SeekOrigin.Begin);
                ushort day = BitConverter.ToUInt16(br.ReadBytes(2), 0);
                day += 1; // update to account for zero-based indexing
                ushort month = BitConverter.ToUInt16(br.ReadBytes(2), 0);
                month += 1; // update to account for zero-based indexing
                ushort year = BitConverter.ToUInt16(br.ReadBytes(2), 0);
                selectedSave.dateTime = new DateTime(year, month, day, hours, minutes, seconds);
                dateTimePicker1.Value = selectedSave.dateTime;
            }
            if (listBox1.SelectedItem!.ToString()!.Contains("Human")) { dateTimePicker1.MinDate = new DateTime(1898, 9, 7, 0, 0, 0); } // human response date
            else { dateTimePicker1.MinDate = new DateTime(1898, 9, 1, 0, 0, 0); } // martian invasion date
            textBox1.Enabled = true; // enable the text box
            dateTimePicker1.Enabled = true; // enable the date picker
            checkBox1.Enabled = true; // enable the checkbox
            checkBox1.CheckedChanged -= checkBox1_CheckedChanged!; // remove event handler to prevent triggering when setting the checkbox to default
            checkBox1.Checked = false; // reset to default
            checkBox1.CheckedChanged += checkBox1_CheckedChanged!; // add event handler back
            button1.Enabled = false; // disables the save button when switching saves
            label3.Text = "Status : No Changes Made"; // update the status label
            saveChanging = false; // selected save file has been changed
        }
        private string fileSafetyCheck()
        {
            string fileName = $"SaveGame\\{listBox1.SelectedItem}";
            if (!File.Exists(fileName))
            {
                MessageBox.Show("Where did the file go?");
                listBox1.Items.Remove(listBox1.SelectedItem!);
                textBox1.Text = ""; // clear the textbox
                textBox1.Enabled = false; // disable the text box
                dateTimePicker1.Enabled = false; // disables the date picker
                checkBox1.Enabled = false; // enable the checkbox
                return null!;
            }
            return fileName;
        }
        // AnyControlChanged handles when controls are changed that do not need extra logic such as the override date limit checkbox
        // save name updated
        // current date updated
        private void AnyControlChanged(object sender, EventArgs e) { compareSaveValues(); }
        // override date limit
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            compareSaveValues();
            dateTimePicker1.MinDate = checkBox1.Checked ? new DateTime(1753, 1, 1, 0, 0, 0, 0) : selectedSave.dateTime; // set the min date to 01/01/1753 if checked
        }
        private void compareSaveValues()
        {
            if (saveChanging) { return; } // prevents text box from triggering text changed event when switching saves
            if (dateTimePicker1.Value != selectedSave.dateTime
                || textBox1.Text != selectedSave.Name
                )
            {
                button1.Enabled = true; // enable the save button
                label3.Text = "Status : Changes Made";
            }
            else
            {
                button1.Enabled = false; // disable the save button
                label3.Text = "Status : No Changes Made";
            }
        }
    }
}
