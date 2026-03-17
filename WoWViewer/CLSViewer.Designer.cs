namespace WoWViewer
{
    partial class CLSViewer
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
            label2 = new Label();
            button1 = new Button();
            label3 = new Label();
            listBox3 = new ListBox();
            checkBox1 = new CheckBox();
            button5 = new Button();
            button4 = new Button();
            textBox1 = new TextBox();
            button3 = new Button();
            button2 = new Button();
            label1 = new Label();
            listBox2 = new ListBox();
            listBox1 = new ListBox();
            pictureBox1 = new PictureBox();
            ((System.ComponentModel.ISupportInitialize)pictureBox1).BeginInit();
            SuspendLayout();
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(693, 501);
            label2.Name = "label2";
            label2.Size = new Size(144, 15);
            label2.TabIndex = 45;
            label2.Text = "Sprite Information Display";
            // 
            // button1
            // 
            button1.Location = new Point(612, 497);
            button1.Name = "button1";
            button1.Size = new Size(75, 23);
            button1.TabIndex = 44;
            button1.Text = "Replace";
            button1.UseVisualStyleBackColor = true;
            button1.Click += button1_Click;
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(283, 30);
            label3.Name = "label3";
            label3.Size = new Size(78, 15);
            label3.TabIndex = 43;
            label3.Text = "Shader Tables";
            // 
            // listBox3
            // 
            listBox3.FormattingEnabled = true;
            listBox3.ItemHeight = 15;
            listBox3.Location = new Point(283, 48);
            listBox3.Name = "listBox3";
            listBox3.Size = new Size(120, 244);
            listBox3.TabIndex = 42;
            listBox3.SelectedIndexChanged += listBox3_SelectedIndexChanged;
            // 
            // checkBox1
            // 
            checkBox1.AutoSize = true;
            checkBox1.Checked = true;
            checkBox1.CheckState = CheckState.Checked;
            checkBox1.Location = new Point(283, 301);
            checkBox1.Name = "checkBox1";
            checkBox1.Size = new Size(97, 19);
            checkBox1.TabIndex = 41;
            checkBox1.Text = "Shader Tables";
            checkBox1.UseVisualStyleBackColor = true;
            checkBox1.CheckedChanged += checkBox1_CheckedChanged;
            // 
            // button5
            // 
            button5.Enabled = false;
            button5.Location = new Point(157, 298);
            button5.Name = "button5";
            button5.Size = new Size(120, 23);
            button5.TabIndex = 40;
            button5.Text = "Export Palette";
            button5.UseVisualStyleBackColor = true;
            button5.Click += button5_Click;
            // 
            // button4
            // 
            button4.Location = new Point(157, 740);
            button4.Name = "button4";
            button4.Size = new Size(75, 23);
            button4.TabIndex = 39;
            button4.Text = "Output";
            button4.UseVisualStyleBackColor = true;
            button4.Click += button4_Click;
            // 
            // textBox1
            // 
            textBox1.Enabled = false;
            textBox1.Location = new Point(238, 741);
            textBox1.Name = "textBox1";
            textBox1.Size = new Size(1014, 23);
            textBox1.TabIndex = 38;
            // 
            // button3
            // 
            button3.Enabled = false;
            button3.Location = new Point(157, 711);
            button3.Name = "button3";
            button3.Size = new Size(75, 23);
            button3.TabIndex = 37;
            button3.Text = "Export All";
            button3.UseVisualStyleBackColor = true;
            button3.Click += button3_Click;
            // 
            // button2
            // 
            button2.Enabled = false;
            button2.Location = new Point(531, 468);
            button2.Name = "button2";
            button2.Size = new Size(75, 23);
            button2.TabIndex = 36;
            button2.Text = "Export";
            button2.UseVisualStyleBackColor = true;
            button2.Click += button2_Click;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(157, 30);
            label1.Name = "label1";
            label1.Size = new Size(69, 15);
            label1.TabIndex = 35;
            label1.Text = "Palette Files";
            // 
            // listBox2
            // 
            listBox2.FormattingEnabled = true;
            listBox2.ItemHeight = 15;
            listBox2.Location = new Point(157, 48);
            listBox2.Name = "listBox2";
            listBox2.Size = new Size(120, 244);
            listBox2.TabIndex = 34;
            listBox2.SelectedIndexChanged += listBox2_SelectedIndexChanged;
            // 
            // listBox1
            // 
            listBox1.FormattingEnabled = true;
            listBox1.ItemHeight = 15;
            listBox1.Location = new Point(12, 11);
            listBox1.Name = "listBox1";
            listBox1.Size = new Size(139, 754);
            listBox1.TabIndex = 33;
            listBox1.SelectedIndexChanged += listBox1_SelectedIndexChanged;
            // 
            // pictureBox1
            // 
            pictureBox1.Location = new Point(612, 11);
            pictureBox1.Name = "pictureBox1";
            pictureBox1.Size = new Size(640, 480);
            pictureBox1.TabIndex = 32;
            pictureBox1.TabStop = false;
            // 
            // CLSViewer
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(1264, 777);
            Controls.Add(label2);
            Controls.Add(button1);
            Controls.Add(label3);
            Controls.Add(listBox3);
            Controls.Add(checkBox1);
            Controls.Add(button5);
            Controls.Add(button4);
            Controls.Add(textBox1);
            Controls.Add(button3);
            Controls.Add(button2);
            Controls.Add(label1);
            Controls.Add(listBox2);
            Controls.Add(listBox1);
            Controls.Add(pictureBox1);
            Name = "CLSViewer";
            Text = "ATMViewer";
            ((System.ComponentModel.ISupportInitialize)pictureBox1).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Label label2;
        private Button button1;
        private Label label3;
        private ListBox listBox3;
        private CheckBox checkBox1;
        private Button button5;
        private Button button4;
        private TextBox textBox1;
        private Button button3;
        private Button button2;
        private Label label1;
        private ListBox listBox2;
        private ListBox listBox1;
        private PictureBox pictureBox1;
    }
}