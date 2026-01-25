namespace WoWViewer
{
    partial class SaveEditorForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(SaveEditorForm));
            button1 = new Button();
            listBox1 = new ListBox();
            label1 = new Label();
            textBox1 = new TextBox();
            listBox2 = new ListBox();
            dateTimePicker1 = new DateTimePicker();
            label2 = new Label();
            checkBox1 = new CheckBox();
            label3 = new Label();
            button2 = new Button();
            button3 = new Button();
            numericUpDown1 = new NumericUpDown();
            checkBox2 = new CheckBox();
            listBox3 = new ListBox();
            listBox4 = new ListBox();
            label4 = new Label();
            label5 = new Label();
            numericUpDown2 = new NumericUpDown();
            numericUpDown3 = new NumericUpDown();
            numericUpDown4 = new NumericUpDown();
            label6 = new Label();
            label7 = new Label();
            label8 = new Label();
            label9 = new Label();
            numericUpDown5 = new NumericUpDown();
            label10 = new Label();
            label11 = new Label();
            numericUpDown6 = new NumericUpDown();
            textBox2 = new TextBox();
            label12 = new Label();
            ((System.ComponentModel.ISupportInitialize)numericUpDown1).BeginInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown2).BeginInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown3).BeginInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown4).BeginInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown5).BeginInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown6).BeginInit();
            SuspendLayout();
            // 
            // button1
            // 
            button1.AccessibleDescription = "Save the changes to the selected save.";
            button1.Enabled = false;
            button1.Location = new Point(469, 12);
            button1.Name = "button1";
            button1.Size = new Size(75, 23);
            button1.TabIndex = 0;
            button1.Text = "Save File";
            button1.UseVisualStyleBackColor = true;
            button1.Click += button1_Click;
            // 
            // listBox1
            // 
            listBox1.AccessibleDescription = "Only loads saves 1 through 5 no backups or alternate names.";
            listBox1.FormattingEnabled = true;
            listBox1.ItemHeight = 15;
            listBox1.Location = new Point(12, 12);
            listBox1.Name = "listBox1";
            listBox1.Size = new Size(120, 154);
            listBox1.TabIndex = 1;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(146, 15);
            label1.Name = "label1";
            label1.Size = new Size(72, 15);
            label1.TabIndex = 3;
            label1.Text = "Save Name :";
            // 
            // textBox1
            // 
            textBox1.AccessibleDescription = "Set the save name for the selected file.";
            textBox1.Enabled = false;
            textBox1.Location = new Point(224, 12);
            textBox1.MaxLength = 36;
            textBox1.Name = "textBox1";
            textBox1.Size = new Size(239, 23);
            textBox1.TabIndex = 4;
            // 
            // listBox2
            // 
            listBox2.AccessibleDescription = "This lists the sectors or areas in the game.";
            listBox2.FormattingEnabled = true;
            listBox2.ItemHeight = 15;
            listBox2.Location = new Point(138, 70);
            listBox2.Name = "listBox2";
            listBox2.Size = new Size(120, 469);
            listBox2.TabIndex = 5;
            // 
            // dateTimePicker1
            // 
            dateTimePicker1.AccessibleDescription = "Set the date and time for the selected save.";
            dateTimePicker1.CustomFormat = "dd/MM/yyyy hh:mm:ss tt";
            dateTimePicker1.Enabled = false;
            dateTimePicker1.Format = DateTimePickerFormat.Custom;
            dateTimePicker1.Location = new Point(224, 41);
            dateTimePicker1.Name = "dateTimePicker1";
            dateTimePicker1.ShowUpDown = true;
            dateTimePicker1.Size = new Size(239, 23);
            dateTimePicker1.TabIndex = 6;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(138, 47);
            label2.Name = "label2";
            label2.Size = new Size(80, 15);
            label2.TabIndex = 7;
            label2.Text = "Current Date :";
            // 
            // checkBox1
            // 
            checkBox1.AccessibleDescription = "This allows you to set the date as far back as 01/01/1753";
            checkBox1.AutoSize = true;
            checkBox1.Enabled = false;
            checkBox1.Location = new Point(469, 45);
            checkBox1.Name = "checkBox1";
            checkBox1.Size = new Size(133, 19);
            checkBox1.TabIndex = 8;
            checkBox1.Text = "Override Date Limits";
            checkBox1.UseVisualStyleBackColor = true;
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(550, 16);
            label3.Name = "label3";
            label3.Size = new Size(133, 15);
            label3.TabIndex = 9;
            label3.Text = "Status : No Save Loaded";
            // 
            // button2
            // 
            button2.AccessibleDescription = "Swap the selected save to playing the opposing side.";
            button2.Enabled = false;
            button2.Location = new Point(713, 12);
            button2.Name = "button2";
            button2.Size = new Size(75, 23);
            button2.TabIndex = 10;
            button2.Text = "Swap Sides";
            button2.UseVisualStyleBackColor = true;
            button2.Click += button2_Click;
            // 
            // button3
            // 
            button3.AccessibleDescription = "Delete the selected save file.";
            button3.Enabled = false;
            button3.Location = new Point(12, 172);
            button3.Name = "button3";
            button3.Size = new Size(75, 23);
            button3.TabIndex = 11;
            button3.Text = "Delete File";
            button3.UseVisualStyleBackColor = true;
            button3.Click += button3_Click;
            // 
            // numericUpDown1
            // 
            numericUpDown1.Enabled = false;
            numericUpDown1.Location = new Point(417, 70);
            numericUpDown1.Maximum = new decimal(new int[] { 9999, 0, 0, 0 });
            numericUpDown1.Name = "numericUpDown1";
            numericUpDown1.Size = new Size(46, 23);
            numericUpDown1.TabIndex = 12;
            // 
            // checkBox2
            // 
            checkBox2.AccessibleDescription = "Overrides the minimum year of 1753 imposed by the DateTimePicker object.";
            checkBox2.AutoSize = true;
            checkBox2.Enabled = false;
            checkBox2.Location = new Point(469, 74);
            checkBox2.Name = "checkBox2";
            checkBox2.Size = new Size(158, 19);
            checkBox2.TabIndex = 13;
            checkBox2.Text = "Force Override Year Limit";
            checkBox2.UseVisualStyleBackColor = true;
            // 
            // listBox3
            // 
            listBox3.AccessibleDescription = "This lists the buildings in the sector.";
            listBox3.FormattingEnabled = true;
            listBox3.ItemHeight = 15;
            listBox3.Location = new Point(264, 175);
            listBox3.Name = "listBox3";
            listBox3.Size = new Size(120, 364);
            listBox3.TabIndex = 14;
            // 
            // listBox4
            // 
            listBox4.AccessibleDescription = "This lists the units in the sector.";
            listBox4.FormattingEnabled = true;
            listBox4.ItemHeight = 15;
            listBox4.Location = new Point(516, 175);
            listBox4.Name = "listBox4";
            listBox4.Size = new Size(120, 364);
            listBox4.TabIndex = 15;
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.Location = new Point(264, 157);
            label4.Name = "label4";
            label4.Size = new Size(62, 15);
            label4.TabIndex = 16;
            label4.Text = "Buildings :";
            // 
            // label5
            // 
            label5.AutoSize = true;
            label5.Location = new Point(516, 157);
            label5.Name = "label5";
            label5.Size = new Size(40, 15);
            label5.TabIndex = 17;
            label5.Text = "Units :";
            // 
            // numericUpDown2
            // 
            numericUpDown2.Enabled = false;
            numericUpDown2.Location = new Point(360, 99);
            numericUpDown2.Name = "numericUpDown2";
            numericUpDown2.Size = new Size(73, 23);
            numericUpDown2.TabIndex = 18;
            // 
            // numericUpDown3
            // 
            numericUpDown3.Enabled = false;
            numericUpDown3.Location = new Point(518, 99);
            numericUpDown3.Name = "numericUpDown3";
            numericUpDown3.Size = new Size(73, 23);
            numericUpDown3.TabIndex = 19;
            // 
            // numericUpDown4
            // 
            numericUpDown4.Enabled = false;
            numericUpDown4.Location = new Point(715, 99);
            numericUpDown4.Name = "numericUpDown4";
            numericUpDown4.Size = new Size(73, 23);
            numericUpDown4.TabIndex = 20;
            // 
            // label6
            // 
            label6.Location = new Point(267, 101);
            label6.Name = "label6";
            label6.Size = new Size(87, 15);
            label6.TabIndex = 21;
            label6.Text = "Type 1 :";
            label6.TextAlign = ContentAlignment.MiddleRight;
            // 
            // label7
            // 
            label7.Location = new Point(460, 101);
            label7.Name = "label7";
            label7.Size = new Size(52, 15);
            label7.TabIndex = 22;
            label7.Text = "Type 2 :";
            label7.TextAlign = ContentAlignment.MiddleRight;
            // 
            // label8
            // 
            label8.Location = new Point(612, 101);
            label8.Name = "label8";
            label8.Size = new Size(97, 15);
            label8.TabIndex = 23;
            label8.Text = "Type 3 :";
            label8.TextAlign = ContentAlignment.MiddleRight;
            // 
            // label9
            // 
            label9.AutoSize = true;
            label9.Location = new Point(267, 72);
            label9.Name = "label9";
            label9.Size = new Size(46, 15);
            label9.TabIndex = 24;
            label9.Text = "Sector :";
            // 
            // numericUpDown5
            // 
            numericUpDown5.Enabled = false;
            numericUpDown5.Location = new Point(469, 175);
            numericUpDown5.Minimum = new decimal(new int[] { 1, 0, 0, 0 });
            numericUpDown5.Name = "numericUpDown5";
            numericUpDown5.Size = new Size(41, 23);
            numericUpDown5.TabIndex = 25;
            numericUpDown5.Value = new decimal(new int[] { 100, 0, 0, 0 });
            // 
            // label10
            // 
            label10.AutoSize = true;
            label10.Location = new Point(415, 177);
            label10.Name = "label10";
            label10.Size = new Size(48, 15);
            label10.TabIndex = 26;
            label10.Text = "Health :";
            // 
            // label11
            // 
            label11.AutoSize = true;
            label11.Location = new Point(661, 177);
            label11.Name = "label11";
            label11.Size = new Size(48, 15);
            label11.TabIndex = 28;
            label11.Text = "Health :";
            // 
            // numericUpDown6
            // 
            numericUpDown6.Enabled = false;
            numericUpDown6.Location = new Point(715, 175);
            numericUpDown6.Minimum = new decimal(new int[] { 1, 0, 0, 0 });
            numericUpDown6.Name = "numericUpDown6";
            numericUpDown6.Size = new Size(41, 23);
            numericUpDown6.TabIndex = 27;
            numericUpDown6.Value = new decimal(new int[] { 100, 0, 0, 0 });
            // 
            // textBox2
            // 
            textBox2.Location = new Point(12, 515);
            textBox2.Name = "textBox2";
            textBox2.ReadOnly = true;
            textBox2.Size = new Size(120, 23);
            textBox2.TabIndex = 29;
            // 
            // label12
            // 
            label12.AutoSize = true;
            label12.Location = new Point(12, 497);
            label12.Name = "label12";
            label12.Size = new Size(94, 15);
            label12.TabIndex = 30;
            label12.Text = "Unparsed Bytes :";
            // 
            // SaveEditorForm
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 550);
            Controls.Add(label12);
            Controls.Add(textBox2);
            Controls.Add(label11);
            Controls.Add(numericUpDown6);
            Controls.Add(label10);
            Controls.Add(numericUpDown5);
            Controls.Add(label9);
            Controls.Add(label8);
            Controls.Add(label7);
            Controls.Add(label6);
            Controls.Add(numericUpDown4);
            Controls.Add(numericUpDown3);
            Controls.Add(numericUpDown2);
            Controls.Add(label5);
            Controls.Add(label4);
            Controls.Add(listBox4);
            Controls.Add(listBox3);
            Controls.Add(checkBox2);
            Controls.Add(numericUpDown1);
            Controls.Add(button3);
            Controls.Add(button2);
            Controls.Add(label3);
            Controls.Add(checkBox1);
            Controls.Add(label2);
            Controls.Add(dateTimePicker1);
            Controls.Add(listBox2);
            Controls.Add(textBox1);
            Controls.Add(label1);
            Controls.Add(listBox1);
            Controls.Add(button1);
            Icon = (Icon)resources.GetObject("$this.Icon");
            Name = "SaveEditorForm";
            Text = "Save Editor";
            ((System.ComponentModel.ISupportInitialize)numericUpDown1).EndInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown2).EndInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown3).EndInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown4).EndInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown5).EndInit();
            ((System.ComponentModel.ISupportInitialize)numericUpDown6).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Button button1;
        private ListBox listBox1;
        private Label label1;
        private TextBox textBox1;
        private ListBox listBox2;
        private DateTimePicker dateTimePicker1;
        private Label label2;
        private CheckBox checkBox1;
        private Label label3;
        private Button button2;
        private Button button3;
        private NumericUpDown numericUpDown1;
        private CheckBox checkBox2;
        private ListBox listBox3;
        private ListBox listBox4;
        private Label label4;
        private Label label5;
        private NumericUpDown numericUpDown2;
        private NumericUpDown numericUpDown3;
        private NumericUpDown numericUpDown4;
        private Label label6;
        private Label label7;
        private Label label8;
        private Label label9;
        private NumericUpDown numericUpDown5;
        private Label label10;
        private Label label11;
        private NumericUpDown numericUpDown6;
        private TextBox textBox2;
        private Label label12;
    }
}