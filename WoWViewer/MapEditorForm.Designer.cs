namespace WoWViewer
{
    partial class MapEditorForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MapEditorForm));
            listBox1 = new ListBox();
            listBox2 = new ListBox();
            checkBox1 = new CheckBox();
            listBox3 = new ListBox();
            label1 = new Label();
            label2 = new Label();
            label3 = new Label();
            label4 = new Label();
            SuspendLayout();
            // 
            // listBox1
            // 
            listBox1.FormattingEnabled = true;
            listBox1.ItemHeight = 15;
            listBox1.Location = new Point(12, 56);
            listBox1.Name = "listBox1";
            listBox1.Size = new Size(120, 424);
            listBox1.TabIndex = 1;
            listBox1.SelectedIndexChanged += listBox1_SelectedIndexChanged;
            // 
            // listBox2
            // 
            listBox2.FormattingEnabled = true;
            listBox2.ItemHeight = 15;
            listBox2.Location = new Point(138, 56);
            listBox2.Name = "listBox2";
            listBox2.Size = new Size(195, 424);
            listBox2.TabIndex = 2;
            listBox2.SelectedIndexChanged += listBox2_SelectedIndexChanged;
            // 
            // checkBox1
            // 
            checkBox1.AutoSize = true;
            checkBox1.Location = new Point(12, 12);
            checkBox1.Name = "checkBox1";
            checkBox1.Size = new Size(143, 19);
            checkBox1.TabIndex = 3;
            checkBox1.Text = "Martian Sector Names";
            checkBox1.UseVisualStyleBackColor = true;
            checkBox1.CheckedChanged += checkBox1_CheckedChanged;
            // 
            // listBox3
            // 
            listBox3.FormattingEnabled = true;
            listBox3.ItemHeight = 15;
            listBox3.Location = new Point(339, 56);
            listBox3.Name = "listBox3";
            listBox3.Size = new Size(120, 424);
            listBox3.TabIndex = 4;
            listBox3.SelectedIndexChanged += listBox3_SelectedIndexChanged;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(12, 38);
            label1.Name = "label1";
            label1.Size = new Size(40, 15);
            label1.TabIndex = 5;
            label1.Text = "Level :";
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(138, 38);
            label2.Name = "label2";
            label2.Size = new Size(93, 15);
            label2.TabIndex = 6;
            label2.Text = "Known Objects :";
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(339, 38);
            label3.Name = "label3";
            label3.Size = new Size(107, 15);
            label3.TabIndex = 7;
            label3.Text = "Unknown Objects :";
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.Location = new Point(193, 13);
            label4.Name = "label4";
            label4.Size = new Size(203, 15);
            label4.TabIndex = 8;
            label4.Text = "This is a work in progress map parser.";
            // 
            // MapEditorForm
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(800, 492);
            Controls.Add(label4);
            Controls.Add(label3);
            Controls.Add(label2);
            Controls.Add(label1);
            Controls.Add(listBox3);
            Controls.Add(checkBox1);
            Controls.Add(listBox2);
            Controls.Add(listBox1);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            Icon = (Icon)resources.GetObject("$this.Icon");
            Name = "MapEditorForm";
            Text = "Map Editor";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion
        private ListBox listBox1;
        private ListBox listBox2;
        private CheckBox checkBox1;
        private ListBox listBox3;
        private Label label1;
        private Label label2;
        private Label label3;
        private Label label4;
    }
}