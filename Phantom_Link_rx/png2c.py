import sys
from PIL import Image

def convert_font(image_path, output_path):
    # Convert to 1-bit monochrome
    img = Image.open(image_path).convert('1')
    width, height = img.size
    pixels = img.load()

    char_start = -1
    chars = []

    # Scan columns to find characters (black columns are separators)
    for x in range(width):
        is_empty = True
        for y in range(height):
            if pixels[x, y] > 0: # If pixel is white
                is_empty = False
                break
        
        if not is_empty and char_start == -1:
            char_start = x
        elif is_empty and char_start != -1:
            chars.append((char_start, x))
            char_start = -1

    if char_start != -1:
        chars.append((char_start, width))

    # Generate C code
    output = "const unsigned char font_image[] = {\n"
    widths = []
    offsets = []
    current_offset = 0

    for start, end in chars:
        char_width = end - start
        widths.append(char_width)
        offsets.append(current_offset)
        current_offset += char_width

        # Extract the columns (8-bit vertical bytes)
        for x in range(start, end):
            byte = 0
            for bit in range(min(8, height)):
                if pixels[x, bit] > 0:
                    byte |= (1 << bit)
            output += f"0x{byte:02X}, "
        output += "\n"
    output += "};\n\n"

    # Output arrays for widths and offsets
    output += f"const int font_widths[] = {{{', '.join(map(str, widths))}}};\n"
    output += f"const int font_offsets[] = {{{', '.join(map(str, offsets))}}};\n"
    output += f"const int font_char_count = {len(chars)};\n"

    with open(output_path, 'w') as f:
        f.write(output)

    print(f"Generated {len(chars)} characters.")

if __name__ == "__main__":
    convert_font(sys.argv[1], sys.argv[2])