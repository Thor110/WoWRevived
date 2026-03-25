namespace WoWViewer
{
    partial class SprViewer
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
            pictureBox1 = new PictureBox();
            listBox1 = new ListBox();
            listBox2 = new ListBox();
            label1 = new Label();
            button1 = new Button();
            button2 = new Button();
            button3 = new Button();
            textBox1 = new TextBox();
            button4 = new Button();
            comboBox1 = new ComboBox();
            label2 = new Label();
            button5 = new Button();
            listBox3 = new ListBox();
            label3 = new Label();
            checkBox1 = new CheckBox();
            button6 = new Button();
            ((System.ComponentModel.ISupportInitialize)pictureBox1).BeginInit();
            SuspendLayout();
            // 
            // pictureBox1
            // 
            pictureBox1.Location = new Point(612, 12);
            pictureBox1.Name = "pictureBox1";
            pictureBox1.Size = new Size(640, 480);
            pictureBox1.TabIndex = 0;
            pictureBox1.TabStop = false;
            // 
            // listBox1
            // 
            listBox1.FormattingEnabled = true;
            listBox1.ItemHeight = 15;
            listBox1.Location = new Point(12, 12);
            listBox1.Name = "listBox1";
            listBox1.Size = new Size(139, 754);
            listBox1.TabIndex = 1;
            listBox1.SelectedIndexChanged += listBox1_SelectedIndexChanged;
            // 
            // listBox2
            // 
            listBox2.FormattingEnabled = true;
            listBox2.ItemHeight = 15;
            listBox2.Location = new Point(157, 49);
            listBox2.Name = "listBox2";
            listBox2.Size = new Size(120, 244);
            listBox2.TabIndex = 2;
            listBox2.SelectedIndexChanged += listBox2_SelectedIndexChanged;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(157, 31);
            label1.Name = "label1";
            label1.Size = new Size(69, 15);
            label1.TabIndex = 3;
            label1.Text = "Palette Files";
            // 
            // button1
            // 
            button1.Enabled = false;
            button1.Location = new Point(612, 498);
            button1.Name = "button1";
            button1.Size = new Size(75, 23);
            button1.TabIndex = 5;
            button1.Text = "Replace";
            button1.UseVisualStyleBackColor = true;
            button1.Click += button1_Click;
            // 
            // button2
            // 
            button2.Enabled = false;
            button2.Location = new Point(531, 469);
            button2.Name = "button2";
            button2.Size = new Size(75, 23);
            button2.TabIndex = 6;
            button2.Text = "Export";
            button2.UseVisualStyleBackColor = true;
            button2.Click += button2_Click;
            // 
            // button3
            // 
            button3.Enabled = false;
            button3.Location = new Point(157, 712);
            button3.Name = "button3";
            button3.Size = new Size(75, 23);
            button3.TabIndex = 7;
            button3.Text = "Export All";
            button3.UseVisualStyleBackColor = true;
            button3.Click += button3_Click;
            // 
            // textBox1
            // 
            textBox1.Enabled = false;
            textBox1.Location = new Point(238, 742);
            textBox1.Name = "textBox1";
            textBox1.Size = new Size(1014, 23);
            textBox1.TabIndex = 8;
            // 
            // button4
            // 
            button4.Location = new Point(157, 741);
            button4.Name = "button4";
            button4.Size = new Size(75, 23);
            button4.TabIndex = 9;
            button4.Text = "Output";
            button4.UseVisualStyleBackColor = true;
            button4.Click += button4_Click;
            // 
            // comboBox1
            // 
            comboBox1.Enabled = false;
            comboBox1.FormattingEnabled = true;
            comboBox1.Location = new Point(693, 498);
            comboBox1.Name = "comboBox1";
            comboBox1.Size = new Size(179, 23);
            comboBox1.TabIndex = 11;
            comboBox1.SelectedIndexChanged += comboBox1_SelectedIndexChanged;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(878, 502);
            label2.Name = "label2";
            label2.Size = new Size(144, 15);
            label2.TabIndex = 12;
            label2.Text = "Sprite Information Display";
            // 
            // button5
            // 
            button5.Enabled = false;
            button5.Location = new Point(157, 299);
            button5.Name = "button5";
            button5.Size = new Size(120, 23);
            button5.TabIndex = 13;
            button5.Text = "Export Palette";
            button5.UseVisualStyleBackColor = true;
            button5.Click += button5_Click;
            // 
            // listBox3
            // 
            listBox3.FormattingEnabled = true;
            listBox3.ItemHeight = 15;
            listBox3.Location = new Point(283, 49);
            listBox3.Name = "listBox3";
            listBox3.Size = new Size(120, 244);
            listBox3.TabIndex = 15;
            listBox3.SelectedIndexChanged += listBox3_SelectedIndexChanged;
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(283, 31);
            label3.Name = "label3";
            label3.Size = new Size(78, 15);
            label3.TabIndex = 16;
            label3.Text = "Shader Tables";
            // 
            // checkBox1
            // 
            checkBox1.AutoSize = true;
            checkBox1.Checked = true;
            checkBox1.CheckState = CheckState.Checked;
            checkBox1.Location = new Point(283, 302);
            checkBox1.Name = "checkBox1";
            checkBox1.Size = new Size(97, 19);
            checkBox1.TabIndex = 14;
            checkBox1.Text = "Shader Tables";
            checkBox1.UseVisualStyleBackColor = true;
            checkBox1.CheckedChanged += checkBox1_CheckedChanged;
            // 
            // button6
            // 
            button6.Enabled = false;
            button6.Location = new Point(531, 527);
            button6.Name = "button6";
            button6.Size = new Size(156, 23);
            button6.TabIndex = 17;
            button6.Text = "Replace All Frames";
            button6.UseVisualStyleBackColor = true;
            button6.Click += button6_Click;
            // 
            // SprViewer
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(1264, 777);
            Controls.Add(button6);
            Controls.Add(label3);
            Controls.Add(listBox3);
            Controls.Add(checkBox1);
            Controls.Add(button5);
            Controls.Add(label2);
            Controls.Add(comboBox1);
            Controls.Add(button4);
            Controls.Add(textBox1);
            Controls.Add(button3);
            Controls.Add(button2);
            Controls.Add(button1);
            Controls.Add(label1);
            Controls.Add(listBox2);
            Controls.Add(listBox1);
            Controls.Add(pictureBox1);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            Name = "SprViewer";
            Text = "SprViewer";
            ((System.ComponentModel.ISupportInitialize)pictureBox1).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private PictureBox pictureBox1;
        private ListBox listBox1;
        private ListBox listBox2;
        private Label label1;
        private Button button1;
        private Button button2;
        private Button button3;
        private TextBox textBox1;
        private Button button4;
        private ComboBox comboBox1;
        private Label label2;
        private Button button5;
        private ListBox listBox3;
        private Label label3;
        private CheckBox checkBox1;
        private Button button6;
    }
}