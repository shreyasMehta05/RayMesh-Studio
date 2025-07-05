import os
import sys
try:
    from PIL import Image
    print("PIL is installed, proceeding...")
except ImportError:
    print("PIL (Pillow) is required. Please install it using: pip install Pillow")
    sys.exit(1)

def convert_ppm_to_png(input_dir, output_dir):
    """Convert all PPM images in input_dir to PNG in output_dir, preserving directory structure."""
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file.endswith(".ppm"):
                input_path = os.path.join(root, file)
                
                # Create corresponding output path with .png extension
                rel_path = os.path.relpath(input_path, input_dir)
                output_path = os.path.join(output_dir, os.path.splitext(rel_path)[0] + ".png")
                
                # Create output directory if it doesn't exist
                os.makedirs(os.path.dirname(output_path), exist_ok=True)
                
                try:
                    print(f"Converting {input_path} to {output_path}")
                    img = Image.open(input_path)
                    img.save(output_path)
                    print(f"Converted: {output_path}")
                except Exception as e:
                    print(f"Error converting {input_path}: {e}")

if __name__ == "__main__":
    convert_ppm_to_png("outputs", "png_outputs")
    print("Conversion complete!")
