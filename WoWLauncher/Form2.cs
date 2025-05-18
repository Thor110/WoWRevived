using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WoWLauncher
{
    public partial class Form2 : Form
    {
        public Form2()
        {
            InitializeComponent();
        }
        // This method compares the registry entry with the value and sets it if they are different.
        private void registryCompare(RegistryKey key, string entry, string value)
        {
            if ((string)key.GetValue(entry)! != value)
                key.SetValue(entry, value);
        }
    }
}
