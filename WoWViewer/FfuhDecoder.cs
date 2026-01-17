namespace WoWViewer
{
    public class FfuhDecoder
    {
        private readonly byte[] huffmanTable;
        private readonly byte[] compressed;

        public FfuhDecoder(byte[] huffmanTable, byte[] compressed)
        {
            this.huffmanTable = huffmanTable;
            this.compressed = compressed;
        }

        public byte[] Decompress(int decompressedSize)
        {
            var output = new byte[decompressedSize];
            int readPtr = 0;
            int writePtr = 0;
            uint bitBuffer = BitConverter.ToUInt32(compressed, readPtr);
            int bitCount = 0;

            while (writePtr < decompressedSize)
            {
                int index = (int)(bitBuffer >> bitCount) & 0xFF;

                int tableOffset = index * 3;
                byte value = huffmanTable[tableOffset];
                byte bitLen = huffmanTable[tableOffset + 1];

                output[writePtr++] = value;
                bitCount += bitLen;

                while (bitCount >= 8)
                {
                    readPtr++;
                    if (readPtr + 4 <= compressed.Length)
                    {
                        bitBuffer = BitConverter.ToUInt32(compressed, readPtr);
                    }
                    bitCount -= 8;
                }
            }

            return output;
        }
    }

}
