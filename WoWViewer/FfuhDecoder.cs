// Decompression algorithm reverse engineered from WoW.exe with assistance from Claude.ai (Anthropic)
// The disassembly was traced through IDA Pro to locate the Huffman decompression loop at 0x415A80,
// from which Claude derived and verified a Python implementation before porting to C#.
namespace WoWViewer
{
    public static class FfuhDecoder
    {
        private class Node
        {
            public int Freq;
            public int Symbol; // -1 for internal nodes
            public Node? Left;
            public Node? Right;
        }

        // Returns true if this file is FFUH compressed
        public static bool IsCompressed(byte[] data) =>
            data.Length >= 4 && data[0] == 'F' && data[1] == 'F' && data[2] == 'U' && data[3] == 'H';

        // Decompress an FFUH compressed file, returns raw decompressed bytes
        public static byte[] Decompress(byte[] data)
        {
            if (!IsCompressed(data)) { return data; } // thor110 edited line

            int offset = 4;

            uint uncompressedSize = BitConverter.ToUInt32(data, offset); offset += 4;
            uint unknown1 = BitConverter.ToUInt32(data, offset); offset += 4; // compressed data size
            uint compressedBits = BitConverter.ToUInt32(data, offset); offset += 4;

            // Single byte fill case - entire output is one repeated byte
            if (compressedBits == 0)
            {
                byte fill = data[offset];
                return Enumerable.Repeat(fill, (int)uncompressedSize).ToArray();
            }

            // Read 256 frequency DWORDs
            int[] frequencies = new int[256];
            for (int i = 0; i < 256; i++)
            {
                frequencies[i] = (int)BitConverter.ToUInt32(data, offset);
                offset += 4;
            }

            // Bitstream starts immediately after frequency table
            byte[] bitstream = new byte[data.Length - offset];
            Array.Copy(data, offset, bitstream, 0, bitstream.Length);

            // Build Huffman tree matching game's algorithm
            Node root = BuildTree(frequencies);

            // Decode - LSB first, 0 = left, 1 = right
            byte[] output = new byte[uncompressedSize];
            int writePos = 0;
            int bitPos = 0;
            int totalBits = bitstream.Length * 8;

            while (writePos < uncompressedSize)
            {
                Node node = root;
                while (node.Left != null || node.Right != null)
                {
                    if (bitPos >= totalBits)
                        throw new InvalidDataException($"Ran out of bits at position {bitPos}");

                    int byteIndex = bitPos / 8;
                    int bitIndex = bitPos % 8; // LSB first
                    int bit = (bitstream[byteIndex] >> bitIndex) & 1;
                    bitPos++;

                    node = bit == 0 ? node.Left! : node.Right!;
                }
                output[writePos++] = (byte)node.Symbol;
            }

            return output;
        }

        private static Node BuildTree(int[] frequencies)
        {
            // Build initial list of leaf nodes in symbol order, sort by frequency (stable)
            var nodes = new List<Node>();
            for (int symbol = 0; symbol < 256; symbol++)
            {
                if (frequencies[symbol] > 0)
                    nodes.Add(new Node { Freq = frequencies[symbol], Symbol = symbol });
            }

            if (nodes.Count == 0)
                throw new InvalidDataException("Empty frequency table.");

            if (nodes.Count == 1)
                return nodes[0];

            // Stable sort by frequency - preserves insertion order for equal frequencies
            nodes = nodes.OrderBy(n => n.Freq).ToList();

            int counter = 256;
            while (nodes.Count > 1)
            {
                Node left = nodes[0]; nodes.RemoveAt(0);
                Node right = nodes[0]; nodes.RemoveAt(0);

                Node parent = new Node
                {
                    Freq = left.Freq + right.Freq,
                    Symbol = counter++,
                    Left = left,
                    Right = right
                };

                // Insert before first node with strictly higher frequency (stable)
                int insertAt = nodes.Count; // default to end
                for (int i = 0; i < nodes.Count; i++)
                {
                    if (parent.Freq < nodes[i].Freq)
                    {
                        insertAt = i;
                        break;
                    }
                }
                nodes.Insert(insertAt, parent);
            }

            return nodes[0];
        }
    }
}