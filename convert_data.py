import numpy as np
import matplotlib.pyplot as plt
import random
from pathlib import Path
from PIL import Image, ImageOps

DATA_PATH = Path("dataset")
CLASSES = ("diamonds", "clubs", "hearts", "spades")
RESOLUTION = 64 # Images get resized to RESOLUTION x RESOLUTION pixels
WHITE_THRESHOLD = 170 # The grayscale pixel value needed to qualify as white
TRAINING_RATIO = 0.8 # The ratio of the data that is used for training (the rest is used for validation)

def file_to_bits(image):
    image = ImageOps.exif_transpose(image) # Photos taken with phone contain information about if they need to be rotated
    image = image.convert("L").resize((RESOLUTION, RESOLUTION)) # Grayscale and desired resolution
    image_array = np.asarray(image)
    #return image_array / 255.0
    return (image_array > WHITE_THRESHOLD).astype(np.uint8)


def visualise_images(images):
    if not isinstance(images, (list, tuple)):
        images = [images]

    for image in images:
        image = np.array(image, dtype=np.uint8)
        bits = np.unpackbits(image)[:RESOLUTION*RESOLUTION]
        image = bits.reshape((RESOLUTION, RESOLUTION))
        
        plt.figure()
        plt.imshow(image, cmap='gray', vmin=0, vmax=1)
        plt.axis('off')
    
    plt.show()

def bits_to_bytes(array):
    bytes_array = []
    for row in array:
        byte = 0
        bits_added = 0
        for i, bit in enumerate(row):
            byte = (byte << 1) | bit
            bits_added += 1
            if (i + 1) % 8 == 0:
                bytes_array.append(byte)
                byte = 0
                bits_added = 0
        if bits_added > 0:
            byte = byte << (8 - bits_added) # Shift the leftover bits so they are padded with 0 to the right instead of left
            bytes_array.append(byte)
    return bytes_array

def load_images():
    images = []
    labels = []
    for label, class_name in enumerate(CLASSES):
        for file_path in DATA_PATH.joinpath(class_name).glob("*.jpeg"):
            image = Image.open(file_path)
            image_array = file_to_bits(image)
            packed_image = bits_to_bytes(image_array)
            images.append(packed_image)
            labels.append(label)
    return np.asarray(images), np.asarray(labels)

def train_val_split(images, labels):
    images_and_labels = list(zip(images, labels))
    random.shuffle(images_and_labels)
    images, labels = zip(*images_and_labels)
    images = np.asarray(images)
    labels = np.asarray(labels)

    first_val_index = int(TRAINING_RATIO * images.shape[0])
    train_images = images[:first_val_index]
    train_labels = labels[:first_val_index]
    val_images = images[first_val_index:]
    val_labels = labels[first_val_index:]

    return train_images, train_labels, val_images, val_labels


def create_file_content(images, labels, data_group):
    content = f"\nconst uint8_t {data_group}Images[{images.shape[0]}][{images.shape[1]}] = {{\n"
    for image in images:
        content += "    {"
        content += ", ".join(str(byte) for byte in image)
        content += "},\n"
    content += "};\n"

    content += f"\nconst int {data_group}Labels[{labels.shape[0]}] = {{\n    "
    content += ", ".join(str(l) for l in labels)
    content += "\n};\n"

    return content

def write_to_file(content_tuple):
    with open("data.h", "w") as file:
        for content in content_tuple:
            file.write(content)


images, labels = load_images()
train_images, train_labels, val_images, val_labels = train_val_split(images, labels)
train_data_string = create_file_content(train_images, train_labels, "train")
val_data_string = create_file_content(val_images, val_labels, "val")
definitions = f"#define NBR_TRAIN_IMAGES {train_images.shape[0]}\n#define NBR_VAL_IMAGES {val_images.shape[0]}\n"
write_to_file((definitions, train_data_string, val_data_string))
