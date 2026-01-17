/// <summary>
/// The BinaryUtility class is used to replace bytes at specified locations in a file.
/// </summary>
public static class BinaryUtility
{
    /// <summary>
    /// The ReplaceByte method opens the relevant file to replace a byte in.
    /// </summary>
    /// <param name="patches">The byte to replace and the address at which to replace it.</param>
    /// <param name="filename">The BinaryWriter Object.</param>
    public static void ReplaceByte(List<(long Offset, byte Value)> patches, string filename)
    {
        using var fs = new FileStream(filename, FileMode.Open, FileAccess.Write, FileShare.None);

        foreach (var (offset, value) in patches)
        {
            fs.Seek(offset, SeekOrigin.Begin);
            fs.WriteByte(value);
        }
    }
    /// <summary>
    /// Reads a single byte from *path* at *offset* and disposes the stream automatically.
    /// </summary>
    public static byte ReadByteAtOffset(string path, long offset)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read);
        using var reader = new BinaryReader(stream);
        stream.Seek(offset, SeekOrigin.Begin);  // seek to the absolute offset
        return reader.ReadByte();               // read the byte
    }
}