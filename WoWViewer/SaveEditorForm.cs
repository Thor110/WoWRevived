using System.Text;

namespace WOWViewer
{
    public partial class SaveEditorForm : Form
    {
        private const int NAME_OFFSET = 0x0C;
        private const int TIME_OFFSET = 0x4C;
        private const int DATE_OFFSET = 0x5A;
        //private DateTime HUMAN_RESPONSE = new DateTime(1898, 9, 7); // human response date // not used due to swap save file ability
        private DateTime MARTIAN_INVASION = new DateTime(1898, 9, 1); // martian invasion date
        // MARTIAN_INVASION is used as the default lower bound for the date time picker unless overridden or mismatching
        private DateTime DATE_LIMIT = new DateTime(1753, 1, 1); // date limit
        private bool saveChanging; // is save changing state
        private string fileName = string.Empty; // selected file name
        private WowSaveEntry selectedSave = new WowSaveEntry(); // selected save file settings
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
            if (!fileSafetyCheck()) { return; }
            using (var fs = new FileStream(fileName, FileMode.Open, FileAccess.Write))
            {
                // only write values that have changed
                if (textBox1.Text != selectedSave.Name)
                {
                    byte[] nameBytes = new byte[36];
                    byte[] newNameBytes = Encoding.ASCII.GetBytes(textBox1.Text);
                    Array.Copy(newNameBytes, nameBytes, Math.Min(36, newNameBytes.Length));
                    fs.Seek(NAME_OFFSET, SeekOrigin.Begin);
                    fs.Write(nameBytes, 0, 36);
                    selectedSave.Name = textBox1.Text; // update the selected save object
                }
                if (dateTimePicker1.Value != selectedSave.dateTime)
                {
                    DateTime dt = dateTimePicker1.Value;
                    float totalHours = dt.Hour + (dt.Minute / 60f) + (dt.Second / 3600f);
                    float tickFloat = totalHours * 20.055f;
                    byte[] timeBytes = BitConverter.GetBytes(tickFloat);
                    fs.Seek(TIME_OFFSET, SeekOrigin.Begin);
                    fs.Write(timeBytes, 0, 4);
                    ushort day = (ushort)(dt.Day - 1); // zero-based indexing
                    ushort month = (ushort)(dt.Month - 1); // zero-based indexing
                    ushort year = (ushort)(dt.Year);
                    byte[] dayBytes = BitConverter.GetBytes(day);
                    byte[] monthBytes = BitConverter.GetBytes(month);
                    byte[] yearBytes = BitConverter.GetBytes(year);
                    fs.Seek(DATE_OFFSET, SeekOrigin.Begin);
                    fs.Write(dayBytes, 0, 2);
                    fs.Write(monthBytes, 0, 2);
                    fs.Write(yearBytes, 0, 2);
                    selectedSave.dateTime = dt; // update the selected save object
                }
            }
            MessageBox.Show("Save Game Updated!");
            label3.Text = "Status : Changes Saved"; // update the status label
            button1.Enabled = false; // disable the save button
        }
        // save selected in list box
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (listBox1.SelectedItem == null) { return; } // prevents the list box from triggering twice when a save file is deleted while the program is open
            saveChanging = true; // prevents the text box from triggering text changed event when switching saves
            button2.Enabled = true; // enable the swap sides button
            button3.Enabled = true; // enable the delete save button
            parseSaveFile();
            listBox2.Items.Clear(); // clear the list box
            parseText(); // parse the text file for debugging purposes
        }
        // parse the save file
        private void parseSaveFile()
        {
            if (!fileSafetyCheck()) { return; }
            using (var br = new BinaryReader(File.OpenRead(fileName)))
            {
                br.BaseStream.Seek(NAME_OFFSET, SeekOrigin.Begin); // start after "....GAME(..."
                byte[] nameBytes = br.ReadBytes(36);
                string saveName = Encoding.ASCII.GetString(nameBytes).Split((char)0x00)[0]; // split on no character
                textBox1.Text = saveName;
                // update selectedSave object
                selectedSave.Name = saveName;
                // current date and time
                br.BaseStream.Seek(TIME_OFFSET, SeekOrigin.Begin);
                byte[] timeBytes = br.ReadBytes(4);
                float tickFloat = BitConverter.ToSingle(timeBytes, 0);
                float totalHours = tickFloat / 20.055f;
                int hours = (int)totalHours;
                float fractionalHour = totalHours - hours;
                int minutes = (int)(fractionalHour * 60);
                int seconds = (int)((fractionalHour * 60 - minutes) * 60);
                br.BaseStream.Seek(DATE_OFFSET, SeekOrigin.Begin);
                ushort day = BitConverter.ToUInt16(br.ReadBytes(2), 0);
                day += 1; // update to account for zero-based indexing
                ushort month = BitConverter.ToUInt16(br.ReadBytes(2), 0);
                month += 1; // update to account for zero-based indexing
                ushort year = BitConverter.ToUInt16(br.ReadBytes(2), 0);
                selectedSave.dateTime = new DateTime(year, month, day, hours, minutes, seconds);
            }
            minimumDateCheck(selectedSave.dateTime); // check if the date is within the minimum date range
            dateTimePicker1.Value = selectedSave.dateTime; // set value after the date check
            textBox1.Enabled = true; // enable the text box
            dateTimePicker1.Enabled = true; // enable the date picker
            checkBox1.Enabled = true; // enable the checkbox
            button1.Enabled = false; // disables the save button when switching saves
            label3.Text = "Status : No Changes Made"; // update the status label
            saveChanging = false; // selected save file has been changed
        }
        // double check the save file exists incase deleted by the user while the program is open
        private bool fileSafetyCheck()
        {
            fileName = $"SaveGame\\{listBox1.SelectedItem}"; // update fileName for reading and writing
            if (!File.Exists(fileName))
            {
                MessageBox.Show("Where did the file go?"); // user deleted the file while the program was open
                listBox1.Items.Remove(listBox1.SelectedItem!); // remove the file from the list box
                textBox1.Text = ""; // clear the textbox
                textBox1.Enabled = false; // disable the text box
                dateTimePicker1.Enabled = false; // disables the date picker
                checkBox1.Enabled = false; // enable the checkbox
                return false; // return false if file doesn't exist after disabling UI elements
            }
            return true; // return true if the file still exists
        }
        // AnyControlChanged handles when controls are changed that do not need extra logic such as the override date limit checkbox
        // save name updated
        // current date updated
        private void AnyControlChanged(object sender, EventArgs e)
        {
            if (saveChanging) { return; } // prevents controls from triggering changed events when switching saves
            compareSaveValues();
        }
        // override date limit
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            if (saveChanging) { return; } // prevents checkbox from triggering changed event when switching saves
            dateTimePicker1.MinDate = checkBox1.Checked ? DATE_LIMIT : MARTIAN_INVASION;
        }
        // minimum date check
        private void minimumDateCheck(DateTime compare)
        {
            if (compare < MARTIAN_INVASION)
            {
                dateTimePicker1.MinDate = DATE_LIMIT; // set the min date to 01/01/1753 ( current default )
                checkBox1.Checked = true; // override enabled
            }
            else
            {
                dateTimePicker1.MinDate = MARTIAN_INVASION; // set the min date to the martian invasion date due to swap save file ability
                checkBox1.Checked = false; // reset to default // HUMAN_RESPONSE date is no longer used
            }
        }
        // compare save values to see if any changes have been made
        private void compareSaveValues()
        {
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
        // swap sides button ( rename Human.00# to Martian.00# and vice versa )
        private void button2_Click(object sender, EventArgs e)
        {
            if (!fileSafetyCheck()) { return; }
            DialogResult result = MessageBox.Show("Are you sure you want to swap sides on this save file?", "Swap Sides", MessageBoxButtons.YesNo);
            if (result == DialogResult.No) { return; }
            string opposite = "";
            if (fileName.Contains("Human")) { opposite = "Martian"; }
            else { opposite = "Human"; }
            for (int i = 1; i <= 5; i++)
            {
                if (!File.Exists($"SaveGame\\{opposite}.00{i}"))
                {
                    File.Move($"{fileName}", $"SaveGame\\{opposite}.00{i}");
                    reInitialize("Save Swapped!"); // reinitialize the save loader and repopulate the list box
                    return;
                }
            }
            MessageBox.Show("No space, please delete a save file on the opposing side!");
        }
        // delete save file button
        private void button3_Click(object sender, EventArgs e)
        {
            if (!fileSafetyCheck()) { return; }
            DialogResult result = MessageBox.Show("Are you sure you want to delete this save file?", "Delete Save File", MessageBoxButtons.YesNo);
            if (result == DialogResult.Yes)
            {
                File.Delete(fileName); // delete the file
                reInitialize("Save Deleted!"); // reinitialize the save loader and repopulate the list box
            }
        }
        private void reInitialize(string message)
        {
            MessageBox.Show(message);
            listBox1.Items.Clear(); // clear list box
            InitializeSaveLoader(); // reinitialize the save loader and repopulate the list box
            button2.Enabled = false; // disable the swap sides button
            button3.Enabled = false; // disable the delete save button
        }
        // parse the text file to get sector names
        public void parseText()
        {
            int offset = 0x289; // Human start
            int endOffset = 0x4EA; // Human end
            if (fileName.Contains("Martian"))
            {
                offset = endOffset; // Human end / Martian begin
                endOffset = 0x72E; // Martian end
            }
            byte[] data = File.ReadAllBytes("TEXT.ojd");
            for (int i = 0; i < 31; i++)
            {
                byte length = data[offset + 8]; // get string length // length is never more than one byte in this case
                int stringOffset = offset + 10; // get string location
                string text = Encoding.ASCII.GetString(data, stringOffset, length-1); // string length is one less than the byte length
                listBox2.Items.Add(text);
                offset += (int)length + 9; // move offset to next entry // not + 10 because length is -1
            }
        }
    }
}