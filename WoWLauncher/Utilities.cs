using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WoWLauncher
{
    internal class Utilities
    {
        public static bool IsDirectoryWritable(string dirPath)
        {
            try
            {
                string testFile = Path.Combine(dirPath, "write_test.tmp");
                File.WriteAllText(testFile, "test");
                File.Delete(testFile);
                return true;
            }
            catch { return false; }
        }
    }
}
