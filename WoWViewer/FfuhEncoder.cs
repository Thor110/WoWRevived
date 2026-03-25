// Compression algorithm - inverse of FfuhDecoder.cs
// Builds the same Huffman tree as the decoder, then encodes LSB-first.
namespace WoWViewer
{
    public static class FfuhEncoder
    {
        private class Node
        {
            public int Freq;
            public int Symbol; // -1 for internal nodes
            public Node? Left;
            public Node? Right;
        }

        public static byte[] Compress(byte[] data)
        {
            uint uncompressedSize = (uint)data.Length;

            // Count byte frequencies
            int[] frequencies = new int[256];
            foreach (byte b in data)
                frequencies[b]++;

            // Handle single-byte fill case (all bytes identical)
            int distinctCount = frequencies.Count(f => f > 0);
            if (distinctCount == 1)
            {
                byte fill = data[0];
                using var fillStream = new MemoryStream();
                using var fillWriter = new BinaryWriter(fillStream);
                fillWriter.Write((byte)'F');
                fillWriter.Write((byte)'F');
                fillWriter.Write((byte)'U');
                fillWriter.Write((byte)'H');
                fillWriter.Write(uncompressedSize);
                fillWriter.Write((uint)0); // compressed data size placeholder
                fillWriter.Write((uint)0); // compressedBits = 0 signals fill case
                for (int i = 0; i < 256; i++) fillWriter.Write((uint)frequencies[i]);
                fillWriter.Write(fill);
                return fillStream.ToArray();
            }

            // Build the same Huffman tree as the decoder
            Node root = BuildTree(frequencies);

            // Build code table: symbol -> (bits, length)
            var codes = new (uint bits, int length)[256];
            BuildCodes(root, 0u, 0, codes);

            // Encode bitstream LSB-first
            var bitstream = new List<byte>();
            uint currentByte = 0;
            int bitPos = 0;
            uint totalBits = 0;

            foreach (byte b in data)
            {
                var (bits, length) = codes[b];
                for (int i = 0; i < length; i++)
                {
                    int bit = (int)((bits >> i) & 1);
                    currentByte |= (uint)(bit << bitPos);
                    bitPos++;
                    totalBits++;
                    if (bitPos == 8)
                    {
                        bitstream.Add((byte)currentByte);
                        currentByte = 0;
                        bitPos = 0;
                    }
                }
            }
            // Flush remaining bits
            if (bitPos > 0)
                bitstream.Add((byte)currentByte);

            // Pad bitstream to next uint64 boundary (game expects 8-byte aligned output)
            while (bitstream.Count % 8 != 0)
                bitstream.Add(0x00);

            byte[] bitstreamBytes = bitstream.ToArray();
            uint compressedDataSize = (uint)bitstreamBytes.Length;

            // Write output
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);
            bw.Write((byte)'F');
            bw.Write((byte)'F');
            bw.Write((byte)'U');
            bw.Write((byte)'H');
            bw.Write(uncompressedSize);
            bw.Write(compressedDataSize);
            bw.Write(totalBits);
            for (int i = 0; i < 256; i++)
                bw.Write((uint)frequencies[i]);
            bw.Write(bitstreamBytes);

            return ms.ToArray();
        }

        private static Node BuildTree(int[] frequencies)
        {
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

            // Must match decoder exactly: stable sort by frequency, symbols in insertion order
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
                int insertAt = nodes.Count;
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

        // Recursively build code table - left=0 bit, right=1 bit, LSB first
        private static void BuildCodes(Node node, uint bits, int depth, (uint bits, int length)[] codes)
        {
            if (node.Left == null && node.Right == null)
            {
                // Leaf: assign code (depth==0 means single-symbol tree, use 1 bit)
                codes[node.Symbol] = (bits, depth == 0 ? 1 : depth);
                return;
            }
            if (node.Left != null)
                BuildCodes(node.Left, bits, depth + 1, codes);
            if (node.Right != null)
                BuildCodes(node.Right, bits | (1u << depth), depth + 1, codes);
        }
    }
}