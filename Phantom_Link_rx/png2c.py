import sys
from PIL import Image
def convert(image_path, output_path):
    img = Image.open(image_path).convert('1')
    width, height = img.size
    pixels = img.load()
    output = "const unsigned char my_image[1024] = {\n"
    for y in range(0, height, 8):
        for x in range(width):
            byte = 0
            for bit in range(8):
                if y + bit < height and pixels[x, y + bit] > 0:
                    byte |= (1 << bit)
            output += f"0x{byte:02X}, "
        output += "\n"
    output += "};\n"
    with open(output_path, 'w') as f:
        f.write(output)
convert(sys.argv[1], sys.argv[2])
