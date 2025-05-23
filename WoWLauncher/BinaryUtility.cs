/// <summary>
/// The BinaryUtility class is used to replace bytes at specified locations in a file.
/// </summary>
public static class BinaryUtility
{
    /// <summary>
    /// The ReplaceBytes method opens the relevant file to replace bytes in.
    /// </summary>
    /// <param name="replacements">The bytes to replace and the address at which to replace them.</param>
    /// <param name="filename">The BinaryWriter Object.</param>
    public static void ReplaceBytes(List<Tuple<long, byte[]>> replacements, string filename)
    {
        using (var stream = new FileStream(filename, FileMode.Open, FileAccess.ReadWrite))
        {
            using (var reader = new BinaryReader(stream))
            {
                using (var writer = new BinaryWriter(stream))
                {
                    Replace(reader, writer, replacements);
                }
            }
        }
    }
    /// <summary>
    /// The Replace method is called by the ReplaceBytes method.
    /// </summary>
    /// <param name="reader">The BinaryReader Object.</param>
    /// <param name="writer">The BinaryWriter Object.</param>
    /// <param name="replacements">The bytes to replace and the address at which to replace them.</param>
    public static void Replace(BinaryReader reader, BinaryWriter writer, IEnumerable<Tuple<long, byte[]>> replacements)
    {
        byte[] bytes = new byte[reader.BaseStream.Length];
        reader.BaseStream.Position = 0;
        reader.Read(bytes, 0, bytes.Length);
        foreach (var replacement in replacements)
        {
            Array.Copy(replacement.Item2, 0, bytes, replacement.Item1, replacement.Item2.Length);
        }
        writer.BaseStream.Position = 0;
        writer.BaseStream.SetLength(0);
        writer.Write(bytes);
    }
}