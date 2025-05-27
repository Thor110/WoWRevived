namespace WOWViewer
{
    partial class WOWViewer
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
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
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(WOWViewer));
            button1 = new Button();
            textBox1 = new TextBox();
            listBox1 = new ListBox();
            label2 = new Label();
            button2 = new Button();
            button3 = new Button();
            textBox2 = new TextBox();
            button4 = new Button();
            label3 = new Label();
            label4 = new Label();
            button5 = new Button();
            button6 = new Button();
            pictureBox1 = new PictureBox();
            label1 = new Label();
            label5 = new Label();
            button7 = new Button();
            button8 = new Button();
            button9 = new Button();
            button10 = new Button();
            button11 = new Button();
            ((System.ComponentModel.ISupportInitialize)pictureBox1).BeginInit();
            SuspendLayout();
            // 
            // button1
            // 
            button1.AccessibleDescription = "Select a .wow file to open.";
            button1.Location = new Point(12, 12);
            button1.Name = "button1";
            button1.Size = new Size(75, 23);
            button1.TabIndex = 0;
            button1.Text = "Open File";
            button1.UseVisualStyleBackColor = true;
            button1.Click += button1_Click;
            // 
            // textBox1
            // 
            textBox1.Enabled = false;
            textBox1.Location = new Point(93, 13);
            textBox1.Name = "textBox1";
            textBox1.Size = new Size(525, 23);
            textBox1.TabIndex = 2;
            // 
            // listBox1
            // 
            listBox1.FormattingEnabled = true;
            listBox1.ItemHeight = 15;
            listBox1.Location = new Point(93, 42);
            listBox1.Name = "listBox1";
            listBox1.Size = new Size(365, 199);
            listBox1.TabIndex = 3;
            listBox1.SelectedIndexChanged += listBox1_SelectedIndexChanged;
            listBox1.MouseDoubleClick += listBox1_MouseDoubleClick;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(93, 259);
            label2.Name = "label2";
            label2.Size = new Size(92, 15);
            label2.TabIndex = 5;
            label2.Text = "Container Type :";
            // 
            // button2
            // 
            button2.AccessibleDescription = "Extract selected file.";
            button2.Enabled = false;
            button2.Location = new Point(383, 247);
            button2.Name = "button2";
            button2.Size = new Size(75, 23);
            button2.TabIndex = 6;
            button2.Text = "Extract";
            button2.UseVisualStyleBackColor = true;
            button2.Click += button2_Click;
            // 
            // button3
            // 
            button3.AccessibleDescription = "Choose a destination to output files to.";
            button3.Location = new Point(12, 286);
            button3.Name = "button3";
            button3.Size = new Size(75, 23);
            button3.TabIndex = 7;
            button3.Text = "Output";
            button3.UseVisualStyleBackColor = true;
            button3.Click += button3_Click;
            // 
            // textBox2
            // 
            textBox2.Enabled = false;
            textBox2.Location = new Point(93, 286);
            textBox2.Name = "textBox2";
            textBox2.Size = new Size(527, 23);
            textBox2.TabIndex = 8;
            textBox2.Text = "Please select an output path before files can be extracted.";
            // 
            // button4
            // 
            button4.AccessibleDescription = "Extract all files.";
            button4.Enabled = false;
            button4.Location = new Point(12, 315);
            button4.Name = "button4";
            button4.Size = new Size(75, 23);
            button4.TabIndex = 9;
            button4.Text = "Extract All";
            button4.UseVisualStyleBackColor = true;
            button4.Click += button4_Click;
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(464, 42);
            label3.Name = "label3";
            label3.Size = new Size(54, 15);
            label3.TabIndex = 10;
            label3.Text = "File Size :";
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.Location = new Point(464, 57);
            label4.Name = "label4";
            label4.Size = new Size(66, 15);
            label4.TabIndex = 11;
            label4.Text = "File Offset :";
            // 
            // button5
            // 
            button5.AccessibleDescription = "Play selected sound file.";
            button5.Enabled = false;
            button5.Location = new Point(464, 218);
            button5.Name = "button5";
            button5.Size = new Size(75, 23);
            button5.TabIndex = 12;
            button5.Text = "Play";
            button5.UseVisualStyleBackColor = true;
            button5.Visible = false;
            button5.Click += button5_Click;
            // 
            // button6
            // 
            button6.AccessibleDescription = "Stop playing the sound file.";
            button6.Enabled = false;
            button6.Location = new Point(545, 218);
            button6.Name = "button6";
            button6.Size = new Size(75, 23);
            button6.TabIndex = 13;
            button6.Text = "Stop";
            button6.UseVisualStyleBackColor = true;
            button6.Visible = false;
            button6.Click += button6_Click;
            // 
            // pictureBox1
            // 
            pictureBox1.Location = new Point(464, 75);
            pictureBox1.Name = "pictureBox1";
            pictureBox1.Size = new Size(156, 137);
            pictureBox1.TabIndex = 14;
            pictureBox1.TabStop = false;
            pictureBox1.Visible = false;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(93, 244);
            label1.Name = "label1";
            label1.Size = new Size(67, 15);
            label1.TabIndex = 4;
            label1.Text = "File Count :";
            // 
            // label5
            // 
            label5.AutoSize = true;
            label5.Location = new Point(464, 244);
            label5.Name = "label5";
            label5.Size = new Size(87, 15);
            label5.TabIndex = 15;
            label5.Text = "Sound Length :";
            label5.Visible = false;
            // 
            // button7
            // 
            button7.Location = new Point(464, 315);
            button7.Name = "button7";
            button7.Size = new Size(75, 23);
            button7.TabIndex = 16;
            button7.Text = "Save Editor";
            button7.UseVisualStyleBackColor = true;
            button7.Click += button7_Click;
            // 
            // button8
            // 
            button8.Location = new Point(545, 315);
            button8.Name = "button8";
            button8.Size = new Size(75, 23);
            button8.TabIndex = 17;
            button8.Text = "Map Editor";
            button8.UseVisualStyleBackColor = true;
            button8.Click += button8_Click;
            // 
            // button9
            // 
            button9.Location = new Point(383, 315);
            button9.Name = "button9";
            button9.Size = new Size(75, 23);
            button9.TabIndex = 18;
            button9.Text = "Text Editor";
            button9.UseVisualStyleBackColor = true;
            button9.Click += button9_Click;
            // 
            // button10
            // 
            button10.Location = new Point(12, 218);
            button10.Name = "button10";
            button10.Size = new Size(75, 23);
            button10.TabIndex = 19;
            button10.Text = "Replace";
            button10.UseVisualStyleBackColor = true;
            button10.Visible = false;
            button10.Click += button10_Click;
            // 
            // button11
            // 
            button11.Location = new Point(12, 189);
            button11.Name = "button11";
            button11.Size = new Size(75, 23);
            button11.TabIndex = 20;
            button11.Text = "Save File";
            button11.UseVisualStyleBackColor = true;
            button11.Visible = false;
            button11.Click += button11_Click;
            // 
            // WOWViewer
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(632, 348);
            Controls.Add(button11);
            Controls.Add(button10);
            Controls.Add(button9);
            Controls.Add(button8);
            Controls.Add(button7);
            Controls.Add(label5);
            Controls.Add(pictureBox1);
            Controls.Add(button6);
            Controls.Add(button5);
            Controls.Add(label4);
            Controls.Add(label3);
            Controls.Add(button4);
            Controls.Add(textBox2);
            Controls.Add(button3);
            Controls.Add(button2);
            Controls.Add(label2);
            Controls.Add(label1);
            Controls.Add(listBox1);
            Controls.Add(textBox1);
            Controls.Add(button1);
            Icon = (Icon)resources.GetObject("$this.Icon");
            Name = "WOWViewer";
            Text = "WoWViewer";
            ((System.ComponentModel.ISupportInitialize)pictureBox1).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Button button1;
        private TextBox textBox1;
        private ListBox listBox1;
        private Label label2;
        private Button button2;
        private Button button3;
        private TextBox textBox2;
        private Button button4;
        private Label label3;
        private Label label4;
        private Button button5;
        private Button button6;
        private PictureBox pictureBox1;
        private Label label1;
        private Label label5;
        private Button button7;
        private Button button8;
        private Button button9;
        private Button button10;
        private Button button11;
    }
}
