namespace WoWLauncher
{
    partial class Form1
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            button1 = new Button();
            button2 = new Button();
            pictureBox1 = new PictureBox();
            button3 = new Button();
            button4 = new Button();
            checkBox1 = new CheckBox();
            checkBox2 = new CheckBox();
            comboBox1 = new ComboBox();
            label1 = new Label();
            comboBox2 = new ComboBox();
            label2 = new Label();
            comboBox3 = new ComboBox();
            label3 = new Label();
            ((System.ComponentModel.ISupportInitialize)pictureBox1).BeginInit();
            SuspendLayout();
            // 
            // button1
            // 
            button1.AccessibleDescription = "";
            button1.Location = new Point(122, 331);
            button1.Name = "button1";
            button1.Size = new Size(195, 23);
            button1.TabIndex = 0;
            button1.Text = "Start Human Game";
            button1.UseVisualStyleBackColor = true;
            button1.Click += button1_Click;
            // 
            // button2
            // 
            button2.Location = new Point(122, 360);
            button2.Name = "button2";
            button2.Size = new Size(195, 23);
            button2.TabIndex = 1;
            button2.Text = "Start Martian Game";
            button2.UseVisualStyleBackColor = true;
            button2.Click += button2_Click;
            // 
            // pictureBox1
            // 
            pictureBox1.BackgroundImage = Properties.Resources.Bitmap1;
            pictureBox1.Location = new Point(12, 12);
            pictureBox1.Name = "pictureBox1";
            pictureBox1.Size = new Size(400, 300);
            pictureBox1.TabIndex = 2;
            pictureBox1.TabStop = false;
            // 
            // button3
            // 
            button3.Location = new Point(122, 389);
            button3.Name = "button3";
            button3.Size = new Size(195, 23);
            button3.TabIndex = 3;
            button3.Text = "Configuration Settings";
            button3.UseVisualStyleBackColor = true;
            button3.Click += button3_Click;
            // 
            // button4
            // 
            button4.Location = new Point(122, 418);
            button4.Name = "button4";
            button4.Size = new Size(195, 23);
            button4.TabIndex = 4;
            button4.Text = "Exit";
            button4.UseVisualStyleBackColor = true;
            button4.Click += button4_Click;
            // 
            // checkBox1
            // 
            checkBox1.AccessibleDescription = "Enable or disable multiplayer.";
            checkBox1.AutoSize = true;
            checkBox1.Location = new Point(12, 318);
            checkBox1.Name = "checkBox1";
            checkBox1.Size = new Size(150, 19);
            checkBox1.TabIndex = 5;
            checkBox1.Text = "Enable Network Version";
            checkBox1.UseVisualStyleBackColor = true;
            checkBox1.CheckedChanged += checkBox1_CheckedChanged;
            // 
            // checkBox2
            // 
            checkBox2.AccessibleDescription = "Enable or disable fullscreen.";
            checkBox2.AutoSize = true;
            checkBox2.Location = new Point(12, 343);
            checkBox2.Name = "checkBox2";
            checkBox2.Size = new Size(83, 19);
            checkBox2.TabIndex = 6;
            checkBox2.Text = "Full Screen";
            checkBox2.UseVisualStyleBackColor = true;
            checkBox2.CheckedChanged += checkBox2_CheckedChanged;
            // 
            // comboBox1
            // 
            comboBox1.AccessibleDescription = "Language options are not available, if you have a version other than English, let us know! Thanks.";
            comboBox1.BackColor = SystemColors.Window;
            comboBox1.FormattingEnabled = true;
            comboBox1.Location = new Point(291, 318);
            comboBox1.Name = "comboBox1";
            comboBox1.Size = new Size(121, 23);
            comboBox1.TabIndex = 7;
            comboBox1.SelectedIndexChanged += comboBox1_SelectedIndexChanged;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(226, 322);
            label1.Name = "label1";
            label1.Size = new Size(59, 15);
            label1.TabIndex = 8;
            label1.Text = "Language";
            // 
            // comboBox2
            // 
            comboBox2.AccessibleDescription = "The resolution for the game.";
            comboBox2.FormattingEnabled = true;
            comboBox2.Location = new Point(291, 347);
            comboBox2.Name = "comboBox2";
            comboBox2.Size = new Size(121, 23);
            comboBox2.TabIndex = 9;
            comboBox2.SelectedIndexChanged += comboBox2_SelectedIndexChanged;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(222, 351);
            label2.Name = "label2";
            label2.Size = new Size(63, 15);
            label2.TabIndex = 10;
            label2.Text = "Resolution";
            // 
            // comboBox3
            // 
            comboBox3.AccessibleDescription = "The refresh rate for the game. ( Higher than 30 might effect game stability )";
            comboBox3.FormattingEnabled = true;
            comboBox3.Location = new Point(291, 376);
            comboBox3.Name = "comboBox3";
            comboBox3.Size = new Size(121, 23);
            comboBox3.TabIndex = 11;
            comboBox3.SelectedIndexChanged += comboBox3_SelectedIndexChanged;
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(213, 380);
            label3.Name = "label3";
            label3.Size = new Size(72, 15);
            label3.TabIndex = 12;
            label3.Text = "Refresh Rate";
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(424, 453);
            Controls.Add(label3);
            Controls.Add(comboBox3);
            Controls.Add(label2);
            Controls.Add(comboBox2);
            Controls.Add(label1);
            Controls.Add(comboBox1);
            Controls.Add(checkBox2);
            Controls.Add(checkBox1);
            Controls.Add(button4);
            Controls.Add(button3);
            Controls.Add(pictureBox1);
            Controls.Add(button2);
            Controls.Add(button1);
            Icon = (Icon)resources.GetObject("$this.Icon");
            Name = "Form1";
            Text = "Jeff Wayne's 'The War Of The Worlds'";
            ((System.ComponentModel.ISupportInitialize)pictureBox1).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Button button1;
        private Button button2;
        private PictureBox pictureBox1;
        private Button button3;
        private Button button4;
        private CheckBox checkBox1;
        private CheckBox checkBox2;
        private ComboBox comboBox1;
        private Label label1;
        private ComboBox comboBox2;
        private Label label2;
        private ComboBox comboBox3;
        private Label label3;
    }
}
