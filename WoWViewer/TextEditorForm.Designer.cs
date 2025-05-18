namespace WoWViewer
{
    partial class TextEditorForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(TextEditorForm));
            button1 = new Button();
            listBox1 = new ListBox();
            label1 = new Label();
            richTextBox1 = new RichTextBox();
            radioButton1 = new RadioButton();
            radioButton2 = new RadioButton();
            radioButton3 = new RadioButton();
            radioButton4 = new RadioButton();
            button2 = new Button();
            button3 = new Button();
            button4 = new Button();
            label2 = new Label();
            SuspendLayout();
            // 
            // button1
            // 
            button1.AccessibleDescription = "This saves the updated TEXT.ojd file.";
            button1.Enabled = false;
            button1.Location = new Point(713, 415);
            button1.Name = "button1";
            button1.Size = new Size(75, 23);
            button1.TabIndex = 0;
            button1.Text = "Save File";
            button1.UseVisualStyleBackColor = true;
            button1.Click += button1_Click;
            // 
            // listBox1
            // 
            listBox1.FormattingEnabled = true;
            listBox1.ItemHeight = 15;
            listBox1.Location = new Point(12, 12);
            listBox1.Name = "listBox1";
            listBox1.Size = new Size(336, 424);
            listBox1.TabIndex = 1;
            listBox1.SelectedIndexChanged += listBox1_SelectedIndexChanged;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(363, 29);
            label1.Name = "label1";
            label1.Size = new Size(91, 15);
            label1.TabIndex = 3;
            label1.Text = "Selected String :";
            // 
            // richTextBox1
            // 
            richTextBox1.AcceptsTab = true;
            richTextBox1.AccessibleDescription = "Enter the desired text here and press enter!";
            richTextBox1.Enabled = false;
            richTextBox1.Location = new Point(460, 26);
            richTextBox1.MaxLength = 65535;
            richTextBox1.Name = "richTextBox1";
            richTextBox1.Size = new Size(328, 383);
            richTextBox1.TabIndex = 4;
            richTextBox1.Text = "Select a string.";
            richTextBox1.KeyDown += richTextBox1_KeyDown;
            // 
            // radioButton1
            // 
            radioButton1.AccessibleDescription = "Filter by Martian string values.";
            radioButton1.AutoSize = true;
            radioButton1.Location = new Point(360, 75);
            radioButton1.Name = "radioButton1";
            radioButton1.Size = new Size(66, 19);
            radioButton1.TabIndex = 5;
            radioButton1.Text = "Martian";
            radioButton1.UseVisualStyleBackColor = true;
            radioButton1.CheckedChanged += radioButton1_CheckedChanged;
            // 
            // radioButton2
            // 
            radioButton2.AccessibleDescription = "Filter by Human string values.";
            radioButton2.AutoSize = true;
            radioButton2.Location = new Point(360, 100);
            radioButton2.Name = "radioButton2";
            radioButton2.Size = new Size(65, 19);
            radioButton2.TabIndex = 6;
            radioButton2.Text = "Human";
            radioButton2.UseVisualStyleBackColor = true;
            radioButton2.CheckedChanged += radioButton2_CheckedChanged;
            // 
            // radioButton3
            // 
            radioButton3.AccessibleDescription = "Filter by UI (user interface) string values.";
            radioButton3.AutoSize = true;
            radioButton3.Location = new Point(360, 125);
            radioButton3.Name = "radioButton3";
            radioButton3.Size = new Size(36, 19);
            radioButton3.TabIndex = 7;
            radioButton3.Text = "UI";
            radioButton3.UseVisualStyleBackColor = true;
            radioButton3.CheckedChanged += radioButton3_CheckedChanged;
            // 
            // radioButton4
            // 
            radioButton4.AccessibleDescription = "Show all strings.";
            radioButton4.AutoSize = true;
            radioButton4.Checked = true;
            radioButton4.Location = new Point(360, 150);
            radioButton4.Name = "radioButton4";
            radioButton4.Size = new Size(71, 19);
            radioButton4.TabIndex = 8;
            radioButton4.TabStop = true;
            radioButton4.Text = "Show All";
            radioButton4.UseVisualStyleBackColor = true;
            radioButton4.CheckedChanged += radioButton4_CheckedChanged;
            // 
            // button2
            // 
            button2.AccessibleDescription = "Exports the strings to a text file.";
            button2.Location = new Point(632, 415);
            button2.Name = "button2";
            button2.Size = new Size(75, 23);
            button2.TabIndex = 9;
            button2.Text = "Export";
            button2.UseVisualStyleBackColor = true;
            button2.Click += button2_Click;
            // 
            // button3
            // 
            button3.AccessibleDescription = "Import the strings from a text file.";
            button3.Location = new Point(551, 415);
            button3.Name = "button3";
            button3.Size = new Size(75, 23);
            button3.TabIndex = 10;
            button3.Text = "Import";
            button3.UseVisualStyleBackColor = true;
            button3.Click += button3_Click;
            // 
            // button4
            // 
            button4.AccessibleDescription = "Undo changes to the selected string.";
            button4.Enabled = false;
            button4.Location = new Point(379, 47);
            button4.Name = "button4";
            button4.Size = new Size(75, 23);
            button4.TabIndex = 11;
            button4.Text = "Undo";
            button4.UseVisualStyleBackColor = true;
            button4.Click += button4_Click;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(399, 419);
            label2.Name = "label2";
            label2.Size = new Size(146, 15);
            label2.TabIndex = 12;
            label2.Text = "Status : No Changes Made";
            // 
            // TextEditorForm
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 450);
            Controls.Add(label2);
            Controls.Add(button4);
            Controls.Add(button3);
            Controls.Add(button2);
            Controls.Add(radioButton4);
            Controls.Add(radioButton3);
            Controls.Add(radioButton2);
            Controls.Add(radioButton1);
            Controls.Add(richTextBox1);
            Controls.Add(label1);
            Controls.Add(listBox1);
            Controls.Add(button1);
            Icon = (Icon)resources.GetObject("$this.Icon");
            Name = "TextEditorForm";
            Text = "Text Editor";
            FormClosing += TextEditorForm_FormClosing;
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Button button1;
        private ListBox listBox1;
        private Label label1;
        private RichTextBox richTextBox1;
        private RadioButton radioButton1;
        private RadioButton radioButton2;
        private RadioButton radioButton3;
        private RadioButton radioButton4;
        private Button button2;
        private Button button3;
        private Button button4;
        private Label label2;
    }
}