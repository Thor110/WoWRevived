namespace WoWLauncher
{
    partial class Form2
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form2));
            trackBar1 = new TrackBar();
            label1 = new Label();
            button1 = new Button();
            label2 = new Label();
            trackBar2 = new TrackBar();
            label3 = new Label();
            trackBar3 = new TrackBar();
            label4 = new Label();
            trackBar4 = new TrackBar();
            label5 = new Label();
            trackBar5 = new TrackBar();
            label6 = new Label();
            trackBar6 = new TrackBar();
            label7 = new Label();
            trackBar7 = new TrackBar();
            label8 = new Label();
            label9 = new Label();
            label10 = new Label();
            label11 = new Label();
            label12 = new Label();
            label13 = new Label();
            label14 = new Label();
            button2 = new Button();
            label15 = new Label();
            label16 = new Label();
            label17 = new Label();
            label18 = new Label();
            label19 = new Label();
            trackBar8 = new TrackBar();
            label20 = new Label();
            trackBar9 = new TrackBar();
            trackBar10 = new TrackBar();
            label21 = new Label();
            label22 = new Label();
            ((System.ComponentModel.ISupportInitialize)trackBar1).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBar2).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBar3).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBar4).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBar5).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBar6).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBar7).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBar8).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBar9).BeginInit();
            ((System.ComponentModel.ISupportInitialize)trackBar10).BeginInit();
            SuspendLayout();
            // 
            // trackBar1
            // 
            trackBar1.AccessibleDescription = "Adjust the damage reduction divisor in increments of 100 the default value is 500 for the game.";
            trackBar1.LargeChange = 2;
            trackBar1.Location = new Point(363, 25);
            trackBar1.Minimum = 1;
            trackBar1.Name = "trackBar1";
            trackBar1.Size = new Size(327, 45);
            trackBar1.TabIndex = 0;
            trackBar1.Value = 5;
            // 
            // label1
            // 
            label1.Location = new Point(7, 25);
            label1.Name = "label1";
            label1.RightToLeft = RightToLeft.No;
            label1.Size = new Size(350, 15);
            label1.TabIndex = 1;
            label1.Text = "Damage reduction divisor :";
            // 
            // button1
            // 
            button1.Location = new Point(363, 620);
            button1.Name = "button1";
            button1.Size = new Size(75, 23);
            button1.TabIndex = 2;
            button1.Text = "Return";
            button1.UseVisualStyleBackColor = true;
            button1.Click += button1_Click;
            // 
            // label2
            // 
            label2.Location = new Point(7, 76);
            label2.Name = "label2";
            label2.RightToLeft = RightToLeft.No;
            label2.Size = new Size(350, 15);
            label2.TabIndex = 4;
            label2.Text = "Max units in sector :";
            // 
            // trackBar2
            // 
            trackBar2.AccessibleDescription = "Adjust the maximum units per sector. ( default is 15 )";
            trackBar2.LargeChange = 2;
            trackBar2.Location = new Point(363, 76);
            trackBar2.Maximum = 255;
            trackBar2.Minimum = 15;
            trackBar2.Name = "trackBar2";
            trackBar2.Size = new Size(327, 45);
            trackBar2.TabIndex = 3;
            trackBar2.Value = 15;
            // 
            // label3
            // 
            label3.Location = new Point(7, 127);
            label3.Name = "label3";
            label3.RightToLeft = RightToLeft.No;
            label3.Size = new Size(350, 15);
            label3.TabIndex = 6;
            label3.Text = "Max boats in sector :";
            // 
            // trackBar3
            // 
            trackBar3.AccessibleDescription = "Adjust the maximum boats per sector. ( default is 5 )";
            trackBar3.LargeChange = 2;
            trackBar3.Location = new Point(363, 127);
            trackBar3.Maximum = 255;
            trackBar3.Minimum = 5;
            trackBar3.Name = "trackBar3";
            trackBar3.Size = new Size(327, 45);
            trackBar3.TabIndex = 5;
            trackBar3.Value = 5;
            // 
            // label4
            // 
            label4.Location = new Point(7, 229);
            label4.Name = "label4";
            label4.RightToLeft = RightToLeft.No;
            label4.Size = new Size(350, 15);
            label4.TabIndex = 10;
            label4.Text = "Martian Open Rate :";
            // 
            // trackBar4
            // 
            trackBar4.AccessibleDescription = "Adjust the Martian Open Rate research value. ( default is 10 )";
            trackBar4.LargeChange = 2;
            trackBar4.Location = new Point(363, 229);
            trackBar4.Maximum = 255;
            trackBar4.Minimum = 1;
            trackBar4.Name = "trackBar4";
            trackBar4.Size = new Size(327, 45);
            trackBar4.TabIndex = 9;
            trackBar4.Value = 10;
            // 
            // label5
            // 
            label5.Location = new Point(7, 178);
            label5.Name = "label5";
            label5.RightToLeft = RightToLeft.No;
            label5.Size = new Size(350, 15);
            label5.TabIndex = 8;
            label5.Text = "Human Open Rate :";
            // 
            // trackBar5
            // 
            trackBar5.AccessibleDescription = "Adjust the Human Open Rate research value. ( default is 20 )";
            trackBar5.LargeChange = 2;
            trackBar5.Location = new Point(363, 178);
            trackBar5.Maximum = 255;
            trackBar5.Minimum = 1;
            trackBar5.Name = "trackBar5";
            trackBar5.Size = new Size(327, 45);
            trackBar5.TabIndex = 7;
            trackBar5.Value = 20;
            // 
            // label6
            // 
            label6.Location = new Point(7, 280);
            label6.Name = "label6";
            label6.RightToLeft = RightToLeft.No;
            label6.Size = new Size(350, 15);
            label6.TabIndex = 12;
            label6.Text = "Pod Interval (hours) :";
            // 
            // trackBar6
            // 
            trackBar6.AccessibleDescription = "Adjust the Pod interval (hours) value. ( default is 24 )";
            trackBar6.LargeChange = 2;
            trackBar6.Location = new Point(363, 280);
            trackBar6.Maximum = 255;
            trackBar6.Minimum = 1;
            trackBar6.Name = "trackBar6";
            trackBar6.Size = new Size(327, 45);
            trackBar6.TabIndex = 11;
            trackBar6.Value = 24;
            // 
            // label7
            // 
            label7.Location = new Point(7, 331);
            label7.Name = "label7";
            label7.RightToLeft = RightToLeft.No;
            label7.Size = new Size(350, 15);
            label7.TabIndex = 14;
            label7.Text = "AI Hours Per Turn :";
            // 
            // trackBar7
            // 
            trackBar7.AccessibleDescription = "Adjust the AI Hours Per Turn value. ( default is 5 )";
            trackBar7.LargeChange = 2;
            trackBar7.Location = new Point(363, 331);
            trackBar7.Maximum = 255;
            trackBar7.Minimum = 1;
            trackBar7.Name = "trackBar7";
            trackBar7.Size = new Size(327, 45);
            trackBar7.TabIndex = 13;
            trackBar7.Value = 5;
            // 
            // label8
            // 
            label8.AutoSize = true;
            label8.Location = new Point(696, 25);
            label8.Name = "label8";
            label8.Size = new Size(38, 15);
            label8.TabIndex = 15;
            label8.Text = "label8";
            // 
            // label9
            // 
            label9.AutoSize = true;
            label9.Location = new Point(696, 76);
            label9.Name = "label9";
            label9.Size = new Size(38, 15);
            label9.TabIndex = 16;
            label9.Text = "label9";
            // 
            // label10
            // 
            label10.AutoSize = true;
            label10.Location = new Point(696, 127);
            label10.Name = "label10";
            label10.Size = new Size(44, 15);
            label10.TabIndex = 17;
            label10.Text = "label10";
            // 
            // label11
            // 
            label11.AutoSize = true;
            label11.Location = new Point(696, 178);
            label11.Name = "label11";
            label11.Size = new Size(44, 15);
            label11.TabIndex = 18;
            label11.Text = "label11";
            // 
            // label12
            // 
            label12.AutoSize = true;
            label12.Location = new Point(696, 229);
            label12.Name = "label12";
            label12.Size = new Size(44, 15);
            label12.TabIndex = 19;
            label12.Text = "label12";
            // 
            // label13
            // 
            label13.AutoSize = true;
            label13.Location = new Point(696, 280);
            label13.Name = "label13";
            label13.Size = new Size(44, 15);
            label13.TabIndex = 20;
            label13.Text = "label13";
            // 
            // label14
            // 
            label14.AutoSize = true;
            label14.Location = new Point(696, 331);
            label14.Name = "label14";
            label14.Size = new Size(44, 15);
            label14.TabIndex = 21;
            label14.Text = "label14";
            // 
            // button2
            // 
            button2.AccessibleDescription = "Restore the settings to their default state.";
            button2.Location = new Point(621, 620);
            button2.Name = "button2";
            button2.Size = new Size(167, 23);
            button2.TabIndex = 22;
            button2.Text = "Restore Default Settings";
            button2.UseVisualStyleBackColor = true;
            button2.Click += button2_Click;
            // 
            // label15
            // 
            label15.AutoSize = true;
            label15.Location = new Point(66, 575);
            label15.Name = "label15";
            label15.Size = new Size(500, 15);
            label15.TabIndex = 23;
            label15.Text = "These settings are all available in the registry entry for the game, these are the default settings.";
            // 
            // label16
            // 
            label16.AutoSize = true;
            label16.Location = new Point(117, 590);
            label16.Name = "label16";
            label16.Size = new Size(403, 15);
            label16.TabIndex = 24;
            label16.Text = "I suggest leaving them as they are unless you have played the game before.";
            // 
            // label17
            // 
            label17.AutoSize = true;
            label17.Location = new Point(696, 433);
            label17.Name = "label17";
            label17.Size = new Size(44, 15);
            label17.TabIndex = 30;
            label17.Text = "label17";
            // 
            // label18
            // 
            label18.AutoSize = true;
            label18.Location = new Point(696, 382);
            label18.Name = "label18";
            label18.Size = new Size(44, 15);
            label18.TabIndex = 29;
            label18.Text = "label18";
            // 
            // label19
            // 
            label19.Location = new Point(7, 433);
            label19.Name = "label19";
            label19.RightToLeft = RightToLeft.No;
            label19.Size = new Size(350, 15);
            label19.TabIndex = 28;
            label19.Text = "AI strength table Martian multiplier :";
            // 
            // trackBar8
            // 
            trackBar8.AccessibleDescription = "Adjust the AI Martian strength table multiplier.";
            trackBar8.LargeChange = 2;
            trackBar8.Location = new Point(363, 433);
            trackBar8.Maximum = 300;
            trackBar8.Minimum = 10;
            trackBar8.Name = "trackBar8";
            trackBar8.Size = new Size(327, 45);
            trackBar8.TabIndex = 27;
            trackBar8.Value = 200;
            // 
            // label20
            // 
            label20.Location = new Point(7, 382);
            label20.Name = "label20";
            label20.RightToLeft = RightToLeft.No;
            label20.Size = new Size(350, 15);
            label20.TabIndex = 26;
            label20.Text = "AI strength table Human multiplier :";
            // 
            // trackBar9
            // 
            trackBar9.AccessibleDescription = "Adjust the AI Human strength table multiplier.";
            trackBar9.LargeChange = 2;
            trackBar9.Location = new Point(363, 382);
            trackBar9.Maximum = 300;
            trackBar9.Minimum = 10;
            trackBar9.Name = "trackBar9";
            trackBar9.Size = new Size(327, 45);
            trackBar9.TabIndex = 25;
            trackBar9.Value = 100;
            // 
            // trackBar10
            // 
            trackBar10.AccessibleDescription = "Adjust the turret build limit.";
            trackBar10.LargeChange = 2;
            trackBar10.Location = new Point(363, 484);
            trackBar10.Maximum = 50;
            trackBar10.Minimum = 12;
            trackBar10.Name = "trackBar10";
            trackBar10.Size = new Size(327, 45);
            trackBar10.TabIndex = 31;
            trackBar10.Value = 50;
            // 
            // label21
            // 
            label21.AutoSize = true;
            label21.Location = new Point(696, 484);
            label21.Name = "label21";
            label21.Size = new Size(44, 15);
            label21.TabIndex = 32;
            label21.Text = "label21";
            // 
            // label22
            // 
            label22.Location = new Point(7, 484);
            label22.Name = "label22";
            label22.RightToLeft = RightToLeft.No;
            label22.Size = new Size(350, 15);
            label22.TabIndex = 33;
            label22.Text = "Turret Build Limit :";
            // 
            // Form2
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 655);
            Controls.Add(label22);
            Controls.Add(label21);
            Controls.Add(trackBar10);
            Controls.Add(label17);
            Controls.Add(label18);
            Controls.Add(label19);
            Controls.Add(trackBar8);
            Controls.Add(label20);
            Controls.Add(trackBar9);
            Controls.Add(label16);
            Controls.Add(label15);
            Controls.Add(button2);
            Controls.Add(label14);
            Controls.Add(label13);
            Controls.Add(label12);
            Controls.Add(label11);
            Controls.Add(label10);
            Controls.Add(label9);
            Controls.Add(label8);
            Controls.Add(label7);
            Controls.Add(trackBar7);
            Controls.Add(label6);
            Controls.Add(trackBar6);
            Controls.Add(label4);
            Controls.Add(trackBar4);
            Controls.Add(label5);
            Controls.Add(trackBar5);
            Controls.Add(label3);
            Controls.Add(trackBar3);
            Controls.Add(label2);
            Controls.Add(trackBar2);
            Controls.Add(button1);
            Controls.Add(label1);
            Controls.Add(trackBar1);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            Icon = (Icon)resources.GetObject("$this.Icon");
            Name = "Form2";
            Text = "Advanced Settings";
            ((System.ComponentModel.ISupportInitialize)trackBar1).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBar2).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBar3).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBar4).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBar5).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBar6).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBar7).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBar8).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBar9).EndInit();
            ((System.ComponentModel.ISupportInitialize)trackBar10).EndInit();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private TrackBar trackBar1;
        private Label label1;
        private Button button1;
        private Label label2;
        private TrackBar trackBar2;
        private Label label3;
        private TrackBar trackBar3;
        private Label label4;
        private TrackBar trackBar4;
        private Label label5;
        private TrackBar trackBar5;
        private Label label6;
        private TrackBar trackBar6;
        private Label label7;
        private TrackBar trackBar7;
        private Label label8;
        private Label label9;
        private Label label10;
        private Label label11;
        private Label label12;
        private Label label13;
        private Label label14;
        private Button button2;
        private Label label15;
        private Label label16;
        private Label label17;
        private Label label18;
        private Label label19;
        private TrackBar trackBar8;
        private Label label20;
        private TrackBar trackBar9;
        private TrackBar trackBar10;
        private Label label21;
        private Label label22;
    }
}