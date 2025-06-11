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
            comboBox4 = new ComboBox();
            label4 = new Label();
            checkBox3 = new CheckBox();
            button5 = new Button();
            button6 = new Button();
            checkBox4 = new CheckBox();
            button7 = new Button();
            ((System.ComponentModel.ISupportInitialize)pictureBox1).BeginInit();
            SuspendLayout();
            // 
            // button1
            // 
            button1.AccessibleDescription = "";
            button1.Location = new Point(122, 325);
            button1.Name = "button1";
            button1.Size = new Size(195, 23);
            button1.TabIndex = 0;
            button1.Text = "Start Human Game";
            button1.UseVisualStyleBackColor = true;
            button1.Click += button1_Click;
            // 
            // button2
            // 
            button2.Location = new Point(122, 354);
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
            button3.Location = new Point(122, 383);
            button3.Name = "button3";
            button3.Size = new Size(195, 23);
            button3.TabIndex = 3;
            button3.Text = "Configuration Settings";
            button3.UseVisualStyleBackColor = true;
            button3.Click += button3_Click;
            // 
            // button4
            // 
            button4.Location = new Point(122, 470);
            button4.Name = "button4";
            button4.Size = new Size(195, 23);
            button4.TabIndex = 4;
            button4.Text = "Exit";
            button4.UseVisualStyleBackColor = true;
            button4.Click += button4_Click;
            // 
            // checkBox1
            // 
            checkBox1.AccessibleDescription = "Enable or disable multiplayer. ( This option messes with single player resume and save campaign options, careful! )";
            checkBox1.AutoSize = true;
            checkBox1.Location = new Point(11, 322);
            checkBox1.Name = "checkBox1";
            checkBox1.Size = new Size(150, 19);
            checkBox1.TabIndex = 5;
            checkBox1.Text = "Enable Network Version";
            checkBox1.UseVisualStyleBackColor = true;
            checkBox1.Visible = false;
            // 
            // checkBox2
            // 
            checkBox2.AccessibleDescription = "Enable or disable fullscreen.";
            checkBox2.AutoSize = true;
            checkBox2.Location = new Point(167, 322);
            checkBox2.Name = "checkBox2";
            checkBox2.Size = new Size(83, 19);
            checkBox2.TabIndex = 6;
            checkBox2.Text = "Full Screen";
            checkBox2.UseVisualStyleBackColor = true;
            checkBox2.Visible = false;
            // 
            // comboBox1
            // 
            comboBox1.AccessibleDescription = "Language options are not available, if you have a version other than English, let us know! Thanks.";
            comboBox1.BackColor = SystemColors.Window;
            comboBox1.FormattingEnabled = true;
            comboBox1.Location = new Point(86, 350);
            comboBox1.Name = "comboBox1";
            comboBox1.Size = new Size(121, 23);
            comboBox1.TabIndex = 7;
            comboBox1.Visible = false;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(21, 354);
            label1.Name = "label1";
            label1.Size = new Size(59, 15);
            label1.TabIndex = 8;
            label1.Text = "Language";
            label1.Visible = false;
            // 
            // comboBox2
            // 
            comboBox2.AccessibleDescription = "The resolution for the game.";
            comboBox2.FormattingEnabled = true;
            comboBox2.Location = new Point(291, 350);
            comboBox2.Name = "comboBox2";
            comboBox2.Size = new Size(121, 23);
            comboBox2.TabIndex = 9;
            comboBox2.Visible = false;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(222, 354);
            label2.Name = "label2";
            label2.Size = new Size(63, 15);
            label2.TabIndex = 10;
            label2.Text = "Resolution";
            label2.Visible = false;
            // 
            // comboBox3
            // 
            comboBox3.AccessibleDescription = "The refresh rate for the game. ( Higher than 30 might effect game stability )";
            comboBox3.FormattingEnabled = true;
            comboBox3.Location = new Point(291, 379);
            comboBox3.Name = "comboBox3";
            comboBox3.Size = new Size(121, 23);
            comboBox3.TabIndex = 11;
            comboBox3.Visible = false;
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(213, 383);
            label3.Name = "label3";
            label3.Size = new Size(72, 15);
            label3.TabIndex = 12;
            label3.Text = "Refresh Rate";
            label3.Visible = false;
            // 
            // comboBox4
            // 
            comboBox4.AccessibleDescription = "Simple difficulty settings.";
            comboBox4.FormattingEnabled = true;
            comboBox4.Location = new Point(86, 379);
            comboBox4.Name = "comboBox4";
            comboBox4.Size = new Size(121, 23);
            comboBox4.TabIndex = 13;
            comboBox4.Visible = false;
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.Location = new Point(25, 383);
            label4.Name = "label4";
            label4.Size = new Size(55, 15);
            label4.TabIndex = 14;
            label4.Text = "Difficulty";
            label4.Visible = false;
            // 
            // checkBox3
            // 
            checkBox3.AccessibleDescription = "Enable or disable fog of war.";
            checkBox3.AutoSize = true;
            checkBox3.Location = new Point(256, 322);
            checkBox3.Name = "checkBox3";
            checkBox3.Size = new Size(84, 19);
            checkBox3.TabIndex = 15;
            checkBox3.Text = "Fog of War";
            checkBox3.UseVisualStyleBackColor = true;
            checkBox3.Visible = false;
            // 
            // button5
            // 
            button5.Location = new Point(122, 441);
            button5.Name = "button5";
            button5.Size = new Size(195, 23);
            button5.TabIndex = 16;
            button5.Text = "Advanced";
            button5.UseVisualStyleBackColor = true;
            button5.Visible = false;
            button5.Click += button5_Click;
            // 
            // button6
            // 
            button6.Location = new Point(122, 441);
            button6.Name = "button6";
            button6.Size = new Size(195, 23);
            button6.TabIndex = 17;
            button6.Text = "Development Tools";
            button6.UseVisualStyleBackColor = true;
            button6.Click += button6_Click;
            // 
            // checkBox4
            // 
            checkBox4.AccessibleDescription = "Enable or disable resizing of the window.";
            checkBox4.AutoSize = true;
            checkBox4.Location = new Point(346, 322);
            checkBox4.Name = "checkBox4";
            checkBox4.Size = new Size(58, 19);
            checkBox4.TabIndex = 18;
            checkBox4.Text = "Resize";
            checkBox4.UseVisualStyleBackColor = true;
            checkBox4.Visible = false;
            // 
            // button7
            // 
            button7.Enabled = false;
            button7.Location = new Point(122, 412);
            button7.Name = "button7";
            button7.Size = new Size(195, 23);
            button7.TabIndex = 19;
            button7.Text = "Keyboard Shortcuts";
            button7.UseVisualStyleBackColor = true;
            button7.Click += button7_Click;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(424, 504);
            Controls.Add(button7);
            Controls.Add(checkBox4);
            Controls.Add(button6);
            Controls.Add(button5);
            Controls.Add(checkBox3);
            Controls.Add(label4);
            Controls.Add(comboBox4);
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
        private ComboBox comboBox4;
        private Label label4;
        private CheckBox checkBox3;
        private Button button5;
        private Button button6;
        private CheckBox checkBox4;
        private Button button7;
    }
}
